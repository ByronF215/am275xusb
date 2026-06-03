#include "am275x_usb_msc.h"

#include <string.h>
#include <kernel/dpl/CacheP.h>
#include <kernel/dpl/DebugP.h>
#include <kernel/dpl/SystemP.h>

#define USB_DESC_TYPE_DEVICE             (0x01U)
#define USB_DESC_TYPE_CONFIGURATION      (0x02U)
#define USB_DESC_TYPE_STRING             (0x03U)
#define USB_DESC_TYPE_INTERFACE          (0x04U)
#define USB_DESC_TYPE_ENDPOINT           (0x05U)
#define USB_DESC_TYPE_DEVICE_QUALIFIER   (0x06U)
#define USB_DESC_TYPE_OTHER_SPEED_CONFIG (0x07U)

#define USB_CLASS_MASS_STORAGE           (0x08U)
#define USB_MSC_SUBCLASS_SCSI            (0x06U)
#define USB_MSC_PROTOCOL_BOT             (0x50U)

#define MSC_REQ_BULK_ONLY_RESET          (0xFFU)
#define MSC_REQ_GET_MAX_LUN              (0xFEU)

#define MSC_CBW_SIGNATURE                (0x43425355UL)
#define MSC_CSW_SIGNATURE                (0x53425355UL)
#define MSC_CSW_PASS                     (0U)
#define MSC_CSW_FAIL                     (1U)

#define SCSI_TEST_UNIT_READY             (0x00U)
#define SCSI_REQUEST_SENSE               (0x03U)
#define SCSI_INQUIRY                     (0x12U)
#define SCSI_MODE_SELECT6                (0x15U)
#define SCSI_MODE_SENSE6                 (0x1AU)
#define SCSI_START_STOP_UNIT             (0x1BU)
#define SCSI_PREVENT_ALLOW               (0x1EU)
#define SCSI_READ_FORMAT_CAPACITIES      (0x23U)
#define SCSI_READ_CAPACITY10             (0x25U)
#define SCSI_READ10                      (0x28U)
#define SCSI_WRITE10                     (0x2AU)
#define SCSI_VERIFY10                    (0x2FU)
#define SCSI_SYNCHRONIZE_CACHE10         (0x35U)

#define SENSE_NO_SENSE                   (0x00U)
#define SENSE_NOT_READY                  (0x02U)
#define SENSE_ILLEGAL_REQUEST            (0x05U)
#define SENSE_DATA_PROTECT               (0x07U)

#define ASC_INVALID_COMMAND              (0x20U)
#define ASC_LBA_OUT_OF_RANGE             (0x21U)
#define ASC_WRITE_PROTECTED              (0x27U)
#define ASC_MEDIUM_NOT_PRESENT           (0x3AU)

#define U16_LE(v)  ((uint8_t)((v) & 0xFFU)), ((uint8_t)(((v) >> 8U) & 0xFFU))
#define U16_BE(v)  ((uint8_t)(((v) >> 8U) & 0xFFU)), ((uint8_t)((v) & 0xFFU))

static const uint8_t gMscDeviceDescriptor[] = {
    18, USB_DESC_TYPE_DEVICE,
    U16_LE(0x0200),
    0x00, 0x00, 0x00,
    64,
    U16_LE(0x0451),
    U16_LE(0xA297),
    U16_LE(0x0100),
    1, 2, 3,
    1,
};

static const uint8_t gMscConfigDescriptor[] = {
    9, USB_DESC_TYPE_CONFIGURATION,
    U16_LE(32),
    1,
    1,
    0,
    0x80,
    50,

    9, USB_DESC_TYPE_INTERFACE,
    0,
    0,
    2,
    USB_CLASS_MASS_STORAGE,
    USB_MSC_SUBCLASS_SCSI,
    USB_MSC_PROTOCOL_BOT,
    0,

    7, USB_DESC_TYPE_ENDPOINT,
    AM275X_USB_MSC_EP_OUT_ADDR,
    0x02,
    U16_LE(AM275X_USB_MSC_MAX_PACKET),
    0,

    7, USB_DESC_TYPE_ENDPOINT,
    AM275X_USB_MSC_EP_IN_ADDR,
    0x02,
    U16_LE(AM275X_USB_MSC_MAX_PACKET),
    0,
};

static const uint8_t gMscDeviceQualifierDescriptor[] = {
    10, USB_DESC_TYPE_DEVICE_QUALIFIER,
    U16_LE(0x0200),
    0x00, 0x00, 0x00,
    64,
    1,
    0,
};

static const uint8_t gMscOtherSpeedConfigDescriptor[] = {
    9, USB_DESC_TYPE_OTHER_SPEED_CONFIG,
    U16_LE(32),
    1,
    1,
    0,
    0x80,
    50,

    9, USB_DESC_TYPE_INTERFACE,
    0,
    0,
    2,
    USB_CLASS_MASS_STORAGE,
    USB_MSC_SUBCLASS_SCSI,
    USB_MSC_PROTOCOL_BOT,
    0,

    7, USB_DESC_TYPE_ENDPOINT,
    AM275X_USB_MSC_EP_OUT_ADDR,
    0x02,
    U16_LE(64),
    0,

    7, USB_DESC_TYPE_ENDPOINT,
    AM275X_USB_MSC_EP_IN_ADDR,
    0x02,
    U16_LE(64),
    0,
};

static const uint8_t gStringLangId[] = {
    4, USB_DESC_TYPE_STRING, U16_LE(0x0409),
};

static const uint8_t gStringManufacturer[] = {
    22, USB_DESC_TYPE_STRING,
    'A', 0, 'M', 0, '2', 0, '7', 0, '5', 0, 'x', 0, ' ', 0, 'M', 0, 'S', 0, 'C', 0,
};

static const uint8_t gStringProduct[] = {
    30, USB_DESC_TYPE_STRING,
    'e', 0, 'M', 0, 'M', 0, 'C', 0, ' ', 0, 'S', 0, 't', 0, 'o', 0,
    'r', 0, 'a', 0, 'g', 0, 'e', 0, ' ', 0, 'R', 0,
};

static const uint8_t gStringSerial[] = {
    18, USB_DESC_TYPE_STRING,
    '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '1', 0,
};

static const uint8_t gStringUnsupported[] = {
    4, USB_DESC_TYPE_STRING, '-', 0,
};

volatile uint32_t gUsbMscEpEventCount;
volatile uint32_t gUsbMscEpOutCompleteCount;
volatile uint32_t gUsbMscEpInCompleteCount;
volatile uint32_t gUsbMscLastEpEventRaw;
volatile uint32_t gUsbMscLastEpEvent;
volatile uint32_t gUsbMscInvalidCbwCount;
volatile uint32_t gUsbMscLastOutStartStatus;
volatile uint32_t gUsbMscLastInStartStatus;
volatile uint32_t gUsbMscOutStartErrorCount;
volatile uint32_t gUsbMscInStartErrorCount;
volatile uint32_t gUsbMscSetConfiguredCount;
volatile uint32_t gUsbMscPollCount;
volatile uint32_t gUsbMscPrimeCbwCount;
volatile uint32_t gUsbMscConfiguredDebug;
volatile uint32_t gUsbMscStateDebug;
volatile uint32_t gUsbMscEndpointRecoverCount;
volatile uint32_t gUsbMscLastActiveEndpointMask;
volatile uint32_t gUsbMscGetMaxLunCount;
volatile uint32_t gUsbMscBulkResetCount;
volatile uint32_t gUsbMscLastClassSetup0;
volatile uint32_t gUsbMscLastClassSetup1;
volatile uint32_t gUsbMscLastOutTrbAddressLow;
volatile uint32_t gUsbMscLastOutTrbAddressHigh;
volatile uint32_t gUsbMscLastOutTrbSize;
volatile uint32_t gUsbMscLastOutTrbControl;
volatile uint32_t gUsbMscLastOutBufferAddress;
volatile uint32_t gUsbMscLastOutDepcmd;
volatile uint32_t gUsbMscLastOutDepcmdPar0;
volatile uint32_t gUsbMscLastOutDepcmdPar1;
volatile uint32_t gUsbMscLastOutDepcmdPar2;
volatile uint32_t gUsbMscLastInDepcmd;
volatile uint32_t gUsbMscLastCdbOpcode;
volatile uint32_t gUsbMscInquiryCount;
volatile uint32_t gUsbMscReadFormatCapacitiesCount;
volatile uint32_t gUsbMscReadCapacity10Count;
volatile uint32_t gUsbMscModeSense6Count;
volatile uint32_t gUsbMscTestUnitReadyCount;
volatile uint32_t gUsbMscRead10Count;
volatile uint32_t gUsbMscWrite10Count;
volatile uint32_t gUsbMscDataOutCompleteCount;
volatile uint32_t gUsbMscUnsupportedCdbCount;
volatile uint32_t gUsbMscRequestSenseCount;
volatile uint32_t gUsbMscInquiryVpdCount;
volatile uint32_t gUsbMscLastInquiryPage;
volatile uint32_t gUsbMscLastSense0;
volatile uint32_t gUsbMscLastSense1;
volatile uint32_t gUsbMscLastCswResidue;
volatile uint32_t gUsbMscLastCswStatus;

static uint16_t be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8U) | (uint16_t)p[1];
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
           ((uint32_t)p[2] << 8U) | (uint32_t)p[3];
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8U);
    p[2] = (uint8_t)(v >> 16U);
    p[3] = (uint8_t)(v >> 24U);
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24U);
    p[1] = (uint8_t)(v >> 16U);
    p[2] = (uint8_t)(v >> 8U);
    p[3] = (uint8_t)v;
}

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static uint32_t usb_reg32(uintptr_t base, uint32_t offset)
{
    return *(volatile const uint32_t *)(base + offset);
}

static void capture_endpoint_registers(const Am275xUsbMsc *msc)
{
    uint32_t outOffset;
    uint32_t inOffset;

    if ((msc == NULL) || (msc->dcd == NULL)) {
        return;
    }

    outOffset = Am275xUsbHw_getEndpointRegisterOffset(AM275X_USB_MSC_EP_OUT_INDEX, 0U);
    inOffset = Am275xUsbHw_getEndpointRegisterOffset(AM275X_USB_MSC_EP_IN_INDEX, 0U);
    gUsbMscLastOutDepcmdPar2 = usb_reg32(msc->dcd->config.coreDeviceBase,
                                         outOffset + AM275X_USB2SS_DEV_DEPCMDPAR2_BASE_OFFSET);
    gUsbMscLastOutDepcmdPar1 = usb_reg32(msc->dcd->config.coreDeviceBase,
                                         outOffset + AM275X_USB2SS_DEV_DEPCMDPAR1_BASE_OFFSET);
    gUsbMscLastOutDepcmdPar0 = usb_reg32(msc->dcd->config.coreDeviceBase,
                                         outOffset + AM275X_USB2SS_DEV_DEPCMDPAR0_BASE_OFFSET);
    gUsbMscLastOutDepcmd = usb_reg32(msc->dcd->config.coreDeviceBase,
                                     outOffset + AM275X_USB2SS_DEV_DEPCMD_BASE_OFFSET);
    gUsbMscLastInDepcmd = usb_reg32(msc->dcd->config.coreDeviceBase,
                                    inOffset + AM275X_USB2SS_DEV_DEPCMD_BASE_OFFSET);
}

static void set_sense(Am275xUsbMsc *msc, uint8_t key, uint8_t asc, uint8_t ascq)
{
    msc->senseKey = key;
    msc->asc = asc;
    msc->ascq = ascq;
}

static int32_t queue_out(Am275xUsbMsc *msc, void *buffer, uint32_t length, Am275xUsbMscState state)
{
    uint32_t commandStatus = 0U;
    Am275xUsbMscState previousState = msc->state;

    Am275xUsbDcd_prepareTrb(&msc->outTrb, buffer, length, AM275X_USB_DCD_TRBCTL_NORMAL, true);
    gUsbMscLastOutTrbAddressLow = msc->outTrb.bufferPointerLow;
    gUsbMscLastOutTrbAddressHigh = msc->outTrb.bufferPointerHigh;
    gUsbMscLastOutTrbSize = msc->outTrb.size;
    gUsbMscLastOutTrbControl = msc->outTrb.control;
    gUsbMscLastOutBufferAddress = (uint32_t)((uintptr_t)buffer & 0xFFFFFFFFUL);
    msc->state = state;
    gUsbMscStateDebug = (uint32_t)msc->state;
    if (Am275xUsbDcd_startTransfer(msc->dcd, AM275X_USB_MSC_EP_OUT_INDEX, &msc->outTrb, 0U, &commandStatus) != AM275X_USB_HW_OK) {
        gUsbMscOutStartErrorCount++;
        capture_endpoint_registers(msc);
        msc->state = previousState;
        gUsbMscStateDebug = (uint32_t)msc->state;
        return AM275X_USB_MSC_HW_ERROR;
    }
    gUsbMscLastOutStartStatus = commandStatus;
    capture_endpoint_registers(msc);
    if (commandStatus != 0U) {
        gUsbMscOutStartErrorCount++;
    }
    if (commandStatus != 0U) {
        msc->state = previousState;
        gUsbMscStateDebug = (uint32_t)msc->state;
        return AM275X_USB_MSC_HW_ERROR;
    }

    return AM275X_USB_MSC_OK;
}

static int32_t queue_in(Am275xUsbMsc *msc, const void *buffer, uint32_t length, Am275xUsbMscState state)
{
    uint32_t commandStatus = 0U;
    Am275xUsbMscState previousState = msc->state;

    Am275xUsbDcd_prepareTrb(&msc->inTrb, buffer, length, AM275X_USB_DCD_TRBCTL_NORMAL, true);
    msc->state = state;
    gUsbMscStateDebug = (uint32_t)msc->state;
    if (Am275xUsbDcd_startTransfer(msc->dcd, AM275X_USB_MSC_EP_IN_INDEX, &msc->inTrb, 0U, &commandStatus) != AM275X_USB_HW_OK) {
        gUsbMscInStartErrorCount++;
        msc->state = previousState;
        gUsbMscStateDebug = (uint32_t)msc->state;
        return AM275X_USB_MSC_HW_ERROR;
    }
    gUsbMscLastInStartStatus = commandStatus;
    if (commandStatus != 0U) {
        gUsbMscInStartErrorCount++;
    }
    if (commandStatus != 0U) {
        msc->state = previousState;
        gUsbMscStateDebug = (uint32_t)msc->state;
        return AM275X_USB_MSC_HW_ERROR;
    }

    return AM275X_USB_MSC_OK;
}

static int32_t prime_cbw(Am275xUsbMsc *msc)
{
    return queue_out(msc, msc->cbw, AM275X_USB_MSC_MAX_PACKET, AM275X_USB_MSC_STATE_WAIT_CBW);
}

static int32_t send_csw(Am275xUsbMsc *msc, uint8_t status)
{
    put_le32(&msc->csw[0], MSC_CSW_SIGNATURE);
    put_le32(&msc->csw[4], msc->tag);
    put_le32(&msc->csw[8], msc->residue);
    msc->csw[12] = status;
    gUsbMscLastCswResidue = msc->residue;
    gUsbMscLastCswStatus = status;
    return queue_in(msc, msc->csw, sizeof(msc->csw), AM275X_USB_MSC_STATE_CSW_IN);
}

static int32_t send_data(Am275xUsbMsc *msc, const void *data, uint32_t length)
{
    uint32_t txLength = min_u32(length, msc->residue);

    msc->residue -= txLength;
    return queue_in(msc, data, txLength, AM275X_USB_MSC_STATE_DATA_IN);
}

static void fill_inquiry(uint8_t *d)
{
    (void)memset(d, 0, 36U);
    d[0] = 0x00;
    d[1] = 0x80;
    d[2] = 0x05;
    d[3] = 0x02;
    d[4] = 31;
    (void)memcpy(&d[8], "TI      ", 8U);
    (void)memcpy(&d[16], "AM275x eMMC     ", 16U);
    (void)memcpy(&d[32], "0001", 4U);
}

static uint32_t fill_inquiry_vpd(uint8_t *d, uint8_t pageCode)
{
    (void)memset(d, 0, 64U);
    d[0] = 0x00;
    d[1] = pageCode;

    switch (pageCode) {
        case 0x00:
            d[3] = 3U;
            d[4] = 0x00U;
            d[5] = 0x80U;
            d[6] = 0x83U;
            return 7U;

        case 0x80:
            d[3] = 8U;
            (void)memcpy(&d[4], "00000001", 8U);
            return 12U;

        case 0x83:
            d[3] = 16U;
            d[4] = 0x01U;
            d[5] = 0x03U;
            d[7] = 12U;
            (void)memcpy(&d[8], "AM275XEMMC01", 12U);
            return 20U;

        default:
            break;
    }

    return 0U;
}

static int32_t start_next_read(Am275xUsbMsc *msc)
{
    uint32_t blocksThisRead;
    uint32_t maxBlocksPerRead;
    uint32_t txLength;

    if (msc->readBlocksLeft == 0U) {
        return send_csw(msc, MSC_CSW_PASS);
    }

    maxBlocksPerRead = AM275X_USB_MSC_DATA_BUFFER_SIZE / msc->blockSize;
    if (maxBlocksPerRead == 0U) {
        maxBlocksPerRead = 1U;
    }
    blocksThisRead = min_u32(msc->readBlocksLeft, maxBlocksPerRead);

    if (MMCSD_read(msc->media, msc->data, msc->readLba, blocksThisRead) != SystemP_SUCCESS) {
        set_sense(msc, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT, 0U);
        return send_csw(msc, MSC_CSW_FAIL);
    }

    msc->readLba += blocksThisRead;
    msc->readBlocksLeft -= blocksThisRead;
    txLength = min_u32(blocksThisRead * msc->blockSize, msc->residue);
    msc->residue -= txLength;
    return queue_in(msc, msc->data, txLength, AM275X_USB_MSC_STATE_DATA_IN);
}

static int32_t start_next_write_discard(Am275xUsbMsc *msc)
{
    if (msc->residue == 0U) {
        return send_csw(msc, msc->dataOutStatus);
    }

    msc->dataOutWriteToMedia = 0U;
    msc->dataOutBlocks = 0U;
    msc->dataOutLength = min_u32(AM275X_USB_MSC_DATA_BUFFER_SIZE, msc->residue);
    return queue_out(msc, msc->data, msc->dataOutLength, AM275X_USB_MSC_STATE_DATA_OUT);
}

static int32_t start_next_write_to_media(Am275xUsbMsc *msc)
{
    uint32_t maxBlocksPerWrite;
    uint32_t blocksThisWrite;
    uint32_t residueBlocks;

    if ((msc->writeBlocksLeft == 0U) || (msc->residue == 0U)) {
        return send_csw(msc, msc->dataOutStatus);
    }

    maxBlocksPerWrite = AM275X_USB_MSC_DATA_BUFFER_SIZE / msc->blockSize;
    if (maxBlocksPerWrite == 0U) {
        maxBlocksPerWrite = 1U;
    }
    residueBlocks = msc->residue / msc->blockSize;
    blocksThisWrite = min_u32(msc->writeBlocksLeft, maxBlocksPerWrite);
    blocksThisWrite = min_u32(blocksThisWrite, residueBlocks);
    if (blocksThisWrite == 0U) {
        set_sense(msc, SENSE_ILLEGAL_REQUEST, ASC_INVALID_COMMAND, 0U);
        msc->dataOutStatus = MSC_CSW_FAIL;
        msc->residue = 0U;
        return send_csw(msc, MSC_CSW_FAIL);
    }

    msc->dataOutWriteToMedia = 1U;
    msc->dataOutBlocks = (uint16_t)blocksThisWrite;
    msc->dataOutLength = blocksThisWrite * msc->blockSize;
    return queue_out(msc, msc->data, msc->dataOutLength, AM275X_USB_MSC_STATE_DATA_OUT);
}

static uint32_t fill_mode_sense6(uint8_t *d, uint8_t pageCode)
{
    uint32_t offset = 4U;
    uint8_t requestedPage = pageCode & 0x3FU;

    (void)memset(d, 0, AM275X_USB_MSC_DATA_BUFFER_SIZE);
    d[2] = 0x00U;

    if ((requestedPage == 0x08U) || (requestedPage == 0x3FU)) {
        d[offset + 0U] = 0x08U;
        d[offset + 1U] = 0x12U;
        d[offset + 2U] = 0x00U;
        offset += 20U;
    }

    if ((requestedPage == 0x1CU) || (requestedPage == 0x3FU)) {
        d[offset + 0U] = 0x1CU;
        d[offset + 1U] = 0x0AU;
        d[offset + 3U] = 0x05U;
        offset += 12U;
    }

    if (offset == 4U) {
        return 4U;
    }

    d[0] = (uint8_t)(offset - 1U);
    return offset;
}

static int32_t handle_cbw(Am275xUsbMsc *msc)
{
    const uint8_t *cbw = msc->cbw;
    const uint8_t *cdb = &cbw[15];
    uint32_t signature = le32(&cbw[0]);
    uint32_t dataLength = le32(&cbw[8]);
    uint32_t lba;
    uint16_t blocks;
    uint32_t bytes;
    uint8_t opcode;

    if ((signature != MSC_CBW_SIGNATURE) || (cbw[14] == 0U) || (cbw[14] > 16U)) {
        gUsbMscInvalidCbwCount++;
        set_sense(msc, SENSE_ILLEGAL_REQUEST, ASC_INVALID_COMMAND, 0U);
        msc->tag = le32(&cbw[4]);
        msc->residue = dataLength;
        return send_csw(msc, MSC_CSW_FAIL);
    }

    msc->tag = le32(&cbw[4]);
    msc->residue = dataLength;
    opcode = cdb[0];
    gUsbMscLastCdbOpcode = opcode;

    if (!msc->mediaReady) {
        set_sense(msc, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT, 0U);
        return send_csw(msc, MSC_CSW_FAIL);
    }

    switch (opcode) {
        case SCSI_TEST_UNIT_READY:
            gUsbMscTestUnitReadyCount++;
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return send_csw(msc, MSC_CSW_PASS);

        case SCSI_PREVENT_ALLOW:
        case SCSI_START_STOP_UNIT:
        case SCSI_VERIFY10:
        case SCSI_SYNCHRONIZE_CACHE10:
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return send_csw(msc, MSC_CSW_PASS);

        case SCSI_MODE_SELECT6:
            msc->dataOutStatus = MSC_CSW_PASS;
            msc->dataOutWriteToMedia = 0U;
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return start_next_write_discard(msc);

        case SCSI_INQUIRY:
            gUsbMscInquiryCount++;
            if ((cdb[1] & 0x01U) != 0U) {
                uint32_t vpdLength;

                gUsbMscInquiryVpdCount++;
                gUsbMscLastInquiryPage = cdb[2];
                vpdLength = fill_inquiry_vpd(msc->data, cdb[2]);
                if (vpdLength == 0U) {
                    set_sense(msc, SENSE_ILLEGAL_REQUEST, ASC_INVALID_COMMAND, 0U);
                    return send_csw(msc, MSC_CSW_FAIL);
                }
                set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
                return send_data(msc, msc->data, vpdLength);
            } else {
                fill_inquiry(msc->data);
                set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
                return send_data(msc, msc->data, 36U);
            }

        case SCSI_REQUEST_SENSE:
            gUsbMscRequestSenseCount++;
            (void)memset(msc->data, 0, 18U);
            msc->data[0] = 0x70;
            msc->data[2] = msc->senseKey;
            msc->data[7] = 10;
            msc->data[12] = msc->asc;
            msc->data[13] = msc->ascq;
            gUsbMscLastSense0 = ((uint32_t)msc->senseKey) |
                                ((uint32_t)msc->asc << 8U) |
                                ((uint32_t)msc->ascq << 16U);
            gUsbMscLastSense1 = ((uint32_t)cdb[4] << 24U) |
                                ((uint32_t)msc->residue & 0x00FFFFFFUL);
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return send_data(msc, msc->data, 18U);

        case SCSI_READ_CAPACITY10:
            gUsbMscReadCapacity10Count++;
            (void)memset(msc->data, 0, 8U);
            put_be32(&msc->data[0], (msc->blockCount > 0U) ? (msc->blockCount - 1U) : 0U);
            put_be32(&msc->data[4], msc->blockSize);
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return send_data(msc, msc->data, 8U);

        case SCSI_READ_FORMAT_CAPACITIES:
            gUsbMscReadFormatCapacitiesCount++;
            (void)memset(msc->data, 0, 12U);
            msc->data[3] = 8U;
            put_be32(&msc->data[4], msc->blockCount);
            msc->data[8] = 0x02U;
            msc->data[9] = (uint8_t)(msc->blockSize >> 16U);
            msc->data[10] = (uint8_t)(msc->blockSize >> 8U);
            msc->data[11] = (uint8_t)msc->blockSize;
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return send_data(msc, msc->data, 12U);

        case SCSI_MODE_SENSE6:
        {
            uint32_t modeLength;

            gUsbMscModeSense6Count++;
            modeLength = fill_mode_sense6(msc->data, cdb[2]);
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return send_data(msc, msc->data, modeLength);
        }

        case SCSI_READ10:
            gUsbMscRead10Count++;
            lba = be32(&cdb[2]);
            blocks = be16(&cdb[7]);
            bytes = (uint32_t)blocks * msc->blockSize;
            if ((blocks == 0U) || (lba >= msc->blockCount) || ((uint64_t)lba + blocks > msc->blockCount)) {
                set_sense(msc, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0U);
                return send_csw(msc, MSC_CSW_FAIL);
            }
            msc->readLba = lba;
            msc->readBlocksLeft = blocks;
            msc->residue = min_u32(bytes, dataLength);
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return start_next_read(msc);

        case SCSI_WRITE10:
            gUsbMscWrite10Count++;
            lba = be32(&cdb[2]);
            blocks = be16(&cdb[7]);
            bytes = (uint32_t)blocks * msc->blockSize;
            if ((blocks == 0U) || (lba >= msc->blockCount) || ((uint64_t)lba + blocks > msc->blockCount) ||
                (dataLength < bytes)) {
                set_sense(msc, SENSE_ILLEGAL_REQUEST, ASC_LBA_OUT_OF_RANGE, 0U);
                return send_csw(msc, MSC_CSW_FAIL);
            }
            msc->writeLba = lba;
            msc->writeBlocksLeft = blocks;
            msc->residue = bytes;
            msc->dataOutStatus = MSC_CSW_PASS;
            msc->dataOutWriteToMedia = 1U;
            set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
            return start_next_write_to_media(msc);

        default:
            gUsbMscUnsupportedCdbCount++;
            set_sense(msc, SENSE_ILLEGAL_REQUEST, ASC_INVALID_COMMAND, 0U);
            if ((dataLength > 0U) && ((cbw[12] & 0x80U) == 0U)) {
                msc->dataOutStatus = MSC_CSW_FAIL;
                msc->dataOutWriteToMedia = 0U;
                return start_next_write_discard(msc);
            }
            return send_csw(msc, MSC_CSW_FAIL);
    }
}

int32_t Am275xUsbMsc_init(Am275xUsbMsc *msc, MMCSD_Handle media)
{
    if (msc == NULL) {
        return AM275X_USB_MSC_BAD_ARGUMENT;
    }

    *msc = (Am275xUsbMsc){0};
    msc->media = media;
    msc->blockSize = AM275X_USB_MSC_BLOCK_SIZE;
    if (media != NULL) {
        msc->blockSize = MMCSD_getBlockSize(media);
        msc->blockCount = MMCSD_getBlockCount(media);
        msc->mediaReady = ((msc->blockSize == AM275X_USB_MSC_BLOCK_SIZE) && (msc->blockCount > 0U));
    }
    set_sense(msc, SENSE_NO_SENSE, 0U, 0U);

    return AM275X_USB_MSC_OK;
}

void Am275xUsbMsc_attachDcd(Am275xUsbMsc *msc, Am275xUsbDcd *dcd)
{
    if (msc != NULL) {
        msc->dcd = dcd;
    }
}

int32_t Am275xUsbMsc_configureEndpoints(Am275xUsbMsc *msc)
{
    int32_t status;

    if ((msc == NULL) || (msc->dcd == NULL)) {
        return AM275X_USB_MSC_BAD_ARGUMENT;
    }

    (void)Am275xUsbDcd_endTransfer(msc->dcd, AM275X_USB_MSC_EP_IN_INDEX, true);
    (void)Am275xUsbDcd_endTransfer(msc->dcd, AM275X_USB_MSC_EP_OUT_INDEX, true);

    status = Am275xUsbDcd_startNewConfig(msc->dcd, 2U);
    if (status != AM275X_USB_HW_OK) {
        return AM275X_USB_MSC_HW_ERROR;
    }

    status = Am275xUsbDcd_configureEndpoint(msc->dcd,
                                            AM275X_USB_MSC_EP_OUT_INDEX,
                                            AM275X_USB_DCD_EP_TYPE_BULK,
                                            AM275X_USB_MSC_MAX_PACKET,
                                            0U,
                                            false);
    if (status != AM275X_USB_HW_OK) {
        return AM275X_USB_MSC_HW_ERROR;
    }

    status = Am275xUsbDcd_configureEndpoint(msc->dcd,
                                            AM275X_USB_MSC_EP_IN_INDEX,
                                            AM275X_USB_DCD_EP_TYPE_BULK,
                                            AM275X_USB_MSC_MAX_PACKET,
                                            1U,
                                            false);
    if (status != AM275X_USB_HW_OK) {
        return AM275X_USB_MSC_HW_ERROR;
    }

    status = Am275xUsbDcd_setTransferResource(msc->dcd, AM275X_USB_MSC_EP_OUT_INDEX, 1U);
    if (status != AM275X_USB_HW_OK) {
        return AM275X_USB_MSC_HW_ERROR;
    }

    status = Am275xUsbDcd_setTransferResource(msc->dcd, AM275X_USB_MSC_EP_IN_INDEX, 1U);
    return (status == AM275X_USB_HW_OK) ? AM275X_USB_MSC_OK : AM275X_USB_MSC_HW_ERROR;
}

int32_t Am275xUsbMsc_setConfigured(Am275xUsbMsc *msc, bool configured)
{
    if ((msc == NULL) || (msc->dcd == NULL)) {
        return AM275X_USB_MSC_BAD_ARGUMENT;
    }

    msc->configured = configured;
    gUsbMscSetConfiguredCount++;
    gUsbMscConfiguredDebug = configured ? 1U : 0U;
    if (!configured) {
        msc->endpointsConfigured = false;
        msc->state = AM275X_USB_MSC_STATE_DISABLED;
        gUsbMscStateDebug = (uint32_t)msc->state;
        return AM275X_USB_MSC_OK;
    }

    msc->state = AM275X_USB_MSC_STATE_DISABLED;
    gUsbMscStateDebug = (uint32_t)msc->state;
    return AM275X_USB_MSC_OK;
}

int32_t Am275xUsbMsc_poll(Am275xUsbMsc *msc)
{
    int32_t status;
    uint32_t activeEndpointMask;
    uint32_t requiredEndpointMask;

    if ((msc == NULL) || (msc->dcd == NULL)) {
        return AM275X_USB_MSC_BAD_ARGUMENT;
    }

    gUsbMscPollCount++;
    gUsbMscConfiguredDebug = msc->configured ? 1U : 0U;
    gUsbMscStateDebug = (uint32_t)msc->state;
    if (!msc->configured) {
        return AM275X_USB_MSC_OK;
    }

    requiredEndpointMask = (1UL << AM275X_USB_MSC_EP_OUT_INDEX) |
                           (1UL << AM275X_USB_MSC_EP_IN_INDEX);
    activeEndpointMask = Am275xUsbDcd_readActiveEndpointMask(msc->dcd);
    gUsbMscLastActiveEndpointMask = activeEndpointMask;
    if (msc->endpointsConfigured &&
        ((activeEndpointMask & requiredEndpointMask) != requiredEndpointMask)) {
        msc->endpointsConfigured = false;
        msc->state = AM275X_USB_MSC_STATE_DISABLED;
        gUsbMscEndpointRecoverCount++;
        gUsbMscStateDebug = (uint32_t)msc->state;
    }

    if (msc->state != AM275X_USB_MSC_STATE_DISABLED) {
        return AM275X_USB_MSC_OK;
    }

    if (!msc->endpointsConfigured) {
        status = Am275xUsbMsc_configureEndpoints(msc);
        if (status != AM275X_USB_MSC_OK) {
            return status;
        }
        msc->endpointsConfigured = true;
    }

    gUsbMscPrimeCbwCount++;
    return prime_cbw(msc);
}

void Am275xUsbMsc_busReset(Am275xUsbMsc *msc)
{
    if (msc != NULL) {
        if (msc->dcd != NULL) {
            (void)Am275xUsbDcd_endTransfer(msc->dcd, AM275X_USB_MSC_EP_IN_INDEX, true);
            (void)Am275xUsbDcd_endTransfer(msc->dcd, AM275X_USB_MSC_EP_OUT_INDEX, true);
        }
        msc->configured = false;
        msc->endpointsConfigured = false;
        msc->state = AM275X_USB_MSC_STATE_DISABLED;
        set_sense(msc, SENSE_NO_SENSE, 0U, 0U);
    }
}

void Am275xUsbMsc_processEvent(Am275xUsbMsc *msc, const Am275xUsbDcdEvent *event)
{
    if ((msc != NULL) && (event != NULL) && (event->kind == AM275X_USB_DCD_EVENT_ENDPOINT) &&
        ((event->endpointNumber == AM275X_USB_MSC_EP_OUT_INDEX) ||
         (event->endpointNumber == AM275X_USB_MSC_EP_IN_INDEX))) {
        gUsbMscEpEventCount++;
        gUsbMscLastEpEventRaw = event->raw;
        gUsbMscLastEpEvent = ((uint32_t)event->endpointNumber << 24U) |
                             ((uint32_t)event->eventType << 16U) |
                             ((uint32_t)event->status << 8U) |
                             (uint32_t)msc->state;
    }

    if ((msc == NULL) || (event == NULL) || (event->kind != AM275X_USB_DCD_EVENT_ENDPOINT) ||
        (event->eventType != AM275X_USB_DCD_ENDPOINT_EVENT_XFER_COMPLETE)) {
        return;
    }

    if ((event->endpointNumber == AM275X_USB_MSC_EP_OUT_INDEX) &&
        (msc->state == AM275X_USB_MSC_STATE_WAIT_CBW)) {
        gUsbMscEpOutCompleteCount++;
        CacheP_inv(msc->cbw, sizeof(msc->cbw), CacheP_TYPE_ALLD);
        (void)handle_cbw(msc);
        return;
    }

    if ((event->endpointNumber == AM275X_USB_MSC_EP_IN_INDEX) &&
        (msc->state == AM275X_USB_MSC_STATE_DATA_IN)) {
        gUsbMscEpInCompleteCount++;
        if (msc->readBlocksLeft > 0U) {
            (void)start_next_read(msc);
        } else {
            (void)send_csw(msc, MSC_CSW_PASS);
        }
        return;
    }

    if ((event->endpointNumber == AM275X_USB_MSC_EP_OUT_INDEX) &&
        (msc->state == AM275X_USB_MSC_STATE_DATA_OUT)) {
        gUsbMscDataOutCompleteCount++;
        CacheP_inv(msc->data, msc->dataOutLength, CacheP_TYPE_ALLD);
        if (msc->dataOutWriteToMedia != 0U) {
            if (MMCSD_write(msc->media, msc->data, msc->writeLba, msc->dataOutBlocks) != SystemP_SUCCESS) {
                set_sense(msc, SENSE_NOT_READY, ASC_MEDIUM_NOT_PRESENT, 0U);
                msc->dataOutStatus = MSC_CSW_FAIL;
                msc->residue = 0U;
                msc->writeBlocksLeft = 0U;
                (void)send_csw(msc, MSC_CSW_FAIL);
                return;
            }
            msc->writeLba += msc->dataOutBlocks;
            msc->writeBlocksLeft -= msc->dataOutBlocks;
        }
        msc->residue -= min_u32(msc->dataOutLength, msc->residue);
        if (msc->residue > 0U) {
            if (msc->dataOutWriteToMedia != 0U) {
                (void)start_next_write_to_media(msc);
            } else {
                (void)start_next_write_discard(msc);
            }
        } else {
            (void)send_csw(msc, msc->dataOutStatus);
        }
        return;
    }

    if ((event->endpointNumber == AM275X_USB_MSC_EP_IN_INDEX) &&
        (msc->state == AM275X_USB_MSC_STATE_CSW_IN)) {
        gUsbMscEpInCompleteCount++;
        (void)prime_cbw(msc);
    }
}

int32_t Am275xUsbMsc_getDescriptor(Am275xUsbDescriptorType type,
                                   uint8_t index,
                                   const uint8_t **data,
                                   uint16_t *length)
{
    if ((data == NULL) || (length == NULL)) {
        return AM275X_USB_MSC_BAD_ARGUMENT;
    }

    *data = NULL;
    *length = 0U;

    if (type == AM275X_USB_DESC_DEVICE) {
        *data = gMscDeviceDescriptor;
        *length = (uint16_t)sizeof(gMscDeviceDescriptor);
        return AM275X_USB_MSC_OK;
    }

    if (type == AM275X_USB_DESC_CONFIGURATION) {
        *data = gMscConfigDescriptor;
        *length = (uint16_t)sizeof(gMscConfigDescriptor);
        return AM275X_USB_MSC_OK;
    }

    if ((uint8_t)type == USB_DESC_TYPE_DEVICE_QUALIFIER) {
        *data = gMscDeviceQualifierDescriptor;
        *length = (uint16_t)sizeof(gMscDeviceQualifierDescriptor);
        return AM275X_USB_MSC_OK;
    }

    if ((uint8_t)type == USB_DESC_TYPE_OTHER_SPEED_CONFIG) {
        *data = gMscOtherSpeedConfigDescriptor;
        *length = (uint16_t)sizeof(gMscOtherSpeedConfigDescriptor);
        return AM275X_USB_MSC_OK;
    }

    if (type == AM275X_USB_DESC_STRING) {
        switch (index) {
            case 0:
                *data = gStringLangId;
                *length = (uint16_t)sizeof(gStringLangId);
                return AM275X_USB_MSC_OK;
            case 1:
                *data = gStringManufacturer;
                *length = (uint16_t)sizeof(gStringManufacturer);
                return AM275X_USB_MSC_OK;
            case 2:
                *data = gStringProduct;
                *length = (uint16_t)sizeof(gStringProduct);
                return AM275X_USB_MSC_OK;
            case 3:
                *data = gStringSerial;
                *length = (uint16_t)sizeof(gStringSerial);
                return AM275X_USB_MSC_OK;
            default:
                *data = gStringUnsupported;
                *length = (uint16_t)sizeof(gStringUnsupported);
                return AM275X_USB_MSC_OK;
        }
    }

    return AM275X_USB_MSC_NOT_SUPPORTED;
}

int32_t Am275xUsbMsc_handleClassRequest(Am275xUsbMsc *msc,
                                        const Am275xUsbSetupPacket *setup,
                                        const uint8_t **txData,
                                        uint16_t *txLength,
                                        bool *statusOnly)
{
    static const uint8_t maxLun = 0U;

    if ((msc == NULL) || (setup == NULL) || (txData == NULL) ||
        (txLength == NULL) || (statusOnly == NULL)) {
        return AM275X_USB_MSC_BAD_ARGUMENT;
    }

    *txData = NULL;
    *txLength = 0U;
    *statusOnly = true;

    if ((setup->bRequest == MSC_REQ_GET_MAX_LUN) &&
        (setup->bmRequestType == 0xA1U) &&
        (setup->wValue == 0U) &&
        (setup->wIndex == 0U) &&
        (setup->wLength == 1U)) {
        gUsbMscGetMaxLunCount++;
        gUsbMscLastClassSetup0 = ((uint32_t)setup->bmRequestType) |
                                 ((uint32_t)setup->bRequest << 8U) |
                                 ((uint32_t)setup->wValue << 16U);
        gUsbMscLastClassSetup1 = ((uint32_t)setup->wIndex) |
                                 ((uint32_t)setup->wLength << 16U);
        *txData = &maxLun;
        *txLength = 1U;
        *statusOnly = false;
        return AM275X_USB_MSC_OK;
    }

    if ((setup->bRequest == MSC_REQ_BULK_ONLY_RESET) &&
        (setup->bmRequestType == 0x21U) &&
        (setup->wValue == 0U) &&
        (setup->wIndex == 0U) &&
        (setup->wLength == 0U)) {
        gUsbMscBulkResetCount++;
        gUsbMscLastClassSetup0 = ((uint32_t)setup->bmRequestType) |
                                 ((uint32_t)setup->bRequest << 8U) |
                                 ((uint32_t)setup->wValue << 16U);
        gUsbMscLastClassSetup1 = ((uint32_t)setup->wIndex) |
                                 ((uint32_t)setup->wLength << 16U);
        (void)prime_cbw(msc);
        return AM275X_USB_MSC_OK;
    }

    return AM275X_USB_MSC_NOT_SUPPORTED;
}

static int32_t msc_class_get_descriptor(void *context,
                                        Am275xUsbDescriptorType type,
                                        uint8_t index,
                                        const uint8_t **data,
                                        uint16_t *length)
{
    (void)context;

    return (Am275xUsbMsc_getDescriptor(type, index, data, length) == AM275X_USB_MSC_OK) ? 0 : -1;
}

static int32_t msc_class_handle_request(void *context,
                                        const Am275xUsbSetupPacket *setup,
                                        const uint8_t **txData,
                                        uint16_t *txLength,
                                        bool *statusOnly)
{
    return Am275xUsbMsc_handleClassRequest((Am275xUsbMsc *)context,
                                           setup,
                                           txData,
                                           txLength,
                                           statusOnly) == AM275X_USB_MSC_OK ? 0 : -1;
}

static int32_t msc_class_set_configured(void *context, bool configured)
{
    return Am275xUsbMsc_setConfigured((Am275xUsbMsc *)context, configured) == AM275X_USB_MSC_OK ? 0 : -1;
}

static int32_t msc_class_get_interface(void *context, uint8_t interfaceNumber, uint8_t *alternateSetting)
{
    (void)context;

    if ((interfaceNumber != 0U) || (alternateSetting == NULL)) {
        return -1;
    }

    *alternateSetting = 0U;
    return 0;
}

static int32_t msc_class_set_interface(void *context, uint8_t interfaceNumber, uint8_t alternateSetting)
{
    (void)context;

    return ((interfaceNumber == 0U) && (alternateSetting == 0U)) ? 0 : -1;
}

static void msc_class_bus_reset(void *context)
{
    Am275xUsbMsc_busReset((Am275xUsbMsc *)context);
}

static void msc_class_process_event(void *context, const Am275xUsbDcdEvent *event)
{
    Am275xUsbMsc_processEvent((Am275xUsbMsc *)context, event);
}

static int32_t msc_class_poll(void *context)
{
    return Am275xUsbMsc_poll((Am275xUsbMsc *)context);
}

const Am275xUsbClassDriver *Am275xUsbMsc_getClassDriver(void)
{
    static const Am275xUsbClassDriver driver = {
        .getDescriptor = msc_class_get_descriptor,
        .handleClassRequest = msc_class_handle_request,
        .setConfigured = msc_class_set_configured,
        .getInterface = msc_class_get_interface,
        .setInterface = msc_class_set_interface,
        .busReset = msc_class_bus_reset,
        .processEvent = msc_class_process_event,
        .poll = msc_class_poll,
    };

    return &driver;
}
