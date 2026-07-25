#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define AUDIO_SAMPLE_RATE_HZ 48000

// ES8388 と I2S TX を初期化する。ES8388未実装時はエラーを返す。
esp_err_t audio_init(void);

// 16-bit signed little-endian stereo PCMを書き込む。
esp_err_t audio_write(const int16_t *samples, size_t frame_count,
                      size_t *frames_written, uint32_t timeout_ms);

// DAC出力音量を0～100%で設定する。
esp_err_t audio_set_volume(uint8_t percent);
