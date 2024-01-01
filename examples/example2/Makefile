CC=gcc
CFLAGS= -Wall `pkg-config --cflags glib-2.0` -I. `pkg-config --cflags gio-unix-2.0`
LIBES=`pkg-config --libs glib-2.0` `pkg-config --libs gio-unix-2.0`

main: main.c
	$(CC) $(CFLAGS) main.c $(LIBES) -o main
clean:
	$(RM) main