#ifndef AM275X_UAC2_H_
#define AM275X_UAC2_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "am275x_usb_class.h"
#include "am275x_usb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AM275X_UAC2_EP0_STALL       (-1)
#define AM275X_UAC2_NOT_SUPPORTED   (-2)
#define AM275X_UAC2_BAD_ARGUMENT    (-3)

#define AM275X_UAC2_IF_AUDIO_CONTROL  (0U)
#define AM275X_UAC2_IF_CAPTURE_STREAM (1U)
#define AM275X_UAC2_ALT_ZERO_BW       (0U)
#define AM275X_UAC2_ALT_CAP_48K_S16   (1U)

typedef enum Am275xUac2Mode_e {
    AM275X_UAC2_MODE_CAPTURE_ONLY = 0,
} Am275xUac2Mode;

typedef enum Am275xUac2Format_e {
    AM275X_UAC2_FORMAT_PCM_S16 = 0,
} Am275xUac2Format;

#define AM275X_UAC2_DESC_DEVICE        AM275X_USB_DESC_DEVICE
#define AM275X_UAC2_DESC_CONFIGURATION AM275X_USB_DESC_CONFIGURATION
#define AM275X_UAC2_DESC_STRING        AM275X_USB_DESC_STRING

typedef Am275xUsbDescriptorType Am275xUac2DescriptorType;

typedef enum Am275xUac2StreamDir_e {
    AM275X_UAC2_STREAM_CAPTURE = 0,
} Am275xUac2StreamDir;

typedef struct Am275xUac2Config_s {
    Am275xUac2Mode mode;
    Am275xUac2Format format;
    uint32_t sampleRateHz;
    uint8_t channels;
    uint16_t vendorId;
    uint16_t productId;
    uint16_t deviceBcd;
} Am275xUac2Config;

typedef Am275xUsbSetupPacket Am275xUac2SetupPacket;
typedef Am275xUsbControlTransfer Am275xUac2ControlTransfer;

typedef struct Am275xUac2Stats_s {
    uint32_t usbResetCount;
    uint32_t suspendCount;
    uint32_t resumeCount;
    uint32_t setIfAlt0Count;
    uint32_t setIfAlt1Count;
    uint32_t unsupportedRequestCount;
    uint32_t controlErrorCount;
    uint32_t captureOverrunCount;
    uint32_t dmaErrorCount;
} Am275xUac2Stats;

typedef struct Am275xUac2Context_s {
    Am275xUac2Config config;
    bool configured;
    bool captureStreaming;
    bool clockValid;
    uint8_t captureMute;
    int16_t captureVolumeQ8_8;
    Am275xUac2Stats stats;
} Am275xUac2Context;

void Am275xUac2_getDefaultConfig(Am275xUac2Config *config);
int32_t Am275xUac2_init(Am275xUac2Context *ctx, const Am275xUac2Config *config);

int32_t Am275xUac2_getDescriptor(Am275xUac2Context *ctx,
                                 Am275xUac2DescriptorType type,
                                 uint8_t index,
                                 const uint8_t **data,
                                 uint16_t *length);

int32_t Am275xUac2_handleClassRequest(Am275xUac2Context *ctx,
                                      const Am275xUac2SetupPacket *setup,
                                      Am275xUac2ControlTransfer *transfer);

int32_t Am275xUac2_setInterface(Am275xUac2Context *ctx, uint8_t interfaceNumber, uint8_t alternateSetting);
void Am275xUac2_onUsbReset(Am275xUac2Context *ctx);
void Am275xUac2_onSuspend(Am275xUac2Context *ctx);
void Am275xUac2_onResume(Am275xUac2Context *ctx);
void Am275xUac2_periodic1ms(Am275xUac2Context *ctx);

const Am275xUac2Stats *Am275xUac2_getStats(const Am275xUac2Context *ctx);
const Am275xUsbClassDriver *Am275xUac2_getClassDriver(void);

#ifdef __cplusplus
}
#endif

#endif
