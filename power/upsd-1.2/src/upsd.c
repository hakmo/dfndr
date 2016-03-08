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
#include "main.h"
#include "settings.h"
#include "upsd.h"

#include <dirent.h>
#include <errno.h>
#include <net/if.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>



/* ========= Private definitions ========= */



/**
 * Represents the main program instance.
 */
struct Upsd {
	
	/**
	 * Stores how the upsd program should be run.
	 *  '.' = Default. Run upsd as daemon.
	 *  'i' = Print machine-readable status information about an already running upsd instance.
	 *  'p' = Print program paths and exit.
	 *  'f' = Run upsd as a normal foreground program.
	 *  'v' = Print version information and exit.
	 */
	char progMode;
	
	/**
	 * Stores the assumed initial charge level of the battery, in percent.
	 */
	int initialLevel;
	
	/**
	 * The file pointer to the pid file. This file is kept open (and also locked) all the time, so that we can use it to check whether upsd is already running.
	 */
	FILE *pid_fp;
	
	/**
	 * The file pointer to the status file. This will be NULL if the status file isn't currently open.
	 */
	FILE *status_fp;
	
	/**
	 * The Settings object, holding all program settings.
	 */
	struct Settings *settings;
};

/**
 * Prints the given time @a t in the format hh:mm:ss and returns the result within a static string buffer. The given time must be a value in seconds.
 */
const char *Upsd_printTime(int t);

/**
 * Prints the current timestamp in the format YYYY-MM-DD hh:mm:ss and returns the result within a static string buffer.
 */
const char *Upsd_printTimeStamp(void);

/**
 * Computes and returns the difference between the two given times @a from and @a to, in milliseconds.
 */
int Upsd_timeDiff(const struct timespec *from, const struct timespec *to);

/**
 * Prints the program usage.
 */
void Upsd_printUsage(void);



/* ========= Implementations ========= */



/* See above for documentation. */
const char *Upsd_printTime(int t) {
	static char buf[9];
	(void)snprintf(buf, ARRAY_SIZE(buf), "%02d:%02d:%02d", t / 3600, (t / 60) % 60, t % 60);
	return buf;
}

/* See above for documentation. */
const char *Upsd_printTimeStamp(void) {
	static char buf[20];
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	(void)snprintf(buf, ARRAY_SIZE(buf), "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	return buf;
}

/* See above for documentation. */
int Upsd_timeDiff(const struct timespec *from, const struct timespec *to) {
	return (to->tv_sec - from->tv_sec) * 1000 + (to->tv_nsec - from->tv_nsec) / 1000000;
}

/* See above for documentation. */
void Upsd_printUsage(void) {
	(void)printf("Usage: %s [-f|-i|-p|-v] [-l level]\n", PACKAGE_NAME);
	(void)printf("Type \"man %s\" for further information.\n", PACKAGE_NAME);
}

/* See header for documentation. */
struct Upsd *Upsd_construct(int argc, char **argv) {
	/* Allocate the main Upsd object. */
	struct Upsd *self = (struct Upsd *)malloc(sizeof(struct Upsd));
	if (self == NULL) {
		Logger_log(LOG_ERR, "Out of memory error. Will quit now.");
		return NULL;
	}
	
	/* Set up default values. */
	(void)memset(self, 0, sizeof(struct Upsd));
	self->progMode = '.';
	self->initialLevel = 100;
	
	/* Parse the program options. */
	opterr = 0;
	while (true) {
		int option = getopt(argc, argv, "fipvl:");
		if (option == 'f' || option == 'i' || option == 'p' || option == 'v') {
			/* Set program mode. */
			if (self->progMode != '.') {
				Upsd_printUsage();
				Upsd_destruct(self);
				return NULL;
			} else {
				self->progMode = option;
			}
		} else if (option == 'l') {
			/* Set initial battery charge level. */
			self->initialLevel = strtol(optarg, NULL, 10);
			if (self->initialLevel < 0 || self->initialLevel > 100) {
				Upsd_printUsage();
				Upsd_destruct(self);
				return NULL;
			}
		} else if (option == -1) {
			/* All options parsed. */
			break;
		} else {
			/* Invalid option. */
			Upsd_printUsage();
			Upsd_destruct(self);
			return NULL;
		}
	}
	
	/* Check the number of arguments. */
	int argumentCount = argc - optind;
	if (argumentCount != 0) {
		Upsd_printUsage();
		Upsd_destruct(self);
		return NULL;
	}
	
	
	if (self->progMode == 'p') {
		/* Print program paths and exit. */
		(void)printf("Configuration file: %s\n", CONFIG_FILE);
		(void)printf("Scripts directory:  %s\n", SCRIPT_DIR);
		(void)printf("Status file:        %s\n", STATUS_FILE);
		(void)printf("PID file:           %s\n", PID_FILE);
		return self;
	} else if (self->progMode == 'v') {
		/* Print version information and exit. */
		(void)printf("%s\n", PACKAGE_STRING);
		(void)printf("\n");
		(void)printf("Copyright (c) 2015 Mathias Kunter\n");
		(void)printf("\n");
		(void)printf("This program is free software: you can redistribute it and/or modify\n");
		(void)printf("it under the terms of the GNU General Public License as published by\n");
		(void)printf("the Free Software Foundation, either version 3 of the License, or\n");
		(void)printf("(at your option) any later version.\n");
		(void)printf("\n");
		(void)printf("This program is distributed in the hope that it will be useful,\n");
		(void)printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
		(void)printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
		(void)printf("GNU General Public License for more details.\n");
		(void)printf("\n");
		(void)printf("Visit http://raspi-ups.appspot.com for further information.\n");
		return self;
	}
	
	
	/* Validate that the script path is actually the absolute path of an existing directory, and nothing else.
	 * This is done for security reasons, as we'll pass this on to the system() call later. */
	bool validScriptPath = false;
	if (SCRIPT_DIR[0] == '/') {
		DIR *dir = opendir(SCRIPT_DIR);
		if (dir != NULL) {
			if (closedir(dir) == -1) {
				Logger_log(LOG_WARNING, "Could not close directory %s.", SCRIPT_DIR);
			}
			validScriptPath = true;
		} else if (errno != ENOENT) {
			Logger_log(LOG_ERR, "Could not open directory %s. Will quit now.", SCRIPT_DIR);
			Upsd_destruct(self);
			return NULL;
		}
	}
	if (!validScriptPath) {
		errno = 0;
		Logger_log(LOG_ERR, "The script directory is %s, but this path does not exist. Will quit now.", SCRIPT_DIR);
		Upsd_destruct(self);
		return NULL;
	}
	
	
	/* Check whether there already is a running upsd instance by checking if there is a locked pid file present.
	 * This should also work if we're not root, so try to open the file in read-only mode here. */
	pid_t pid = -1;
	self->pid_fp = fopen(PID_FILE, "r");
	if (self->pid_fp != NULL) {
		/* There is a pid file present. Check whether it's locked as well. */
		if (flock(fileno(self->pid_fp), LOCK_EX | LOCK_NB) == -1) {
			if (errno == EWOULDBLOCK) {
				/* The pid file is locked, so there is already a running upsd instance. Obtain its pid. */
				if (fscanf(self->pid_fp, "%d", &pid) == EOF) {
					Logger_log(LOG_ERR, "Could not read pid from pid file %s. Will quit now.", PID_FILE);
					Upsd_destruct(self);
					return NULL;
				}
			} else {
				/* Locking failed for some other reason. */
				Logger_log(LOG_ERR, "Could not lock pid file %s. Will quit now.", PID_FILE);
				Upsd_destruct(self);
				return NULL;
			}
		}
		/* Close down the pid file again (this will also unlock the file, if we should have locked it just now).
		 * We need to open up the pid file in write mode again later (for this we'll need to be root). */
		if (fclose(self->pid_fp) == EOF) {
			Logger_log(LOG_WARNING, "Could not close pid file %s.", PID_FILE);
		}
		self->pid_fp = NULL;
	}
	errno = 0;
	
	
	/* Set the umask to 022 in both program and daemon mode. */
	umask(022);
	
	/* Check for running / not running instance. */
	if (self->progMode == 'i') {
		if (pid == -1) {
			/* Print the running indicator in machine-readable format. */
			(void)printf("IS_RUNNING=NO\n");
			return self;
		} else {
			(void)printf("IS_RUNNING=YES\n");
			(void)printf("PID=%d\n", pid);
		}
	} else {
		if (pid != -1) {
			Logger_log(LOG_ERR, "There is already an %s instance running (pid %i). Will quit now.", PACKAGE_NAME, pid);
			Logger_log(LOG_INFO, "To display status information, use the command \"%s -i\".", PACKAGE_NAME);
			Logger_log(LOG_INFO, "To restart the program, use the command \"sudo /etc/init.d/%s restart\".", PACKAGE_NAME);
			Logger_log(LOG_INFO, "To get further help, use the command \"man %s\".", PACKAGE_NAME);
			Upsd_destruct(self);
			return NULL;
		}
	}
	
	/* Check for root. */
	if (getuid() != 0 && self->progMode != 'i') {
		Logger_log(LOG_WARNING, "This program is not running as root and thus may not work as expected.", PACKAGE_NAME);
	}
	
	if (self->progMode == '.' || self->progMode == 'f') {
		/* Inform the user about the initial battery charge level as well. */
		Logger_log(LOG_INFO, "The current battery charge level is assumed to be %i %%.", self->initialLevel);
	} else if (self->progMode == 'i') {
		/* Print out status information about the running upsd process. */
		/* Open the status file. */
		self->status_fp = fopen(STATUS_FILE, "r");
		if (self->status_fp == NULL) {
			Logger_log(LOG_ERR, "Could not open status file %s. Will quit now.", STATUS_FILE);
			Upsd_destruct(self);
			return NULL;
		}
		
		/* Lock the status file. */
		if (flock(fileno(self->status_fp), LOCK_EX) == -1) {
			Logger_log(LOG_ERR, "Could not lock status file %s.", STATUS_FILE);
			Upsd_destruct(self);
			return NULL;
		}
		
		/* Print the content of the status file. */
		char *line = NULL;
		size_t len = 0;
		while (getline(&line, &len, self->status_fp) != -1) {
			(void)printf("%s", line);
		}
		free(line);
		
		/* Close the status file (this will also unlock it). */
		if (fclose(self->status_fp) == EOF) {
			Logger_log(LOG_WARNING, "Could not close status file %s.", STATUS_FILE);
		}
		self->status_fp = NULL;
		
		/* We're done. */
		return self;
	}
	
	/* Read the program settings. */
	self->settings = Settings_construct(CONFIG_FILE);
	if (self->settings == NULL) {
		Logger_log(LOG_ERR, "Error reading configuration file or incorrect configuration. Will quit now.");
		Upsd_destruct(self);
		return NULL;
	}
	if (!Settings_getEnabled(self->settings)) {
		Logger_log(LOG_INFO, "%s is disabled. Will quit now.", PACKAGE_NAME);
		Upsd_destruct(self);
		return NULL;
	}
	if (Settings_getRunTime(self->settings) < Settings_getChargeTime(self->settings)) {
		Logger_log(LOG_ERR, "The battery runtime is lower than the battery charge time. The battery is insufficient to power the system. Will quit now.");
		Upsd_destruct(self);
		return NULL;
	}
	
	
	/* Set up a signal handler. */
	sigset_t blocked_signals;
	if (sigfillset(&blocked_signals) < 0) {
		Logger_log(LOG_ERR, "Could not initialize blocked signals list. Will quit now.");
		Upsd_destruct(self);
		return NULL;
	}
	
	const int signals[] = { SIGINT, SIGTERM };
	
	struct sigaction s;
	s.sa_handler = handleSignal;
	(void)memcpy(&s.sa_mask, &blocked_signals, sizeof(s.sa_mask));
	s.sa_flags = SA_RESTART;
	
	for (int i = 0; i < ARRAY_SIZE(signals); i++) {
		if (sigaction(signals[i], &s, NULL) < 0) {
			Logger_log(LOG_ERR, "Could not install signal handler for signal %s. Will quit now.", Upsd_getSignalName(signals[i]));
			Upsd_destruct(self);
			return NULL;
		}
	}
	
	
	/* Open up the pid file and lock it. We do that before daemonizing, to allow to print pid file accessing errors to stderr. */
	self->pid_fp = fopen(PID_FILE, "w");
	if (self->pid_fp == NULL) {
		Logger_log(LOG_ERR, "Could not open pid file %s. Will quit now.", PID_FILE);
		Upsd_destruct(self);
		return NULL;
	}
	if (flock(fileno(self->pid_fp), LOCK_EX | LOCK_NB) == -1) {
		/* Locking the pid file failed. */
		Logger_log(LOG_ERR, "Could not lock pid file %s. Will quit now.", PID_FILE);
		Upsd_destruct(self);
		return NULL;
	}
	
	
	if (self->progMode == '.') {
		/* Fork off the parent process. */
		pid = fork();
		if (pid == -1) {
			Logger_log(LOG_ERR, "Could not fork off parent process. Will quit now.");
			Upsd_destruct(self);
			return NULL;
		} else if (pid > 0) {
			/* Terminate the parent process. This will free up any resources held by this process. */
			exit(EXIT_SUCCESS);
		}
		
		/* Become the session leader. */
		if (setsid() == -1) {
			Logger_log(LOG_ERR, "Could not become session leader. Will quit now.");
			Upsd_destruct(self);
			return NULL;
		}
		
		/* Fork off the session leading process. */
		pid = fork();
		if (pid == -1) {
			Logger_log(LOG_ERR, "Could not fork off session leading process. Will quit now.");
			Upsd_destruct(self);
			return NULL;
		} else if (pid > 0) {
			/* Terminate the session leading process. This will free up any resources held by this process. */
			exit(EXIT_SUCCESS);
		}
		
		/* Note: the umask is kept here - it has already been set before. */
		
		/* Change the working directory to the root directory. */
		if (chdir("/") == -1) {
			Logger_log(LOG_WARNING, "Could not change the current working directory.");
		}
		
		/* Redirect stdin, stdout and stderr to /dev/null. (From now on we'll direct all output to the system log instead,
		 * and we don't need any standard input anyway.) */
		if (
				close(fileno(stdin)) == -1 ||
				close(fileno(stdout)) == -1 ||
				close(fileno(stderr)) == -1 ||
				open("/dev/null", O_RDONLY) == -1 ||	/* open stdin */
				open("/dev/null", O_WRONLY) == -1 ||	/* open stdout */
				open("/dev/null", O_WRONLY) == -1) {	/* open stderr */
			Logger_log(LOG_ERR, "Could not redirect stdin, stdout and stderr to /dev/null. Will quit now.");
			Upsd_destruct(self);
			return NULL;
		}
		
		/* Start logging to the syslog. */
		Logger_setUseSyslog(true);
		
		/* Print a note to the log file. */
		Logger_log(LOG_NOTICE, "%s daemon started.", PACKAGE_NAME);
	}
	
	
	/* Write the current pid to the pid file. */
	if (fprintf(self->pid_fp, "%d\n", getpid()) <= 0) {
		Logger_log(LOG_ERR, "Could not write pid file %s. Will quit now.", PID_FILE);
		Upsd_destruct(self);
		return NULL;
	}
	if (fflush(self->pid_fp) == EOF || fsync(fileno(self->pid_fp)) == -1) {
		Logger_log(LOG_ERR, "Could not flush pid file %s. Will quit now.", PID_FILE);
		Upsd_destruct(self);
		return NULL;
	}
	
	
	return self;
}

/* See header for documentation. */
void Upsd_destruct(struct Upsd *self) {
	/* Close down the pid file. */
	if (self->pid_fp != NULL) {
		if (fclose(self->pid_fp) == EOF) {
			Logger_log(LOG_WARNING, "Could not close pid file %s.", PID_FILE);
		}
	}
	
	/* Close down the status file. */
	if (self->status_fp != NULL) {
		if (fclose(self->status_fp) == EOF) {
			Logger_log(LOG_WARNING, "Could not close status file %s.", STATUS_FILE);
		}
	}
	
	/* Destruct the settings object and this object itself. */
	if (self->settings != NULL) {
		Settings_destruct(self->settings);
	}
	free(self);
}

/* See header for documentation. */
void Upsd_run(struct Upsd *self) {
	/* Stores whether there currently is a power outage. */
	bool isOutage = false;
	
	/* Stores the timestamp of the last power check. This is used to compute the elapsed time between two checks. */
	struct timespec lastCheck;
	(void)memset(&lastCheck, 0, sizeof(lastCheck));
	
	/* Stores the timestamp of the last power outage. This is used to compute the elapsed time since the system is running on battery power.
	 * This is zeroed if there was no power outage so far. */
	struct timespec lastOutage;
	(void)memset(&lastOutage, 0, sizeof(lastOutage));
	
	/* Stores the timestamp of the last status file update. This is zeroed if there was no status file update so far. */
	struct timespec lastStatusUpdate;
	(void)memset(&lastStatusUpdate, 0, sizeof(lastStatusUpdate));
	
	/* Stores the battery charge level at the beginning of the last power outage. */
	int outageChargeLevel;
	
	/* Stores the total battery runtime, in milliseconds. */
	int runTime = Settings_getRunTime(self->settings) * 1000;
	
	/* Stores the current estimated remaining time, in milliseconds. */
	int remainTime = runTime / 100 * self->initialLevel;
	
	/* Stores the total time needed to fully charge the battery. */
	int chargeTime = Settings_getChargeTime(self->settings) * 1000;
	
	/* Stores whether we should issue a warning the next time the interface is found to be down. */
	bool warnIfDown = true;
	
	/* Stores whether the battery is already low. */
	bool lowBattery = false;
	
	
	/* Create a dummy socket for the interface I/O. */
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		Logger_log(LOG_ERR, "Could not create socket for interface I/O.");
		return;
	}
	
	/* Set up the interface structure. */
	struct ifreq ifr;
	(void)memset(&ifr, 0, sizeof(ifr));
	(void)strncpy(ifr.ifr_name, Settings_getInterface(self->settings), IFNAMSIZ - 1);
	
	Logger_log(LOG_INFO, "%s is now ready and running. The interface %s is used to determine power outages.", PACKAGE_NAME, ifr.ifr_name);
	
	/* Run forever. */
	while (true) {
		/* Get the current interface status. */
		if (ioctl(fd, SIOCGIFFLAGS, &ifr) == -1) {
			Logger_log(LOG_ERR, "Could not obtain the current status of interface %s.", ifr.ifr_name);
			return;
		}
		
		/* Get the current time. */
		struct timespec currentTime;
		if (clock_gettime(CLOCK_MONOTONIC, &currentTime) == -1) {
			Logger_log(LOG_ERR, "Could not obtain the current clock time.");
			return;
		}
		
		/* Get the elapsed time in milliseconds. */
		int elapsedTime;
		if (lastCheck.tv_sec != 0 || lastCheck.tv_nsec != 0) {
			elapsedTime = Upsd_timeDiff(&lastCheck, &currentTime);
		} else {
			elapsedTime = 0;
		}
		lastCheck = currentTime;
		
		/* Determine the time-to-empty in seconds. */
		int emptyTime = remainTime - runTime / 100 * Settings_getLowLevel(self->settings);
		emptyTime /= 1000;
		if (emptyTime < 0) {
			emptyTime = 0;
		}
		
		/* Check the interface up status. */
		if ((ifr.ifr_flags & IFF_UP) != 0) {
			/* The interface is up. */
			warnIfDown = true;
			
			/* Check the interface running status. */
			if ((ifr.ifr_flags & IFF_RUNNING) != 0) {
				/* The interface is running. */
				if (isOutage) {
					/* Power is coming back right now after there was a power outage. */
					Logger_log(LOG_INFO, "The power outage is over now. It could be successfully bridged over by using the battery.");
					Logger_log(LOG_INFO, "The outage lasted for %s hours.", Upsd_printTime(Upsd_timeDiff(&lastOutage, &currentTime) / 1000));
					if (runTime != 0) {
						Logger_log(LOG_INFO, "The estimated battery charge level decreased from %i to %i %% during the outage.", outageChargeLevel, remainTime / (runTime / 100));
						Logger_log(LOG_INFO, "The system could have run for %s more hours before the battery would have been empty.", Upsd_printTime(emptyTime));
					}
					
					/* Run the "power_back" shell script. */
					if (system(inline_strcat(SCRIPT_DIR, "/power_back")) == -1) {
						Logger_log(LOG_WARNING, "Could not run the \"power_back\" shell script.");
					}
					
					/* Clear the power outage flag. */
					isOutage = false;
				}
			} else {
				/* The interface is not running. */
				if (!isOutage) {
					/* There is a power outage right now. */
					Logger_log(LOG_WARNING, "There is a power outage right now.");
					if (runTime != 0) {
						Logger_log(LOG_INFO, "The estimated current battery charge level is %i %%.", remainTime / (runTime / 100));
						Logger_log(LOG_INFO, "There are %s hours remaining before the battery will be empty.", Upsd_printTime(emptyTime));
					}
					Logger_log(LOG_INFO, "If the battery should actually run empty, appropiate action will be taken.");
					
					/* Run the "power_outage" shell script. */
					if (system(inline_strcat(SCRIPT_DIR, "/power_outage")) == -1) {
						Logger_log(LOG_WARNING, "Could not run the \"power_outage\" shell script.");
					}
					
					/* Set variables. */
					if (runTime != 0) {
						outageChargeLevel = remainTime / (runTime / 100);
					}
					lastOutage = currentTime;
					isOutage = true;
				}
			}
		} else {
			/* The interface is down. */
			if (warnIfDown) {
				Logger_log(LOG_WARNING, "The interface %s is down right now. This program will not work until the interface is brought up.", ifr.ifr_name);
				warnIfDown = false;
			}
		}
		
		/* Update the remaining battery time. */
		if (runTime != 0) {
			remainTime -= elapsedTime;
			if (chargeTime != 0 && !isOutage) {
				remainTime += (int)((long long)elapsedTime * runTime / chargeTime);
			}
		}
		if (remainTime < 0) {
			remainTime = 0;
		} else if (remainTime > runTime) {
			remainTime = runTime;
		}
		
		/* Update the status file. */
		if ((lastStatusUpdate.tv_sec == 0 && lastStatusUpdate.tv_nsec == 0) ||
				Upsd_timeDiff(&lastStatusUpdate, &currentTime) / 1000 >= Settings_getUpdateInterval(self->settings)) {
			FILE *fp = fopen(STATUS_FILE, "w");
			if (fp == NULL) {
				Logger_log(LOG_WARNING, "Could not open status file %s.", STATUS_FILE);
			} else {
				/* Write the status file. */
				if (flock(fileno(fp), LOCK_EX) == -1) {
					Logger_log(LOG_WARNING, "Could not lock status file %s.", STATUS_FILE);
				} else {
					/* Write last updated timestamp (now). */
					(void)fprintf(fp, "LAST_UPDATED=%s\n", Upsd_printTimeStamp());
					
					/* Write the current power outage status. */
					(void)fprintf(fp, "POWER_OUTAGE=%s\n", isOutage ? "YES" : "NO");
					
					/* If there currently is a power outage, write the outage time. */
					if (isOutage) {
						(void)fprintf(fp, "POWER_OUTAGE_TIME=%s\n", Upsd_printTime(Upsd_timeDiff(&lastOutage, &currentTime) / 1000));
					}
					
					/* Write the charge level of the battery, if known. */
					if (runTime != 0) {
						(void)fprintf(fp, "BATTERY_CHARGE_LEVEL=%i\n", remainTime / (runTime / 100));
					}
					
					/* Write the expected remaining time, if known. */
					if (emptyTime != 0) {
						(void)fprintf(fp, "BATTERY_REMAIN_TIME=%s\n", Upsd_printTime(emptyTime));
					}
					
					/* Write the current battery status. */
					(void)fprintf(fp, "BATTERY_LOW=%s\n", lowBattery ? "YES" : "NO");
					
					if (flock(fileno(fp), LOCK_UN) == -1) {
						Logger_log(LOG_WARNING, "Could not unlock status file %s.", STATUS_FILE);
					}
				}
				
				/* Close the status file. */
				if (fclose(fp) == EOF) {
					Logger_log(LOG_WARNING, "Could not close status file %s.", STATUS_FILE);
				}
			}
			
			lastStatusUpdate = currentTime;
		}
		
		/* Check whether we're running out of battery. */
		if (isOutage) {
			int lowLevel = Settings_getLowLevel(self->settings);
			int lowTime = Settings_getLowTime(self->settings);
			int outageTime = Upsd_timeDiff(&lastOutage, &currentTime) / 1000;
			if ((lowLevel != 0 && remainTime < runTime / 100 * lowLevel) || (lowTime != 0 && outageTime > lowTime)) {
				if (!lowBattery) {
					/* The battery wasn't considered empty until yet, but it has turned empty just now. */
					Logger_log(LOG_WARNING, "The system is running out of battery now.");
					Logger_log(LOG_INFO, "The outage is already lasting for %s hours now.", Upsd_printTime(outageTime));
					if (runTime != 0) {
						Logger_log(LOG_INFO, "The estimated current battery charge level is %i %%. The battery is considered empty now.", remainTime / (runTime / 100));
					}
					Logger_log(LOG_INFO, "Appropiate action will be taken now (usually a system shutdown).");
					
					/* Run the "low_battery" shell script. */
					if (system(inline_strcat(SCRIPT_DIR, "/low_battery")) == -1) {
						Logger_log(LOG_WARNING, "Could not run the \"low_battery\" shell script.");
					}
					
					lowBattery = true;
				}
			} else {
				/* The battery isn't considered empty any more. */
				lowBattery = false;
			}
		}
		
		/* Sleep until the next heartbeat. */
		(void)usleep(Settings_getCheckInterval(self->settings) * 1000);
	}
}

/* See header for documentation. */
char Upsd_getProgMode(const struct Upsd *self) {
	return self->progMode;
}

/* See header for documentation. */
const char *Upsd_getSignalName(int signal) {
	if (signal == SIGINT) {
		return "SIGINT";
	} else if (signal == SIGTERM) {
		return "SIGTERM";
	} else {
		return NULL;
	}
}
