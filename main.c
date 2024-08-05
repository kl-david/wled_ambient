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
    long_RGB total_rgb;
    total_rgb.r = 0;
    total_rgb.g = 0;
    total_rgb.b = 0;

    unsigned long pixel;
    int x, y;

    for (y = 0; y < scaled_height; y++) {
        for (x = 0; x < scaled_width; x++) {
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

    RGB rgb;
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
        rgb = apply_gamma_correction(rgb);
        printf("Average R: %u, Average G: %u, Average B: %u\n", rgb.r, rgb.g, rgb.b);
        post_rgb(rgb);
        nanosleep(&sleep_time, NULL);
    }

    XCloseDisplay(display);
    return 0;
}