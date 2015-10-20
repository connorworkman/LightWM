CC=gcc
CFLAGS=-Wall
XLIB=/usr/include/X11/
LOCAL_LIBRARIES= $(XLIB)
all: basicwin.o
	$(CC) -c basicwin.c
	$(CC) basicwin.o -lX11 -o basicwin
clean:
	rm basicwin
