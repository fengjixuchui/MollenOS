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
 * MollenOS MCore - USB Controller Manager
 * - Contains the implementation of a shared controller manager
 *   for all the usb drivers
 */

#ifndef __USB_HCI__
#define __USB_HCI__

/* Includes
 * - Library */
#include <os/osdefs.h>
#include <os/contracts/usbhost.h>
#include "manager.h"

/* HciControllerCreate 
 * Initializes and creates a new Hci Controller instance
 * from a given new system device on the bus. */
__EXTERN
UsbManagerController_t*
HciControllerCreate(
    _In_ MCoreDevice_t*             Device);

/* HciControllerDestroy
 * Destroys an existing controller instance and cleans up
 * any resources related to it */
__EXTERN
OsStatus_t
HciControllerDestroy(
    _In_ UsbManagerController_t*    Controller);

/* HciPortReset
 * Resets the given port and returns the result of the reset */
__EXTERN
OsStatus_t
HciPortReset(
    _In_ UsbManagerController_t*    Controller, 
    _In_ int                        Index);

/* HciPortGetStatus 
 * Retrieve the current port status, with connected and enabled information */
__EXTERN
void
HciPortGetStatus(
    _In_  UsbManagerController_t*   Controller,
    _In_  int                       Index,
    _Out_ UsbHcPortDescriptor_t*    Port);

/* HciQueueTransferGeneric 
 * Queues a new asynchronous/interrupt transfer for the given driver and pipe. 
 * The function does not block. */
__EXTERN
UsbTransferStatus_t
HciQueueTransferGeneric(
    _In_ UsbManagerTransfer_t*      Transfer);

/* HciQueueTransferIsochronous 
 * Queues a new isochronous transfer for the given driver and pipe. 
 * The function does not block. */
__EXTERN
UsbTransferStatus_t
HciQueueTransferIsochronous(
    _In_ UsbManagerTransfer_t*      Transfer);

/* HciDequeueTransfer 
 * Removes a queued transfer from the controller's transfer list */
__EXTERN
UsbTransferStatus_t
HciDequeueTransfer(
    _In_ UsbManagerTransfer_t*      Transfer);

#endif //!__USB_HCI__
