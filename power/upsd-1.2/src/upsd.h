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



/* ========= Public definitions ========= */



/**
 * Constructs a new upsd program instance from the given @a argc and @a argv and returns it,
 * or returns NULL if there was an error.
 */
struct Upsd *Upsd_construct(int argc, char **argv);

/**
 * Destructs the upsd program instance.
 */
void Upsd_destruct(struct Upsd *self);

/**
 * Do the program work. This function will normally not return, since it's intended to run forever.
 */
void Upsd_run(struct Upsd *self);

/**
 * Returns the program mode.
 */
char Upsd_getProgMode(const struct Upsd *self);

/**
 * Returns the name of the given @a signal, or NULL if the signal name is unknown.
 */
const char *Upsd_getSignalName(int signal);
