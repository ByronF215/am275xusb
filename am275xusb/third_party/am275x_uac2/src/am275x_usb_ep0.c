#include "am275x_usb_ep0.h"

#include <string.h>
#include <kernel/dpl/CacheP.h>

volatile uint32_t gUsbEp0LastStartEp;
volatile uint32_t gUsbEp0LastStartTrb;
volatile uint32_t gUsbEp0LastStartStatus;
volatile uint32_t gUsbEp0StartErrorCount;
volatile uint32_t gUsbEp0LastDataInLength;
volatile uint32_t gUsbEp0EventHistory[32];
volatile uint32_t gUsbEp0StateHistory[32];
volatile uint32_t gUsbEp0EventHistoryIndex;
volatile uint32_t gUsbEp0SetupCompleteCount;
volatile uint32_t gUsbEp0DataInCompleteCount;
volatile uint32_t gUsbEp0DataInRetryCount;
volatile uint32_t gUsbEp0StatusOutQueuedCount;
volatile uint32_t gUsbEp0StatusOutCompleteCount;
volatile uint32_t gUsbEp0StatusInCompleteCount;

static uint16_t get_u16_le(const uint8_t *src)
{
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8U);
}

static void decode_setup(const uint8_t *src, Am275xUac2SetupPacket *setup)
{
    setup->bmRequestType = src[0];
    setup->bRequest = src[1];
    setup->wValue = get_u16_le(&src[2]);
    setup->wIndex = get_u16_le(&src[4]);
    setup->wLength = get_u16_le(&src[6]);
}

static int32_t ep0_start(Am275xUsbEp0 *ep0, uint32_t endpointIndex, uint32_t trbIndex)
{
    uint32_t commandStatus = 0U;
    int32_t status;

    status = Am275xUsbDcd_startTransfer(ep0->dcd,
                                        endpointIndex,
                                        &ep0->trb[trbIndex],
                                        0U,
                                        &commandStatus);
    gUsbEp0LastStartEp = endpointIndex;
    gUsbEp0LastStartTrb = trbIndex;
    gUsbEp0LastStartStatus = commandStatus;
    if ((status != AM275X_USB_HW_OK) || (commandStatus != 0U)) {
        gUsbEp0StartErrorCount++;
        return AM275X_USB_EP0_HW_ERROR;
    }
    return AM275X_USB_EP0_OK;
}

static int32_t queue_status_in(Am275xUsbEp0 *ep0, uint32_t trbControl)
{
    Am275xUsbDcd_prepareTrb(&ep0->trb[2],
                            NULL,
                            0U,
                            trbControl,
                            true);
    ep0->state = AM275X_USB_EP0_STATE_STATUS_IN;

    return ep0_start(ep0, AM275X_USB_DCD_EP0_IN_INDEX, 2U);
}

static int32_t queue_status_out(Am275xUsbEp0 *ep0, uint32_t trbControl)
{
    Am275xUsbDcd_prepareTrb(&ep0->trb[2],
                            NULL,
                            0U,
                            trbControl,
                            true);
    ep0->state = AM275X_USB_EP0_STATE_STATUS_OUT;

    return ep0_start(ep0, AM275X_USB_DCD_EP0_OUT_INDEX, 2U);
}

static int32_t queue_data_in(Am275xUsbEp0 *ep0, const Am275xUsbEp0Response *response)
{
    ep0->dataLength = response->txLength;
    gUsbEp0LastDataInLength = ep0->dataLength;
    if (ep0->dataLength > AM275X_USB_EP0_BUFFER_SIZE) {
        return AM275X_USB_EP0_NEEDS_STALL;
    }

    if ((response->txData != NULL) && (ep0->dataLength > 0U)) {
        (void)memcpy(ep0->dataBuffer, response->txData, ep0->dataLength);
    }

    Am275xUsbDcd_prepareTrb(&ep0->trb[1],
                            ep0->dataBuffer,
                            ep0->dataLength,
                            AM275X_USB_DCD_TRBCTL_CONTROL_DATA,
                            true);
    ep0->state = AM275X_USB_EP0_STATE_DATA_IN;

    return ep0_start(ep0, AM275X_USB_DCD_EP0_IN_INDEX, 1U);
}

static int32_t handle_setup_complete(Am275xUsbEp0 *ep0)
{
    Am275xUac2SetupPacket setup;
    Am275xUsbEp0Response response;
    int32_t status;

    CacheP_inv(ep0->setupBuffer, AM275X_USB_EP0_SETUP_SIZE, CacheP_TYPE_ALLD);
    decode_setup(ep0->setupBuffer, &setup);
    if ((setup.bmRequestType == 0U) &&
        (setup.bRequest == 0U) &&
        (setup.wValue == 0U) &&
        (setup.wIndex == 0U) &&
        (setup.wLength == 0U)) {
        return Am275xUsbEp0_primeSetup(ep0);
    }
    status = Am275xUsbDevice_handleSetup(ep0->device, &setup, NULL, 0U, &response);
    if (status != AM275X_USB_OK) {
        (void)Am275xUsbDcd_stallEndpoint(ep0->dcd, AM275X_USB_DCD_EP0_OUT_INDEX);
        (void)Am275xUsbDcd_stallEndpoint(ep0->dcd, AM275X_USB_DCD_EP0_IN_INDEX);
        ep0->state = AM275X_USB_EP0_STATE_STALLED;
        return AM275X_USB_EP0_NEEDS_STALL;
    }

    if ((setup.bmRequestType & 0x80U) != 0U) {
        if (response.txLength > 0U) {
            return queue_data_in(ep0, &response);
        }

        ep0->state = AM275X_USB_EP0_STATE_STATUS_OUT;
        ep0->deferredStatusOut = true;
        return AM275X_USB_EP0_OK;
    }

    ep0->state = AM275X_USB_EP0_STATE_STATUS_IN;
    ep0->deferredStatusIn = true;
    return AM275X_USB_EP0_OK;
}

int32_t Am275xUsbEp0_init(Am275xUsbEp0 *ep0, Am275xUsbDevice *device, Am275xUsbDcd *dcd)
{
    if ((ep0 == NULL) || (device == NULL) || (dcd == NULL)) {
        return AM275X_USB_EP0_BAD_ARGUMENT;
    }

    *ep0 = (Am275xUsbEp0){0};
    ep0->device = device;
    ep0->dcd = dcd;
    ep0->state = AM275X_USB_EP0_STATE_DISABLED;

    return AM275X_USB_EP0_OK;
}

int32_t Am275xUsbEp0_primeSetup(Am275xUsbEp0 *ep0)
{
    if ((ep0 == NULL) || (ep0->dcd == NULL)) {
        return AM275X_USB_EP0_BAD_ARGUMENT;
    }

    Am275xUsbDcd_prepareTrb(&ep0->trb[0],
                            ep0->setupBuffer,
                            AM275X_USB_EP0_SETUP_SIZE,
                            AM275X_USB_DCD_TRBCTL_CONTROL_SETUP,
                            true);
    ep0->state = AM275X_USB_EP0_STATE_WAIT_SETUP;

    return ep0_start(ep0, AM275X_USB_DCD_EP0_OUT_INDEX, 0U);
}

int32_t Am275xUsbEp0_poll(Am275xUsbEp0 *ep0)
{
    if ((ep0 == NULL) || (ep0->dcd == NULL)) {
        return AM275X_USB_EP0_BAD_ARGUMENT;
    }

    return AM275X_USB_EP0_OK;
}

int32_t Am275xUsbEp0_processEvent(Am275xUsbEp0 *ep0, const Am275xUsbDcdEvent *event)
{
    if ((ep0 == NULL) || (event == NULL)) {
        return AM275X_USB_EP0_BAD_ARGUMENT;
    }

    if ((event->kind != AM275X_USB_DCD_EVENT_ENDPOINT) ||
        ((event->eventType != AM275X_USB_DCD_ENDPOINT_EVENT_XFER_COMPLETE) &&
         (event->eventType != AM275X_USB_DCD_ENDPOINT_EVENT_XFER_NOT_READY))) {
        return AM275X_USB_EP0_OK;
    }

    gUsbEp0EventHistory[gUsbEp0EventHistoryIndex & 31U] = event->raw;
    gUsbEp0StateHistory[gUsbEp0EventHistoryIndex & 31U] =
        ((uint32_t)ep0->state << 24U) |
        ((uint32_t)event->endpointNumber << 16U) |
        ((uint32_t)event->eventType << 8U) |
        (uint32_t)event->status;
    gUsbEp0EventHistoryIndex++;

    if ((ep0->state == AM275X_USB_EP0_STATE_STATUS_OUT) &&
        ep0->deferredStatusOut &&
        (event->endpointNumber == AM275X_USB_DCD_EP0_OUT_INDEX) &&
        (event->eventType == AM275X_USB_DCD_ENDPOINT_EVENT_XFER_NOT_READY)) {
        ep0->deferredStatusOut = false;
        gUsbEp0StatusOutQueuedCount++;
        return queue_status_out(ep0, AM275X_USB_DCD_TRBCTL_CONTROL_STATUS3);
    }

    if ((ep0->state == AM275X_USB_EP0_STATE_STATUS_IN) &&
        ep0->deferredStatusIn &&
        (event->endpointNumber == AM275X_USB_DCD_EP0_IN_INDEX) &&
        (event->eventType == AM275X_USB_DCD_ENDPOINT_EVENT_XFER_NOT_READY)) {
        ep0->deferredStatusIn = false;
        return queue_status_in(ep0, AM275X_USB_DCD_TRBCTL_CONTROL_STATUS2);
    }

    if (event->eventType != AM275X_USB_DCD_ENDPOINT_EVENT_XFER_COMPLETE) {
        return AM275X_USB_EP0_OK;
    }

    if ((ep0->state == AM275X_USB_EP0_STATE_WAIT_SETUP) &&
        (event->endpointNumber == AM275X_USB_DCD_EP0_OUT_INDEX)) {
        gUsbEp0SetupCompleteCount++;
        return handle_setup_complete(ep0);
    }

    if ((ep0->state == AM275X_USB_EP0_STATE_DATA_IN) &&
        (event->endpointNumber == AM275X_USB_DCD_EP0_IN_INDEX)) {
        gUsbEp0DataInCompleteCount++;
        ep0->state = AM275X_USB_EP0_STATE_STATUS_OUT;
        ep0->deferredStatusOut = true;
        return AM275X_USB_EP0_OK;
    }

    if ((ep0->state == AM275X_USB_EP0_STATE_STATUS_OUT) ||
        (ep0->state == AM275X_USB_EP0_STATE_STATUS_IN)) {
        if (ep0->state == AM275X_USB_EP0_STATE_STATUS_OUT) {
            gUsbEp0StatusOutCompleteCount++;
        } else {
            gUsbEp0StatusInCompleteCount++;
        }
        Am275xUsbDevice_statusStageComplete(ep0->device);
        return Am275xUsbEp0_primeSetup(ep0);
    }

    return AM275X_USB_EP0_OK;
}

Am275xUsbEp0State Am275xUsbEp0_getState(const Am275xUsbEp0 *ep0)
{
    if (ep0 == NULL) {
        return AM275X_USB_EP0_STATE_DISABLED;
    }

    return ep0->state;
}
