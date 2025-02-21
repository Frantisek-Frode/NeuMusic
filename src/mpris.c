#include "mpris.h"
#include "channel.h"
#include "comain.h"
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// @linux
#define BUS_NAME "org.mpris.MediaPlayer2.fmusic"
#define OBJECT_PATH "/org/mpris/MediaPlayer2"
#define INTERFACE_NAME "org.mpris.MediaPlayer2"
#define PLAYER_NAME "org.mpris.MediaPlayer2.Player"


const char* const NotImplMessage = "Not implemented";

const char* action_names[] = {
	[ACTION_NEXT] = "Next",
	[ACTION_PREV] = "Previous",
	[ACTION_PAUSE] = "Pause",
	[ACTION_PLAYPAUSE] = "PlayPause",
	[ACTION_STOP] = "Stop",
	[ACTION_PLAY] = "Play",
};

int mpris_init(MprisContext* ctx) {
	DBusError err;
	dbus_error_init(&err);

	// Connect to the D-Bus session bus
	DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Connection failed: %s\n", err.message);
		dbus_error_free(&err);
		return -1;
	}

	pid_t pid = getpid();
	char bus_name[10 + sizeof(BUS_NAME)];
	snprintf(bus_name, sizeof(bus_name), BUS_NAME"%d", pid);

	// Request a well-known name
	dbus_bus_request_name(conn, bus_name, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Failed to request name: %s\n", err.message);
		dbus_error_free(&err);
		return -1;
	}

	// Export the object with its interface
	dbus_connection_flush(conn);

	MprisContext result = {
		.connection = conn
	};
	*ctx = result;

	return 0;
}

void mpris_free(MprisContext* ctx) {
	dbus_connection_unref(ctx->connection);
}

static void handle_method_call(MprisContext* ctx, DBusMessage* msg) {
	DBusMessage* reply = dbus_message_new_method_return(msg);

	for (PlayerAction act = 0; act < ACTION_COUNT; act++) {
		if (dbus_message_is_method_call(msg, PLAYER_NAME, action_names[act])) {
			dbus_connection_send(ctx->connection, reply, NULL);
			dbus_message_unref(reply);

			channel_write(ctx->event_channel, &act, sizeof(act));

			return;
		}
	}

	dbus_message_append_args(reply, DBUS_TYPE_STRING, &NotImplMessage, DBUS_TYPE_INVALID);
	dbus_connection_send(ctx->connection, reply, NULL);
	dbus_message_unref(reply);
}

void mpris_check(MprisContext* ctx) {
	for (;;) {
		dbus_connection_read_write(ctx->connection, 0);
		DBusMessage* msg = dbus_connection_pop_message(ctx->connection);

		if (NULL == msg) return;

		handle_method_call(ctx, msg);
		dbus_message_unref(msg);
	}
}

