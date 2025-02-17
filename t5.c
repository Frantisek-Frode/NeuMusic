#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUS_NAME "org.mpris.MediaPlayer2.asdfgh"
#define OBJECT_PATH "/org/mpris/MediaPlayer2"
#define INTERFACE_NAME "org.mpris.MediaPlayer2"
#define PLAYER_NAME "org.mpris.MediaPlayer2.Player"

DBusConnection* conn;
DBusError err;


const char* const NotImplMessage = "Not implemented";

typedef enum {
	ACTION_NEXT,
	ACTION_PREV,
	ACTION_PAUSE,
	ACTION_PLAYPAUSE,
	ACTION_STOP,
	ACTION_PLAY,

	ACTION_COUNT,
} PlayerAction;

const char* action_names[] = {
	[ACTION_NEXT] = "Next",
	[ACTION_PREV] = "Previous",
	[ACTION_PAUSE] = "Pause",
	[ACTION_PLAYPAUSE] = "PlayPause",
	[ACTION_STOP] = "Stop",
	[ACTION_PLAY] = "Play",
};

static void handle_method_call(DBusMessage* msg) {
	DBusMessage* reply = dbus_message_new_method_return(msg);

	for (PlayerAction act = 0; act < ACTION_COUNT; act++) {
		if (dbus_message_is_method_call(msg, PLAYER_NAME, action_names[act])) {
			dbus_connection_send(conn, reply, NULL);
			dbus_message_unref(reply);

			printf("[%d]: %s\n", act, action_names[act]);

			return;
		}
	}

	dbus_message_append_args(reply, DBUS_TYPE_STRING, &NotImplMessage, DBUS_TYPE_INVALID);
	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
}

int main(int argc, char** argv) {
	dbus_error_init(&err);

	// Connect to the D-Bus session bus
	conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Connection failed: %s\n", err.message);
		dbus_error_free(&err);
		exit(1);
	}

	// Request a well-known name
	int ret = dbus_bus_request_name(conn, BUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Failed to request name: %s\n", err.message);
		dbus_error_free(&err);
		exit(1);
	}

	// Export the object with its interface
	dbus_connection_flush(conn);
	printf("Service running, waiting for method calls...\n");

	while (1) {
		dbus_connection_read_write(conn, 0);
		DBusMessage* msg = dbus_connection_pop_message(conn);

		if (NULL == msg) {
			usleep(10000);
			continue;
		}

		handle_method_call(msg);
		dbus_message_unref(msg);
	}

	dbus_connection_unref(conn);
	return 0;
}

