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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>



/* ========= Implementations ========= */



/* See header for documentation. */
bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/* See header for documentation. */
const char **split(const char *str, int *parts) {
	/* Run a first pass to determine the number of splits. This avoids to reallocate the target buffer multiple times. */
	*parts = 0;
	int lastPos = -1;
	const char *val;
	for (val = str; *val != '\0'; val++) {
		if (isWhitespace(*val)) {
			int curPos = val - str;
			if (curPos > lastPos + 1) {
				/* New split found. */
				(*parts)++;
			}
			lastPos = curPos;
		}
	}
	/* Consider last split as well. */
	if (val - str > lastPos + 1) {
		(*parts)++;
	}

	/* Allocate the target buffer. */
	const char **splits = (const char **)malloc(sizeof(char *) * (*parts));
	if (splits == NULL) {
		return NULL;
	}

	/* Run a second pass to extract the splits. */
	*parts = 0;
	lastPos = -1;
	for (val = str; *val != '\0'; val++) {
		if (isWhitespace(*val)) {
			int curPos = val - str;
			if (curPos > lastPos + 1) {
				/* New split found. */
				splits[*parts] = strndup(str + lastPos + 1, curPos - lastPos - 1);
				if (splits[*parts] == NULL) {
					freeStrings(splits, *parts);
					(*parts)++;
					return NULL;
				}
				(*parts)++;
			}
			lastPos = curPos;
		}
	}
	/* Consider last split as well. */
	if (val - str > lastPos + 1) {
		splits[*parts] = strdup(str + lastPos + 1);
		if (splits[*parts] == NULL) {
			freeStrings(splits, *parts);
			(*parts)++;
			return NULL;
		}
		(*parts)++;
	}

	return splits;
}

/* See header for documentation. */
void freeStrings(const char **strings, int count) {
	if (strings != NULL) {
		for (int i = 0; i < count; i++) {
			free((void *)strings[i]);
		}
		free(strings);
	}
}

/* See header for documentation. */
const char *inline_strcat(const char *str1, const char *str2) {
	static char buf[512];
	(void)snprintf(buf, ARRAY_SIZE(buf), "%s%s", str1, str2);
	return buf;
}