#include "am275x_usb_hw.h"

#include <stddef.h>
#include <kernel/dpl/CacheP.h>

#define USB_XHCI_CAPLENGTH_OFFSET     (0x00U)
#define USB_XHCI_HCIVERSION_OFFSET    (0x02U)
#define USB_XHCI_HCSPARAMS1_OFFSET    (0x04U)
#define USB_XHCI_HCCPARAMS1_OFFSET    (0x10U)

#define USB_DCD_RESET_TIMEOUT          (1000000U)
#define USB_DCD_COMMAND_TIMEOUT        (1000000U)
#define USB_DCD_EVENT_BUFFER_MIN_SIZE  (32U)
#define USB_DCD_EVENT_BUFFER_ALIGN     (4U)
#define USB_DCD_EVENT_SIZE             (4U)

#define USB_DCD_EVENT_DEVICE_FLAG      (1UL << 0U)
#define USB_DCD_DEVICE_EVENT_TYPE_SHIFT (8U)
#define USB_DCD_DEVICE_EVENT_TYPE_MASK (0xFUL << USB_DCD_DEVICE_EVENT_TYPE_SHIFT)
#define USB_DCD_DEVICE_EVENT_PARAM_SHIFT (16U)
#define USB_DCD_DEVICE_EVENT_PARAM_MASK (0xFFFFUL << USB_DCD_DEVICE_EVENT_PARAM_SHIFT)
#define USB_DCD_ENDPOINT_EVENT_EP_SHIFT (1U)
#define USB_DCD_ENDPOINT_EVENT_EP_MASK (0x1FUL << USB_DCD_ENDPOINT_EVENT_EP_SHIFT)
#define USB_DCD_ENDPOINT_EVENT_TYPE_SHIFT (6U)
#define USB_DCD_ENDPOINT_EVENT_TYPE_MASK (0xFUL << USB_DCD_ENDPOINT_EVENT_TYPE_SHIFT)
#define USB_DCD_ENDPOINT_EVENT_STATUS_SHIFT (12U)
#define USB_DCD_ENDPOINT_EVENT_STATUS_MASK (0xFUL << USB_DCD_ENDPOINT_EVENT_STATUS_SHIFT)
#define USB_DCD_ENDPOINT_EVENT_PARAM_SHIFT (16U)
#define USB_DCD_ENDPOINT_EVENT_PARAM_MASK (0xFFFFUL << USB_DCD_ENDPOINT_EVENT_PARAM_SHIFT)

volatile uint32_t gUsbDcdLastCommandEp;
volatile uint32_t gUsbDcdLastCommandType;
volatile uint32_t gUsbDcdLastCommandStatus;
volatile uint32_t gUsbDcdLastCommandParam;
volatile uint32_t gUsbDcdLastCommandDalepena;
volatile uint32_t gUsbDcdLastConfigureEp;
volatile uint32_t gUsbDcdLastConfigureStatus;
volatile uint32_t gUsbDcdLastDalepenaBeforeEnable;
volatile uint32_t gUsbDcdLastDalepenaAfterEnable;
volatile uint32_t gUsbDcdLastResourceEp;
volatile uint32_t gUsbDcdLastResourceStatus;
volatile uint32_t gUsbDcdLastDalepenaAfterResource;
volatile uint32_t gUsbDcdLastStartTransferEp;
volatile uint32_t gUsbDcdLastStartTransferStatus;
volatile uint32_t gUsbDcdLastDalepenaAtStartTransfer;
volatile uint32_t gUsbDcdLastGusb2PhyCfgBeforeCommand;
volatile uint32_t gUsbDcdLastGusb2PhyCfgDuringCommand;
volatile uint32_t gUsbDcdLastGusb2PhyCfgAfterCommand;
volatile uint32_t gUsbDcdLastEventCountRaw;
volatile uint32_t gUsbDcdLastEventCountBytes;
volatile uint32_t gUsbDcdLastEventReadOffset;
volatile uint32_t gUsbDcdLastEventRaw;
volatile uint32_t gUsbDcdReadEventReadyCount;
volatile uint32_t gUsbDcdReadEventNotReadyCount;
volatile uint32_t gUsbDcdEventCountHistory[32];
volatile uint32_t gUsbDcdEventCountHistoryIndex;

static uint8_t reg8(uintptr_t base, uint32_t offset)
{
    return *(volatile const uint8_t *)(base + offset);
}

static uint16_t reg16(uintptr_t base, uint32_t offset)
{
    return *(volatile const uint16_t *)(base + offset);
}

static uint32_t reg32(uintptr_t base, uint32_t offset)
{
    return *(volatile const uint32_t *)(base + offset);
}

static void reg32_write(uintptr_t base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(base + offset) = value;
}

static void reg32_rmw(uintptr_t base, uint32_t offset, uint32_t clearMask, uint32_t setMask)
{
    uint32_t value = reg32(base, offset);

    value &= ~clearMask;
    value |= setMask;
    reg32_write(base, offset, value);
}

static int32_t wait_mask_clear(uintptr_t base, uint32_t offset, uint32_t mask, uint32_t timeout)
{
    while (timeout > 0U) {
        if ((reg32(base, offset) & mask) == 0U) {
            return AM275X_USB_HW_OK;
        }
        timeout--;
    }

    return AM275X_USB_HW_TIMEOUT;
}

static uint32_t read_event_word(const Am275xUsbDcd *dcd)
{
    uintptr_t eventAddress = dcd->config.eventBufferAddress + dcd->eventReadOffset;

    CacheP_inv((void *)eventAddress, USB_DCD_EVENT_SIZE, CacheP_TYPE_ALLD);
    return *(volatile const uint32_t *)eventAddress;
}

static void decode_event(uint32_t raw, Am275xUsbDcdEvent *event)
{
    *event = (Am275xUsbDcdEvent) {
        .raw = raw,
        .kind = ((raw & USB_DCD_EVENT_DEVICE_FLAG) != 0U) ?
            AM275X_USB_DCD_EVENT_DEVICE :
            AM275X_USB_DCD_EVENT_ENDPOINT,
        .eventType = 0U,
        .endpointNumber = 0U,
        .status = 0U,
        .parameter = 0U,
    };

    if (event->kind == AM275X_USB_DCD_EVENT_DEVICE) {
        event->eventType = (uint8_t)((raw & USB_DCD_DEVICE_EVENT_TYPE_MASK) >>
                                     USB_DCD_DEVICE_EVENT_TYPE_SHIFT);
        event->parameter = (uint16_t)((raw & USB_DCD_DEVICE_EVENT_PARAM_MASK) >>
                                      USB_DCD_DEVICE_EVENT_PARAM_SHIFT);
    } else {
        event->endpointNumber = (uint8_t)((raw & USB_DCD_ENDPOINT_EVENT_EP_MASK) >>
                                          USB_DCD_ENDPOINT_EVENT_EP_SHIFT);
        event->eventType = (uint8_t)((raw & USB_DCD_ENDPOINT_EVENT_TYPE_MASK) >>
                                     USB_DCD_ENDPOINT_EVENT_TYPE_SHIFT);
        event->status = (uint8_t)((raw & USB_DCD_ENDPOINT_EVENT_STATUS_MASK) >>
                                  USB_DCD_ENDPOINT_EVENT_STATUS_SHIFT);
        event->parameter = (uint16_t)((raw & USB_DCD_ENDPOINT_EVENT_PARAM_MASK) >>
                                      USB_DCD_ENDPOINT_EVENT_PARAM_SHIFT);
    }
}

void Am275xUsbHw_getUsb0Info(Am275xUsbHwInfo *info)
{
    if (info == NULL) {
        return;
    }

    *info = (Am275xUsbHwInfo){0};
    info->mmrBase = AM275X_USB0_MMR_BASE;
    info->phyBase = AM275X_USB0_PHY2_BASE;
    info->coreCapBase = AM275X_USB0_CORE_CAP_BASE;
    info->coreGlobalBase = AM275X_USB0_CORE_GLOBAL_BASE;
    info->coreDeviceBase = AM275X_USB0_CORE_DEVICE_BASE;
}

int32_t Am275xUsbHw_probeUsb0(Am275xUsbHwInfo *info)
{
    Am275xUsbHwInfo localInfo;

    if (info == NULL) {
        info = &localInfo;
    }

    Am275xUsbHw_getUsb0Info(info);

    info->capabilityLength = reg8(info->coreCapBase, USB_XHCI_CAPLENGTH_OFFSET);
    info->hciVersion = reg16(info->coreCapBase, USB_XHCI_HCIVERSION_OFFSET);
    info->hcsParams1 = reg32(info->coreCapBase, USB_XHCI_HCSPARAMS1_OFFSET);
    info->hccParams1 = reg32(info->coreCapBase, USB_XHCI_HCCPARAMS1_OFFSET);
    info->gsnpsid = reg32(info->coreGlobalBase, AM275X_USB2SS_GBL_GSNPSID_OFFSET);
    info->gctl = reg32(info->coreGlobalBase, AM275X_USB2SS_GBL_GCTL_OFFSET);
    info->dcfg = reg32(info->coreDeviceBase, AM275X_USB2SS_DEV_DCFG_OFFSET);
    info->dctl = reg32(info->coreDeviceBase, AM275X_USB2SS_DEV_DCTL_OFFSET);
    info->dsts = reg32(info->coreDeviceBase, AM275X_USB2SS_DEV_DSTS_OFFSET);

    if ((info->capabilityLength == 0U) || (info->capabilityLength == 0xFFU)) {
        return -1;
    }

    return 0;
}

uint32_t Am275xUsbHw_getEndpointRegisterOffset(uint32_t endpointIndex, uint32_t endpointRegisterBaseOffset)
{
    return endpointRegisterBaseOffset + (endpointIndex * AM275X_USB2SS_DEV_ENDPOINT_STRIDE);
}

void Am275xUsbDcd_getDefaultUsb0Config(Am275xUsbDcdConfig *config)
{
    if (config == NULL) {
        return;
    }

    *config = (Am275xUsbDcdConfig) {
        .mmrBase = AM275X_USB0_MMR_BASE,
        .coreGlobalBase = AM275X_USB0_CORE_GLOBAL_BASE,
        .coreDeviceBase = AM275X_USB0_CORE_DEVICE_BASE,
        .eventBufferAddress = 0U,
        .eventBufferSize = 0U,
        .interruptNumber = 0U,
        .enableSofEvent = false,
    };
}

int32_t Am275xUsbDcd_init(Am275xUsbDcd *dcd, const Am275xUsbDcdConfig *config)
{
    uint32_t dcfg;
    uint32_t devten;
    int32_t status;

    if ((dcd == NULL) || (config == NULL) ||
        (config->coreGlobalBase == 0U) ||
        (config->coreDeviceBase == 0U) ||
        (config->eventBufferAddress == 0U) ||
        (config->eventBufferSize < USB_DCD_EVENT_BUFFER_MIN_SIZE) ||
        ((config->eventBufferSize % USB_DCD_EVENT_BUFFER_ALIGN) != 0U) ||
        ((config->eventBufferAddress % USB_DCD_EVENT_BUFFER_ALIGN) != 0U) ||
        (config->interruptNumber >= AM275X_USB0_IRQ_COUNT)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    *dcd = (Am275xUsbDcd) {
        .config = *config,
        .eventReadOffset = 0U,
        .initialized = false,
        .connected = false,
    };

    reg32_rmw(config->coreGlobalBase,
              AM275X_USB2SS_GBL_GCTL_OFFSET,
              AM275X_USB2SS_GCTL_PRTCAPDIR_MASK,
              AM275X_USB2SS_GCTL_PRTCAPDIR_DEVICE | AM275X_USB2SS_GCTL_DSBLCLKGTNG);

    reg32_write(config->coreDeviceBase,
                AM275X_USB2SS_DEV_DCTL_OFFSET,
                reg32(config->coreDeviceBase, AM275X_USB2SS_DEV_DCTL_OFFSET) |
                    AM275X_USB2SS_DCTL_CSFTRST);
    status = wait_mask_clear(config->coreDeviceBase,
                             AM275X_USB2SS_DEV_DCTL_OFFSET,
                             AM275X_USB2SS_DCTL_CSFTRST,
                             USB_DCD_RESET_TIMEOUT);
    if (status != AM275X_USB_HW_OK) {
        return status;
    }

    dcfg = reg32(config->coreDeviceBase, AM275X_USB2SS_DEV_DCFG_OFFSET);
    dcfg &= ~(AM275X_USB2SS_DCFG_DEVADDR_MASK |
              AM275X_USB2SS_DCFG_INTRNUM_MASK |
              AM275X_USB2SS_DCFG_DEVSPD_MASK);
    dcfg |= (((uint32_t)config->interruptNumber << AM275X_USB2SS_DCFG_INTRNUM_SHIFT) &
             AM275X_USB2SS_DCFG_INTRNUM_MASK);
    dcfg |= AM275X_USB2SS_DCFG_DEVSPD_HS;
    reg32_write(config->coreDeviceBase, AM275X_USB2SS_DEV_DCFG_OFFSET, dcfg);

    reg32_write(config->coreGlobalBase,
                AM275X_USB2SS_GBL_GEVNTADRLO0_OFFSET,
                (uint32_t)(config->eventBufferAddress & 0xFFFFFFFFUL));
    reg32_write(config->coreGlobalBase,
                AM275X_USB2SS_GBL_GEVNTADRHI0_OFFSET,
                (uint32_t)(((uint64_t)config->eventBufferAddress >> 32U) & 0xFFFFFFFFUL));
    reg32_write(config->coreGlobalBase,
                AM275X_USB2SS_GBL_GEVNTSIZ0_OFFSET,
                ((uint32_t)config->eventBufferSize & AM275X_USB2SS_GEVNTSIZ_SIZE_MASK) |
                    AM275X_USB2SS_GEVNTSIZ_INTMASK);
    reg32_write(config->coreGlobalBase, AM275X_USB2SS_GBL_GEVNTCOUNT0_OFFSET, 0U);

    devten = AM275X_USB2SS_DEVTEN_DISSCONNEVTEN |
             AM275X_USB2SS_DEVTEN_USBRSTEVTEN |
             AM275X_USB2SS_DEVTEN_CONNECTDONEEVTEN;
    if (config->enableSofEvent) {
        devten |= AM275X_USB2SS_DEVTEN_SOFTEVTEN;
    }
    reg32_write(config->coreDeviceBase, AM275X_USB2SS_DEV_DEVTEN_OFFSET, devten);

    reg32_write(config->coreDeviceBase,
                AM275X_USB2SS_DEV_DALEPENA_OFFSET,
                AM275X_USB2SS_DALEPENA_EP0_OUT | AM275X_USB2SS_DALEPENA_EP0_IN);

    reg32_rmw(config->coreDeviceBase,
              AM275X_USB2SS_DEV_DCTL_OFFSET,
              AM275X_USB2SS_DCTL_RUN_STOP,
              0U);

    dcd->initialized = true;

    return AM275X_USB_HW_OK;
}

int32_t Am275xUsbDcd_connect(Am275xUsbDcd *dcd)
{
    if ((dcd == NULL) || (!dcd->initialized)) {
        return AM275X_USB_HW_NOT_READY;
    }

    reg32_rmw(dcd->config.coreDeviceBase,
              AM275X_USB2SS_DEV_DCTL_OFFSET,
              AM275X_USB2SS_DCTL_ULSTCHNGREQ_MASK,
              AM275X_USB2SS_DCTL_ULSTCHNGREQ_RX_DETECT);
    reg32_rmw(dcd->config.coreDeviceBase,
              AM275X_USB2SS_DEV_DCTL_OFFSET,
              0U,
              AM275X_USB2SS_DCTL_RUN_STOP);
    dcd->connected = true;

    return AM275X_USB_HW_OK;
}

int32_t Am275xUsbDcd_disconnect(Am275xUsbDcd *dcd)
{
    if ((dcd == NULL) || (!dcd->initialized)) {
        return AM275X_USB_HW_NOT_READY;
    }

    reg32_rmw(dcd->config.coreDeviceBase,
              AM275X_USB2SS_DEV_DCTL_OFFSET,
              AM275X_USB2SS_DCTL_RUN_STOP,
              0U);
    dcd->connected = false;

    return AM275X_USB_HW_OK;
}

int32_t Am275xUsbDcd_setAddress(Am275xUsbDcd *dcd, uint8_t address)
{
    uint32_t dcfg;

    if ((dcd == NULL) || (!dcd->initialized) || (address > 127U)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    dcfg = reg32(dcd->config.coreDeviceBase, AM275X_USB2SS_DEV_DCFG_OFFSET);
    dcfg &= ~AM275X_USB2SS_DCFG_DEVADDR_MASK;
    dcfg |= ((uint32_t)address << AM275X_USB2SS_DCFG_DEVADDR_SHIFT) &
            AM275X_USB2SS_DCFG_DEVADDR_MASK;
    reg32_write(dcd->config.coreDeviceBase, AM275X_USB2SS_DEV_DCFG_OFFSET, dcfg);

    return AM275X_USB_HW_OK;
}

int32_t Am275xUsbDcd_issueEndpointCommand(Am275xUsbDcd *dcd,
                                          uint32_t endpointIndex,
                                          uint32_t command,
                                          uint32_t param0,
                                          uint32_t param1,
                                          uint32_t param2,
                                          uint32_t commandParam,
                                          uint32_t *commandStatus)
{
    uint32_t depcmdOffset;
    uint32_t depcmd;
    uint32_t gusb2phycfg;
    uint32_t timeout;

    if ((dcd == NULL) || (!dcd->initialized) ||
        (endpointIndex >= 32U) ||
        ((command & ~(AM275X_USB2SS_DEPCMD_CMDTYP_MASK |
                      AM275X_USB2SS_DEPCMD_HIPRI_FORCERM |
                      AM275X_USB2SS_DEPCMD_CMDIOC)) != 0U) ||
        ((commandParam & ~0xFFFFUL) != 0U)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    reg32_write(dcd->config.coreDeviceBase,
                Am275xUsbHw_getEndpointRegisterOffset(endpointIndex,
                                                       AM275X_USB2SS_DEV_DEPCMDPAR0_BASE_OFFSET),
                param0);
    reg32_write(dcd->config.coreDeviceBase,
                Am275xUsbHw_getEndpointRegisterOffset(endpointIndex,
                                                       AM275X_USB2SS_DEV_DEPCMDPAR1_BASE_OFFSET),
                param1);
    reg32_write(dcd->config.coreDeviceBase,
                Am275xUsbHw_getEndpointRegisterOffset(endpointIndex,
                                                       AM275X_USB2SS_DEV_DEPCMDPAR2_BASE_OFFSET),
                param2);

    depcmdOffset = Am275xUsbHw_getEndpointRegisterOffset(endpointIndex,
                                                         AM275X_USB2SS_DEV_DEPCMD_BASE_OFFSET);
    depcmd = ((commandParam << AM275X_USB2SS_DEPCMD_COMMAND_PARAM_SHIFT) &
              AM275X_USB2SS_DEPCMD_COMMAND_PARAM_MASK) |
             AM275X_USB2SS_DEPCMD_CMDACT |
             (command & (AM275X_USB2SS_DEPCMD_CMDTYP_MASK |
                         AM275X_USB2SS_DEPCMD_HIPRI_FORCERM |
                         AM275X_USB2SS_DEPCMD_CMDIOC));
    gusb2phycfg = reg32(dcd->config.coreGlobalBase,
                        AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET);
    gUsbDcdLastGusb2PhyCfgBeforeCommand = gusb2phycfg;
    reg32_write(dcd->config.coreGlobalBase,
                AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET,
                gusb2phycfg & ~(AM275X_USB2SS_GUSB2PHYCFG_SUSPENDUSB20 |
                                AM275X_USB2SS_GUSB2PHYCFG_ENBLSLPM));
    gUsbDcdLastGusb2PhyCfgDuringCommand = reg32(dcd->config.coreGlobalBase,
                                                AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET);
    reg32_write(dcd->config.coreDeviceBase, depcmdOffset, depcmd);

    timeout = USB_DCD_COMMAND_TIMEOUT;
    while (timeout > 0U) {
        depcmd = reg32(dcd->config.coreDeviceBase, depcmdOffset);
        if ((depcmd & AM275X_USB2SS_DEPCMD_CMDACT) == 0U) {
            gUsbDcdLastCommandEp = endpointIndex;
            gUsbDcdLastCommandType = command;
            gUsbDcdLastCommandParam = (depcmd & AM275X_USB2SS_DEPCMD_COMMAND_PARAM_MASK) >>
                                      AM275X_USB2SS_DEPCMD_COMMAND_PARAM_SHIFT;
            gUsbDcdLastCommandStatus = (depcmd & AM275X_USB2SS_DEPCMD_CMDSTATUS_MASK) >>
                                       AM275X_USB2SS_DEPCMD_CMDSTATUS_SHIFT;
            gUsbDcdLastCommandDalepena = reg32(dcd->config.coreDeviceBase,
                                               AM275X_USB2SS_DEV_DALEPENA_OFFSET);
            if (commandStatus != NULL) {
                *commandStatus = gUsbDcdLastCommandStatus;
            }
            reg32_write(dcd->config.coreGlobalBase,
                        AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET,
                        gusb2phycfg);
            gUsbDcdLastGusb2PhyCfgAfterCommand = reg32(dcd->config.coreGlobalBase,
                                                       AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET);
            return AM275X_USB_HW_OK;
        }
        timeout--;
    }

    reg32_write(dcd->config.coreGlobalBase,
                AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET,
                gusb2phycfg);
    gUsbDcdLastGusb2PhyCfgAfterCommand = reg32(dcd->config.coreGlobalBase,
                                               AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET);
    return AM275X_USB_HW_TIMEOUT;
}

int32_t Am275xUsbDcd_startNewConfig(Am275xUsbDcd *dcd, uint8_t resourceIndex)
{
    return Am275xUsbDcd_issueEndpointCommand(dcd,
                                             AM275X_USB_DCD_EP0_OUT_INDEX,
                                             AM275X_USB2SS_DEPCMD_START_NEW_CONFIG,
                                             0U,
                                             0U,
                                             0U,
                                             resourceIndex,
                                             NULL);
}

int32_t Am275xUsbDcd_configureEndpoint(Am275xUsbDcd *dcd,
                                       uint32_t endpointIndex,
                                       uint32_t endpointType,
                                       uint16_t maxPacketSize,
                                       uint8_t fifoNumber,
                                       bool enableTransferNotReady)
{
    uint32_t param0;
    uint32_t param1;
    uint32_t endpointNumber;
    int32_t status;

    if ((dcd == NULL) ||
        (endpointIndex >= 32U) ||
        (endpointType > AM275X_USB_DCD_EP_TYPE_INTERRUPT) ||
        (maxPacketSize == 0U)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    param0 = AM275X_USB2SS_DEPCFG_EP_TYPE(endpointType) |
             AM275X_USB2SS_DEPCFG_MAX_PACKET_SIZE(maxPacketSize) |
             AM275X_USB2SS_DEPCFG_FIFO_NUMBER(fifoNumber) |
             AM275X_USB2SS_DEPCFG_BURST_SIZE(0U) |
             AM275X_USB2SS_DEPCFG_ACTION_INIT;

    endpointNumber = endpointIndex;

    param1 = AM275X_USB2SS_DEPCFG_INT_NUM(dcd->config.interruptNumber) |
             AM275X_USB2SS_DEPCFG_XFER_COMPLETE_EN |
             AM275X_USB2SS_DEPCFG_FIFO_ERROR_EN |
             AM275X_USB2SS_DEPCFG_EP_NUMBER(endpointNumber);
    if (endpointType != AM275X_USB_DCD_EP_TYPE_CONTROL) {
        param1 |= AM275X_USB2SS_DEPCFG_XFER_IN_PROGRESS_EN;
    }
    if (enableTransferNotReady) {
        param1 |= AM275X_USB2SS_DEPCFG_XFER_NOT_READY_EN;
    }

    status = Am275xUsbDcd_issueEndpointCommand(dcd,
                                               endpointIndex,
                                               AM275X_USB2SS_DEPCMD_SET_EP_CONFIG,
                                               param0,
                                               param1,
                                               0U,
                                               0U,
                                               NULL);
    gUsbDcdLastConfigureEp = endpointIndex;
    gUsbDcdLastConfigureStatus = (uint32_t)status;
    gUsbDcdLastDalepenaBeforeEnable = reg32(dcd->config.coreDeviceBase,
                                            AM275X_USB2SS_DEV_DALEPENA_OFFSET);
    if (status == AM275X_USB_HW_OK) {
        reg32_rmw(dcd->config.coreDeviceBase,
                  AM275X_USB2SS_DEV_DALEPENA_OFFSET,
                  0U,
                  (1UL << endpointIndex));
        gUsbDcdLastDalepenaAfterEnable = reg32(dcd->config.coreDeviceBase,
                                               AM275X_USB2SS_DEV_DALEPENA_OFFSET);
    }

    return status;
}

int32_t Am275xUsbDcd_setTransferResource(Am275xUsbDcd *dcd,
                                         uint32_t endpointIndex,
                                         uint16_t resourceCount)
{
    int32_t status;

    if ((dcd == NULL) || (endpointIndex >= 32U) || (resourceCount == 0U)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    status = Am275xUsbDcd_issueEndpointCommand(dcd,
                                               endpointIndex,
                                               AM275X_USB2SS_DEPCMD_SET_XFER_RESOURCE,
                                               AM275X_USB2SS_DEPXFERCFG_NUM_XFER_RES(resourceCount),
                                               0U,
                                               0U,
                                               0U,
                                               NULL);
    gUsbDcdLastResourceEp = endpointIndex;
    gUsbDcdLastResourceStatus = (uint32_t)status;
    gUsbDcdLastDalepenaAfterResource = reg32(dcd->config.coreDeviceBase,
                                             AM275X_USB2SS_DEV_DALEPENA_OFFSET);
    return status;
}

int32_t Am275xUsbDcd_endTransfer(Am275xUsbDcd *dcd, uint32_t endpointIndex, bool forceRemove)
{
    uint32_t command;
    uint32_t commandStatus = 0U;
    uint32_t resourceIndex;
    int32_t status;

    if ((dcd == NULL) || (endpointIndex >= 32U)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    if (!dcd->endpointTransferActive[endpointIndex]) {
        return AM275X_USB_HW_OK;
    }

    resourceIndex = dcd->endpointTransferResource[endpointIndex] & 0x7FU;
    command = AM275X_USB2SS_DEPCMD_END_TRANSFER;
    if (forceRemove) {
        command |= AM275X_USB2SS_DEPCMD_HIPRI_FORCERM;
    }

    status = Am275xUsbDcd_issueEndpointCommand(dcd,
                                               endpointIndex,
                                               command,
                                               0U,
                                               0U,
                                               0U,
                                               resourceIndex,
                                               &commandStatus);
    dcd->endpointTransferActive[endpointIndex] = false;
    dcd->endpointTransferResource[endpointIndex] = 0U;

    if ((status != AM275X_USB_HW_OK) || (commandStatus != 0U)) {
        return AM275X_USB_HW_TIMEOUT;
    }

    return AM275X_USB_HW_OK;
}

int32_t Am275xUsbDcd_configureEp0(Am275xUsbDcd *dcd)
{
    int32_t status;

    status = Am275xUsbDcd_startNewConfig(dcd, 0U);
    if (status != AM275X_USB_HW_OK) {
        return status;
    }

    status = Am275xUsbDcd_configureEndpoint(dcd,
                                            AM275X_USB_DCD_EP0_OUT_INDEX,
                                            AM275X_USB_DCD_EP_TYPE_CONTROL,
                                            64U,
                                            0U,
                                            true);
    if (status != AM275X_USB_HW_OK) {
        return status;
    }

    status = Am275xUsbDcd_configureEndpoint(dcd,
                                            AM275X_USB_DCD_EP0_IN_INDEX,
                                            AM275X_USB_DCD_EP_TYPE_CONTROL,
                                            64U,
                                            0U,
                                            true);
    if (status != AM275X_USB_HW_OK) {
        return status;
    }

    status = Am275xUsbDcd_setTransferResource(dcd, AM275X_USB_DCD_EP0_OUT_INDEX, 1U);
    if (status != AM275X_USB_HW_OK) {
        return status;
    }

    return Am275xUsbDcd_setTransferResource(dcd, AM275X_USB_DCD_EP0_IN_INDEX, 1U);
}

int32_t Am275xUsbDcd_stallEndpoint(Am275xUsbDcd *dcd, uint32_t endpointIndex)
{
    return Am275xUsbDcd_issueEndpointCommand(dcd,
                                             endpointIndex,
                                             AM275X_USB2SS_DEPCMD_SET_STALL,
                                             0U,
                                             0U,
                                             0U,
                                             0U,
                                             NULL);
}

int32_t Am275xUsbDcd_clearEndpointStall(Am275xUsbDcd *dcd, uint32_t endpointIndex)
{
    return Am275xUsbDcd_issueEndpointCommand(dcd,
                                             endpointIndex,
                                             AM275X_USB2SS_DEPCMD_CLEAR_STALL,
                                             0U,
                                             0U,
                                             0U,
                                             0U,
                                             NULL);
}

void Am275xUsbDcd_prepareTrb(Am275xUsbDcdTrb *trb,
                             const void *buffer,
                             uint32_t length,
                             uint32_t trbControl,
                             bool interruptOnComplete)
{
    uintptr_t bufferAddress;
    uint32_t control;

    if (trb == NULL) {
        return;
    }

    bufferAddress = (uintptr_t)buffer;
    control = ((trbControl << AM275X_USB_DCD_TRB_CTRL_TRBCTL_SHIFT) &
               AM275X_USB_DCD_TRB_CTRL_TRBCTL_MASK) |
              AM275X_USB_DCD_TRB_CTRL_LST |
              AM275X_USB_DCD_TRB_CTRL_HWO;
    if (interruptOnComplete) {
        control |= AM275X_USB_DCD_TRB_CTRL_IOC;
    }

    trb->bufferPointerLow = (uint32_t)(bufferAddress & 0xFFFFFFFFUL);
    trb->bufferPointerHigh = (uint32_t)(((uint64_t)bufferAddress >> 32U) & 0xFFFFFFFFUL);
    trb->size = length & AM275X_USB_DCD_TRB_SIZE_MASK;
    trb->control = control;

    if ((buffer != NULL) && (length > 0U)) {
        CacheP_wbInv((void *)(uintptr_t)buffer, length, CacheP_TYPE_ALLD);
    }
    CacheP_wbInv(trb, sizeof(*trb), CacheP_TYPE_ALLD);
}

int32_t Am275xUsbDcd_startTransfer(Am275xUsbDcd *dcd,
                                   uint32_t endpointIndex,
                                   const Am275xUsbDcdTrb *firstTrb,
                                   uint16_t streamOrMicroFrame,
                                   uint32_t *commandStatus)
{
    uintptr_t trbAddress;
    int32_t status;

    if ((dcd == NULL) || (firstTrb == NULL)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    CacheP_wbInv((void *)(uintptr_t)firstTrb, sizeof(*firstTrb), CacheP_TYPE_ALLD);
    trbAddress = (uintptr_t)firstTrb;

    status = Am275xUsbDcd_issueEndpointCommand(dcd,
                                               endpointIndex,
                                               AM275X_USB2SS_DEPCMD_START_TRANSFER,
                                               (uint32_t)(((uint64_t)trbAddress >> 32U) & 0xFFFFFFFFUL),
                                               (uint32_t)(trbAddress & 0xFFFFFFFFUL),
                                               0U,
                                               streamOrMicroFrame,
                                               commandStatus);
    gUsbDcdLastStartTransferEp = endpointIndex;
    gUsbDcdLastStartTransferStatus = (uint32_t)status;
    if ((status == AM275X_USB_HW_OK) && ((commandStatus == NULL) || (*commandStatus == 0U))) {
        dcd->endpointTransferResource[endpointIndex] = (uint8_t)(gUsbDcdLastCommandParam & 0x7FU);
        dcd->endpointTransferActive[endpointIndex] = true;
    }
    gUsbDcdLastDalepenaAtStartTransfer = reg32(dcd->config.coreDeviceBase,
                                               AM275X_USB2SS_DEV_DALEPENA_OFFSET);
    return status;
}

int32_t Am275xUsbDcd_readEvent(Am275xUsbDcd *dcd, Am275xUsbDcdEvent *event)
{
    uint32_t eventCount;

    if ((dcd == NULL) || (event == NULL) || (!dcd->initialized)) {
        return AM275X_USB_HW_BAD_ARGUMENT;
    }

    eventCount = reg32(dcd->config.coreGlobalBase, AM275X_USB2SS_GBL_GEVNTCOUNT0_OFFSET);
    gUsbDcdLastEventCountRaw = eventCount;
    gUsbDcdLastEventCountBytes = eventCount & AM275X_USB2SS_GEVNTCOUNT_COUNT_MASK;
    gUsbDcdLastEventReadOffset = dcd->eventReadOffset;
    gUsbDcdEventCountHistory[gUsbDcdEventCountHistoryIndex & 31U] = eventCount;
    gUsbDcdEventCountHistoryIndex++;
    if ((eventCount & AM275X_USB2SS_GEVNTCOUNT_COUNT_MASK) < USB_DCD_EVENT_SIZE) {
        gUsbDcdReadEventNotReadyCount++;
        *event = (Am275xUsbDcdEvent) {
            .raw = 0U,
            .kind = AM275X_USB_DCD_EVENT_NONE,
            .eventType = 0U,
            .endpointNumber = 0U,
            .status = 0U,
            .parameter = 0U,
        };
        return AM275X_USB_HW_NOT_READY;
    }

    decode_event(read_event_word(dcd), event);
    gUsbDcdLastEventRaw = event->raw;
    gUsbDcdReadEventReadyCount++;
    if ((event->kind == AM275X_USB_DCD_EVENT_ENDPOINT) &&
        (event->endpointNumber < 32U) &&
        (event->eventType == AM275X_USB_DCD_ENDPOINT_EVENT_XFER_COMPLETE)) {
        dcd->endpointTransferActive[event->endpointNumber] = false;
        dcd->endpointTransferResource[event->endpointNumber] = 0U;
    }

    return AM275X_USB_HW_OK;
}

void Am275xUsbDcd_acknowledgeEvent(Am275xUsbDcd *dcd)
{
    if ((dcd == NULL) || (!dcd->initialized)) {
        return;
    }

    dcd->eventReadOffset = (uint16_t)(dcd->eventReadOffset + USB_DCD_EVENT_SIZE);
    if (dcd->eventReadOffset >= dcd->config.eventBufferSize) {
        dcd->eventReadOffset = 0U;
    }

    reg32_write(dcd->config.coreGlobalBase,
                AM275X_USB2SS_GBL_GEVNTCOUNT0_OFFSET,
                USB_DCD_EVENT_SIZE);
}

uint32_t Am275xUsbDcd_readDeviceStatus(const Am275xUsbDcd *dcd)
{
    if ((dcd == NULL) || (!dcd->initialized)) {
        return 0U;
    }

    return reg32(dcd->config.coreDeviceBase, AM275X_USB2SS_DEV_DSTS_OFFSET);
}

uint32_t Am275xUsbDcd_readActiveEndpointMask(const Am275xUsbDcd *dcd)
{
    if ((dcd == NULL) || (!dcd->initialized)) {
        return 0U;
    }

    return reg32(dcd->config.coreDeviceBase, AM275X_USB2SS_DEV_DALEPENA_OFFSET);
}
