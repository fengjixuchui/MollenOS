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
 * MollenOS MCore - Enhanced Host Controller Interface Driver
 * TODO:
 * - Power Management
 * - Transaction Translator Support
 */
#define __TRACE

/* Includes
 * - System */
#include <os/utils.h>
#include "ehci.h"

/* Includes
 * - Library */
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* EchiCleanupTransferIsoc
 * Cleans up transation and the transfer resources. This can only
 * be called after the hardware reference has been dropped. */
OsStatus_t
EchiCleanupTransferIsoc(
    _In_  EhciController_t*     Controller,
    _In_  UsbManagerTransfer_t* Transfer)
{
    // Variables
    EhciIsochronousDescriptor_t *iTd = (EhciIsochronousDescriptor_t*)Transfer->EndpointDescriptor;
    while (iTd) {
        int16_t QueueIndex = iTd->QueueIndex;
        UsbSchedulerReleaseBandwidth(Controller->Scheduler, iTd->Interval, iTd->Bandwidth, iTd->StartFrame, 0);
        memset((void*)iTd, 0, sizeof(EhciIsochronousDescriptor_t));
        if (QueueIndex != EHCI_NO_INDEX) {
            iTd = &Controller->QueueControl.ITDPool[QueueIndex];
        }
        else {
            break;
        }
    }
    return OsSuccess;
}

/* EhciTransactionFinalizeIsoc
 * Cleans up the transfer, deallocates resources and validates the td's */
OsStatus_t
EhciTransactionFinalizeIsoc(
    _In_ EhciController_t*      Controller,
    _In_ UsbManagerTransfer_t*  Transfer)
{
    // Variables
    EhciIsochronousDescriptor_t *iTd = (EhciIsochronousDescriptor_t*)Transfer->EndpointDescriptor;

    // Debug
    TRACE("EhciTransactionFinalizeIsoc()");

    // Unlink the endpoint descriptor 
    SpinlockAcquire(&Controller->Base.Lock);
    EhciSetPrefetching(Controller, Transfer->Transfer.Type, 0);
    while (iTd) {
        EhciUnlinkPeriodic(Controller, (uintptr_t)iTd, iTd->Interval, iTd->StartFrame);
        if (iTd->QueueIndex != EHCI_NO_INDEX) {
            iTd = &Controller->QueueControl.ITDPool[iTd->QueueIndex];
        }
        else {
            break;
        }
    }
    EhciSetPrefetching(Controller, Transfer->Transfer.Type, 1);
    SpinlockRelease(&Controller->Base.Lock);

    // Mark for unscheduling, this will then be handled at the next door-bell
    iTd->HcdFlags |= EHCI_HCDFLAGS_UNSCHEDULE;
    EhciRingDoorbell(Controller);
    return OsSuccess;
}

/* HciQueueTransferIsochronous 
 * Queues a new isochronous transfer for the given driver and pipe. 
 * The function does not block. */
UsbTransferStatus_t
HciQueueTransferIsochronous(
    _In_ UsbManagerTransfer_t*      Transfer)
{
    // Variables
    EhciIsochronousDescriptor_t *FirstTd    = NULL;
    EhciIsochronousDescriptor_t *PreviousTd = NULL;
    EhciController_t *Controller            = NULL;
    uintptr_t AddressPointer                = Transfer->Transfer.Transactions[0].BufferAddress;
    size_t BytesToTransfer                  = Transfer->Transfer.Transactions[0].Length;
    size_t MaxBytesPerDescriptor            = 0;
    size_t Address, Endpoint;
    DataKey_t Key;
    int i;

    // Get Controller
    Controller              = (EhciController_t *)UsbManagerGetController(Transfer->DeviceId);
    Transfer->Status        = TransferNotProcessed;

    // Extract address and endpoint
    Address                 = HIWORD(Transfer->Pipe);
    Endpoint                = LOWORD(Transfer->Pipe);

    // Calculate mpd
    MaxBytesPerDescriptor   = 1024 * MAX(3, Transfer->Transfer.Endpoint.Bandwidth);
    MaxBytesPerDescriptor  *= 8;

    // Allocate resources
    while (BytesToTransfer) {
        EhciIsochronousDescriptor_t *iTd    = NULL;
        size_t BytesStep                    = MIN(BytesToTransfer, MaxBytesPerDescriptor);

        // Allocate td
        iTd = EhciIsocTdAllocate(Controller);
        if (iTd == NULL) {
            // Out of resources @todo
            ERROR("EHCI::Isoc out of resources");
            for(;;);
        }

        if (EhciIsocTdInitialize(Controller, &Transfer->Transfer, iTd, AddressPointer, BytesStep, Address, Endpoint) != OsSuccess) {
            // Out of bandwidth @todo
            ERROR("EHCI::Isoc out of bandwidth");
            for(;;);
        }

        // Update pointers
        if (FirstTd == NULL) {
            FirstTd                 = iTd;
        }
        else {
            // Not last in chain, clear IOC
            PreviousTd->QueueIndex  = iTd->Index;
            for (i = 0; i < 8; i++) {
                if (PreviousTd->Transactions[i] & EHCI_iTD_IOC) {
                    PreviousTd->Transactions[i] &= ~(EHCI_iTD_IOC);
                    PreviousTd->TransactionsCopy[i] &= ~(EHCI_iTD_IOC);
                }
            }
        }
        PreviousTd                  = iTd;
        AddressPointer              += BytesStep;
        BytesToTransfer             -= BytesStep;
    }

    // Add transfer
    Transfer->EndpointDescriptor    = (void*)FirstTd;
    Key.Value                       = 0;
    CollectionAppend(Controller->Base.TransactionList, CollectionCreateNode(Key, Transfer));
    return EhciTransactionDispatch(Controller, Transfer);
}