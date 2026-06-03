#include "am275x_uac2.h"

#define USB_DESC_TYPE_DEVICE             (0x01U)
#define USB_DESC_TYPE_CONFIGURATION      (0x02U)
#define USB_DESC_TYPE_STRING             (0x03U)
#define USB_DESC_TYPE_INTERFACE          (0x04U)
#define USB_DESC_TYPE_ENDPOINT           (0x05U)
#define USB_DESC_TYPE_IAD                (0x0BU)
#define USB_DESC_TYPE_CS_INTERFACE       (0x24U)
#define USB_DESC_TYPE_CS_ENDPOINT        (0x25U)

#define USB_CLASS_MISCELLANEOUS          (0xEFU)
#define USB_SUBCLASS_COMMON              (0x02U)
#define USB_PROTOCOL_IAD                 (0x01U)

#define USB_CLASS_AUDIO                  (0x01U)
#define USB_AUDIO_SUBCLASS_CONTROL       (0x01U)
#define USB_AUDIO_SUBCLASS_STREAMING     (0x02U)
#define USB_AUDIO_PROTOCOL_V2            (0x20U)

#define UAC2_CS_AC_HEADER                (0x01U)
#define UAC2_CS_CLOCK_SOURCE             (0x0AU)
#define UAC2_CS_INPUT_TERMINAL           (0x02U)
#define UAC2_CS_OUTPUT_TERMINAL          (0x03U)
#define UAC2_CS_FEATURE_UNIT             (0x06U)
#define UAC2_CS_AS_GENERAL               (0x01U)
#define UAC2_CS_FORMAT_TYPE              (0x02U)

#define UAC2_TERMINAL_MICROPHONE         (0x0201U)
#define UAC2_TERMINAL_USB_STREAMING      (0x0101U)

#define UAC2_ID_IT_MIC                   (0x04U)
#define UAC2_ID_FU_CAPTURE               (0x05U)
#define UAC2_ID_OT_USB_CAPTURE           (0x06U)
#define UAC2_ID_CLOCK_MAIN               (0x10U)

#define UAC2_EP_CAPTURE_IN               (0x81U)
#define UAC2_CAPTURE_MAX_PACKET_48K_S16  (192U)

#define U16_LE(v)  ((uint8_t)((v) & 0xFFU)), ((uint8_t)(((v) >> 8U) & 0xFFU))
#define U32_LE(v)  U16_LE((uint16_t)((v) & 0xFFFFU)), U16_LE((uint16_t)(((v) >> 16U) & 0xFFFFU))

static const uint8_t gDeviceDescriptor[] = {
    18, USB_DESC_TYPE_DEVICE,
    U16_LE(0x0200),
    USB_CLASS_MISCELLANEOUS, USB_SUBCLASS_COMMON, USB_PROTOCOL_IAD,
    64,
    U16_LE(0x0451),
    U16_LE(0xA275),
    U16_LE(0x0100),
    1, 2, 3,
    1,
};

static const uint8_t gConfigDescriptor[] = {
    9, USB_DESC_TYPE_CONFIGURATION,
    U16_LE(145),
    2,
    1,
    0,
    0x80,
    50,

    8, USB_DESC_TYPE_IAD,
    AM275X_UAC2_IF_AUDIO_CONTROL,
    2,
    USB_CLASS_AUDIO,
    USB_AUDIO_SUBCLASS_CONTROL,
    USB_AUDIO_PROTOCOL_V2,
    2,

    9, USB_DESC_TYPE_INTERFACE,
    AM275X_UAC2_IF_AUDIO_CONTROL,
    0,
    0,
    USB_CLASS_AUDIO,
    USB_AUDIO_SUBCLASS_CONTROL,
    USB_AUDIO_PROTOCOL_V2,
    2,

    9, USB_DESC_TYPE_CS_INTERFACE,
    UAC2_CS_AC_HEADER,
    U16_LE(0x0200),
    0x00,
    U16_LE(64),
    0x00,

    8, USB_DESC_TYPE_CS_INTERFACE,
    UAC2_CS_CLOCK_SOURCE,
    UAC2_ID_CLOCK_MAIN,
    0x03,
    0x03,
    0x07,
    0,

    17, USB_DESC_TYPE_CS_INTERFACE,
    UAC2_CS_INPUT_TERMINAL,
    UAC2_ID_IT_MIC,
    U16_LE(UAC2_TERMINAL_MICROPHONE),
    0,
    UAC2_ID_CLOCK_MAIN,
    2,
    U32_LE(0x00000003),
    0,
    U16_LE(0),
    0,

    18, USB_DESC_TYPE_CS_INTERFACE,
    UAC2_CS_FEATURE_UNIT,
    UAC2_ID_FU_CAPTURE,
    UAC2_ID_IT_MIC,
    U32_LE(0x00000003),
    U32_LE(0x00000000),
    U32_LE(0x00000000),
    0,

    12, USB_DESC_TYPE_CS_INTERFACE,
    UAC2_CS_OUTPUT_TERMINAL,
    UAC2_ID_OT_USB_CAPTURE,
    U16_LE(UAC2_TERMINAL_USB_STREAMING),
    0,
    UAC2_ID_FU_CAPTURE,
    UAC2_ID_CLOCK_MAIN,
    U16_LE(0),
    0,

    9, USB_DESC_TYPE_INTERFACE,
    AM275X_UAC2_IF_CAPTURE_STREAM,
    AM275X_UAC2_ALT_ZERO_BW,
    0,
    USB_CLASS_AUDIO,
    USB_AUDIO_SUBCLASS_STREAMING,
    USB_AUDIO_PROTOCOL_V2,
    0,

    9, USB_DESC_TYPE_INTERFACE,
    AM275X_UAC2_IF_CAPTURE_STREAM,
    AM275X_UAC2_ALT_CAP_48K_S16,
    1,
    USB_CLASS_AUDIO,
    USB_AUDIO_SUBCLASS_STREAMING,
    USB_AUDIO_PROTOCOL_V2,
    0,

    16, USB_DESC_TYPE_CS_INTERFACE,
    UAC2_CS_AS_GENERAL,
    UAC2_ID_OT_USB_CAPTURE,
    0x00,
    0x01,
    U32_LE(0x00000001),
    1,
    U32_LE(0x00000003),
    2,

    6, USB_DESC_TYPE_CS_INTERFACE,
    UAC2_CS_FORMAT_TYPE,
    0x01,
    2,
    16,

    7, USB_DESC_TYPE_ENDPOINT,
    UAC2_EP_CAPTURE_IN,
    0x05,
    U16_LE(UAC2_CAPTURE_MAX_PACKET_48K_S16),
    4,

    8, USB_DESC_TYPE_CS_ENDPOINT,
    0x01,
    0x00,
    0x00,
    U16_LE(0),
    0x00,
};

static const uint8_t gStringLangId[] = {
    4, USB_DESC_TYPE_STRING, U16_LE(0x0409),
};

static const uint8_t gStringManufacturer[] = {
    22, USB_DESC_TYPE_STRING,
    'A', 0, 'M', 0, '2', 0, '7', 0, '5', 0, 'x', 0, ' ', 0, 'U', 0, 'A', 0, 'C', 0,
};

static const uint8_t gStringProduct[] = {
    32, USB_DESC_TYPE_STRING,
    'U', 0, 'A', 0, 'C', 0, '2', 0, ' ', 0, 'M', 0, 'V', 0, 'P', 0,
    ' ', 0, 'M', 0, 'i', 0, 'c', 0, ' ', 0, 'I', 0, 'N', 0,
};

static const uint8_t gStringSerial[] = {
    18, USB_DESC_TYPE_STRING,
    '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '1', 0,
};

int32_t Am275xUac2_getDescriptor(Am275xUac2Context *ctx,
                                 Am275xUac2DescriptorType type,
                                 uint8_t index,
                                 const uint8_t **data,
                                 uint16_t *length)
{
    (void)ctx;

    if ((data == NULL) || (length == NULL)) {
        return AM275X_UAC2_BAD_ARGUMENT;
    }

    *data = NULL;
    *length = 0;

    if (type == AM275X_UAC2_DESC_DEVICE) {
        *data = gDeviceDescriptor;
        *length = (uint16_t)sizeof(gDeviceDescriptor);
        return 0;
    }

    if (type == AM275X_UAC2_DESC_CONFIGURATION) {
        *data = gConfigDescriptor;
        *length = (uint16_t)sizeof(gConfigDescriptor);
        return 0;
    }

    if (type == AM275X_UAC2_DESC_STRING) {
        switch (index) {
            case 0:
                *data = gStringLangId;
                *length = (uint16_t)sizeof(gStringLangId);
                return 0;
            case 1:
                *data = gStringManufacturer;
                *length = (uint16_t)sizeof(gStringManufacturer);
                return 0;
            case 2:
                *data = gStringProduct;
                *length = (uint16_t)sizeof(gStringProduct);
                return 0;
            case 3:
                *data = gStringSerial;
                *length = (uint16_t)sizeof(gStringSerial);
                return 0;
            default:
                break;
        }
    }

    return AM275X_UAC2_NOT_SUPPORTED;
}
