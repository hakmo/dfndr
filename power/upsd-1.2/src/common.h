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

#pragma once



/* ========= Includes ========= */



#include <stdbool.h>



/* ========= Public definitions ========= */



/**
 * Defines the static path of the configuration file.
 */
#define CONFIG_FILE CONF_DIR "/" PACKAGE_NAME ".conf"

/**
 * Defines the static path of the status file.
 */
#define STATUS_FILE STATE_DIR "/" PACKAGE_NAME ".status"

/**
 * Defines the static path of the pid file.
 */
#define PID_FILE "/var/run/" PACKAGE_NAME ".pid"

/**
 * Macro for obtaining the size of an array.
 */
#define ARRAY_SIZE(x) ((int)(sizeof(x) / sizeof(x[0])))

/**
 * Returns whether or not the given character @a c is a whitespace character (ASCII only).
 */
bool isWhitespace(char c);

/**
 * Splits the given string @a str by whitespaces and returns the list of splitted words. The returned list
 * will never contain any empty strings. The @a parts parameter is set to the number of splits contained
 * within the returned list by this function. Note that if @a parts is set to zero by this function, then
 * this function MAY return NULL. If @a parts is set to any value greater than zero, then a return value of
 * NULL indicates an error. The returned strings are freshly allocated and must be freed by the function
 * caller. Use the provided freeStrings() function to do that in a convenient way.
 */
const char **split(const char *str, int *parts);

/**
 * Cleans up the given @a strings. The @a count parameter must be set to the number of strings contained
 * in @a strings. It is safe to pass NULL for the @a strings parameter.
 */
void freeStrings(const char **strings, int count);

/**
 * Concatenates the given strings @a str1 and @a str2 and returns the result within a static string buffer.
 */
const char *inline_strcat(const char *str1, const char *str2);
