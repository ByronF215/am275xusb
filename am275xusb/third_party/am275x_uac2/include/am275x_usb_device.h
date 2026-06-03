#ifndef AM275X_USB_DEVICE_H_
#define AM275X_USB_DEVICE_H_

#include <stdbool.h>
#include <stdint.h>

#include "am275x_uac2.h"
#include "am275x_usb_hw.h"
#include "am275x_usb_msc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Am275xUsbDcd_s;

#define AM275X_USB_OK                 (0)
#define AM275X_USB_EP0_STALL          (-1)
#define AM275X_USB_BAD_ARGUMENT       (-2)
#define AM275X_USB_NOT_SUPPORTED      (-3)

#define AM275X_USB_REQ_GET_STATUS        (0x00U)
#define AM275X_USB_REQ_CLEAR_FEATURE     (0x01U)
#define AM275X_USB_REQ_SET_FEATURE       (0x03U)
#define AM275X_USB_REQ_SET_ADDRESS       (0x05U)
#define AM275X_USB_REQ_GET_DESCRIPTOR    (0x06U)
#define AM275X_USB_REQ_SET_DESCRIPTOR    (0x07U)
#define AM275X_USB_REQ_GET_CONFIGURATION (0x08U)
#define AM275X_USB_REQ_SET_CONFIGURATION (0x09U)
#define AM275X_USB_REQ_GET_INTERFACE     (0x0AU)
#define AM275X_USB_REQ_SET_INTERFACE     (0x0BU)

#define AM275X_USB_REQ_TYPE_MASK         (0x60U)
#define AM275X_USB_REQ_TYPE_STANDARD     (0x00U)
#define AM275X_USB_REQ_TYPE_CLASS        (0x20U)

#define AM275X_USB_RECIPIENT_MASK        (0x1FU)
#define AM275X_USB_RECIPIENT_DEVICE      (0x00U)
#define AM275X_USB_RECIPIENT_INTERFACE   (0x01U)
#define AM275X_USB_RECIPIENT_ENDPOINT    (0x02U)

typedef struct Am275xUsbEp0Response_s {
    const uint8_t *txData;
    uint16_t txLength;
    bool statusOnly;
} Am275xUsbEp0Response;

typedef struct Am275xUsbDevice_s {
    Am275xUsbMsc *msc;
    struct Am275xUsbDcd_s *dcd;
    uint8_t address;
    uint8_t pendingAddress;
    uint8_t configuration;
    uint8_t pendingConfiguration;
    bool pendingAddressValid;
    bool pendingConfigurationValid;
    uint32_t setupCount;
    uint32_t standardRequestCount;
    uint32_t classRequestCount;
    uint32_t stallCount;
} Am275xUsbDevice;

int32_t Am275xUsbDevice_init(Am275xUsbDevice *dev, Am275xUsbMsc *msc);
void Am275xUsbDevice_attachDcd(Am275xUsbDevice *dev, struct Am275xUsbDcd_s *dcd);
int32_t Am275xUsbDevice_handleSetup(Am275xUsbDevice *dev,
                                    const Am275xUac2SetupPacket *setup,
                                    const uint8_t *outData,
                                    uint16_t outLength,
                                    Am275xUsbEp0Response *response);
void Am275xUsbDevice_processDcdEvent(Am275xUsbDevice *dev, const Am275xUsbDcdEvent *event);
void Am275xUsbDevice_statusStageComplete(Am275xUsbDevice *dev);
void Am275xUsbDevice_busReset(Am275xUsbDevice *dev);

#ifdef __cplusplus
}
#endif

#endif
