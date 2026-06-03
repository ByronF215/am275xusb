#ifndef AM275X_USB_HW_H_
#define AM275X_USB_HW_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AM275X_USB0_MMR_BASE          (0x0F900000UL)
#define AM275X_USB0_PHY2_BASE         (0x0F908000UL)
#define AM275X_USB0_CORE_CAP_BASE     (0x31000000UL)
#define AM275X_USB0_CORE_OPER_BASE    (0x31000020UL)
#define AM275X_USB0_CORE_PORT_BASE    (0x31000420UL)
#define AM275X_USB0_CORE_INTR_BASE    (0x31000460UL)
#define AM275X_USB0_CORE_DOORBELL_BASE (0x31000560UL)
#define AM275X_USB0_CORE_GLOBAL_BASE  (0x3100C100UL)
#define AM275X_USB0_CORE_DEVICE_BASE  (0x3100C700UL)
#define AM275X_USB0_CORE_LINK_BASE    (0x3100D000UL)

#define AM275X_USB2SS_CFG_REVISION_OFFSET             (0x000U)
#define AM275X_USB2SS_CFG_OVERCURRENT_CONTROL_OFFSET  (0x004U)
#define AM275X_USB2SS_CFG_PHY_CONFIG_OFFSET           (0x008U)
#define AM275X_USB2SS_CFG_PHY_TEST_OFFSET             (0x00CU)
#define AM275X_USB2SS_CFG_CORE_STAT_OFFSET            (0x014U)
#define AM275X_USB2SS_CFG_HOST_VBUS_CTRL_OFFSET       (0x018U)
#define AM275X_USB2SS_CFG_MODE_CONTROL_OFFSET         (0x01CU)
#define AM275X_USB2SS_CFG_WAKEUP_CONFIG_OFFSET        (0x030U)
#define AM275X_USB2SS_CFG_WAKEUP_STAT_OFFSET          (0x034U)
#define AM275X_USB2SS_CFG_OVERRIDE_CONFIG_OFFSET      (0x038U)
#define AM275X_USB2SS_CFG_IRQ_MISC_STATUS_RAW_OFFSET  (0x430U)
#define AM275X_USB2SS_CFG_IRQ_MISC_STATUS_OFFSET      (0x434U)
#define AM275X_USB2SS_CFG_IRQ_MISC_ENABLE_SET_OFFSET  (0x438U)
#define AM275X_USB2SS_CFG_IRQ_MISC_ENABLE_CLR_OFFSET  (0x43CU)
#define AM275X_USB2SS_CFG_IRQ_MISC_EOI_OFFSET         (0x440U)
#define AM275X_USB2SS_CFG_VBUS_FILTER_OFFSET          (0x614U)
#define AM275X_USB2SS_CFG_VBUS_STAT_OFFSET            (0x618U)

#define AM275X_USB2SS_GBL_GSBUSCFG0_OFFSET            (0x000U)
#define AM275X_USB2SS_GBL_GSBUSCFG1_OFFSET            (0x004U)
#define AM275X_USB2SS_GBL_GTXTHRCFG_OFFSET            (0x008U)
#define AM275X_USB2SS_GBL_GRXTHRCFG_OFFSET            (0x00CU)
#define AM275X_USB2SS_GBL_GCTL_OFFSET                 (0x010U)
#define AM275X_USB2SS_GBL_GSTS_OFFSET                 (0x018U)
#define AM275X_USB2SS_GBL_GUCTL1_OFFSET               (0x01CU)
#define AM275X_USB2SS_GBL_GSNPSID_OFFSET              (0x020U)
#define AM275X_USB2SS_GBL_GUCTL_OFFSET                (0x02CU)
#define AM275X_USB2SS_GBL_GHWPARAMS0_OFFSET           (0x040U)
#define AM275X_USB2SS_GBL_GHWPARAMS7_OFFSET           (0x05CU)
#define AM275X_USB2SS_GBL_GUSB2PHYCFG0_OFFSET         (0x100U)
#define AM275X_USB2SS_GBL_GTXFIFOSIZ0_OFFSET          (0x200U)
#define AM275X_USB2SS_GBL_GRXFIFOSIZ0_OFFSET          (0x280U)
#define AM275X_USB2SS_GBL_GEVNTADRLO0_OFFSET          (0x300U)
#define AM275X_USB2SS_GBL_GEVNTADRHI0_OFFSET          (0x304U)
#define AM275X_USB2SS_GBL_GEVNTSIZ0_OFFSET            (0x308U)
#define AM275X_USB2SS_GBL_GEVNTCOUNT0_OFFSET          (0x30CU)

#define AM275X_USB2SS_GCTL_PRTCAPDIR_SHIFT            (12U)
#define AM275X_USB2SS_GCTL_PRTCAPDIR_MASK             (0x3UL << AM275X_USB2SS_GCTL_PRTCAPDIR_SHIFT)
#define AM275X_USB2SS_GCTL_PRTCAPDIR_HOST             (0x1UL << AM275X_USB2SS_GCTL_PRTCAPDIR_SHIFT)
#define AM275X_USB2SS_GCTL_PRTCAPDIR_DEVICE           (0x2UL << AM275X_USB2SS_GCTL_PRTCAPDIR_SHIFT)
#define AM275X_USB2SS_GCTL_CORESOFTRESET              (1UL << 11U)
#define AM275X_USB2SS_GCTL_DSBLCLKGTNG                (1UL << 0U)
#define AM275X_USB2SS_GUSB2PHYCFG_SUSPENDUSB20        (1UL << 6U)
#define AM275X_USB2SS_GUSB2PHYCFG_ENBLSLPM            (1UL << 8U)

#define AM275X_USB2SS_DEV_DCFG_OFFSET                 (0x000U)
#define AM275X_USB2SS_DEV_DCTL_OFFSET                 (0x004U)
#define AM275X_USB2SS_DEV_DEVTEN_OFFSET               (0x008U)
#define AM275X_USB2SS_DEV_DSTS_OFFSET                 (0x00CU)
#define AM275X_USB2SS_DEV_DGCMDPAR_OFFSET             (0x010U)
#define AM275X_USB2SS_DEV_DGCMD_OFFSET                (0x014U)
#define AM275X_USB2SS_DEV_DALEPENA_OFFSET             (0x020U)
#define AM275X_USB2SS_DEV_DEPCMDPAR2_BASE_OFFSET      (0x100U)
#define AM275X_USB2SS_DEV_DEPCMDPAR1_BASE_OFFSET      (0x104U)
#define AM275X_USB2SS_DEV_DEPCMDPAR0_BASE_OFFSET      (0x108U)
#define AM275X_USB2SS_DEV_DEPCMD_BASE_OFFSET          (0x10CU)
#define AM275X_USB2SS_DEV_ENDPOINT_STRIDE             (0x010U)
#define AM275X_USB2SS_DEV_IMOD_BASE_OFFSET            (0x300U)

#define AM275X_USB2SS_DCFG_DEVADDR_SHIFT              (3U)
#define AM275X_USB2SS_DCFG_DEVADDR_MASK               (0x7FUL << AM275X_USB2SS_DCFG_DEVADDR_SHIFT)
#define AM275X_USB2SS_DCFG_INTRNUM_SHIFT              (12U)
#define AM275X_USB2SS_DCFG_INTRNUM_MASK               (0x1FUL << AM275X_USB2SS_DCFG_INTRNUM_SHIFT)
#define AM275X_USB2SS_DCFG_DEVSPD_MASK                (0x7UL)
#define AM275X_USB2SS_DCFG_DEVSPD_HS                  (0x0UL)
#define AM275X_USB2SS_DCFG_DEVSPD_FS                  (0x1UL)
#define AM275X_USB2SS_DCFG_DEVSPD_SS                  (0x4UL)

#define AM275X_USB2SS_DCTL_RUN_STOP                   (1UL << 31U)
#define AM275X_USB2SS_DCTL_CSFTRST                    (1UL << 30U)
#define AM275X_USB2SS_DCTL_ULSTCHNGREQ_SHIFT          (5U)
#define AM275X_USB2SS_DCTL_ULSTCHNGREQ_MASK           (0xFUL << AM275X_USB2SS_DCTL_ULSTCHNGREQ_SHIFT)
#define AM275X_USB2SS_DCTL_ULSTCHNGREQ_RX_DETECT      (0x5UL << AM275X_USB2SS_DCTL_ULSTCHNGREQ_SHIFT)
#define AM275X_USB2SS_DCTL_ULSTCHNGREQ_REMOTE_WAKE    (0x8UL << AM275X_USB2SS_DCTL_ULSTCHNGREQ_SHIFT)

#define AM275X_USB2SS_DEVTEN_SOFTEVTEN                (1UL << 7U)
#define AM275X_USB2SS_DEVTEN_U3L2L1SUSPEN             (1UL << 6U)
#define AM275X_USB2SS_DEVTEN_WKUPEVTEN                (1UL << 4U)
#define AM275X_USB2SS_DEVTEN_ULSTCNGEN                (1UL << 3U)
#define AM275X_USB2SS_DEVTEN_CONNECTDONEEVTEN         (1UL << 2U)
#define AM275X_USB2SS_DEVTEN_USBRSTEVTEN              (1UL << 1U)
#define AM275X_USB2SS_DEVTEN_DISSCONNEVTEN            (1UL << 0U)

#define AM275X_USB2SS_DSTS_DEVCTRLHLT                 (1UL << 22U)
#define AM275X_USB2SS_DSTS_USBLNKST_SHIFT             (18U)
#define AM275X_USB2SS_DSTS_USBLNKST_MASK              (0xFUL << AM275X_USB2SS_DSTS_USBLNKST_SHIFT)
#define AM275X_USB2SS_DSTS_CONNECTSPD_MASK            (0x7UL)
#define AM275X_USB2SS_DSTS_CONNECTSPD_HS              (0x0UL)
#define AM275X_USB2SS_DSTS_CONNECTSPD_FS              (0x1UL)
#define AM275X_USB2SS_DSTS_CONNECTSPD_SS              (0x4UL)

#define AM275X_USB2SS_DALEPENA_EP0_OUT                (1UL << 0U)
#define AM275X_USB2SS_DALEPENA_EP0_IN                 (1UL << 1U)
#define AM275X_USB2SS_DALEPENA_EP_OUT(n)              (1UL << ((uint32_t)(n) * 2UL))
#define AM275X_USB2SS_DALEPENA_EP_IN(n)               (1UL << (((uint32_t)(n) * 2UL) + 1UL))

#define AM275X_USB2SS_DEPCMD_CMDACT                   (1UL << 10U)
#define AM275X_USB2SS_DEPCMD_CMDIOC                   (1UL << 8U)
#define AM275X_USB2SS_DEPCMD_HIPRI_FORCERM            (1UL << 11U)
#define AM275X_USB2SS_DEPCMD_CMDTYP_MASK              (0xFUL)
#define AM275X_USB2SS_DEPCMD_SET_EP_CONFIG            (0x1UL)
#define AM275X_USB2SS_DEPCMD_SET_XFER_RESOURCE        (0x2UL)
#define AM275X_USB2SS_DEPCMD_GET_EP_STATE             (0x3UL)
#define AM275X_USB2SS_DEPCMD_SET_STALL                (0x4UL)
#define AM275X_USB2SS_DEPCMD_CLEAR_STALL              (0x5UL)
#define AM275X_USB2SS_DEPCMD_START_TRANSFER           (0x6UL)
#define AM275X_USB2SS_DEPCMD_UPDATE_TRANSFER          (0x7UL)
#define AM275X_USB2SS_DEPCMD_END_TRANSFER             (0x8UL)
#define AM275X_USB2SS_DEPCMD_START_NEW_CONFIG         (0x9UL)
#define AM275X_USB2SS_DEPCMD_COMMAND_PARAM_SHIFT      (16U)
#define AM275X_USB2SS_DEPCMD_COMMAND_PARAM_MASK       (0xFFFFUL << AM275X_USB2SS_DEPCMD_COMMAND_PARAM_SHIFT)
#define AM275X_USB2SS_DEPCMD_CMDSTATUS_SHIFT          (12U)
#define AM275X_USB2SS_DEPCMD_CMDSTATUS_MASK           (0xFUL << AM275X_USB2SS_DEPCMD_CMDSTATUS_SHIFT)

#define AM275X_USB2SS_DEPCFG_EP_TYPE(n)               (((uint32_t)(n) & 0x3UL) << 1U)
#define AM275X_USB2SS_DEPCFG_MAX_PACKET_SIZE(n)       (((uint32_t)(n) & 0x7FFUL) << 3U)
#define AM275X_USB2SS_DEPCFG_FIFO_NUMBER(n)           (((uint32_t)(n) & 0x1FUL) << 17U)
#define AM275X_USB2SS_DEPCFG_BURST_SIZE(n)            (((uint32_t)(n) & 0xFUL) << 22U)
#define AM275X_USB2SS_DEPCFG_ACTION_INIT              (0UL << 30U)
#define AM275X_USB2SS_DEPCFG_ACTION_RESTORE           (1UL << 30U)
#define AM275X_USB2SS_DEPCFG_ACTION_MODIFY            (2UL << 30U)
#define AM275X_USB2SS_DEPCFG_INT_NUM(n)               (((uint32_t)(n) & 0x1FUL) << 0U)
#define AM275X_USB2SS_DEPCFG_XFER_COMPLETE_EN         (1UL << 8U)
#define AM275X_USB2SS_DEPCFG_XFER_IN_PROGRESS_EN      (1UL << 9U)
#define AM275X_USB2SS_DEPCFG_XFER_NOT_READY_EN        (1UL << 10U)
#define AM275X_USB2SS_DEPCFG_FIFO_ERROR_EN            (1UL << 11U)
#define AM275X_USB2SS_DEPCFG_EP_NUMBER(n)             (((uint32_t)(n) & 0x1FUL) << 25U)
#define AM275X_USB2SS_DEPXFERCFG_NUM_XFER_RES(n)      ((uint32_t)(n) & 0xFFFFUL)

#define AM275X_USB_DCD_EP_TYPE_CONTROL                (0U)
#define AM275X_USB_DCD_EP_TYPE_ISOCHRONOUS            (1U)
#define AM275X_USB_DCD_EP_TYPE_BULK                   (2U)
#define AM275X_USB_DCD_EP_TYPE_INTERRUPT              (3U)

#define AM275X_USB2SS_GEVNTSIZ_INTMASK                (1UL << 31U)
#define AM275X_USB2SS_GEVNTSIZ_SIZE_MASK              (0xFFFFUL)
#define AM275X_USB2SS_GEVNTCOUNT_BUSY                 (1UL << 31U)
#define AM275X_USB2SS_GEVNTCOUNT_COUNT_MASK           (0xFFFFUL)

#define AM275X_USB_DCD_EP0_OUT_INDEX  (0U)
#define AM275X_USB_DCD_EP0_IN_INDEX   (1U)
#define AM275X_USB_DCD_EP_OUT_INDEX(n) ((uint32_t)(n) * 2UL)
#define AM275X_USB_DCD_EP_IN_INDEX(n) (((uint32_t)(n) * 2UL) + 1UL)

#define AM275X_USB_DCD_TRB_CTRL_HWO           (1UL << 0U)
#define AM275X_USB_DCD_TRB_CTRL_LST           (1UL << 1U)
#define AM275X_USB_DCD_TRB_CTRL_CHN           (1UL << 2U)
#define AM275X_USB_DCD_TRB_CTRL_CSP           (1UL << 3U)
#define AM275X_USB_DCD_TRB_CTRL_TRBCTL_SHIFT  (4U)
#define AM275X_USB_DCD_TRB_CTRL_TRBCTL_MASK   (0x3FUL << AM275X_USB_DCD_TRB_CTRL_TRBCTL_SHIFT)
#define AM275X_USB_DCD_TRB_CTRL_ISP_IMI       (1UL << 10U)
#define AM275X_USB_DCD_TRB_CTRL_IOC           (1UL << 11U)
#define AM275X_USB_DCD_TRB_SIZE_MASK          (0x7FFFFFUL)

#define AM275X_USB_DCD_TRBCTL_NORMAL          (1U)
#define AM275X_USB_DCD_TRBCTL_CONTROL_SETUP   (2U)
#define AM275X_USB_DCD_TRBCTL_CONTROL_STATUS2 (3U)
#define AM275X_USB_DCD_TRBCTL_CONTROL_STATUS3 (4U)
#define AM275X_USB_DCD_TRBCTL_CONTROL_DATA    (5U)

#define AM275X_USB0_IRQ_FIRST         (220U)
#define AM275X_USB0_IRQ_COUNT         (8U)
#define AM275X_USB0_MISC_IRQ          (228U)

#define AM275X_USB_HW_OK              (0)
#define AM275X_USB_HW_BAD_ARGUMENT    (-1)
#define AM275X_USB_HW_TIMEOUT         (-2)
#define AM275X_USB_HW_NOT_READY       (-3)

typedef struct Am275xUsbHwInfo_s {
    uintptr_t mmrBase;
    uintptr_t phyBase;
    uintptr_t coreCapBase;
    uintptr_t coreGlobalBase;
    uintptr_t coreDeviceBase;
    uint8_t capabilityLength;
    uint16_t hciVersion;
    uint32_t hcsParams1;
    uint32_t hccParams1;
    uint32_t gsnpsid;
    uint32_t gctl;
    uint32_t dcfg;
    uint32_t dctl;
    uint32_t dsts;
} Am275xUsbHwInfo;

typedef struct Am275xUsbDcdConfig_s {
    uintptr_t mmrBase;
    uintptr_t coreGlobalBase;
    uintptr_t coreDeviceBase;
    uintptr_t eventBufferAddress;
    uint16_t eventBufferSize;
    uint8_t interruptNumber;
    bool enableSofEvent;
} Am275xUsbDcdConfig;

typedef struct Am275xUsbDcd_s {
    Am275xUsbDcdConfig config;
    uint8_t endpointTransferResource[32];
    bool endpointTransferActive[32];
    uint16_t eventReadOffset;
    bool initialized;
    bool connected;
} Am275xUsbDcd;

typedef enum Am275xUsbDcdEventKind_e {
    AM275X_USB_DCD_EVENT_NONE = 0,
    AM275X_USB_DCD_EVENT_DEVICE,
    AM275X_USB_DCD_EVENT_ENDPOINT,
} Am275xUsbDcdEventKind;

typedef enum Am275xUsbDcdDeviceEvent_e {
    AM275X_USB_DCD_DEVICE_EVENT_DISCONNECT = 0,
    AM275X_USB_DCD_DEVICE_EVENT_RESET = 1,
    AM275X_USB_DCD_DEVICE_EVENT_CONNECT_DONE = 2,
    AM275X_USB_DCD_DEVICE_EVENT_LINK_STATE_CHANGE = 3,
    AM275X_USB_DCD_DEVICE_EVENT_WAKEUP = 4,
    AM275X_USB_DCD_DEVICE_EVENT_HIBERNATION_REQUEST = 5,
    AM275X_USB_DCD_DEVICE_EVENT_EOPF = 6,
    AM275X_USB_DCD_DEVICE_EVENT_SOF = 7,
    AM275X_USB_DCD_DEVICE_EVENT_ERRATIC_ERROR = 9,
    AM275X_USB_DCD_DEVICE_EVENT_COMMAND_COMPLETE = 10,
    AM275X_USB_DCD_DEVICE_EVENT_EVENT_BUFFER_OVERFLOW = 11,
} Am275xUsbDcdDeviceEvent;

typedef enum Am275xUsbDcdEndpointEvent_e {
    AM275X_USB_DCD_ENDPOINT_EVENT_XFER_COMPLETE = 1,
    AM275X_USB_DCD_ENDPOINT_EVENT_XFER_IN_PROGRESS = 2,
    AM275X_USB_DCD_ENDPOINT_EVENT_XFER_NOT_READY = 3,
    AM275X_USB_DCD_ENDPOINT_EVENT_RX_TX_FIFO_EVENT = 4,
    AM275X_USB_DCD_ENDPOINT_EVENT_COMMAND_COMPLETE = 7,
} Am275xUsbDcdEndpointEvent;

typedef struct Am275xUsbDcdEvent_s {
    uint32_t raw;
    Am275xUsbDcdEventKind kind;
    uint8_t eventType;
    uint8_t endpointNumber;
    uint8_t status;
    uint16_t parameter;
} Am275xUsbDcdEvent;

typedef struct Am275xUsbDcdTrb_s {
    uint32_t bufferPointerLow;
    uint32_t bufferPointerHigh;
    uint32_t size;
    uint32_t control;
} Am275xUsbDcdTrb;

void Am275xUsbHw_getUsb0Info(Am275xUsbHwInfo *info);
int32_t Am275xUsbHw_probeUsb0(Am275xUsbHwInfo *info);
uint32_t Am275xUsbHw_getEndpointRegisterOffset(uint32_t endpointIndex, uint32_t endpointRegisterBaseOffset);
void Am275xUsbDcd_getDefaultUsb0Config(Am275xUsbDcdConfig *config);
int32_t Am275xUsbDcd_init(Am275xUsbDcd *dcd, const Am275xUsbDcdConfig *config);
int32_t Am275xUsbDcd_connect(Am275xUsbDcd *dcd);
int32_t Am275xUsbDcd_disconnect(Am275xUsbDcd *dcd);
int32_t Am275xUsbDcd_setAddress(Am275xUsbDcd *dcd, uint8_t address);
int32_t Am275xUsbDcd_issueEndpointCommand(Am275xUsbDcd *dcd,
                                          uint32_t endpointIndex,
                                          uint32_t command,
                                          uint32_t param0,
                                          uint32_t param1,
                                          uint32_t param2,
                                          uint32_t commandParam,
                                          uint32_t *commandStatus);
int32_t Am275xUsbDcd_startNewConfig(Am275xUsbDcd *dcd, uint8_t resourceIndex);
int32_t Am275xUsbDcd_configureEndpoint(Am275xUsbDcd *dcd,
                                       uint32_t endpointIndex,
                                       uint32_t endpointType,
                                       uint16_t maxPacketSize,
                                       uint8_t fifoNumber,
                                       bool enableTransferNotReady);
int32_t Am275xUsbDcd_setTransferResource(Am275xUsbDcd *dcd,
                                         uint32_t endpointIndex,
                                         uint16_t resourceCount);
int32_t Am275xUsbDcd_endTransfer(Am275xUsbDcd *dcd, uint32_t endpointIndex, bool forceRemove);
int32_t Am275xUsbDcd_configureEp0(Am275xUsbDcd *dcd);
int32_t Am275xUsbDcd_stallEndpoint(Am275xUsbDcd *dcd, uint32_t endpointIndex);
int32_t Am275xUsbDcd_clearEndpointStall(Am275xUsbDcd *dcd, uint32_t endpointIndex);
void Am275xUsbDcd_prepareTrb(Am275xUsbDcdTrb *trb,
                             const void *buffer,
                             uint32_t length,
                             uint32_t trbControl,
                             bool interruptOnComplete);
int32_t Am275xUsbDcd_startTransfer(Am275xUsbDcd *dcd,
                                   uint32_t endpointIndex,
                                   const Am275xUsbDcdTrb *firstTrb,
                                   uint16_t streamOrMicroFrame,
                                   uint32_t *commandStatus);
int32_t Am275xUsbDcd_readEvent(Am275xUsbDcd *dcd, Am275xUsbDcdEvent *event);
void Am275xUsbDcd_acknowledgeEvent(Am275xUsbDcd *dcd);
uint32_t Am275xUsbDcd_readDeviceStatus(const Am275xUsbDcd *dcd);
uint32_t Am275xUsbDcd_readActiveEndpointMask(const Am275xUsbDcd *dcd);

#ifdef __cplusplus
}
#endif

#endif
