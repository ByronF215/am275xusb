#include "am275x_usb_device.h"
#include "am275x_usb_hw.h"


static uint8_t gEp0Scratch[4];

volatile uint32_t gUsbDeviceLastSetup0;
volatile uint32_t gUsbDeviceLastSetup1;
volatile uint32_t gUsbDeviceAddressDebug;
volatile uint32_t gUsbDeviceConfigurationDebug;
volatile uint32_t gUsbDeviceSetAddressCount;
volatile uint32_t gUsbDeviceSetConfigurationCount;
volatile uint32_t gUsbDeviceStandardRequestCountDebug;
volatile uint32_t gUsbDeviceClassRequestCountDebug;
volatile uint32_t gUsbDevicePendingConfigurationDebug;
volatile uint32_t gUsbDeviceApplyConfigurationCount;
volatile uint32_t gUsbDeviceSetupHistory0[32];
volatile uint32_t gUsbDeviceSetupHistory1[32];
volatile uint32_t gUsbDeviceSetupHistoryIndex;

static uint16_t min_u16(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}

static void put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static int32_t ep0_tx(Am275xUsbEp0Response *response, const uint8_t *data, uint16_t length, uint16_t requestLength)
{
    response->txData = data;
    response->txLength = min_u16(length, requestLength);
    response->statusOnly = false;
    return AM275X_USB_OK;
}

static int32_t ep0_status(Am275xUsbEp0Response *response)
{
    response->txData = NULL;
    response->txLength = 0;
    response->statusOnly = true;
    return AM275X_USB_OK;
}

static int32_t stall(Am275xUsbDevice *dev)
{
    dev->stallCount++;
    return AM275X_USB_EP0_STALL;
}

int32_t Am275xUsbDevice_init(Am275xUsbDevice *dev, Am275xUsbMsc *msc)
{
    if ((dev == NULL) || (msc == NULL)) {
        return AM275X_USB_BAD_ARGUMENT;
    }

    *dev = (Am275xUsbDevice){0};
    dev->msc = msc;

    return AM275X_USB_OK;
}

void Am275xUsbDevice_attachDcd(Am275xUsbDevice *dev, struct Am275xUsbDcd_s *dcd)
{
    if (dev == NULL) {
        return;
    }

    dev->dcd = dcd;
}

static int32_t handle_get_descriptor(Am275xUsbDevice *dev,
                                     const Am275xUac2SetupPacket *setup,
                                     Am275xUsbEp0Response *response)
{
    const uint8_t *desc = NULL;
    uint16_t descLen = 0;
    Am275xUac2DescriptorType descType = (Am275xUac2DescriptorType)((setup->wValue >> 8U) & 0xFFU);
    uint8_t descIndex = (uint8_t)(setup->wValue & 0xFFU);

    if (Am275xUsbMsc_getDescriptor(descType, descIndex, &desc, &descLen) != 0) {
        return stall(dev);
    }

    return ep0_tx(response, desc, descLen, setup->wLength);
}

static int32_t handle_get_status(Am275xUsbDevice *dev,
                                 const Am275xUac2SetupPacket *setup,
                                 Am275xUsbEp0Response *response)
{
    uint8_t recipient = setup->bmRequestType & AM275X_USB_RECIPIENT_MASK;

    if ((recipient != AM275X_USB_RECIPIENT_DEVICE) &&
        (recipient != AM275X_USB_RECIPIENT_INTERFACE) &&
        (recipient != AM275X_USB_RECIPIENT_ENDPOINT)) {
        return stall(dev);
    }

    put_u16_le(gEp0Scratch, 0U);
    return ep0_tx(response, gEp0Scratch, 2U, setup->wLength);
}

static int32_t handle_get_configuration(Am275xUsbDevice *dev,
                                        const Am275xUac2SetupPacket *setup,
                                        Am275xUsbEp0Response *response)
{
    (void)setup;

    gEp0Scratch[0] = dev->configuration;
    return ep0_tx(response, gEp0Scratch, 1U, setup->wLength);
}

static int32_t handle_set_configuration(Am275xUsbDevice *dev,
                                        const Am275xUac2SetupPacket *setup,
                                        Am275xUsbEp0Response *response)
{
    uint8_t configuration = (uint8_t)(setup->wValue & 0xFFU);

    if ((setup->wLength != 0U) || (configuration > 1U)) {
        return stall(dev);
    }

    dev->pendingConfiguration = configuration;
    dev->pendingConfigurationValid = true;
    gUsbDevicePendingConfigurationDebug = configuration;
    gUsbDeviceSetConfigurationCount++;

    return ep0_status(response);
}

static int32_t handle_get_interface(Am275xUsbDevice *dev,
                                    const Am275xUac2SetupPacket *setup,
                                    Am275xUsbEp0Response *response)
{
    uint8_t interfaceNumber = (uint8_t)(setup->wIndex & 0xFFU);

    if ((dev->configuration == 0U) || (interfaceNumber != 0U)) {
        return stall(dev);
    }

    gEp0Scratch[0] = 0U;
    return ep0_tx(response, gEp0Scratch, 1U, setup->wLength);
}

static int32_t handle_set_interface(Am275xUsbDevice *dev,
                                    const Am275xUac2SetupPacket *setup,
                                    Am275xUsbEp0Response *response)
{
    uint8_t alternateSetting = (uint8_t)(setup->wValue & 0xFFU);

    if ((dev->configuration == 0U) || (setup->wLength != 0U) ||
        ((setup->wIndex & 0xFFU) != 0U) || (alternateSetting != 0U)) {
        return stall(dev);
    }

    return ep0_status(response);
}

static int32_t handle_standard_request(Am275xUsbDevice *dev,
                                       const Am275xUac2SetupPacket *setup,
                                       Am275xUsbEp0Response *response)
{
    switch (setup->bRequest) {
        case AM275X_USB_REQ_GET_STATUS:
            return handle_get_status(dev, setup, response);

        case AM275X_USB_REQ_SET_ADDRESS:
            if ((setup->wValue > 127U) || (setup->wLength != 0U)) {
                return stall(dev);
            }
            dev->address = (uint8_t)(setup->wValue & 0x7FU);
            dev->pendingAddress = 0U;
            dev->pendingAddressValid = false;
            gUsbDeviceAddressDebug = dev->address;
            gUsbDeviceSetAddressCount++;
            if (dev->dcd != NULL) {
                (void)Am275xUsbDcd_setAddress(dev->dcd, dev->address);
            }
            return ep0_status(response);

        case AM275X_USB_REQ_GET_DESCRIPTOR:
            return handle_get_descriptor(dev, setup, response);

        case AM275X_USB_REQ_GET_CONFIGURATION:
            return handle_get_configuration(dev, setup, response);

        case AM275X_USB_REQ_SET_CONFIGURATION:
            return handle_set_configuration(dev, setup, response);

        case AM275X_USB_REQ_GET_INTERFACE:
            return handle_get_interface(dev, setup, response);

        case AM275X_USB_REQ_SET_INTERFACE:
            return handle_set_interface(dev, setup, response);

        default:
            break;
    }

    return stall(dev);
}

static int32_t handle_class_request(Am275xUsbDevice *dev,
                                    const Am275xUac2SetupPacket *setup,
                                    const uint8_t *outData,
                                    uint16_t outLength,
                                    Am275xUsbEp0Response *response)
{
    const uint8_t *txData = NULL;
    uint16_t txLength = 0U;
    bool statusOnly = true;
    int32_t status;

    (void)outData;
    (void)outLength;

    status = Am275xUsbMsc_handleClassRequest(dev->msc, setup, &txData, &txLength, &statusOnly);
    if (status != AM275X_USB_MSC_OK) {
        return stall(dev);
    }

    response->txData = txData;
    response->txLength = txLength;
    response->statusOnly = statusOnly;

    return AM275X_USB_OK;
}

int32_t Am275xUsbDevice_handleSetup(Am275xUsbDevice *dev,
                                    const Am275xUac2SetupPacket *setup,
                                    const uint8_t *outData,
                                    uint16_t outLength,
                                    Am275xUsbEp0Response *response)
{
    uint8_t requestType;

    if ((dev == NULL) || (setup == NULL) || (response == NULL)) {
        return AM275X_USB_BAD_ARGUMENT;
    }

    response->txData = NULL;
    response->txLength = 0;
    response->statusOnly = false;
    dev->setupCount++;
    gUsbDeviceLastSetup0 = ((uint32_t)setup->bmRequestType) |
                           ((uint32_t)setup->bRequest << 8U) |
                           ((uint32_t)setup->wValue << 16U);
    gUsbDeviceLastSetup1 = ((uint32_t)setup->wIndex) |
                           ((uint32_t)setup->wLength << 16U);
    gUsbDeviceSetupHistory0[gUsbDeviceSetupHistoryIndex & 31U] = gUsbDeviceLastSetup0;
    gUsbDeviceSetupHistory1[gUsbDeviceSetupHistoryIndex & 31U] = gUsbDeviceLastSetup1;
    gUsbDeviceSetupHistoryIndex++;

    requestType = setup->bmRequestType & AM275X_USB_REQ_TYPE_MASK;
    if (requestType == AM275X_USB_REQ_TYPE_STANDARD) {
        dev->standardRequestCount++;
        gUsbDeviceStandardRequestCountDebug = dev->standardRequestCount;
        return handle_standard_request(dev, setup, response);
    }

    if (requestType == AM275X_USB_REQ_TYPE_CLASS) {
        dev->classRequestCount++;
        gUsbDeviceClassRequestCountDebug = dev->classRequestCount;
        return handle_class_request(dev, setup, outData, outLength, response);
    }

    return stall(dev);
}

void Am275xUsbDevice_statusStageComplete(Am275xUsbDevice *dev)
{
    if (dev == NULL) {
        return;
    }

    if (dev->pendingAddressValid) {
        dev->address = dev->pendingAddress;
        dev->pendingAddress = 0U;
        dev->pendingAddressValid = false;
        gUsbDeviceAddressDebug = dev->address;
        if (dev->dcd != NULL) {
            (void)Am275xUsbDcd_setAddress(dev->dcd, dev->address);
        }
    }

    if (dev->pendingConfigurationValid) {
        dev->configuration = dev->pendingConfiguration;
        dev->pendingConfiguration = 0U;
        dev->pendingConfigurationValid = false;
        gUsbDeviceConfigurationDebug = dev->configuration;
        gUsbDeviceApplyConfigurationCount++;
        if (dev->msc != NULL) {
            (void)Am275xUsbMsc_setConfigured(dev->msc, (dev->configuration != 0U));
        }
    }
}

void Am275xUsbDevice_processDcdEvent(Am275xUsbDevice *dev, const Am275xUsbDcdEvent *event)
{
    if ((dev == NULL) || (event == NULL)) {
        return;
    }

    if (event->kind != AM275X_USB_DCD_EVENT_DEVICE) {
        return;
    }

    switch ((Am275xUsbDcdDeviceEvent)event->eventType) {
        case AM275X_USB_DCD_DEVICE_EVENT_RESET:
            Am275xUsbDevice_busReset(dev);
            break;

        case AM275X_USB_DCD_DEVICE_EVENT_DISCONNECT:
            break;

        case AM275X_USB_DCD_DEVICE_EVENT_WAKEUP:
        case AM275X_USB_DCD_DEVICE_EVENT_CONNECT_DONE:
            break;

        case AM275X_USB_DCD_DEVICE_EVENT_LINK_STATE_CHANGE:
            break;

        default:
            break;
    }
}

void Am275xUsbDevice_busReset(Am275xUsbDevice *dev)
{
    if (dev == NULL) {
        return;
    }

    dev->address = 0U;
    dev->pendingAddress = 0U;
    dev->pendingAddressValid = false;
    dev->pendingConfiguration = 0U;
    dev->pendingConfigurationValid = false;
    dev->configuration = 0U;
    if (dev->dcd != NULL) {
        (void)Am275xUsbDcd_setAddress(dev->dcd, 0U);
    }
    if (dev->msc != NULL) {
        Am275xUsbMsc_busReset(dev->msc);
    }
}
