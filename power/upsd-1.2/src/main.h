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
 * The main entry point of the program, giving the argument count @a argc and the argument vector @a argv. Returns EXIT_SUCCESS
 * on success and EXIT_FAILURE on failure.
 */
int main(int argc, char **argv);

/**
 * Handles the given @a signal. This includes logging an error message, closing down all allocated resources and terminating the
 * program with exit code EXIT_FAILURE.
 */
void handleSignal(int signal);
