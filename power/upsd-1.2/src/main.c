/*
 * Copyright (c) 2015 Mathias Kunter
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



/* ========= Includes ========= */



#include "common.h"
#include "config.h"
#include "logger.h"
#include "main.h"
#include "upsd.h"

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>



/* ========= Private definitions ========= */



/**
 * The program instance.
 */
static struct Upsd *upsd;

/**
 * Ensures that cleanup is only performed once.
 */
static volatile sig_atomic_t isTerminating;

/**
 * Quits the program. This includes logging the @a message by using the given log @a level, closing down all
 * allocated resources and terminating the program with the given @a exitCode.
 */
static void quitProgram(int exitCode, int level, const char *message, ...);

/**
 * Closes down all allocated resources.
 */
static void cleanup(void);



/* ========= Implementations ========= */



/* See above for documentation. */
static void quitProgram(int exitCode, int level, const char *message, ...) {
	/* Log the message. */
	if (message != NULL) {
		va_list ap;
		va_start(ap, message);
		Logger_vlog(level, message, ap);
		va_end(ap);
	}
	
	/* Clean up allocated resources. Although the OS would actually do this job for us, we still
	 * need to make sure that the pid and status files become deleted. */
	cleanup();
	
	/* Print a shutdown message. */
	Logger_log(LOG_NOTICE, "%s has been successfully terminated.", PACKAGE_NAME);
	
	/* Quit. */
	exit(exitCode);
}

/* See above for documentation. */
static void cleanup(void) {
	/* Avoid racing. This might happen when receiving a signal at the same time as this function is executed. */
	if (isTerminating == 1) {
		return;
	}
	isTerminating = 1;
	
	/* Get the program mode. */
	char progMode = Upsd_getProgMode(upsd);
	
	/* Destruct resources. */
	if (upsd != NULL) {
		Upsd_destruct(upsd);
		upsd = NULL;
	}
	
	if (progMode == '.' || progMode == 'f') {
		/* Remove the pid and status files. */
		if (access(PID_FILE, F_OK) == 0 && unlink(PID_FILE) == -1) {
			Logger_log(LOG_WARNING, "Could not remove pid file %s.", PID_FILE);
		}
		if (access(STATUS_FILE, F_OK) == 0 && unlink(STATUS_FILE) == -1) {
			Logger_log(LOG_WARNING, "Could not remove status file %s.", STATUS_FILE);
		}
	}
}

/* See header for documentation. */
int main(int argc, char **argv) {
	/* Initialize variables. */
	upsd = NULL;
	isTerminating = 0;
	
	/* Create the program instance. */
	upsd = Upsd_construct(argc, argv);
	if (upsd == NULL) {
		return EXIT_FAILURE;
	}
	
	char progMode = Upsd_getProgMode(upsd);
	if (progMode == '.' || progMode == 'f') {
		/* Do the actual work. This function will normally not return, since it's intended to run forever. */
		Upsd_run(upsd);
	}
	
	/* Clean up allocated resources. */
	cleanup();
	
	return EXIT_SUCCESS;
}

/* See header for documentation. */
void handleSignal(int signal) {
	const char *logMessage = "Shutting down after receiving signal";
	const char *signalName = Upsd_getSignalName(signal);
	errno = 0;
	if (signalName != NULL) {
		quitProgram(EXIT_SUCCESS, LOG_WARNING, "%s %s.", logMessage, signalName);
	} else {
		quitProgram(EXIT_SUCCESS, LOG_WARNING, "%s.", logMessage);
	}
}
