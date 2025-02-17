#include <dbus/dbus.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static DBusHandlerResult MPRISEntryPoint(DBusConnection *p_conn, DBusMessage *p_from, void *p_this)
{
	const char *psz_target_interface;
	const char *psz_interface = dbus_message_get_interface(p_from);
	const char *psz_method    = dbus_message_get_member(p_from);

	DBusError error;

	printf("interface: %s, method: %s\n", psz_interface, psz_method);

/*
if( psz_interface && strcmp( psz_interface, DBUS_INTERFACE_PROPERTIES ) )
psz_target_interface = psz_interface;

else
{
dbus_error_init( &error );
dbus_message_get_args( p_from, &error,
DBUS_TYPE_STRING, &psz_target_interface,
DBUS_TYPE_INVALID );

if( dbus_error_is_set( &error ) )
{
msg_Err( (vlc_object_t*) p_this, "D-Bus error on %s.%s: %s",
psz_interface, psz_method,
error.message );
dbus_error_free( &error );
return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
}

if( !strcmp( psz_target_interface, DBUS_INTERFACE_INTROSPECTABLE ) )
return handle_introspect( p_conn, p_from, p_this );

if( !strcmp( psz_target_interface, DBUS_MPRIS_ROOT_INTERFACE ) )
return handle_root( p_conn, p_from, p_this );

if( !strcmp( psz_target_interface, DBUS_MPRIS_PLAYER_INTERFACE ) )
return handle_player( p_conn, p_from, p_this );

if( !strcmp( psz_target_interface, DBUS_MPRIS_TRACKLIST_INTERFACE ) )
return handle_tracklist( p_conn, p_from, p_this );
*/

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const DBusObjectPathVTable dbus_mpris_vtable = {
	NULL, MPRISEntryPoint, /* handler function */
	NULL, NULL, NULL, NULL
};

void reply_to_method_call(DBusMessage* msg, DBusConnection* conn)
{
	DBusMessage* reply;
	DBusMessageIter args;
	bool stat = true;
	dbus_uint32_t level = 21614;
	dbus_uint32_t serial = 0;
	char* param = "";

	// read the arguments
	if (!dbus_message_iter_init(msg, &args))
		fprintf(stderr, "Message has no arguments!\n");
	else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
		fprintf(stderr, "Argument is not string!\n");
	else
		dbus_message_iter_get_basic(&args, &param);

	printf("Method called with %s\n", param);

	// create a reply from the message
	reply = dbus_message_new_method_return(msg);

	// add the arguments to the reply
	dbus_message_iter_init_append(reply, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &stat)) {
		fprintf(stderr, "Out Of Memory!\n");
		exit(1);
	}
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &level)) {
		fprintf(stderr, "Out Of Memory!\n");
		exit(1);
	}

	// send the reply && flush the connection
	if (!dbus_connection_send(conn, reply, &serial)) {
		fprintf(stderr, "Out Of Memory!\n");
		exit(1);
	}
	dbus_connection_flush(conn);

	// free the reply
	dbus_message_unref(reply);
}


/**
 * Server that exposes a method call and waits for it to be called
 */
void listen()
{
	DBusMessage* msg;
	DBusMessage* reply;
	DBusMessageIter args;
	DBusConnection* conn;
	DBusError err;
	int ret;
	char* param;

	printf("Listening for method calls\n");

	// initialise the error
	dbus_error_init(&err);

	// connect to the bus and check for errors
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Connection Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (NULL == conn) {
		fprintf(stderr, "Connection Null\n");
		exit(1);
	}

	dbus_connection_register_object_path(conn, "/org/mpris/MediaPlayer2", &dbus_mpris_vtable, NULL);

	// request our name on the bus and check for errors
	ret = dbus_bus_request_name(conn, "org.mpris.MediaPlayer2.asdfgh", DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Name Error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
		fprintf(stderr, "Not Primary Owner (%d)\n", ret);
		exit(1);
	}

	// loop, testing for new messages
	while (true) {
		// non blocking read of the next available message
		dbus_connection_read_write(conn, 0);
		msg = dbus_connection_pop_message(conn);

		// loop again if we haven't got a message
		if (NULL == msg) {
			usleep(10000);
			continue;
		}

		printf("interface: %s\n", dbus_message_get_interface(msg));

		// check this is a method call for the right interface & method
		if (dbus_message_is_method_call(msg, "test.method.Type", "Method"))
			reply_to_method_call(msg, conn);

		// free the message
		dbus_message_unref(msg);
	}
}

int main(void) {
	listen();
}

