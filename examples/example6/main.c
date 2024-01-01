/* example6.c */
#include <stdio.h>
#include <glib.h>
#include <unistd.h>

#define THREAD_NUM 3
#define INTERVAL   1000 // milliseconds
#define SLEEP_TIME 1    // seconds

gboolean func(gpointer user_data)
{
    guint id = *((guint*) user_data);
    GThread* thread = g_thread_self();

    printf("%u (thread %p): Hello from func! Entering sleep...\n", id, thread);
    sleep(SLEEP_TIME);
    printf("%u (thread%p): Sleep complete. Goodbye from func!\n", id, thread);

    return TRUE;
}


gpointer thread_func(gpointer data)
{
    GMainContext *context = g_main_context_default();
    GThread* thread = g_thread_self();
    g_print("Default context of thread %p is %p\n", thread, context);
    context = g_main_context_get_thread_default();
    g_print("Thread-default context of thread %p is %p\n", thread, context);

    g_timeout_add(INTERVAL, func, data);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);
    return NULL;
}


int main()
{
    GThread *threads[THREAD_NUM];
    guint thread_data[THREAD_NUM];
    guint i;

    GMainContext *context = g_main_context_default();
    g_print("Default context of main function is %p\n", context);

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