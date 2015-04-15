/**
 *  @file syscall_example.h
 *
 *  @section LICENSE
 *  Copyright (C) 2009 Kalray
 *  @author Patrice, GERIN patrice.gerin@kalray.eu
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _SYSCALL_EXAMPLE_H
#define _SYSCALL_EXAMPLE_H

#include "syscall_interface.h"

// Declare syscalls helpers
errcode_t do_example(debug_agent_interface_t *interface, int *sys_ret);

#endif
