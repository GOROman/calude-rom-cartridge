#include "audio.h"

#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "pins.h"

static const char *TAG = "audio";

static i2s_chan_handle_t s_i2s_tx;

esp_err_t audio_init(void)
{
    esp_err_t ret = ESP_OK;

    // PT8211はBCK/WS/DINのみを使うため、MCLKとI2C初期化は不要。
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_i2s_tx, NULL),
                        TAG, "I2S channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),
        // PT8211の16-bit Japanese formatは、16-bitスロットでは
        // MSB-justified (bit_shift=false) と同じタイミングになる。
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_LRCK,
            .dout = PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                // PT8211はWS=Lで右、WS=Hで左。ESP-IDFのL/R極性を反転する。
                .ws_inv = true,
            },
        },
    };

    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_i2s_tx, &std_cfg),
                      fail_i2s, TAG, "I2S standard mode");
    ESP_GOTO_ON_ERROR(i2s_channel_enable(s_i2s_tx),
                      fail_i2s, TAG, "I2S enable");

    // DMAへ無音を1ブロック投入し、起動直後からクロックを安定させる。
    static const int16_t silence[64 * 2] = {0};
    size_t written = 0;
    ESP_GOTO_ON_ERROR(i2s_channel_write(s_i2s_tx, silence, sizeof(silence),
                                        &written, pdMS_TO_TICKS(100)),
                      fail_i2s, TAG, "initial silence");

    ESP_LOGI(TAG, "PT8211 ready: 48 kHz, 16-bit stereo, Japanese format");
    return ESP_OK;

fail_i2s:
    if (s_i2s_tx) {
        i2s_channel_disable(s_i2s_tx);
        i2s_del_channel(s_i2s_tx);
        s_i2s_tx = NULL;
    }
    return ret;
}

esp_err_t audio_write(const int16_t *samples, size_t frame_count,
                      size_t *frames_written, uint32_t timeout_ms)
{
    if (!s_i2s_tx || !samples) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(
        s_i2s_tx, samples, frame_count * 2 * sizeof(int16_t),
        &bytes_written, pdMS_TO_TICKS(timeout_ms));
    if (frames_written) {
        *frames_written = bytes_written / (2 * sizeof(int16_t));
    }
    return err;
}
