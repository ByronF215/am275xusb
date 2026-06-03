#ifndef AM275X_USB_MSC_H_
#define AM275X_USB_MSC_H_

#include <stdbool.h>
#include <stdint.h>

#include <drivers/mmcsd.h>

#include "am275x_usb_types.h"
#include "am275x_usb_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AM275X_USB_MSC_OK              (0)
#define AM275X_USB_MSC_BAD_ARGUMENT    (-1)
#define AM275X_USB_MSC_HW_ERROR        (-2)
#define AM275X_USB_MSC_NOT_SUPPORTED   (-3)

#define AM275X_USB_MSC_EP_IN_ADDR      (0x81U)
#define AM275X_USB_MSC_EP_OUT_ADDR     (0x01U)
#define AM275X_USB_MSC_EP_IN_INDEX     AM275X_USB_DCD_EP_IN_INDEX(1U)
#define AM275X_USB_MSC_EP_OUT_INDEX    AM275X_USB_DCD_EP_OUT_INDEX(1U)
#define AM275X_USB_MSC_MAX_PACKET      (512U)
#define AM275X_USB_MSC_BLOCK_SIZE      (512U)
#define AM275X_USB_MSC_DATA_BUFFER_SIZE (4096U)

typedef enum Am275xUsbMscState_e {
    AM275X_USB_MSC_STATE_DISABLED = 0,
    AM275X_USB_MSC_STATE_WAIT_CBW,
    AM275X_USB_MSC_STATE_DATA_IN,
    AM275X_USB_MSC_STATE_DATA_OUT,
    AM275X_USB_MSC_STATE_CSW_IN,
} Am275xUsbMscState;

typedef struct Am275xUsbMsc_s {
    Am275xUsbDcd *dcd;
    MMCSD_Handle media;
    Am275xUsbDcdTrb outTrb __attribute__((aligned(32)));
    Am275xUsbDcdTrb inTrb __attribute__((aligned(32)));
    uint8_t cbw[AM275X_USB_MSC_MAX_PACKET] __attribute__((aligned(32)));
    uint8_t csw[13] __attribute__((aligned(32)));
    uint8_t data[AM275X_USB_MSC_DATA_BUFFER_SIZE] __attribute__((aligned(32)));
    uint32_t blockCount;
    uint32_t blockSize;
    uint32_t tag;
    uint32_t residue;
    uint32_t readLba;
    uint32_t writeLba;
    uint32_t dataOutLength;
    uint16_t readBlocksLeft;
    uint16_t writeBlocksLeft;
    uint16_t dataOutBlocks;
    uint8_t dataOutStatus;
    uint8_t dataOutWriteToMedia;
    uint8_t senseKey;
    uint8_t asc;
    uint8_t ascq;
    Am275xUsbMscState state;
    bool configured;
    bool endpointsConfigured;
    bool mediaReady;
} Am275xUsbMsc;

int32_t Am275xUsbMsc_init(Am275xUsbMsc *msc, MMCSD_Handle media);
void Am275xUsbMsc_attachDcd(Am275xUsbMsc *msc, Am275xUsbDcd *dcd);
int32_t Am275xUsbMsc_configureEndpoints(Am275xUsbMsc *msc);
int32_t Am275xUsbMsc_setConfigured(Am275xUsbMsc *msc, bool configured);
int32_t Am275xUsbMsc_poll(Am275xUsbMsc *msc);
void Am275xUsbMsc_busReset(Am275xUsbMsc *msc);
void Am275xUsbMsc_processEvent(Am275xUsbMsc *msc, const Am275xUsbDcdEvent *event);

int32_t Am275xUsbMsc_getDescriptor(Am275xUsbDescriptorType type,
                                   uint8_t index,
                                   const uint8_t **data,
                                   uint16_t *length);
int32_t Am275xUsbMsc_handleClassRequest(Am275xUsbMsc *msc,
                                        const Am275xUsbSetupPacket *setup,
                                        const uint8_t **txData,
                                        uint16_t *txLength,
                                        bool *statusOnly);

#ifdef __cplusplus
}
#endif

#endif
