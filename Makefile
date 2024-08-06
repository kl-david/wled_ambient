CC		= gcc
CLFAGS	= -g

main: main.c
	$(CC) $(CLFAGS) -o main main.c -lX11 -lcurl -lm

debug: main.c
	$(CC) $(CLFAGS) -o debug main.c -lX11 -lcurl -lm -D DEBUG