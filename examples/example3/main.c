/* example3.c */
#include <stdio.h>
#include <glib.h>
#include <unistd.h>


gboolean func(gpointer user_data)
{
    guint id = *((guint*) user_data);
    printf("%u: Hello from func! Entering sleep...\n", id);
    sleep(5); // This sleep is problematic
    printf("%u: Sleep complete. Goodbye from func!\n", id);
    return TRUE; // TRUE indicates that the function should be run again
}


int main()
{
    guint interval = 1000;
    guint id1 = 1;
    guint id2 = 2;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add(interval, func, &id1);
    g_timeout_add(interval, func, &id2);
    g_main_loop_run(loop); // This calls runs an event servicing loop
    return 0;
}