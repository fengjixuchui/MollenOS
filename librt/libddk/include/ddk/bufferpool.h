/* MollenOS
 *
 * Copyright 2011 - 2017, Philip Meulengracht
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
 * BufferPool Support Definitions & Structures
 * - This header describes the base bufferpool-structures, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

#ifndef _BUFFERPOOL_INTERFACE_H_
#define _BUFFERPOOL_INTERFACE_H_

#include <ddk/buffer.h>
#include <ddk/ddkdefs.h>

typedef struct _BufferPool BufferPool_t;

_CODE_BEGIN
/* BufferPoolCreate
 * Creates a new buffer-pool from the given buffer object. 
 * This allows sub-allocations from a buffer-object. */
DDKDECL(
OsStatus_t,
BufferPoolCreate(
    _In_  DmaBuffer_t*      Buffer,
    _Out_ BufferPool_t**    Pool));

/* BufferPoolDestroy
 * Cleans up the buffer-pool and deallocates resources previously
 * allocated. This does not destroy the buffer-object. */
DDKDECL(
OsStatus_t,
BufferPoolDestroy(
    _In_ BufferPool_t*      Pool));

/* BufferPoolAllocate
 * Allocates the requested size and outputs two addresses. The
 * virtual pointer to the accessible data, and the address of its 
 * corresponding physical address for hardware. */
DDKDECL(
OsStatus_t,
BufferPoolAllocate(
    _In_ BufferPool_t*      Pool,
    _In_ size_t             Size,
    _Out_ uintptr_t**       VirtualPointer,
    _Out_ uintptr_t*        PhysicalAddress));

/* BufferPoolFree
 * Frees previously allocations made by the buffer-pool. The virtual
 * address must be the one passed back. */
DDKDECL(
OsStatus_t,
BufferPoolFree(
    _In_ BufferPool_t*      Pool,
    _In_ uintptr_t*         VirtualPointer));
_CODE_END

#endif //!_BUFFERPOOL_INTERFACE_H_
