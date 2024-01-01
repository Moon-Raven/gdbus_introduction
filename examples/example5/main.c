/* example5.c */
#include <stdio.h>
#include <glib.h>
#include <unistd.h>

#define THREAD_NUM 3
#define INTERVAL   1000 // milliseconds
#define SLEEP_TIME 1    // seconds

gboolean func(gpointer user_data)
{
    guint id = *((guint*) user_data);
    printf("%u: Hello from func! Entering sleep...\n", id);
    sleep(SLEEP_TIME); // This sleep is no longer problematic
    printf("%u: Sleep complete. Goodbye from func!\n", id);
    return TRUE; // TRUE indicates that the function should be run again
}


gpointer thread_func(gpointer data)
{
    GSource *timeout_source = g_timeout_source_new(INTERVAL);
    GMainContext *context = g_main_context_new();
    g_source_set_callback(timeout_source, func, data, NULL);
    g_source_attach(timeout_source, context);
    GMainLoop* loop = g_main_loop_new(context, FALSE);
    g_main_loop_run(loop);
    return NULL;
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

    for(i = 0; i < THREAD_NUM; i++)
    {
        g_thread_join(threads[i]);
    }

    return 0;
}