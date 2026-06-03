#ifndef AM275X_USB_EP0_H_
#define AM275X_USB_EP0_H_

#include <stdbool.h>
#include <stdint.h>

#include "am275x_usb_device.h"
#include "am275x_usb_hw.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AM275X_USB_EP0_OK             (0)
#define AM275X_USB_EP0_BAD_ARGUMENT   (-1)
#define AM275X_USB_EP0_HW_ERROR       (-2)
#define AM275X_USB_EP0_NEEDS_STALL    (-3)

#define AM275X_USB_EP0_SETUP_SIZE     (8U)
#define AM275X_USB_EP0_BUFFER_SIZE    (512U)

typedef enum Am275xUsbEp0State_e {
    AM275X_USB_EP0_STATE_DISABLED = 0,
    AM275X_USB_EP0_STATE_WAIT_SETUP,
    AM275X_USB_EP0_STATE_DATA_IN,
    AM275X_USB_EP0_STATE_DATA_OUT,
    AM275X_USB_EP0_STATE_STATUS_IN,
    AM275X_USB_EP0_STATE_STATUS_OUT,
    AM275X_USB_EP0_STATE_STALLED,
} Am275xUsbEp0State;

typedef struct Am275xUsbEp0_s {
    Am275xUsbDcdTrb trb[3] __attribute__((aligned(32)));
    uint8_t setupBuffer[AM275X_USB_EP0_SETUP_SIZE] __attribute__((aligned(32)));
    uint8_t dataBuffer[AM275X_USB_EP0_BUFFER_SIZE] __attribute__((aligned(32)));
    Am275xUsbDevice *device;
    Am275xUsbDcd *dcd;
    Am275xUsbEp0State state;
    uint16_t dataLength;
    uint8_t pendingAddress;
    bool pendingAddressValid;
    bool deferredStatusOut;
    bool deferredStatusIn;
} Am275xUsbEp0;

int32_t Am275xUsbEp0_init(Am275xUsbEp0 *ep0, Am275xUsbDevice *device, Am275xUsbDcd *dcd);
int32_t Am275xUsbEp0_primeSetup(Am275xUsbEp0 *ep0);
int32_t Am275xUsbEp0_poll(Am275xUsbEp0 *ep0);
int32_t Am275xUsbEp0_processEvent(Am275xUsbEp0 *ep0, const Am275xUsbDcdEvent *event);
Am275xUsbEp0State Am275xUsbEp0_getState(const Am275xUsbEp0 *ep0);

#ifdef __cplusplus
}
#endif

#endif
