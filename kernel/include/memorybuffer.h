/* MollenOS
 *
 * Copyright 2018, Philip Meulengracht
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
 * MollenOS Memory Buffer Interface
 * - Implementation of the memory dma buffers. This provides a transfer buffer
 *   that is not bound to any specific virtual memory area but instead are bound
 *   to fixed physical addreses.
 */

#ifndef __MEMORY_BUFFER_INTERFACE__
#define __MEMORY_BUFFER_INTERFACE__

#include <os/osdefs.h>
#include <ddk/buffer.h>
#include <memoryspace.h>

typedef struct {
    uintptr_t Physical;
    size_t    Capacity;
} SystemMemoryBuffer_t;

/* CreateMemoryBuffer 
 * Creates a new memory buffer instance of the given size. The allocation
 * of resources happens at this call, and reference is set to 1. Size is automatically
 * rounded up to a block-alignment */
KERNELAPI OsStatus_t KERNELABI
CreateMemoryBuffer(
    _In_  size_t        Size,
    _Out_ DmaBuffer_t*  MemoryBuffer);

/* AcquireMemoryBuffer
 * Acquires an existing memory buffer into the current memory space. This will
 * add it to the list of in-use buffers and increase reference count. */
KERNELAPI OsStatus_t KERNELABI
AcquireMemoryBuffer(
    _In_  UUId_t        Handle,
    _Out_ DmaBuffer_t*  MemoryBuffer);

/* QueryMemoryBuffer
 * Queries the handle for information instead of acquiring the memory buffer. This
 * can be usefull when no access is needed to the buffer. */
KERNELAPI OsStatus_t KERNELABI
QueryMemoryBuffer(
    _In_  UUId_t        Handle,
    _Out_ uintptr_t*    Dma,
    _Out_ size_t*       Capacity);

/* DestroyMemoryBuffer
 * Cleans up the resources associated with the handle. This function is registered
 * with the handle manager. */
KERNELAPI OsStatus_t KERNELABI
DestroyMemoryBuffer(
    _In_  void*         Resource);

#endif //! __MEMORY_BUFFER_INTERFACE__
