/* MollenOS
 *
 * Copyright 2019, Philip Meulengracht
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
 * DLL Service Definitions & Structures
 * - This header describes the base library-structure, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

#include <internal/_syscalls.h>
#include <internal/_utils.h>

#include <os/services/targets.h>
#include <os/services/sharedobject.h>
#include <ddk/services/process.h>

#include <ds/collection.h>
#include <ds/mstring.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

typedef struct _LibraryItem {
    CollectionItem_t Header;
    Handle_t         Handle;
    int              References;
} LibraryItem_t;

typedef void (*SOInitializer_t)(int);
static Collection_t LoadedLibraries = COLLECTION_INIT(KeyId);

static size_t SharedObjectHash(const char *String) {
	uint8_t* Pointer    = (uint8_t*)String;
	size_t Hash         = 5381;
	int Character       = 0;
	if (String == NULL) {
        return 0;
    }
	while ((Character = tolower(*Pointer++)) != 0)
		Hash = ((Hash << 5) + Hash) + Character; /* hash * 33 + c */
	return Hash;
}

static void
SetErrnoFromOsStatus(
    _In_ OsStatus_t Status)
{
    switch (Status) {
        case OsSuccess:
            _set_errno(EOK);
            break;
        case OsError:
            _set_errno(ELIBACC);
            break;
        case OsExists:
            _set_errno(EEXIST);
            break;
        case OsDoesNotExist:
            _set_errno(ENOENT);
            break;
        case OsInvalidParameters:
            _set_errno(EINVAL);
            break;
        case OsInvalidPermissions:
            _set_errno(EACCES);
            break;
        case OsTimeout:
            _set_errno(ETIME);
            break;
        case OsNotSupported:
            _set_errno(ENOSYS);
            break;
    }
}

Handle_t 
SharedObjectLoad(
	_In_ const char* SharedObject)
{
    SOInitializer_t Initializer = NULL;
    LibraryItem_t*  Library     = NULL;
    Handle_t        Result      = HANDLE_INVALID;
    DataKey_t       Key         = { .Value.Id = SharedObjectHash(SharedObject) };
    OsStatus_t      Status      = OsSuccess;

    // Special case
    if (SharedObject == NULL) {
        return HANDLE_GLOBAL;
    }

    Library = CollectionGetDataByKey(&LoadedLibraries, Key, 0);
    if (Library == NULL) {
        if (IsProcessModule()) {
            Status = Syscall_LibraryLoad(SharedObject, &Result);
        }
        else {
            Status = ProcessLoadLibrary(SharedObject, &Result);
        }

        if (Status == OsSuccess && Result != HANDLE_INVALID) {
            Library = (LibraryItem_t*)malloc(sizeof(LibraryItem_t));
            COLLECTION_NODE_INIT((CollectionItem_t*)Library, Key);
            Library->Handle     = Result;
            Library->References = 0;
            CollectionAppend(&LoadedLibraries, (CollectionItem_t*)Library);
        }
    }

    if (Library != NULL) {
        Library->References++;
        if (Library->References == 1) {
            Initializer = (SOInitializer_t)SharedObjectGetFunction(Library->Handle, "__CrtLibraryEntry");
            if (Initializer != NULL) {
                Initializer(DLL_ACTION_INITIALIZE);
            }
        }
        Result = Library->Handle;
    }
    SetErrnoFromOsStatus(Status);
    return Result;
}

void*
SharedObjectGetFunction(
	_In_ Handle_t       Handle, 
	_In_ const char*    Function)
{
    OsStatus_t Status;
	if (Handle == HANDLE_INVALID || Function == NULL) {
	    _set_errno(EINVAL);
		return NULL;
	}

    if (IsProcessModule()) {
        return (void*)Syscall_LibraryFunction(Handle, Function);
    }
	else {
        uintptr_t AddressOfFunction;
        Status = ProcessGetLibraryFunction(Handle, Function, &AddressOfFunction);
        SetErrnoFromOsStatus(Status);
        if (Status != OsSuccess) {
            return NULL;
        }
        return (void*)AddressOfFunction;
    }
}

OsStatus_t
SharedObjectUnload(
	_In_ Handle_t Handle)
{
    SOInitializer_t Initialize = NULL;
    LibraryItem_t*  Library    = NULL;
    OsStatus_t      Status;

	if (Handle == HANDLE_INVALID) {
	    _set_errno(EINVAL);
		return OsError;
	}
    if (Handle == HANDLE_GLOBAL) {
        return OsSuccess;
    }
    foreach(Node, &LoadedLibraries) {
        if (((LibraryItem_t*)Node)->Handle == Handle) {
            Library = (LibraryItem_t*)Node;
            break;
        }
    }
    if (Library != NULL) {
        Library->References--;
        if (Library->References == 0) {
            // Run finalizer before unload
            Initialize = (SOInitializer_t)SharedObjectGetFunction(Handle, "__CrtLibraryEntry");
            if (Initialize != NULL) {
                Initialize(DLL_ACTION_FINALIZE);
            }
            
            if (IsProcessModule()) {
                Status = Syscall_LibraryUnload(Handle);
            }
	        else {
                Status = ProcessUnloadLibrary(Handle);
            }
            SetErrnoFromOsStatus(Status);
            return Status;
        }
    }
	_set_errno(EFAULT);
    return OsError;
}
