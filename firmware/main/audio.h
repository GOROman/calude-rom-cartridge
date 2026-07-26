#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define AUDIO_SAMPLE_RATE_HZ 48000

// PCM5102A向け3線I2S TXを初期化する。
esp_err_t audio_init(void);

// 16-bit signed little-endian stereo PCMを書き込む。
esp_err_t audio_write(const int16_t *samples, size_t frame_count,
                      size_t *frames_written, uint32_t timeout_ms);
