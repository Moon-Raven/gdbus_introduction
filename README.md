## Introduction
The purpose of these examples is to provide insight into:
- Glib main loop handling (GMainLoop, GMainContext)
- GDBus bindings

## Observations
### GDBus basics
Roughly speaking, DBus is an IPC protocol that can be run across a number of different transport layers.
It can be used with a messaging bus, or as peer-to-peer communication.
When communicating peer-to-peer, an application can host a server on a transport layer, such as:
- TCP
- Unix domain sockets

A server application can wait for incoming requests from clients.
Client applications can connect to the server and exchange messages.
There are three main mechanisms that DBus defines:
- **Methods** (RPC - the client calls methods that are executed by the server)
- **Signals** (server broadcasts information to clients, expecting no replies)
- **Properties** (client asks server to provide the current value of a variable)

Servers expose objects, which have well-defined interfaces.
There are two ways in which client and server applications using DBus can be written:
1. By using the (fairly) low-level API that the gdbus library provides
2. By using the codegen tools which generate helper APIs for the specific interfaces being used,
   based on a XML definitions of the APIs

The code generation tools achieve the latter by subclassing the low-level classes that gdbus provides.
For an example of this, see example 8.
GDBus also provides debugging data by setting an environment variable to certain value.
This can be handy when diagnosing GDBus-related issues.

## GLib event handling
GLib provides a convenient way of achieving event-driven programming.
It involves usage of two important glib classes:
- ```GMainLoop```
- ```GMainContext```

```GMainContext``` is a class meant to be instantiated.
Each instance (object) of ```GMainContext``` has a set of sources attached.
Each source represents an event that can be triggered (such as a timeout expiring,
or a DBus signal being received).
Custom sources can also be written (see documentation for ```GSource```).
A ```GMainContext``` object is attached to a ```GMainLoop```.
When a ```GMainLoop``` is run, the execution of the calling function is "blocked",
and the main loop keeps iterating through the sources of the associated ```GMainContext```,
triggering any callbacks accordingly (depending on the source type).
This has one major caveat - the associated callbacks themselves must not be blocking,
otherwise they would block the polling loop of ```GMainLoop``` from executing.

Instead of creating explicit ```GMainContext```, a ```GMainLoop``` can be set up to operate
on one of the two special "default" contexts as well:
1. The global default context
2. The thread default context

Let us walk through several examples.
First, consider the following code snippet:

```c
/* example1.c */
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
```

The function ```g_timeout_add``` creates a source and attaches it to a main context.
However, the context cannot be selected - this API automatically uses the default main context.
Since we are not explicitly using any threads now, the *global* default main context is used.

The function ```g_main_loop_new``` accepts a ```GMainContext``` as an argument.
However, it can also accept NULL, leading to the default main context being used.
As a result, when the newly created ```GMainLoop``` is run, the timeout source is being iterated,
and the function ```func``` is being called periodically.

We can create multiple sources, adding them and iterating them inside the same (in this case default) context,
as shown in ```example2.c```:

```c
/* example2.c */
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
    guint id1 = 1;
    guint id2 = 2;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_timeout_add(interval, func, &id1);
    g_timeout_add(interval, func, &id2);
    g_main_loop_run(loop); // This calls runs an event servicing loop
    return 0;
}
```

However, we must be careful not to block code execution in any of the callbacks,
since it would result in blocking the polling loop of ```g_main_loop_run```.
The following (erronous) example demonstrates this:

```c
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
```

The same (unwanted) behavior occurs even if we add timeouts from different threads,
because they are still added to the *global* default main context.
The following example shows this:

```c
/* example4.c */
#include <stdio.h>
#include <glib.h>
#include <unistd.h>

#define THREAD_NUM 3
#define INTERVAL 1000

gboolean func(gpointer user_data)
{
    guint id = *((guint*) user_data);
    printf("%u: Hello from func! Entering sleep...\n", id);
    sleep(1); // This sleep is problematic
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
```

The first and foremost way to avoid this kind of problems is to make sure that
all callbacks are non-blocking.
In order to better understand ```GMainContext``` and ```GMainLoop```,
we will explore solutions which allow concurrent execution of callbacks.

In order to achieve concurrency, two instances of ```GMainLoop```
must be running in two different threads.
Each instance must have an associated ```GMainContext```, with
one source/callback tied to each.
Hence, the solution is to explicitly create two instances of ```GMainContext```,
attach them to a ```GMainLoop``` each, and run the main loops in different threads.
Example 5 shows this solution:
```c
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
    sleep(SLEEP_TIME); // This sleep is no longer (that much) problematic
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
```

At first glance, another solution seemt to be using thread default contexts
instead of global default contexts (as in example 4),
or explicit contexts (as in example 5).
However, a naive solution such as the one presented in example 6 doesn't work:

```c
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
```

Although it might seem that each thread is running a main loop for the thread-default
context of that thread, this is not true.
As can be seen from the output messages, all threads (including the main one) share
the same default context - the global one.
The first thread that comes across `g_main_loop_run` starts polling the global default context,
and the other two threads are simply suspended when they execute `g_main_loop_run`
(such behavior of this API is mildly documented).
This fact can also be asserted by calling `g_main_context_get_thread_default`, which returns null
if the default context for the currently running thread is the global-default context instead of
a thread-default context.

One would expect the correct solution is to create a thread-default context,
as shown in the incorrect example 7:

```c
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
```

But unfortunately, this doesn't work either.
At this point, the peculiarities of several GNOME APIs come into play.
Specifically, the issue is that some functions always use the global-default
main context, even if there is a thread-default context available.
```g_timeout_add``` is one example of such a function.
Hence, even if this approach would work for some GLib APIs,
it doesn't work for ```g_timeout_add```.

## Including a GDBus client
Let us look at a situation in which the approach from example 7 *would* work.
Consider a scenario in which we want concurrent execution of two event handlers:
1. A simple timeout handler, as the ones used in previous examples
2. A DBus signal handler, acting as a DBus peer-to-peer client accepting signals from a server

Example 8 shows how to achieve this in a similar fashion compared to example 7:
```c
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
```

The listed code works (despite example 7 not working) since the API
for creating the GDBus listeners (specifically ```org_alarm_proxy_new_sync```)
operates on the thread-default context, instead of the global-default context.

# Resources
- [GLib documentation notes](https://docs.gtk.org/glib/main-loop.html)
- [Gnome Developer Programming Guidelines (GLib Main Contexts)](https://developer-old.gnome.org/programming-guidelines/stable/main-contexts.html.en)
- [A book about GLib](https://people.gnome.org/~swilmet/glib-gtk-dev-platform.pdf)
- [Articles by Philip Withnall](https://tecnocode.co.uk/tag/gmainloop/)