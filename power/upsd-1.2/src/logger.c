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



#include "config.h"
#include "common.h"
#include "logger.h"
#include "upsd.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>



/* ========= Private definitions ========= */



/**
 * Stores whether the syslog is used for logging. If this is set to false, then all log
 * messages will be printed to stdout instead.
 */
static bool useSyslog = false;



/* ========= Implementations ========= */



/* See header for documentation. */
void Logger_vlog(int level, const char *message, va_list ap) {
	/* Set up the output parts of the string. */
	char outMessage[512];
	char outErrnoString[512];

	/* Print the message string. */
	(void)vsnprintf(outMessage, ARRAY_SIZE(outMessage), message, ap);

	/* Print the error string. */
	outErrnoString[0] = '\0';
	if (level <= LOG_ERR && errno != 0) {
		(void)snprintf(outErrnoString, ARRAY_SIZE(outErrnoString), " Cause: %s.", strerror(errno));
	}
	
	if (useSyslog) {
		/* Write to the system log. */
		syslog(level, "%s%s\n", outMessage, outErrnoString);
	} else {
		/* Write to stdout. */
		const char *prefix;
		if (level <= LOG_ERR) {
			prefix = "Error";
		} else if (level == LOG_WARNING) {
			prefix = "Warning";
		} else {
			prefix = "Info";
		}
		(void)fprintf(stdout, "%s: %s: %s%s\n", PACKAGE_NAME, prefix, outMessage, outErrnoString);
	}
}

/* See header for documentation. */
void Logger_log(int level, const char *message, ...) {
	va_list ap;
	va_start(ap, message);
	Logger_vlog(level, message, ap);
	va_end(ap);
}

/* See header for documentation. */
bool Logger_getUseSyslog(void) {
	return useSyslog;
}

/* See header for documentation. */
void Logger_setUseSyslog(bool useSyslogParam) {
	if (!useSyslog && useSyslogParam) {
		/* We're switching from stdout-logging to syslog-logging. */
		openlog(PACKAGE_NAME, LOG_PID, LOG_DAEMON);
	} else if (useSyslog && !useSyslogParam) {
		/* We're switching from syslog-logging to stdout-logging. */
		closelog();
	}
	useSyslog = useSyslogParam;
}

