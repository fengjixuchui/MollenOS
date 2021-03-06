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
 * Process Service Definitions & Structures
 * - This header describes the base process-structure, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

#include <internal/_syscalls.h>
#include <internal/_utils.h>

#include <ddk/services/process.h>
#include <os/services/targets.h>
#include <os/services/process.h>
#include <os/context.h>
#include <threads.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include "../../stdio/local.h"

void
InitializeStartupInformation(
    _In_ ProcessStartupInformation_t* StartupInformation)
{
    memset(StartupInformation, 0, sizeof(ProcessStartupInformation_t));

    // Reset handles
    StartupInformation->StdOutHandle = STDOUT_FILENO;
    StartupInformation->StdInHandle  = STDIN_FILENO;
    StartupInformation->StdErrHandle = STDERR_FILENO;
}

UUId_t 
ProcessSpawn(
    _In_     const char* Path,
    _In_Opt_ const char* Arguments)
{
    ProcessStartupInformation_t StartupInformation;

    // Sanitize parameters
    if (Path == NULL) {
        _set_errno(EINVAL);
        return UUID_INVALID;
    }

    // Setup information block
    InitializeStartupInformation(&StartupInformation);
    StartupInformation.InheritFlags = PROCESS_INHERIT_NONE;
    return ProcessSpawnEx(Path, Arguments, &StartupInformation);
}

UUId_t
ProcessSpawnEx(
    _In_     const char*                  Path,
    _In_Opt_ const char*                  Arguments,
    _In_     ProcessStartupInformation_t* StartupInformation)
{
    MRemoteCall_t Request;
    UUId_t        Handle                  = UUID_INVALID;
    void*         InheritationBlock       = NULL;
    size_t        InheritationBlockLength = 0;
    OsStatus_t    Status;
    assert(Path != NULL);
    assert(StartupInformation != NULL);

    StdioCreateInheritanceBlock(StartupInformation, &InheritationBlock, &InheritationBlockLength);
    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_CREATE_PROCESS);
    RPCSetArgument(&Request, 0, (const void*)Path, strlen(Path) + 1);
    RPCSetArgument(&Request, 1, (const void*)StartupInformation, sizeof(ProcessStartupInformation_t));
    if (InheritationBlock != NULL) {
        RPCSetArgument(&Request, 2, (const void*)InheritationBlock, InheritationBlockLength);
    }
    if (Arguments != NULL) {
        RPCSetArgument(&Request, 3, (const void*)Arguments, strlen(Arguments) + 1);
    }
    RPCSetResult(&Request, (const void*)&Handle, sizeof(UUId_t));

    Status = RPCExecute(&Request);
    if (InheritationBlock != NULL) {
        free(InheritationBlock);
    }
    return Handle;
}

OsStatus_t 
ProcessJoin(
	_In_  UUId_t Handle,
    _In_  size_t Timeout,
    _Out_ int*   ExitCode)
{
    JoinProcessPackage_t Package;
    MRemoteCall_t        Request;
    OsStatus_t           Status;

    if (Handle == UUID_INVALID || ExitCode == NULL) {
        _set_errno(EINVAL);
        return OsError;
    }

    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_JOIN_PROCESS);
    RPCSetArgument(&Request, 0, (const void*)&Handle, sizeof(Handle));
    RPCSetArgument(&Request, 1, (const void*)&Timeout, sizeof(size_t));
    RPCSetResult(&Request, (const void*)&Package, sizeof(JoinProcessPackage_t));
    Status = RPCExecute(&Request);
    if (Status != OsSuccess) {
        return Status;
    }
    *ExitCode = Package.ExitCode;
    return Package.Status;
}

OsStatus_t
ProcessKill(
	_In_ UUId_t Handle)
{
    MRemoteCall_t Request;
    OsStatus_t    Status = OsSuccess;
    OsStatus_t    Result = OsSuccess;

    if (Handle == UUID_INVALID ) {
        _set_errno(EINVAL);
        return OsError;
    }

    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_KILL_PROCESS);
    RPCSetArgument(&Request, 0, (const void*)&Handle, sizeof(Handle));
    RPCSetResult(&Request, (const void*)&Result, sizeof(OsStatus_t));
    Status = RPCExecute(&Request);
    if (Status != OsSuccess) {
        return Status;
    }
    return Result;
}

UUId_t
ProcessGetCurrentId(void)
{
    MRemoteCall_t Request;
    UUId_t        ProcessId = *GetInternalProcessId();
    OsStatus_t    Status;
    
    if (ProcessId == UUID_INVALID) {
        if (IsProcessModule()) {
            Syscall_ModuleId(&ProcessId);
        }
        else {
            RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_GET_PROCESS_ID);
            RPCSetResult(&Request, (const void*)&ProcessId, sizeof(UUId_t));
            
            // Don't invoke the regular execute as that results in call to this function
            // We don't need a proper set From.Process for this call to work
            Status = Syscall_RemoteCall(&Request, 0);
            assert(Status == OsSuccess);
        }
        *GetInternalProcessId() = ProcessId;
    }
    return ProcessId;
}

OsStatus_t
ProcessGetTickBase(
    _Out_ clock_t* Tick)
{
    MRemoteCall_t Request;
    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_GET_PROCESS_TICK);
    RPCSetResult(&Request, (const void*)Tick, sizeof(clock_t));
    return RPCExecute(&Request);
}

OsStatus_t
GetProcessCommandLine(
    _In_    const char* Buffer,
    _InOut_ size_t*     Length)
{
    MRemoteCall_t Request;
    OsStatus_t    Status = OsSuccess;

    if (IsProcessModule()) {
        return Syscall_ModuleGetStartupInfo(NULL, NULL, Buffer, Length);
    }

    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_GET_ARGUMENTS);
    RPCSetResult(&Request, (const void*)Buffer, *Length);
    Status = RPCExecute(&Request);
    *Length = Request.Result.Length;
    return Status;
}

OsStatus_t
ProcessGetCurrentName(
    _In_ const char* Buffer,
    _In_ size_t      MaxLength)
{
    MRemoteCall_t Request;
    
    if (IsProcessModule()) {
        return Syscall_ModuleName(Buffer, MaxLength);
    }

    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_GET_PROCESS_NAME);
    RPCSetResult(&Request, (const void*)Buffer, MaxLength);
    return RPCExecute(&Request);
}

OsStatus_t
ProcessGetAssemblyDirectory(
    _In_ UUId_t      Handle,
    _In_ const char* Buffer,
    _In_ size_t      MaxLength)
{
    MRemoteCall_t Request;
    
    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_GET_ASSEMBLY_DIRECTORY);
    RPCSetArgument(&Request, 0, (const void*)&Handle, sizeof(UUId_t));
    RPCSetResult(&Request, (const void*)Buffer, MaxLength);
    return RPCExecute(&Request);
}

OsStatus_t
ProcessGetWorkingDirectory(
    _In_ UUId_t      Handle,
    _In_ const char* Buffer,
    _In_ size_t      MaxLength)
{
    MRemoteCall_t Request;
    
    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_GET_WORKING_DIRECTORY);
    RPCSetArgument(&Request, 0, (const void*)&Handle, sizeof(UUId_t));
    RPCSetResult(&Request, (const void*)Buffer, MaxLength);
    return RPCExecute(&Request);
}

OsStatus_t
ProcessSetWorkingDirectory(
    _In_ const char* Path)
{
    MRemoteCall_t Request;
    OsStatus_t    Status = OsSuccess;
    OsStatus_t    Result = OsSuccess;
    
    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_SET_WORKING_DIRECTORY);
    RPCSetArgument(&Request, 0, (const void*)Path, strlen(Path) + 1);
    RPCSetResult(&Request, (const void*)&Result, sizeof(OsStatus_t));
    Status = RPCExecute(&Request);
    if (Status != OsSuccess) {
        return Status;
    }
    return Result;
}

OsStatus_t
ProcessGetLibraryEntryPoints(
    _Out_ Handle_t LibraryList[PROCESS_MAXMODULES])
{
    MRemoteCall_t Request;
    
    if (IsProcessModule()) {
        return Syscall_ModuleGetModuleEntryPoints(LibraryList);
    }

    RPCInitialize(&Request, __PROCESSMANAGER_TARGET, 1, __PROCESSMANAGER_GET_LIBRARY_ENTRIES);
    RPCSetResult(&Request, (const void*)&LibraryList[0], sizeof(Handle_t) * PROCESS_MAXMODULES);
    return RPCExecute(&Request);
}
