#ifndef AM275X_USB_CLASS_H_
#define AM275X_USB_CLASS_H_

#include <stdbool.h>
#include <stdint.h>

#include "am275x_usb_hw.h"
#include "am275x_usb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Am275xUsbClassDriver_s {
    int32_t (*getDescriptor)(void *context,
                             Am275xUsbDescriptorType type,
                             uint8_t index,
                             const uint8_t **data,
                             uint16_t *length);
    int32_t (*handleClassRequest)(void *context,
                                  const Am275xUsbSetupPacket *setup,
                                  const uint8_t **txData,
                                  uint16_t *txLength,
                                  bool *statusOnly);
    int32_t (*setConfigured)(void *context, bool configured);
    int32_t (*getInterface)(void *context, uint8_t interfaceNumber, uint8_t *alternateSetting);
    int32_t (*setInterface)(void *context, uint8_t interfaceNumber, uint8_t alternateSetting);
    void (*busReset)(void *context);
    void (*processEvent)(void *context, const Am275xUsbDcdEvent *event);
    int32_t (*poll)(void *context);
} Am275xUsbClassDriver;

#ifdef __cplusplus
}
#endif

#endif
