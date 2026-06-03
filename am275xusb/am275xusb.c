/*
 *  Copyright (C) 2018-2024 Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <string.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/ClockP.h>
#include <drivers/soc.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include "am275x_usb_msc.h"
#include "am275x_uac2.h"
#include "am275x_usb_device.h"
#include "am275x_usb_ep0.h"
#include "am275x_usb_hw.h"

#define AM275X_USB_APP_MSC   (1)
#define AM275X_USB_APP_UAC2  (2)

#ifndef AM275X_USB_ACTIVE_APP
#define AM275X_USB_ACTIVE_APP AM275X_USB_APP_UAC2
#endif

#ifndef AM275X_USB_ENABLE_USB0_HW_INIT
#define AM275X_USB_ENABLE_USB0_HW_INIT (1)
#endif

#ifndef AM275X_USB_ENABLE_USB0_CONNECT
#define AM275X_USB_ENABLE_USB0_CONNECT (1)
#endif

#ifndef AM275X_USB_USB0_POLL_FOREVER
#define AM275X_USB_USB0_POLL_FOREVER (1)
#endif

#define USB0_EVENT_BUFFER_SIZE (256U)
#define USB0_POLL_IDLE_DELAY   (100U)

#define EMMC_PARTITION_FULL_DISK_TOLERANCE_SECTORS (4096U)
#define EMMC_FAT32_PARTITION_START_SECTOR          (2048U)
#define EMMC_FAT32_RESERVED_SECTORS                (32U)
#define EMMC_FAT32_NUM_FATS                        (2U)
#define EMMC_FAT32_SECTORS_PER_CLUSTER             (64U)
#define EMMC_FORMAT_BUFFER_SECTORS                 (8U)

#define AM275X_DEV_USB0 (161U)
#define AM275X_DEV_MAIN_USB0_ISO_VD (178U)

#if AM275X_USB_ACTIVE_APP == AM275X_USB_APP_MSC
static Am275xUsbMsc gUsbMsc;
#elif AM275X_USB_ACTIVE_APP == AM275X_USB_APP_UAC2
static Am275xUac2Context gUac2;
#else
#error Unsupported AM275X_USB_ACTIVE_APP
#endif
static const Am275xUsbClassDriver *gUsbClassDriver;
static void *gUsbClassContext;
static Am275xUsbDevice gUsbDevice;
static Am275xUsbDcd gUsbDcd;
static Am275xUsbEp0 gUsbEp0 __attribute__((aligned(32)));
static uint32_t gUsb0EventBuffer[USB0_EVENT_BUFFER_SIZE / sizeof(uint32_t)] __attribute__((aligned(32)));
static uint8_t gEmmcMbrSector[AM275X_USB_MSC_BLOCK_SIZE] __attribute__((aligned(128)));
static uint8_t gEmmcFormatBuffer[AM275X_USB_MSC_BLOCK_SIZE * EMMC_FORMAT_BUFFER_SECTORS] __attribute__((aligned(128)));

volatile uint32_t gUsbBringupStep;
volatile int32_t gUsbBringupStatus;
volatile uint32_t gUsbLastEventRaw;
volatile uint32_t gUsbEventCount;
volatile uint32_t gUsbLastDsts;
volatile uint32_t gUsbLastEventKind;
volatile uint32_t gUsbLastEventType;
volatile uint32_t gUsbLastEventEp;
volatile uint32_t gUsbLastEventStatus;
volatile uint32_t gUsbLastEventParam;
volatile uint32_t gUsbEp0StateDebug;
volatile uint32_t gUsbSetupCountDebug;
volatile uint32_t gUsbStallCountDebug;
volatile uint32_t gUsbEventHistory[32];
volatile uint32_t gUsbEventHistoryIndex;

static uint16_t usb_msc_read_le16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0]) |
                      ((uint16_t)data[1] << 8U));
}

static uint32_t usb_msc_read_le32(const uint8_t *data)
{
    return (((uint32_t)data[0]) |
            ((uint32_t)data[1] << 8U) |
            ((uint32_t)data[2] << 16U) |
            ((uint32_t)data[3] << 24U));
}

static void usb_msc_write_le16(uint8_t *data, uint32_t offset, uint16_t value)
{
    data[offset] = (uint8_t)(value & 0xffU);
    data[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
}

static void usb_msc_write_le32(uint8_t *data, uint32_t offset, uint32_t value)
{
    data[offset] = (uint8_t)(value & 0xffU);
    data[offset + 1U] = (uint8_t)((value >> 8U) & 0xffU);
    data[offset + 2U] = (uint8_t)((value >> 16U) & 0xffU);
    data[offset + 3U] = (uint8_t)((value >> 24U) & 0xffU);
}

static bool emmc_has_full_disk_partition(MMCSD_Handle media, uint32_t blockCount)
{
    const uint32_t mbrEntryOffset = 446U;
    uint8_t partitionType;
    uint32_t partitionStart;
    uint32_t partitionSectors;
    uint32_t expectedSectors;
    uint64_t partitionEnd;
    int32_t status;

    if ((media == NULL) || (blockCount <= EMMC_PARTITION_FULL_DISK_TOLERANCE_SECTORS)) {
        return false;
    }

    status = MMCSD_read(media, gEmmcMbrSector, 0U, 1U);
    if (status != SystemP_SUCCESS) {
        DebugP_log("eMMC MBR read failed, status %d\r\n", status);
        return false;
    }

    if ((gEmmcMbrSector[510] != 0x55U) || (gEmmcMbrSector[511] != 0xAAU)) {
        return false;
    }

    partitionType = gEmmcMbrSector[mbrEntryOffset + 4U];
    partitionStart = usb_msc_read_le32(&gEmmcMbrSector[mbrEntryOffset + 8U]);
    partitionSectors = usb_msc_read_le32(&gEmmcMbrSector[mbrEntryOffset + 12U]);
    partitionEnd = (uint64_t)partitionStart + (uint64_t)partitionSectors;

    if ((partitionType == 0U) || (partitionSectors == 0U) ||
        (partitionStart >= blockCount) || (partitionEnd > blockCount)) {
        return false;
    }

    expectedSectors = blockCount - partitionStart;
    if (expectedSectors > EMMC_PARTITION_FULL_DISK_TOLERANCE_SECTORS) {
        expectedSectors -= EMMC_PARTITION_FULL_DISK_TOLERANCE_SECTORS;
    }

    if (partitionSectors < expectedSectors) {
        return false;
    }

    status = MMCSD_read(media, gEmmcMbrSector, partitionStart, 1U);
    if (status != SystemP_SUCCESS) {
        DebugP_log("eMMC FAT32 boot sector read failed, status %d\r\n", status);
        return false;
    }

    if ((gEmmcMbrSector[510] != 0x55U) || (gEmmcMbrSector[511] != 0xAAU)) {
        return false;
    }

    if ((usb_msc_read_le32(&gEmmcMbrSector[32U]) < expectedSectors) ||
        (usb_msc_read_le16(&gEmmcMbrSector[11U]) != AM275X_USB_MSC_BLOCK_SIZE) ||
        (gEmmcMbrSector[13U] != EMMC_FAT32_SECTORS_PER_CLUSTER) ||
        (gEmmcMbrSector[16U] != EMMC_FAT32_NUM_FATS) ||
        (memcmp(&gEmmcMbrSector[82U], "FAT32   ", 8U) != 0)) {
        return false;
    }

    return true;
}

static uint32_t emmc_fat32_calculate_fat_sectors(uint32_t partitionSectors, uint32_t *clusterCount)
{
    uint32_t fatSectors = 1U;
    uint32_t prevFatSectors;
    uint32_t dataSectors;
    uint32_t clusters;

    do {
        prevFatSectors = fatSectors;
        dataSectors = partitionSectors -
                      EMMC_FAT32_RESERVED_SECTORS -
                      (EMMC_FAT32_NUM_FATS * fatSectors);
        clusters = dataSectors / EMMC_FAT32_SECTORS_PER_CLUSTER;
        fatSectors = (((clusters + 2U) * 4U) + (AM275X_USB_MSC_BLOCK_SIZE - 1U)) /
                     AM275X_USB_MSC_BLOCK_SIZE;
    } while (fatSectors != prevFatSectors);

    *clusterCount = clusters;
    return fatSectors;
}

static int32_t emmc_write_zero_sectors(MMCSD_Handle media, uint32_t startSector, uint32_t sectorCount)
{
    uint32_t sectorsLeft = sectorCount;
    uint32_t sector = startSector;
    int32_t status;

    memset(gEmmcFormatBuffer, 0, sizeof(gEmmcFormatBuffer));
    while (sectorsLeft > 0U) {
        uint32_t chunk = sectorsLeft;

        if (chunk > EMMC_FORMAT_BUFFER_SECTORS) {
            chunk = EMMC_FORMAT_BUFFER_SECTORS;
        }

        status = MMCSD_write(media, gEmmcFormatBuffer, sector, chunk);
        if (status != SystemP_SUCCESS) {
            return status;
        }

        sector += chunk;
        sectorsLeft -= chunk;
    }

    return SystemP_SUCCESS;
}

static void emmc_fill_fat32_boot_sector(uint8_t *sector,
                                        uint32_t partitionStart,
                                        uint32_t partitionSectors,
                                        uint32_t fatSectors)
{
    memset(sector, 0, AM275X_USB_MSC_BLOCK_SIZE);

    sector[0] = 0xebU;
    sector[1] = 0x58U;
    sector[2] = 0x90U;
    memcpy(&sector[3], "MSDOS5.0", 8U);
    usb_msc_write_le16(sector, 11U, AM275X_USB_MSC_BLOCK_SIZE);
    sector[13] = EMMC_FAT32_SECTORS_PER_CLUSTER;
    usb_msc_write_le16(sector, 14U, EMMC_FAT32_RESERVED_SECTORS);
    sector[16] = EMMC_FAT32_NUM_FATS;
    usb_msc_write_le16(sector, 17U, 0U);
    usb_msc_write_le16(sector, 19U, 0U);
    sector[21] = 0xf8U;
    usb_msc_write_le16(sector, 22U, 0U);
    usb_msc_write_le16(sector, 24U, 63U);
    usb_msc_write_le16(sector, 26U, 255U);
    usb_msc_write_le32(sector, 28U, partitionStart);
    usb_msc_write_le32(sector, 32U, partitionSectors);
    usb_msc_write_le32(sector, 36U, fatSectors);
    usb_msc_write_le16(sector, 40U, 0U);
    usb_msc_write_le16(sector, 42U, 0U);
    usb_msc_write_le32(sector, 44U, 2U);
    usb_msc_write_le16(sector, 48U, 1U);
    usb_msc_write_le16(sector, 50U, 6U);
    sector[64] = 0x80U;
    sector[66] = 0x29U;
    usb_msc_write_le32(sector, 67U, 0x27540001U);
    memcpy(&sector[71], "AM275X EMMC", 11U);
    memcpy(&sector[82], "FAT32   ", 8U);
    sector[510] = 0x55U;
    sector[511] = 0xaaU;
}

static void emmc_fill_fat32_fsinfo_sector(uint8_t *sector, uint32_t clusterCount)
{
    memset(sector, 0, AM275X_USB_MSC_BLOCK_SIZE);

    usb_msc_write_le32(sector, 0U, 0x41615252U);
    usb_msc_write_le32(sector, 484U, 0x61417272U);
    usb_msc_write_le32(sector, 488U, clusterCount - 1U);
    usb_msc_write_le32(sector, 492U, 3U);
    sector[510] = 0x55U;
    sector[511] = 0xaaU;
}

static int32_t emmc_quick_format_full_disk_fat32(MMCSD_Handle media, uint32_t blockCount)
{
    uint32_t partitionStart = EMMC_FAT32_PARTITION_START_SECTOR;
    uint32_t partitionSectors;
    uint32_t fatSectors;
    uint32_t clusterCount;
    uint32_t fatStart;
    uint32_t rootDirSector;
    int32_t status;

    if (blockCount <= (partitionStart + EMMC_FAT32_RESERVED_SECTORS + 4096U)) {
        return SystemP_FAILURE;
    }

    partitionSectors = blockCount - partitionStart;
    fatSectors = emmc_fat32_calculate_fat_sectors(partitionSectors, &clusterCount);
    fatStart = partitionStart + EMMC_FAT32_RESERVED_SECTORS;
    rootDirSector = partitionStart +
                    EMMC_FAT32_RESERVED_SECTORS +
                    (EMMC_FAT32_NUM_FATS * fatSectors);

    memset(gEmmcMbrSector, 0, sizeof(gEmmcMbrSector));
    gEmmcMbrSector[446U] = 0x00U;
    gEmmcMbrSector[447U] = 0x20U;
    gEmmcMbrSector[448U] = 0x21U;
    gEmmcMbrSector[449U] = 0x00U;
    gEmmcMbrSector[450U] = 0x0cU;
    gEmmcMbrSector[451U] = 0xfeU;
    gEmmcMbrSector[452U] = 0xffU;
    gEmmcMbrSector[453U] = 0xffU;
    usb_msc_write_le32(gEmmcMbrSector, 454U, partitionStart);
    usb_msc_write_le32(gEmmcMbrSector, 458U, partitionSectors);
    gEmmcMbrSector[510] = 0x55U;
    gEmmcMbrSector[511] = 0xaaU;
    status = MMCSD_write(media, gEmmcMbrSector, 0U, 1U);
    if (status != SystemP_SUCCESS) {
        return status;
    }

    status = emmc_write_zero_sectors(media, partitionStart, EMMC_FAT32_RESERVED_SECTORS);
    if (status != SystemP_SUCCESS) {
        return status;
    }

    emmc_fill_fat32_boot_sector(gEmmcMbrSector, partitionStart, partitionSectors, fatSectors);
    status = MMCSD_write(media, gEmmcMbrSector, partitionStart, 1U);
    if (status != SystemP_SUCCESS) {
        return status;
    }
    status = MMCSD_write(media, gEmmcMbrSector, partitionStart + 6U, 1U);
    if (status != SystemP_SUCCESS) {
        return status;
    }

    emmc_fill_fat32_fsinfo_sector(gEmmcMbrSector, clusterCount);
    status = MMCSD_write(media, gEmmcMbrSector, partitionStart + 1U, 1U);
    if (status != SystemP_SUCCESS) {
        return status;
    }
    status = MMCSD_write(media, gEmmcMbrSector, partitionStart + 7U, 1U);
    if (status != SystemP_SUCCESS) {
        return status;
    }

    status = emmc_write_zero_sectors(media, fatStart, fatSectors * EMMC_FAT32_NUM_FATS);
    if (status != SystemP_SUCCESS) {
        return status;
    }

    memset(gEmmcMbrSector, 0, sizeof(gEmmcMbrSector));
    usb_msc_write_le32(gEmmcMbrSector, 0U, 0x0ffffff8U);
    usb_msc_write_le32(gEmmcMbrSector, 4U, 0x0fffffffU);
    usb_msc_write_le32(gEmmcMbrSector, 8U, 0x0fffffffU);
    status = MMCSD_write(media, gEmmcMbrSector, fatStart, 1U);
    if (status != SystemP_SUCCESS) {
        return status;
    }
    status = MMCSD_write(media, gEmmcMbrSector, fatStart + fatSectors, 1U);
    if (status != SystemP_SUCCESS) {
        return status;
    }

    status = emmc_write_zero_sectors(media, rootDirSector, EMMC_FAT32_SECTORS_PER_CLUSTER);
    if (status == SystemP_SUCCESS) {
        DebugP_log("eMMC quick FAT32 ready: part start %u, sectors %u, fat sectors %u, clusters %u\r\n",
                   partitionStart,
                   partitionSectors,
                   fatSectors,
                   clusterCount);
    }

    return status;
}

static int32_t emmc_ensure_full_disk_fat(void)
{
    MMCSD_Handle media = gMmcsdHandle[CONFIG_MMCSD0];
    FF_Disk_t *disk = &gFFDisks[FF_PARTITION_EMMC0];
    uint32_t blockCount;
    int32_t status;

    if (media == NULL) {
        DebugP_log("eMMC handle is NULL\r\n");
        return SystemP_FAILURE;
    }

    blockCount = MMCSD_getBlockCount(media);
    if (emmc_has_full_disk_partition(media, blockCount)) {
        DebugP_log("eMMC full-disk FAT partition already present, blocks %u\r\n", blockCount);
        return SystemP_SUCCESS;
    }

    DebugP_log("Creating full-disk FAT partition on eMMC, blocks %u\r\n", blockCount);
    DebugP_log("Existing eMMC partition table and filesystem will be erased\r\n");

    if ((disk->pxIOManager != NULL) && (FF_Mounted(disk->pxIOManager) != pdFALSE)) {
        (void)FF_Unmount(disk);
    }

    status = emmc_quick_format_full_disk_fat32(media, blockCount);
    if (status != SystemP_SUCCESS) {
        DebugP_log("eMMC quick FAT32 format failed, status %d\r\n", status);
        return status;
    }

    return SystemP_SUCCESS;
}

#if (AM275X_USB_ENABLE_USB0_HW_INIT != 0)
static void usb0_poll_once(void)
{
    Am275xUsbDcdEvent event;
    int32_t status;
    bool resetEvent;

    do {
        status = Am275xUsbDcd_readEvent(&gUsbDcd, &event);
        if (status == AM275X_USB_HW_OK) {
            gUsbLastEventRaw = event.raw;
            gUsbLastEventKind = (uint32_t)event.kind;
            gUsbLastEventType = event.eventType;
            gUsbLastEventEp = event.endpointNumber;
            gUsbLastEventStatus = event.status;
            gUsbLastEventParam = event.parameter;
            gUsbEventHistory[gUsbEventHistoryIndex & 31U] = event.raw;
            gUsbEventHistoryIndex++;
            gUsbEventCount++;
            resetEvent = ((event.kind == AM275X_USB_DCD_EVENT_DEVICE) &&
                          (event.eventType == AM275X_USB_DCD_DEVICE_EVENT_RESET));
            Am275xUsbDcd_acknowledgeEvent(&gUsbDcd);
            Am275xUsbDevice_processDcdEvent(&gUsbDevice, &event);
            (void)Am275xUsbEp0_processEvent(&gUsbEp0, &event);
            if ((gUsbClassDriver != NULL) && (gUsbClassDriver->processEvent != NULL)) {
                gUsbClassDriver->processEvent(gUsbClassContext, &event);
            }
            gUsbEp0StateDebug = (uint32_t)Am275xUsbEp0_getState(&gUsbEp0);
            gUsbSetupCountDebug = gUsbDevice.setupCount;
            gUsbStallCountDebug = gUsbDevice.stallCount;
            if ((gUsbClassDriver != NULL) && (gUsbClassDriver->poll != NULL)) {
                (void)gUsbClassDriver->poll(gUsbClassContext);
            }
            if (resetEvent) {
                status = Am275xUsbDcd_configureEp0(&gUsbDcd);
                gUsbBringupStatus = status;
                DebugP_assert(status == AM275X_USB_HW_OK);
                status = Am275xUsbEp0_init(&gUsbEp0, &gUsbDevice, &gUsbDcd);
                gUsbBringupStatus = status;
                DebugP_assert(status == AM275X_USB_EP0_OK);
                status = Am275xUsbEp0_primeSetup(&gUsbEp0);
                gUsbBringupStatus = status;
                DebugP_assert(status == AM275X_USB_EP0_OK);
            }
        }
    } while (status == AM275X_USB_HW_OK);

    if ((gUsbClassDriver != NULL) && (gUsbClassDriver->poll != NULL)) {
        (void)gUsbClassDriver->poll(gUsbClassContext);
    }
}
#endif

void am275xusb_main(void *args)
{
    Am275xUsbEp0Response ep0Response;
    Am275xUsbSetupPacket setup;
    Am275xUsbHwInfo usbHwInfo;
    const uint8_t *cfgDesc;
    uint16_t cfgDescLen;
    int32_t status;

#if defined (AMP_FREERTOS_A53)
    DebugP_log("AM275x USB device from a53_core%d \r\n",Armv8_getCoreId());
#else
    DebugP_log("AM275x USB device\r\n");
#endif

#if AM275X_USB_ACTIVE_APP == AM275X_USB_APP_MSC
    status = emmc_ensure_full_disk_fat();
    DebugP_assert(status == SystemP_SUCCESS);

    status = Am275xUsbMsc_init(&gUsbMsc, gMmcsdHandle[CONFIG_MMCSD0]);
    DebugP_assert(status == AM275X_USB_MSC_OK);

    gUsbClassDriver = Am275xUsbMsc_getClassDriver();
    gUsbClassContext = &gUsbMsc;
#elif AM275X_USB_ACTIVE_APP == AM275X_USB_APP_UAC2
    status = Am275xUac2_init(&gUac2, NULL);
    DebugP_assert(status == 0);

    gUsbClassDriver = Am275xUac2_getClassDriver();
    gUsbClassContext = &gUac2;
#endif

    status = Am275xUsbDevice_init(&gUsbDevice, gUsbClassDriver, gUsbClassContext);
    DebugP_assert(status == 0);

    status = gUsbClassDriver->getDescriptor(gUsbClassContext,
                                            AM275X_USB_DESC_CONFIGURATION,
                                            0,
                                            &cfgDesc,
                                            &cfgDescLen);
    DebugP_assert(status == 0);
    (void)cfgDesc;

    setup = (Am275xUsbSetupPacket) {
        .bmRequestType = 0x80U,
        .bRequest = AM275X_USB_REQ_GET_DESCRIPTOR,
        .wValue = ((uint16_t)AM275X_USB_DESC_DEVICE << 8U),
        .wIndex = 0U,
        .wLength = 18U,
    };
    status = Am275xUsbDevice_handleSetup(&gUsbDevice, &setup, NULL, 0U, &ep0Response);
    DebugP_assert(status == 0);
    DebugP_assert(ep0Response.txLength == 18U);

    Am275xUsbHw_getUsb0Info(&usbHwInfo);
#if (AM275X_USB_ENABLE_USB0_HW_INIT != 0)
    {
        Am275xUsbDcdConfig dcdConfig;

        gUsbBringupStep = 9U;
        status = SOC_moduleClockEnable(AM275X_DEV_USB0, 1U);
        gUsbBringupStatus = status;
        if (status != SystemP_SUCCESS) {
            DebugP_log("USB0 clock enable failed, status %d\r\n", status);
            return;
        }
        status = SOC_moduleClockEnable(AM275X_DEV_MAIN_USB0_ISO_VD, 1U);
        gUsbBringupStatus = status;
        if (status != SystemP_SUCCESS) {
            DebugP_log("USB0 ISO clock enable failed, status %d\r\n", status);
            return;
        }

        gUsbBringupStep = 10U;
        Am275xUsbDcd_getDefaultUsb0Config(&dcdConfig);
        dcdConfig.eventBufferAddress = (uintptr_t)&gUsb0EventBuffer[0];
        dcdConfig.eventBufferSize = USB0_EVENT_BUFFER_SIZE;
        gUsbBringupStep = 11U;
        status = Am275xUsbDcd_init(&gUsbDcd, &dcdConfig);
        gUsbBringupStatus = status;
        if (status != AM275X_USB_HW_OK) {
            DebugP_log("USB0 DCD init failed, status %d\r\n", status);
            return;
        }
        Am275xUsbDevice_attachDcd(&gUsbDevice, &gUsbDcd);
#if AM275X_USB_ACTIVE_APP == AM275X_USB_APP_MSC
        Am275xUsbMsc_attachDcd(&gUsbMsc, &gUsbDcd);
#endif
        gUsbBringupStep = 12U;
        status = Am275xUsbDcd_configureEp0(&gUsbDcd);
        gUsbBringupStatus = status;
        if (status != AM275X_USB_HW_OK) {
            DebugP_log("USB0 EP0 config failed, status %d\r\n", status);
            return;
        }
        gUsbBringupStep = 13U;
        status = Am275xUsbEp0_init(&gUsbEp0, &gUsbDevice, &gUsbDcd);
        gUsbBringupStatus = status;
        if (status != AM275X_USB_EP0_OK) {
            DebugP_log("USB0 EP0 init failed, status %d\r\n", status);
            return;
        }
#if (AM275X_USB_ENABLE_USB0_CONNECT != 0)
        gUsbBringupStep = 14U;
        status = Am275xUsbDcd_connect(&gUsbDcd);
        gUsbBringupStatus = status;
        if (status != AM275X_USB_HW_OK) {
            DebugP_log("USB0 connect failed, status %d\r\n", status);
            return;
        }
        DebugP_log("USB0 connected to host\r\n");
        gUsbBringupStep = 15U;
        status = Am275xUsbEp0_primeSetup(&gUsbEp0);
        gUsbBringupStatus = status;
        if (status != AM275X_USB_EP0_OK) {
            DebugP_log("USB0 EP0 prime failed, status %d\r\n", status);
            return;
        }
        gUsbLastDsts = Am275xUsbDcd_readDeviceStatus(&gUsbDcd);
        DebugP_log("USB0 DCD and EP0 primed, DSTS 0x%08x\r\n", gUsbLastDsts);
        gUsbBringupStep = 16U;
#else
        DebugP_log("USB0 connect disabled by AM275X_USB_ENABLE_USB0_CONNECT\r\n");
#endif
    }
#else
    (void)gUsbDcd;
    (void)gUsbEp0;
    (void)gUsb0EventBuffer;
#endif
#if AM275X_USB_ACTIVE_APP == AM275X_USB_APP_MSC
    DebugP_log("USB MSC eMMC descriptor ready, config length %u bytes, blocks %u\r\n",
               cfgDescLen,
               (uint32_t)MMCSD_getBlockCount(gMmcsdHandle[CONFIG_MMCSD0]));
#elif AM275X_USB_ACTIVE_APP == AM275X_USB_APP_UAC2
    DebugP_log("USB UAC2 descriptor ready, config length %u bytes\r\n", cfgDescLen);
#endif
    DebugP_log("USB0 core cap base 0x%08x, device base 0x%08x\r\n",
               (uint32_t)usbHwInfo.coreCapBase,
               (uint32_t)usbHwInfo.coreDeviceBase);
#if (AM275X_USB_ENABLE_USB0_HW_INIT == 0)
    DebugP_log("USB0 DCD compiled; hardware init disabled by AM275X_USB_ENABLE_USB0_HW_INIT\r\n");
#endif

#if ((AM275X_USB_ENABLE_USB0_HW_INIT != 0) && (AM275X_USB_USB0_POLL_FOREVER != 0))
    while (1) {
        volatile uint32_t delay;

        usb0_poll_once();
        for (delay = 0U; delay < USB0_POLL_IDLE_DELAY; delay++) {
            ;
        }
    }
#endif
}
