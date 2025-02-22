#include "input.h"
#include "comain.h"
#include "channel.h"
#include "tui.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>


const char* action_commands[] = {
	[ACTION_NEXT] = "next",
	[ACTION_PREV] = "prev",
	[ACTION_PAUSE] = "pause",
	[ACTION_PLAYPAUSE] = "pp",
	[ACTION_STOP] = "stop",
	[ACTION_PLAY] = "play",
};

void input_stdin(Channel* event_channel, double* volume)
{
	char buffer[1024];
	int sin = fileno(stdin);
	int sio_flags = fcntl(sin, F_GETFL);
	fcntl(sin, F_SETFL, sio_flags | O_NONBLOCK);

	int size = fread(buffer, 1, sizeof(buffer) - 1, stdin);
	if (size <= 0) goto END;

	tui_backprompt();

	buffer[size - 1] = 0;

	double vol;
	int res = sscanf(buffer, "vol %lf", &vol);
	if (res == 1) {
		*volume = vol / 100;
		goto END;
	}

	for (PlayerAction act = 0; act < ACTION_COUNT; act++) {
		if (0==strcmp(buffer, action_commands[act])) {
			channel_write(event_channel, &act, sizeof(act));
		}
	}

END:
	fcntl(sin, F_SETFL, sio_flags);
}
