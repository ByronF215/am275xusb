#ifndef AM275X_USB_TYPES_H_
#define AM275X_USB_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Am275xUsbDescriptorType_e {
    AM275X_USB_DESC_DEVICE = 1,
    AM275X_USB_DESC_CONFIGURATION = 2,
    AM275X_USB_DESC_STRING = 3,
} Am275xUsbDescriptorType;

typedef struct Am275xUsbSetupPacket_s {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} Am275xUsbSetupPacket;

typedef struct Am275xUsbControlTransfer_s {
    const uint8_t *txData;
    uint16_t txLength;
    uint8_t *rxData;
    uint16_t rxLength;
} Am275xUsbControlTransfer;

#ifdef __cplusplus
}
#endif

#endif
