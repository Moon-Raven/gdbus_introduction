CC=gcc
CFLAGS= -Wall `pkg-config --cflags glib-2.0` -I. `pkg-config --cflags gio-unix-2.0`
LIBES=`pkg-config --libs glib-2.0` `pkg-config --libs gio-unix-2.0`
SOCK_PATH=~/sockets/sock01

codegen:
	gdbus-codegen --generate-c-code alarm-generated --c-generate-object-manager alarm_interface.xml
server: codegen
	$(CC) $(CFLAGS) server.c alarm-generated.c $(LIBES) -o server
	$(RM) $(SOCK_PATH)
main: codegen main.c
	$(CC) $(CFLAGS) main.c alarm-generated.c $(LIBES) -o main
clean:
	$(RM) main
	$(RM) server
	$(RM) *-generated.*
	$(RM) $(SOCK_PATH)
