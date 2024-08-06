#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <curl/curl.h>
#include <math.h>

#define SCALE_DOWN_FACTOR 10
#define URL "leds.local/json/state"
#define GAMMA_CORRECTION 1.8

const char gamma8[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
    10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
    17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
    25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
    37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
    51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
    69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
    90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
    115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
    144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
    177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
    215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};
typedef struct
{
    unsigned char r, g, b;
}RGB;
typedef struct
{
    unsigned long r, g, b;
}long_RGB;


void calculate_average_rgb(Display *display, Window window, int width, int height, RGB *rgb) {
    int scaled_width = width / SCALE_DOWN_FACTOR;
    int scaled_height = height / SCALE_DOWN_FACTOR;

    XImage *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);

    //unsigned long red_total = 0, green_total = 0, blue_total = 0;
    long_RGB total_rgb = {0};

    unsigned long pixel;

    for (int y = 0; y < scaled_height; y++) {
        for (int x = 0; x < scaled_width; x++) {
            pixel = XGetPixel(image, x*SCALE_DOWN_FACTOR, y*SCALE_DOWN_FACTOR);
            total_rgb.r += (pixel & image->red_mask) >> 16;
            total_rgb.g += (pixel & image->green_mask) >> 8;
            total_rgb.b += pixel & image->blue_mask;
        }
    }

    rgb->r = total_rgb.r / (scaled_width * scaled_height);
    rgb->g = total_rgb.g / (scaled_width * scaled_height);
    rgb->b = total_rgb.b / (scaled_width * scaled_height);

    XDestroyImage(image);
}

Window get_focused_window(Display *display) {
    Window focused_window;
    int revert_to;
    XGetInputFocus(display, &focused_window, &revert_to);
    return focused_window;
}

RGB apply_gamma_correction(RGB input) {
    RGB corrected;
    corrected.r = (unsigned char)(pow(input.r / 255.0, GAMMA_CORRECTION) * 255.0);
    corrected.g = (unsigned char)(pow(input.g / 255.0, GAMMA_CORRECTION) * 255.0);
    corrected.b = (unsigned char)(pow(input.b / 255.0, GAMMA_CORRECTION) * 255.0);
    return corrected;
}

RGB static_apply_gamma_correction(RGB input) {
    RGB ret;
    ret.r = gamma8[input.r];
    ret.g = gamma8[input.g];
    ret.b = gamma8[input.b];

    return ret;
}

void post_rgb(RGB rgb){
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();

    if(!curl){
        fprintf(stderr, "Unable to create curl");
        exit(1);
    }
    char post_data[256];
    snprintf(post_data, sizeof(post_data), "{\"seg\":[{\"col\":[[%u,%u,%u]]}]}", rgb.r, rgb.g, rgb.b);
    curl_easy_setopt(curl, CURLOPT_URL, URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
}

int main() {
    Display *display;
    int screen;
    unsigned int width, height;

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open display\n");
        exit(1);
    }

    screen = DefaultScreen(display);

    RGB rgb = {0};
    RGB old_rgb = {0};

    struct timespec sleep_time;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 33333333L; // 30 Hz refresh rate

    while (1) {
        Window focused_window = get_focused_window(display);
        if (focused_window == None) {
            printf("No window is currently focused.\n");
            nanosleep(&sleep_time, NULL);
            continue;
        }

        XWindowAttributes attributes;
        XGetWindowAttributes(display, focused_window, &attributes);
        width = attributes.width;
        height = attributes.height;

        calculate_average_rgb(display, focused_window, width, height, &rgb);
        rgb = static_apply_gamma_correction(rgb);
        if((abs(rgb.r - old_rgb.r) > 5) || (abs(rgb.g - old_rgb.g) > 5) || (abs(rgb.r - old_rgb.r) > 5)){
            //printf("change\n");
            //printf("Average R: %u, Average G: %u, Average B: %u\n", rgb.r, rgb.g, rgb.b);
            //printf("%u, %u, %u\n", abs(rgb.r - old_rgb.r), abs(rgb.g - old_rgb.g), abs(rgb.b - old_rgb.b));
            post_rgb(rgb);
            old_rgb = rgb;
        }
        nanosleep(&sleep_time, NULL);
    }

    XCloseDisplay(display);
    return 0;
}