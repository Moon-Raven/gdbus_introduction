/* example4.c */
#include <stdio.h>
#include <glib.h>
#include <unistd.h>

#define THREAD_NUM 3
#define INTERVAL 1000

gboolean func(gpointer user_data)
{
    guint sleep_seconds = 1;
    guint id = *((guint*) user_data);
    printf("%u: Hello from func! Entering sleep...\n", id);
    sleep(sleep_seconds); // This sleep is problematic
    printf("%u: Sleep complete. Goodbye from func!\n", id);
    return TRUE; // TRUE indicates that the function should be run again
}


gpointer thread_func(gpointer data)
{
    g_timeout_add(INTERVAL, func, data);
}


int main()
{
    GThread *threads[THREAD_NUM];
    guint thread_data[THREAD_NUM];
    guint i;

    for(i = 0; i < THREAD_NUM; i++)
    {
        thread_data[i] = i;
        threads[i] = g_thread_new(NULL, thread_func, &thread_data[i]);
    }

    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    return 0;
}