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
 * MollenOS MCore - Generic Block Bitmap Implementation
 */

#ifndef _MOLLENOS_BLOCKBITMAP_H_
#define _MOLLENOS_BLOCKBITMAP_H_

#include <os/osdefs.h>
#include <ds/ds.h>
#include <ds/bitmap.h>

/* BlockBitmap_t
 * A specialized bitmap that deals in blocks instead of bits.
 * This way it can be used for keeping track of memory regions in <BlockSize> */
typedef struct _BlockBitmap {
    Bitmap_t            Base;
    SafeMemoryLock_t    SyncObject;
   
    // Block Information
    uintptr_t           BlockStart;
    uintptr_t           BlockEnd;
    size_t              BlockSize;
    size_t              BlockCount;

    // Statistics
    int                 BlocksAllocated;
    int                 NumAllocations;
    int                 NumFrees;
} BlockBitmap_t;

/* BlockBitmapCreate
 * Instantiate a new bitmap that keeps track of a
 * block range between Start -> End with a given block size */
CRTDECL(
BlockBitmap_t*,
BlockBitmapCreate(
    _In_ uintptr_t      BlockStart, 
    _In_ uintptr_t      BlockEnd, 
    _In_ size_t         BlockSize));

/* BlockBitmapDestroy
 * Destroys a block bitmap, and releases 
 * all resources associated with the bitmap */
CRTDECL(
OsStatus_t,
BlockBitmapDestroy(
    _In_ BlockBitmap_t* Blockmap));

/* BlockBitmapAllocate
 * Allocates a number of bytes in the bitmap (rounded up in blocks)
 * and returns the calculated block of the start of block allocated (continously) */
CRTDECL(
uintptr_t,
BlockBitmapAllocate(
    _In_ BlockBitmap_t* Blockmap,
    _In_ size_t         Size));

/* BlockBitmapFree
 * Deallocates a given block translated into offsets 
 * into the given bitmap, and frees them in the bitmap */
CRTDECL(
OsStatus_t,
BlockBitmapFree(
    _In_ BlockBitmap_t* Blockmap,
    _In_ uintptr_t      Block,
    _In_ size_t         Size));

/* BlockBitmapValidate
 * Validates the given block that it's within
 * range of our bitmap and that it is either set or clear */
CRTDECL(
OsStatus_t,
BlockBitmapValidateState(
    _In_ BlockBitmap_t* Blockmap,
    _In_ uintptr_t      Block,
    _In_ int            Set));

#endif //!_MOLLENOS_BLOCKBITMAP_H_
