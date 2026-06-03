#include "am275x_uac2.h"

#define UAC2_REQ_CUR                  (0x01U)
#define UAC2_REQ_RANGE                (0x02U)

#define UAC2_ID_FU_CAPTURE            (0x05U)
#define UAC2_ID_CLOCK_MAIN            (0x10U)

#define UAC2_CS_SAM_FREQ_CONTROL      (0x01U)
#define UAC2_CS_CLOCK_VALID_CONTROL   (0x02U)

#define UAC2_FU_MUTE_CONTROL          (0x01U)
#define UAC2_FU_VOLUME_CONTROL        (0x02U)

#define UAC2_VOLUME_MIN_Q8_8          ((int16_t)-0x4000)
#define UAC2_VOLUME_MAX_Q8_8          ((int16_t)0x0000)
#define UAC2_VOLUME_RES_Q8_8          ((int16_t)0x0100)

static uint8_t gCtrlResponse[16];

static uint8_t hi8(uint16_t value)
{
    return (uint8_t)((value >> 8U) & 0xFFU);
}

static uint8_t lo8(uint16_t value)
{
    return (uint8_t)(value & 0xFFU);
}

static void put_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void put_u32_le(uint8_t *dst, uint32_t value)
{
    put_u16_le(&dst[0], (uint16_t)(value & 0xFFFFU));
    put_u16_le(&dst[2], (uint16_t)((value >> 16U) & 0xFFFFU));
}

static int16_t get_s16_le(const uint8_t *src)
{
    return (int16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8U));
}

static uint16_t clamp_length(uint16_t wanted, uint16_t maxLen)
{
    return (wanted < maxLen) ? wanted : maxLen;
}

void Am275xUac2_getDefaultConfig(Am275xUac2Config *config)
{
    if (config == NULL) {
        return;
    }

    config->mode = AM275X_UAC2_MODE_CAPTURE_ONLY;
    config->format = AM275X_UAC2_FORMAT_PCM_S16;
    config->sampleRateHz = 48000U;
    config->channels = 2U;
    config->vendorId = 0x0451U;
    config->productId = 0xA275U;
    config->deviceBcd = 0x0100U;
}

int32_t Am275xUac2_init(Am275xUac2Context *ctx, const Am275xUac2Config *config)
{
    Am275xUac2Config defaultConfig;

    if (ctx == NULL) {
        return AM275X_UAC2_BAD_ARGUMENT;
    }

    if (config == NULL) {
        Am275xUac2_getDefaultConfig(&defaultConfig);
        config = &defaultConfig;
    }

    if ((config->mode != AM275X_UAC2_MODE_CAPTURE_ONLY) ||
        (config->format != AM275X_UAC2_FORMAT_PCM_S16) ||
        (config->sampleRateHz != 48000U) ||
        (config->channels != 2U)) {
        return AM275X_UAC2_NOT_SUPPORTED;
    }

    *ctx = (Am275xUac2Context){0};
    ctx->config = *config;
    ctx->configured = false;
    ctx->captureStreaming = false;
    ctx->clockValid = true;
    ctx->captureMute = 0U;
    ctx->captureVolumeQ8_8 = 0;

    return 0;
}

static int32_t tx_data(Am275xUac2ControlTransfer *transfer, uint16_t length)
{
    if (transfer == NULL) {
        return AM275X_UAC2_BAD_ARGUMENT;
    }

    transfer->txData = gCtrlResponse;
    transfer->txLength = clamp_length(length, transfer->txLength);
    return 0;
}

static int32_t handle_clock_request(Am275xUac2Context *ctx,
                                    uint8_t request,
                                    uint8_t controlSelector,
                                    bool deviceToHost,
                                    Am275xUac2ControlTransfer *transfer)
{
    if (controlSelector == UAC2_CS_SAM_FREQ_CONTROL) {
        if ((request == UAC2_REQ_CUR) && deviceToHost) {
            put_u32_le(gCtrlResponse, ctx->config.sampleRateHz);
            return tx_data(transfer, 4U);
        }

        if ((request == UAC2_REQ_RANGE) && deviceToHost) {
            put_u16_le(&gCtrlResponse[0], 1U);
            put_u32_le(&gCtrlResponse[2], ctx->config.sampleRateHz);
            put_u32_le(&gCtrlResponse[6], ctx->config.sampleRateHz);
            put_u32_le(&gCtrlResponse[10], 0U);
            return tx_data(transfer, 14U);
        }
    }

    if ((controlSelector == UAC2_CS_CLOCK_VALID_CONTROL) && (request == UAC2_REQ_CUR) && deviceToHost) {
        gCtrlResponse[0] = ctx->clockValid ? 1U : 0U;
        return tx_data(transfer, 1U);
    }

    ctx->stats.unsupportedRequestCount++;
    return AM275X_UAC2_EP0_STALL;
}

static int32_t handle_feature_unit_request(Am275xUac2Context *ctx,
                                           uint8_t request,
                                           uint8_t controlSelector,
                                           uint8_t channelNumber,
                                           bool deviceToHost,
                                           Am275xUac2ControlTransfer *transfer)
{
    if (channelNumber != 0U) {
        ctx->stats.unsupportedRequestCount++;
        return AM275X_UAC2_EP0_STALL;
    }

    if (controlSelector == UAC2_FU_MUTE_CONTROL) {
        if ((request == UAC2_REQ_CUR) && deviceToHost) {
            gCtrlResponse[0] = ctx->captureMute;
            return tx_data(transfer, 1U);
        }

        if ((request == UAC2_REQ_CUR) &&
            !deviceToHost &&
            (transfer != NULL) &&
            (transfer->rxData != NULL) &&
            (transfer->rxLength >= 1U)) {
            ctx->captureMute = transfer->rxData[0] ? 1U : 0U;
            return 0;
        }
    }

    if (controlSelector == UAC2_FU_VOLUME_CONTROL) {
        if ((request == UAC2_REQ_CUR) && deviceToHost) {
            put_u16_le(gCtrlResponse, (uint16_t)ctx->captureVolumeQ8_8);
            return tx_data(transfer, 2U);
        }

        if ((request == UAC2_REQ_RANGE) && deviceToHost) {
            put_u16_le(&gCtrlResponse[0], 1U);
            put_u16_le(&gCtrlResponse[2], (uint16_t)UAC2_VOLUME_MIN_Q8_8);
            put_u16_le(&gCtrlResponse[4], (uint16_t)UAC2_VOLUME_MAX_Q8_8);
            put_u16_le(&gCtrlResponse[6], (uint16_t)UAC2_VOLUME_RES_Q8_8);
            return tx_data(transfer, 8U);
        }

        if ((request == UAC2_REQ_CUR) &&
            !deviceToHost &&
            (transfer != NULL) &&
            (transfer->rxData != NULL) &&
            (transfer->rxLength >= 2U)) {
            int16_t volume = get_s16_le(transfer->rxData);
            if (volume < UAC2_VOLUME_MIN_Q8_8) {
                volume = UAC2_VOLUME_MIN_Q8_8;
            }
            if (volume > UAC2_VOLUME_MAX_Q8_8) {
                volume = UAC2_VOLUME_MAX_Q8_8;
            }
            ctx->captureVolumeQ8_8 = volume;
            return 0;
        }
    }

    ctx->stats.controlErrorCount++;
    return AM275X_UAC2_EP0_STALL;
}

int32_t Am275xUac2_handleClassRequest(Am275xUac2Context *ctx,
                                      const Am275xUac2SetupPacket *setup,
                                      Am275xUac2ControlTransfer *transfer)
{
    uint8_t entityId;
    uint8_t controlSelector;
    uint8_t channelNumber;
    bool deviceToHost;

    if ((ctx == NULL) || (setup == NULL) || (transfer == NULL)) {
        return AM275X_UAC2_BAD_ARGUMENT;
    }

    if (transfer->txLength == 0U) {
        transfer->txLength = setup->wLength;
    }

    entityId = hi8(setup->wIndex);
    controlSelector = hi8(setup->wValue);
    channelNumber = lo8(setup->wValue);
    deviceToHost = ((setup->bmRequestType & 0x80U) != 0U);

    if (entityId == UAC2_ID_CLOCK_MAIN) {
        return handle_clock_request(ctx, setup->bRequest, controlSelector, deviceToHost, transfer);
    }

    if (entityId == UAC2_ID_FU_CAPTURE) {
        return handle_feature_unit_request(ctx,
                                           setup->bRequest,
                                           controlSelector,
                                           channelNumber,
                                           deviceToHost,
                                           transfer);
    }

    ctx->stats.unsupportedRequestCount++;
    return AM275X_UAC2_EP0_STALL;
}

int32_t Am275xUac2_setInterface(Am275xUac2Context *ctx, uint8_t interfaceNumber, uint8_t alternateSetting)
{
    if (ctx == NULL) {
        return AM275X_UAC2_BAD_ARGUMENT;
    }

    if (interfaceNumber != AM275X_UAC2_IF_CAPTURE_STREAM) {
        ctx->stats.unsupportedRequestCount++;
        return AM275X_UAC2_EP0_STALL;
    }

    if (alternateSetting == AM275X_UAC2_ALT_ZERO_BW) {
        ctx->captureStreaming = false;
        ctx->stats.setIfAlt0Count++;
        return 0;
    }

    if (alternateSetting == AM275X_UAC2_ALT_CAP_48K_S16) {
        ctx->captureStreaming = true;
        ctx->stats.setIfAlt1Count++;
        return 0;
    }

    ctx->stats.unsupportedRequestCount++;
    return AM275X_UAC2_EP0_STALL;
}

void Am275xUac2_onUsbReset(Am275xUac2Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->configured = false;
    ctx->captureStreaming = false;
    ctx->stats.usbResetCount++;
}

void Am275xUac2_onSuspend(Am275xUac2Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->captureStreaming = false;
    ctx->stats.suspendCount++;
}

void Am275xUac2_onResume(Am275xUac2Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->stats.resumeCount++;
}

void Am275xUac2_periodic1ms(Am275xUac2Context *ctx)
{
    (void)ctx;
}

const Am275xUac2Stats *Am275xUac2_getStats(const Am275xUac2Context *ctx)
{
    if (ctx == NULL) {
        return NULL;
    }

    return &ctx->stats;
}
