#include "audio.h"

#include <stdbool.h>
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "pins.h"

static const char *TAG = "audio";

// ES8388の資料に記載される0x20はR/Wビット込みの8-bit表記。
static const uint8_t ES8388_I2C_ADDR = 0x10;
static const i2c_port_t ES8388_I2C_PORT = I2C_NUM_0;
static i2s_chan_handle_t s_i2s_tx;
static bool s_i2c_installed;

typedef struct {
    uint8_t reg;
    uint8_t value;
} codec_reg_t;

static esp_err_t codec_write(uint8_t reg, uint8_t value)
{
    const uint8_t data[2] = {reg, value};
    return i2c_master_write_to_device(ES8388_I2C_PORT, ES8388_I2C_ADDR,
                                      data, sizeof(data), pdMS_TO_TICKS(100));
}

static esp_err_t codec_init(void)
{
    // DAC playback、I2S slave、16-bit Philips形式、LOUT1/ROUT1有効。
    // Everestの推奨初期化手順およびEspressif ES8388ドライバに準拠。
    static const codec_reg_t init[] = {
        {0x00, 0x80}, {0x00, 0x00}, // control reset
        {0x01, 0x58},               // enable analog reference
        {0x02, 0xF3}, {0x02, 0x00}, // power cycle analog blocks
        {0x03, 0x09},               // ADC off, DAC enabled
        {0x04, 0x10},               // DAC output power
        {0x05, 0x00}, {0x06, 0x00},
        {0x07, 0x7C},               // master clock management
        {0x08, 0x00},               // codec I2S slave mode
        {0x09, 0x00}, {0x0A, 0x00},
        {0x0B, 0x02}, {0x0C, 0x0C}, {0x0D, 0x02},
        {0x10, 0x00}, {0x11, 0x00},
        {0x12, 0x30}, {0x13, 0x30},
        {0x17, 0x18},               // DAC: I2S, 16 bit
        {0x18, 0x02},               // 48 kHz clock ratio
        {0x1A, 0x00}, {0x1B, 0x00}, // DAC digital volume 0 dB
        {0x26, 0x00},
        {0x27, 0xB8}, {0x2A, 0xB8}, // route DAC to output mixers
        {0x2B, 0x80},               // enable DAC output state machine
        {0x2D, 0x00},
        {0x2E, 0x1E}, {0x2F, 0x1E}, // LOUT1/ROUT1 volume
        {0x30, 0x1E}, {0x31, 0x1E},
    };

    for (size_t i = 0; i < sizeof(init) / sizeof(init[0]); ++i) {
        esp_err_t err = codec_write(init[i].reg, init[i].value);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "ES8388 reg 0x%02x write failed: %s",
                     init[i].reg, esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t audio_set_volume(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }

    // ES8388 output volume: 0=mute付近、0x1E=0 dB。
    const uint8_t value = (uint8_t)((percent * 0x1E + 50) / 100);
    ESP_RETURN_ON_ERROR(codec_write(0x2E, value), TAG, "LOUT1 volume");
    return codec_write(0x2F, value);
}

esp_err_t audio_init(void)
{
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
        .clk_flags = 0,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(ES8388_I2C_PORT, &i2c_cfg),
                        TAG, "I2C config");
    esp_err_t ret = i2c_driver_install(ES8388_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    s_i2c_installed = true;

    // I2S master: 48 kHz × 256 = 12.288 MHz MCLK。
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_RETURN_ON_ERROR(i2s_new_channel(&chan_cfg, &s_i2s_tx, NULL),
                        TAG, "I2S channel");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_I2S_MCLK,
            .bclk = PIN_I2S_BCLK,
            .ws = PIN_I2S_LRCK,
            .dout = PIN_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(s_i2s_tx, &std_cfg),
                      fail_i2s, TAG, "I2S standard mode");
    ESP_GOTO_ON_ERROR(i2s_channel_enable(s_i2s_tx),
                      fail_i2s, TAG, "I2S enable");

    ret = codec_init();
    if (ret != ESP_OK) {
        goto fail_i2s;
    }

    // DMAへ無音を1ブロック投入し、起動直後からクロックを安定させる。
    static const int16_t silence[64 * 2] = {0};
    size_t written = 0;
    ESP_GOTO_ON_ERROR(i2s_channel_write(s_i2s_tx, silence, sizeof(silence),
                                        &written, pdMS_TO_TICKS(100)),
                      fail_i2s, TAG, "initial silence");

    ESP_LOGI(TAG, "ES8388 ready: 48 kHz, 16-bit stereo, MCLK 12.288 MHz");
    return ESP_OK;

fail_i2s:
    if (s_i2s_tx) {
        i2s_channel_disable(s_i2s_tx);
        i2s_del_channel(s_i2s_tx);
        s_i2s_tx = NULL;
    }
    if (s_i2c_installed) {
        i2c_driver_delete(ES8388_I2C_PORT);
        s_i2c_installed = false;
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
