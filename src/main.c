#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "server_state.h"
#include "command_loop.h"

int main() {
	setlogmask (LOG_UPTO (LOG_NOTICE));
	openlog ("radiosocketsd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "Starting radiosocketsd...");

    struct server_state state;
    state.running = 1;

    struct command_loop command_loop;
    state.command_loop = &command_loop;

    command_loop_init(&command_loop, 512);

    while (state.running){
        command_loop_run(&command_loop, &state);
    }

    command_loop_destroy(&command_loop);

    syslog(LOG_NOTICE, "Shutting down radiosocketsd...");
	closelog();
    return 0;
}
