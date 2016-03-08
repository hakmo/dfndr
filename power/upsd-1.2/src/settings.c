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
#include "logger.h"
#include "settings.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>



/* ========= Private definitions ========= */



/**
 * Represents a Settings object.
 */
struct Settings {
	/**
	 * Stores whether this program should actually do anything.
	 */
	bool enabled;
	
	/**
	 * Stores the monitored interface.
	 */
	const char *interface;
	
	/**
	 * Stores the check interval for power outages, in milliseconds.
	 */
	int checkInterval;
	
	/**
	 * Stores the update interval for the status file, in seconds.
	 */
	int updateInterval;
	
	/**
	 * Stores the battery run time, in seconds.
	 */
	int runTime;
	
	/**
	 * Stores the battery charge time, in seconds.
	 */
	int chargeTime;
	
	/**
	 * Stores the lower charge level limit of the battery, in percent, or 0 if there should be no such limit. If the lower charge level limit is reached and the system is
	 * currently powered by the battery, then the "low_battery" script is executed.
	 */
	int lowLevel;
	
	/**
	 * Stores the time limit after a power loss, in seconds, or 0 if there should be no such limit. If the time limit is reached and the system is currently powered by
	 * the battery, then the "low_battery" script is executed.
	 */
	int lowTime;
};



/* ========= Implementations ========= */



/* See header for documentation. */
struct Settings *Settings_construct(const char *configFile) {
	struct Settings *self = (struct Settings *)malloc(sizeof(struct Settings));
	if (self == NULL) {
		return NULL;
	}
	
	/* Set up default values. */
	self->enabled = true;
	self->interface = strdup("eth0");
	self->checkInterval = 1000;
	self->updateInterval = 10;
	self->runTime = 14400;
	self->chargeTime = 6660;
	self->lowLevel = 30;
	self->lowTime = 0;
	
	/* Open the configuration file. */
	FILE *fp = fopen(configFile, "r");
	if (fp == NULL) {
		Logger_log(LOG_ERR, "Could not open configuration file %s.", configFile);
		Settings_destruct(self);
		return NULL;
	}
	
	bool haveRunTime = false;
	bool haveChargeTime = false;
	
	/* Read the lines of the configuration file. */
	char *line = NULL;
	size_t len = 0;
	while (getline(&line, &len, fp) != -1) {
		/* Check for a comment line. */
		if (line[0] == '#') {
			continue;
		}
		
		/* Determine the key and value part of the line by searching for the first '=' character. */
		char *idx = index(line, '=');
		if (idx == NULL) {
			continue;
		}
		char *strKey = line;
		char *strValue = idx + 1;
		
		/* Ignore whitespaces at the end of the key. */
		idx--;
		while (idx >= strKey && isWhitespace(*idx)) {
			idx--;
		}
		*(idx + 1) = '\0';
		
		/* Ignore whitespaces at the beginning of the value. */
		while (isWhitespace(*strValue)) {
			strValue++;
		}
		
		/* Ignore whitespaces at the end of the value. */
		idx = strValue;
		while (*idx != '\0') {
			idx++;
		}
		idx--;
		while (idx >= strValue && isWhitespace(*idx)) {
			idx--;
		}
		*(idx + 1) = '\0';
		
		/* Parse the identifier for known keywords. */
		if (strcasecmp(strKey, "ENABLED") == 0) {
			self->enabled = strcasecmp(strValue, "yes") == 0 || strcasecmp(strValue, "true") == 0 || strcmp(strValue, "1") == 0;
		} else if (strcasecmp(strKey, "INTERFACE") == 0) {
			free((void *)self->interface);
			self->interface = strdup(strValue);
		} else if (strcasecmp(strKey, "CHECK_INTERVAL") == 0) {
			self->checkInterval = strtol(strValue, NULL, 10);
			if (self->checkInterval < 100) {
				self->checkInterval = 100;
			} else if (self->checkInterval > 10000) {
				self->checkInterval = 10000;
			}
		} else if (strcasecmp(strKey, "UPDATE_INTERVAL") == 0) {
			self->updateInterval = strtol(strValue, NULL, 10);
			if (self->updateInterval < 1) {
				self->updateInterval = 1;
			} else if (self->updateInterval > 60) {
				self->updateInterval = 60;
			}
		} else if (strcasecmp(strKey, "BATTERY_RUN_TIME") == 0) {
			self->runTime = strtol(strValue, NULL, 10);
			if (self->runTime < 0) {
				self->runTime = 0;
			}
			haveRunTime = true;
		} else if (strcasecmp(strKey, "BATTERY_CHARGE_TIME") == 0) {
			self->chargeTime = strtol(strValue, NULL, 10);
			if (self->chargeTime < 0) {
				self->chargeTime = 0;
			}
			haveChargeTime = true;
		} else if (strcasecmp(strKey, "BATTERY_LOW_LEVEL") == 0) {
			self->lowLevel = strtol(strValue, NULL, 10);
			if (self->lowLevel <= 0) {
				/* Low level is off. */
				self->lowLevel = 0;
			} else if (self->lowLevel < 10) {
				self->lowLevel = 10;
			} else if (self->lowLevel > 90) {
				self->lowLevel = 90;
			}
		} else if (strcasecmp(strKey, "BATTERY_LOW_TIME") == 0) {
			self->lowTime = strtol(strValue, NULL, 10);
			if (self->lowTime < 0) {
				/* Low time is off. */
				self->lowTime = 0;
			}
		}
	}
	free(line);
	
	/* Close the configuration file. */
	if (fclose(fp) == EOF) {
		Settings_destruct(self);
		return NULL;
	}
	
	/* Sanity check. */
	if (self->interface == NULL) {
		Logger_log(LOG_ERR, "Out of memory error. Will quit now.");
		Settings_destruct(self);
		return NULL;
	}
	
	/* Option check. */
	if (!haveRunTime || !haveChargeTime) {
		Logger_log(LOG_INFO, "A default configuration is used for now. Please check your configuration file.");
	}
	
	return self;
}

/* See header for documentation. */
void Settings_destruct(struct Settings *self) {
	free((void *)self->interface);
	free(self);
}

/* See header for documentation. */
bool Settings_getEnabled(const struct Settings *self) {
	return self->enabled;
}

/* See header for documentation. */
const char *Settings_getInterface(const struct Settings *self) {
	return self->interface;
}

/* See header for documentation. */
int Settings_getCheckInterval(const struct Settings *self) {
	return self->checkInterval;
}

/* See header for documentation. */
int Settings_getUpdateInterval(const struct Settings *self) {
	return self->updateInterval;
}

/* See header for documentation. */
int Settings_getRunTime(const struct Settings *self) {
	return self->runTime;
}

/* See header for documentation. */
int Settings_getChargeTime(const struct Settings *self) {
	return self->chargeTime;
}

/* See header for documentation. */
int Settings_getLowLevel(const struct Settings *self) {
	return self->lowLevel;
}

/* See header for documentation. */
int Settings_getLowTime(const struct Settings *self) {
	return self->lowTime;
}
