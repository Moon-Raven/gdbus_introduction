#include <stdio.h>
#include <glib.h>
#include <unistd.h>


gboolean func(gpointer user_data)
{
    guint id = *((guint*) user_data);
    printf("%u: Hello from func\n", id);
    return TRUE; // TRUE indicates that the function should be run again
}


int main()
{
    guint interval = 1000;
    guint id = 1;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add(interval, func, &id);
    g_main_loop_run(loop); // This calls runs an event servicing loop
    return 0;
}