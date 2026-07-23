// SRAM ロード & ハンドオーバー
//
// 595 チェーン構成 (各3個): [addr上位][addr下位][data] の 24bit。
// addr MSB → data LSB の順に送出し、ラッチ後 /WE をパルスする。

#include "cart_loader.h"
#include "pins.h"

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "cart_loader";

#define PRG_SIZE 32768
#define CHR_SIZE 8192

static inline void set(int pin, int v) { gpio_set_level(pin, v); }

static void write_byte(int pin_ser, int pin_we, uint32_t addr, uint8_t data)
{
    uint32_t word = ((addr & 0x7FFF) << 8) | data;
    for (int i = 23; i >= 0; --i) {
        set(pin_ser, (word >> i) & 1);
        set(PIN_SRCLK, 1);
        set(PIN_SRCLK, 0);
    }
    set(PIN_RCLK, 1);
    set(PIN_RCLK, 0);
    // AS6C62256 tWP min 45ns — gpio_set_level 2回で十分満たす
    set(pin_we, 0);
    set(pin_we, 1);
}

static bool load_region(FILE *f, long offset, size_t size, size_t sram_size,
                        int pin_ser, int pin_we)
{
    // size < sram_size ならミラー書込 (16KB PRG → 2周)
    for (size_t base = 0; base < sram_size; base += size) {
        if (fseek(f, offset, SEEK_SET) != 0) return false;
        for (size_t i = 0; i < size; ++i) {
            int b = fgetc(f);
            if (b < 0) return false;
            write_byte(pin_ser, pin_we, base + i, (uint8_t)b);
        }
    }
    return true;
}

void cart_loader_init(void)
{
    const int out_pins[] = { PIN_SER_A, PIN_SER_B, PIN_SRCLK, PIN_RCLK,
                             PIN_OE_595, PIN_WE_PRG, PIN_WE_CHR, PIN_MODE };
    // バスに触る前に不活性状態を確定: /WE=H, /OE_595=H, MODE=LOAD(L)
    for (size_t i = 0; i < sizeof(out_pins) / sizeof(out_pins[0]); ++i) {
        gpio_reset_pin(out_pins[i]);
        gpio_set_direction(out_pins[i], GPIO_MODE_OUTPUT);
    }
    set(PIN_WE_PRG, 1);
    set(PIN_WE_CHR, 1);
    set(PIN_OE_595, 1);
    set(PIN_MODE, 0);
    set(PIN_SRCLK, 0);
    set(PIN_RCLK, 0);
}

bool cart_load_game(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "open failed: %s", path);
        return false;
    }
    uint8_t header[16];
    if (fread(header, 1, 16, f) != 16 || memcmp(header, "NES\x1a", 4) != 0) {
        ESP_LOGE(TAG, "not an iNES file");
        fclose(f);
        return false;
    }
    size_t prg_size = header[4] * 16384u;
    size_t chr_size = header[5] * 8192u;
    bool has_trainer = header[6] & 0x04;
    if (prg_size == 0 || prg_size > PRG_SIZE || chr_size > CHR_SIZE) {
        ESP_LOGE(TAG, "not NROM-sized: PRG=%u CHR=%u",
                 (unsigned)prg_size, (unsigned)chr_size);
        fclose(f);
        return false;
    }
    long prg_offset = 16 + (has_trainer ? 512 : 0);

    // LOAD モード: コンソール側バッファ無効、595 出力有効
    set(PIN_MODE, 0);
    set(PIN_OE_595, 0);

    ESP_LOGI(TAG, "loading PRG %u bytes", (unsigned)prg_size);
    bool ok = load_region(f, prg_offset, prg_size, PRG_SIZE, PIN_SER_A, PIN_WE_PRG);
    if (ok && chr_size > 0) {
        ESP_LOGI(TAG, "loading CHR %u bytes", (unsigned)chr_size);
        ok = load_region(f, prg_offset + prg_size, chr_size, CHR_SIZE,
                         PIN_SER_B, PIN_WE_CHR);
    }
    fclose(f);

    // RUN モードへハンドオーバー
    set(PIN_OE_595, 1);
    if (ok) {
        set(PIN_MODE, 1);
        ESP_LOGI(TAG, "done. press console RESET.");
    }
    return ok;
}
