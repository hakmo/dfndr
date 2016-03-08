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
 * Forward declaration of the Settings object, allowing no public member access.
 */
struct Settings;

/**
 * Constructs a new Settings object which will read the configuration from the given @a configFile.
 * Returns the constructed Settings object, or NULL on failure.
 */
struct Settings *Settings_construct(const char *configFile);

/**
 * Destructs the given Settings object.
 */
void Settings_destruct(struct Settings *self);

/**
 * Getter for enabled status.
 */
bool Settings_getEnabled(const struct Settings *self);

/**
 * Getter for the interface.
 */
const char *Settings_getInterface(const struct Settings *self);

/**
 * Getter for the check interval.
 */
int Settings_getCheckInterval(const struct Settings *self);

/**
 * Getter for the update interval.
 */
int Settings_getUpdateInterval(const struct Settings *self);

/**
 * Getter for the battery run time.
 */
int Settings_getRunTime(const struct Settings *self);

/**
 * Getter for the battery charge time.
 */
int Settings_getChargeTime(const struct Settings *self);

/**
 * Getter for the battery low level.
 */
int Settings_getLowLevel(const struct Settings *self);

/**
 * Getter for the battery low time.
 */
int Settings_getLowTime(const struct Settings *self);
