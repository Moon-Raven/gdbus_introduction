/* example8.c */
#include <stdio.h>
#include <glib.h>
#include <unistd.h>
#include "alarm-generated.h"

#define INTERVAL   8000 // milliseconds
#define SLEEP_TIME    5 // seconds
#define SOCKET_ADDRESS "unix:path=/home/moon/sockets/sock01"


void notification_callback(OrgAlarm *object, const gchar *arg_message)
{
    printf("Alarm notification triggered: %s\n", arg_message);
}


gboolean timeout_func(gpointer user_data)
{
    guint id = *((guint*) user_data);
    GThread* thread = g_thread_self();

    printf("%u (thread %p): Hello from func! Entering sleep...\n", id, thread);
    sleep(SLEEP_TIME);
    printf("%u (thread %p): Sleep complete. Goodbye from func!\n", id, thread);

    return TRUE;
}


gpointer dbus_handler_thread(gpointer data)
{
    GError* error = NULL;
    GDBusConnection *connection;
    gulong id = 0;
    OrgAlarm* proxy = NULL;

    /* Create a new main context, and set it as the thread-default */
    GMainContext *new_context = g_main_context_new();
    g_main_context_push_thread_default(new_context);

    /* Check what is the thread-default context of this thread */
    GThread* thread = g_thread_self();
    GMainContext *global_default_context = g_main_context_default();
    GMainContext *thread_default_context = g_main_context_get_thread_default();
    g_print("Global-default context of thread %p is %p\n", thread, global_default_context);
    g_print("Thread-default context of thread %p is %p\n", thread, thread_default_context);

    connection = g_dbus_connection_new_for_address_sync(SOCKET_ADDRESS,
                                                        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                        NULL, /* GDBusAuthObserver */
                                                        NULL, /* GCancellable */
                                                        &error);
    if(connection == NULL)
    {
        printf("Connection is NULL, error=%s\n", error->message);
        return NULL;
    }
    printf("Successfully connected to the server\n");

    /* The thread-default context needs to be set prior to this point */
    error = NULL;
    proxy = org_alarm_proxy_new_sync(connection,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     NULL,
                                     "/my/object",
                                     NULL,
                                     &error);
    if(proxy == NULL)
    {
        printf("Proxy is NULL, error=%s\n", error->message);
        return NULL;
    }

    id = g_signal_connect(proxy,
                          "alarm-notification",
                          G_CALLBACK(notification_callback),
                          NULL);
    if(id == 0)
    {
        printf("Failed to connect signal\n");
        return NULL;
    } 
    printf("Signal connection successful, handler_id=%lu!\n", id);

    /* This main loop iterates the thread-default context */
    GMainLoop *loop = g_main_loop_new(new_context, FALSE);
    g_main_loop_run(loop);

    return NULL;
}


int main()
{
    guint data = 42;

    /* Create thread for handling DBus-related events */
    g_thread_new(NULL, dbus_handler_thread, NULL);

    /* Initialize the periodic timeout, and start the main loop */
    g_timeout_add(INTERVAL, timeout_func, &data);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    return 0;
}