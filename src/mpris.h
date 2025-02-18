#pragma once
#include "channel.h"
#include <dbus/dbus.h>

typedef enum {
	ACTION_NEXT,
	ACTION_PREV,
	ACTION_PAUSE,
	ACTION_PLAYPAUSE,
	ACTION_STOP,
	ACTION_PLAY,

	ACTION_COUNT,
} PlayerAction;

typedef struct {
	/// checking for mpris events happens on main thread
	/// so we can share channel with main event channel
	Channel* event_channel;
	DBusConnection* connection;
} MprisContext;

int mpris_init(MprisContext* ctx);
void mpris_check(MprisContext* ctx);
void mpris_free(MprisContext* ctx);

