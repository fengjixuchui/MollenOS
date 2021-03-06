/* MollenOS
 *
 * Copyright 2017, Philip Meulengracht
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ? , either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * MollenOS Inter-Process Communication Interface
 * - Shared definitions
 * - Remote Procedure Call routines
 * - Event Procedure Call routines
 * - Pipe routines
 */

#ifndef __IPC_INTERFACE__
#define __IPC_INTERFACE__

#include <ddk/ddkdefs.h>

/* IPC Declaration definitions that can
 * be used by the different IPC systems */
#define IPC_DECL_FUNCTION(FunctionNo)   (int)FunctionNo
#define IPC_DECL_EVENT(EventNo)         (int)(0x100 + EventNo)
#define IPC_MAX_ARGUMENTS               5
#define IPC_MAX_MESSAGELENGTH           2048

/* Argument type definitions 
 * Used by both RPC and Event argument systems */
#define ARGUMENT_NOTUSED                0
#define ARGUMENT_BUFFER                 1
#define ARGUMENT_REGISTER               2

#include <ddk/ipc/rpc.h>
#include <ddk/ipc/pipe.h>

#endif //!__IPC_INTERFACE__
