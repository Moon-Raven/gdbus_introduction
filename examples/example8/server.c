#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <alarm-generated.h>

#define MAX_MESSAGE_LEN 100
#define SOCKET_PATH "/home/moon/sockets/sock01"
#define MAX_CONNECTION_NUM 100
#define DEFAULT_ALARM_PERIOD 1
#define DEFAULT_ALARM_MESSAGE "Default alarm"

static gchar *opt_address = "unix:path=/home/moon/sockets/sock01";
static OrgAlarm* interface;
static guint connection_count = 0;
static GDBusConnection* connections[MAX_CONNECTION_NUM] = {NULL};

typedef struct
{
    guint period;
    gchar message[MAX_MESSAGE_LEN];
} alarm_t;

static alarm_t alarm_data =
{
    .period =  DEFAULT_ALARM_PERIOD,
    .message = DEFAULT_ALARM_MESSAGE 
};

static gboolean set_period(OrgAlarm* object,
                           GDBusMethodInvocation *invocation,
                           const gchar *arg_message,
                           const guint arg_period)
{
    gchar *response;
    response = g_strdup_printf("Ok");
    printf("Set period triggered with message=%s and period=%u\n", arg_message, arg_period);
    alarm_data.period = arg_period;
    strcpy(alarm_data.message, arg_message);
    org_alarm_complete_set_period(object, invocation, response);
    g_free(response);
    return TRUE;
}


gpointer alarm_function(gpointer data)
{
    gboolean ret_val;
    GError* error;

    while(TRUE)
    {
        printf("%s\n", alarm_data.message);
        for(guint i = 0; i < connection_count; i++)
        {

            OrgAlarmSkeleton *skeleton = ORG_ALARM_SKELETON(interface);
            error = NULL;
            ret_val = g_dbus_connection_emit_signal(
                connections[i],
                NULL,
                g_dbus_interface_skeleton_get_object_path(G_DBUS_INTERFACE_SKELETON(skeleton)),
                "org.alarm",
                "AlarmNotification",
                g_variant_new ("(s)", alarm_data.message),
                &error);

            if(!ret_val)
                printf("Emit signal not successful for connection %u, error=%s\n", i, error->message);
            else
                printf("Emit signal successful for connection %u\n", i);
        }
        sleep(alarm_data.period);
    }
}


static gboolean on_new_connection(GDBusServer *server,
                                  GDBusConnection *connection,
                                  gpointer user_data)
{
    GError *error = NULL;
    gboolean success_code;

    printf("New connection received!\n");
    connections[connection_count++] = connection;

    success_code = g_dbus_interface_skeleton_export(
        G_DBUS_INTERFACE_SKELETON(interface),
        connection,
        "/my/object",
        &error);

    if(!success_code)
    {
        printf("Skeleton export unsuccessful, error=%s\n", error->message);
        return FALSE;
    }

    return TRUE;
}


void init_interface()
{
    interface = org_alarm_skeleton_new();
    g_signal_connect(interface, "handle-set-period", G_CALLBACK(set_period), NULL);
}


void clean_socket()
{
    remove(SOCKET_PATH);
}


int main()
{
    GMainLoop *loop;
    GDBusServer *server;
    GDBusServerFlags server_flags = G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS;
    gchar *guid = g_dbus_generate_guid();
    GError *error = NULL;

    clean_socket();
    init_interface();

    printf("Starting server...\n");
    server = g_dbus_server_new_sync(opt_address,
                                     server_flags,
                                     guid,
                                     NULL, /* GDBusAuthObserver */
                                     NULL, /* GCancellable */
                                     &error);
    g_free(guid);
    g_dbus_server_start(server);

    if(server == NULL)
    {
        printf("Server is NULL; error=%s\n", error->message);
        return 1;
    }
    g_print("Server is listening at: %s\n", g_dbus_server_get_client_address(server));
    g_signal_connect(server,
                     "new-connection",
                     G_CALLBACK(on_new_connection),
                     NULL);

    g_thread_new("my_thread", alarm_function, NULL);

    loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    printf("Exiting server...\n");
    return 0;
}