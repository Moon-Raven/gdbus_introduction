/* example7.c */
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
    printf("%u (thread %p): Sleep complete. Goodbye from func!\n", id, thread);

    return TRUE;
}


gpointer thread_func(gpointer data)
{
    /* Create a new main context, and set it as the thread-default */
    GMainContext *new_context = g_main_context_new();
    g_main_context_push_thread_default(new_context);

    /* Check what is the thread-default context of this thread */
    GThread* thread = g_thread_self();
    GMainContext *global_default_context = g_main_context_default();
    GMainContext *thread_default_context = g_main_context_get_thread_default();
    g_print("Global-default context of thread %p is %p\n", thread, global_default_context);
    g_print("Thread-default context of thread %p is %p\n", thread, thread_default_context);

    g_timeout_add(INTERVAL, func, data);
    // One would expect this to run the thread-default context, but it doesn't
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