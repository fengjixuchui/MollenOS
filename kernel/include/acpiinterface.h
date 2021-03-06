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
 * MollenOS MCore - ACPI(CA) Device Scan Interface
 */

#ifndef __ACPI_INTERFACE_H__
#define __ACPI_INTERFACE_H__

#include <os/osdefs.h>
#include <ds/collection.h>
#include <acpi.h>

/* ACPICA Definitions 
 * Contains generic magic constants and definitions */
#define ACPI_MAX_INIT_TABLES    16
#define APCI_MAX_PRW_RESOURCES  8

#define ACPI_NOT_AVAILABLE      0
#define ACPI_AVAILABLE          1

/* ACPICA Feature Definitions 
 * Contains generic magic constants and definitions */
#define ACPI_FEATURE_STA        0x1
#define ACPI_FEATURE_CID        0x2
#define ACPI_FEATURE_RMV        0x4
#define ACPI_FEATURE_EJD        0x8
#define ACPI_FEATURE_LCK        0x10
#define ACPI_FEATURE_PS0        0x20
#define ACPI_FEATURE_PRW        0x40
#define ACPI_FEATURE_ADR        0x80
#define ACPI_FEATURE_HID        0x100
#define ACPI_FEATURE_UID        0x200
#define ACPI_FEATURE_PRT        0x400
#define ACPI_FEATURE_BBN        0x800
#define ACPI_FEATURE_SEG        0x1000
#define ACPI_FEATURE_REG        0x2000
#define ACPI_FEATURE_CRS        0x4000

/* ACPICA Type Definitions 
 * Contains generic magic constants and definitions */
#define ACPI_BUS_SYSTEM         0x0
#define ACPI_BUS_TYPE_DEVICE    0x1
#define ACPI_BUS_TYPE_PROCESSOR 0x2
#define ACPI_BUS_TYPE_THERMAL   0x3
#define ACPI_BUS_TYPE_POWER     0x4
#define ACPI_BUS_TYPE_SLEEP     0x5
#define ACPI_BUS_TYPE_PWM       0x6
#define ACPI_BUS_ROOT_BRIDGE    0x7

/* ACPICA Video Definitions 
 * Contains generic magic constants and definitions */
#define ACPI_VIDEO_SWITCHING    0x1
#define ACPI_VIDEO_ROM          0x2
#define ACPI_VIDEO_POSTING      0x4
#define ACPI_VIDEO_BACKLIGHT    0x8
#define ACPI_VIDEO_BRIGHTNESS   0x10

/* ACPICA Battery Definitions 
 * Contains generic magic constants and definitions */
#define ACPI_BATTERY_NORMAL     0x1
#define ACPI_BATTERY_EXTENDED   0x2
#define ACPI_BATTERY_QUERY      0x4
#define ACPI_BATTERY_CHARGEINFO 0x8
#define ACPI_BATTERY_CAPMEAS    0x10

/* ACPICA Package Definitions 
 * Package manipulation convenience functions. */
#define ACPI_PKG_VALID(pkg, size)                        \
((pkg) != NULL && (pkg)->Type == ACPI_TYPE_PACKAGE &&    \
 (pkg)->Package.Count >= (size))

PACKED_TYPESTRUCT(AcpiEcdt, {
    ACPI_HANDLE          Handle;
    ACPI_GENERIC_ADDRESS CommandAddress;
    ACPI_GENERIC_ADDRESS DataAddress;
    UINT8                Gpe;
    UINT32               UId;
    char                 NsPath[32];
});

PACKED_TYPESTRUCT(PciRoutingEntry, {
    CollectionItem_t Header;
    int              Irq;
    uint8_t          AcType;
    uint8_t          Trigger;
    uint8_t          Shareable;
    uint8_t          Polarity;
    uint8_t          Fixed;
});

PACKED_TYPESTRUCT(PciRoutingSource, {
    CollectionItem_t   Header;
    ACPI_HANDLE        Handle;
    Collection_t*      Entries;
    PciRoutingEntry_t* ActiveEntry;
});

PACKED_TYPESTRUCT(PciRoutings, {
    Collection_t* Sources; // List of lists of irqs
    Collection_t* InterruptEntries[128];
    int           ActiveIrqs[128];
});

/* AcpiDevicePower
 * ACPI Device Power information. This is needed in
 * order to control the power levels and states. */
PACKED_TYPESTRUCT(AcpiDevicePower, {
    ACPI_HANDLE             GpeHandle;
    UINT32                  GpeBit;
    UINT32                  LowestWakeState; // Lowest state capable of wake
    
    UINT32                  PowerResourceCount;
    ACPI_OBJECT*            PowerResources[APCI_MAX_PRW_RESOURCES];
});

/* AcpiDevice 
 * Generic ACPI device representation in MCore.
 * Contains all information neccessary to use the device. */
PACKED_TYPESTRUCT(AcpiDevice, {
    CollectionItem_t            Header;
    ACPI_HANDLE                 Handle;
    ACPI_HANDLE                 Parent;
    char                        Name[128];
    int                         Type;
    
    char                        HId[16];
    char                        UId[16];
    char                        BusId[8];
    ACPI_PNP_DEVICE_ID_LIST*    CId;
    
    ACPI_PCI_ID                 PciLocation;
    int                         GlobalBus;
    
    size_t                      Features;   // Supported namespace functions
    size_t                      FeaturesEx; // Type features
    size_t                      Status;
    uint64_t                    Address;

    // Feature data
    PciRoutings_t*              Routings;
    AcpiDevicePower_t           PowerSettings;
});

/* AcpiInitializeEarly
 * Initializes Early Access and enumerates the APIC Table */
KERNELAPI OsStatus_t KERNELABI
AcpiInitializeEarly(void);

/* Initializes the full access and functionality
 * of ACPICA / ACPI and allows for scanning of ACPI devices */
KERNELAPI void KERNELABI
AcpiInitialize(void);

/* AcpiDevicesScan
 * Scan the ACPI namespace for devices and irq-routings, 
 * this is very neccessary for getting correct irqs */
KERNELAPI ACPI_STATUS KERNELABI
AcpiDevicesScan(void);

/* This returns ACPI_NOT_AVAILABLE if ACPI is not available
 * on the system, or ACPI_AVAILABLE if acpi is available */
__EXTERN int AcpiAvailable(void);

/* AcpiDeviceLookupBusRoutings
 * lookup a bridge device for the given bus that contains pci routings */
KERNELAPI AcpiDevice_t* KERNELABI
AcpiDeviceLookupBusRoutings(
    _In_ int                Bus);

/* AcpiDeviceAttachData
 * Stores custom context data for an individual acpi-device handle */
KERNELAPI ACPI_STATUS KERNELABI
AcpiDeviceAttachData(
    _In_ AcpiDevice_t*      Device,
    _In_ int                Type);

/* AcpiDeviceGetStatus
 * Retrieves the status of the device by querying the _STA method. */
KERNELAPI ACPI_STATUS KERNELABI
AcpiDeviceGetStatus(
    _InOut_ AcpiDevice_t* Device);

/* AcpiDeviceGetBusAndSegment
 * Retrieves the initial location on the bus for the device */
KERNELAPI ACPI_STATUS KERNELABI
AcpiDeviceGetBusAndSegment(
    _InOut_ AcpiDevice_t* Device);

__EXTERN ACPI_STATUS AcpiDeviceGetBusId(AcpiDevice_t *Device, uint32_t Type);
__EXTERN ACPI_STATUS AcpiDeviceGetFeatures(AcpiDevice_t *Device);

/* AcpiDeviceGetIrqRoutings
 * Utilizies ACPICA to retrieve all the irq-routings from
 * the ssdt information. */
KERNELAPI ACPI_STATUS KERNELABI
AcpiDeviceGetIrqRoutings(
    _In_ AcpiDevice_t *Device);

/* AcpiDeviceGetHWInfo
 * Retrieves acpi-hardware information like Status, Address
 * CId's, HId, UId, CLS etc */
KERNELAPI ACPI_STATUS KERNELABI
AcpiDeviceGetHWInfo(
    _InOut_ AcpiDevice_t *Device,
    _In_ ACPI_HANDLE ParentHandle,
    _In_ int Type);

/* AcpiDeviceParsePower
 * Parses and validates the _PRW feature of a GPE device. */
KERNELAPI ACPI_STATUS KERNELABI
AcpiDeviceParsePower(
    _InOut_ AcpiDevice_t *Device);

/* Device Type Helpers */
__EXTERN ACPI_STATUS AcpiDeviceIsVideo(AcpiDevice_t *Device);
__EXTERN ACPI_STATUS AcpiDeviceIsDock(AcpiDevice_t *Device);
__EXTERN ACPI_STATUS AcpiDeviceIsBay(AcpiDevice_t *Device);
__EXTERN ACPI_STATUS AcpiDeviceIsBattery(AcpiDevice_t *Device);

#endif //!__ACPI_INTERFACE_H__
