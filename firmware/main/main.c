// Famicom NROM cartridge — M5Stamp S3 ファームウェア
//
// 起動フロー:
//   1. GPIO を不活性状態で初期化 (バス衝突防止)
//   2. LittleFS マウント → autoload.txt のゲーム (無ければ game.nes) をSRAMへロード
//   3. LED緑 → ユーザーが本体RESETを押すとゲーム起動
//   4. softAP + Web UI でアップロード/ゲーム切替、拡張ポート経由パッド出力
//
// Bluetooth (BLE) コントローラ対応は Bluepad32 統合で joypad_out_set_buttons()
// に接続する予定 (docs/architecture.md 参照)。

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#include "cart_loader.h"
#include "audio.h"
#include "joypad_out.h"
#include "pins.h"
#include "web_server.h"

static const char *TAG = "main";
static led_strip_handle_t s_led;

static void set_led(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(s_led, 0, r, g, b);
    led_strip_refresh(s_led);
}

static void stat_led(bool on)
{
    gpio_set_level(PIN_LED_STAT, on ? 1 : 0);
}

void app_main(void)
{
    cart_loader_init();  // 最優先: バス不活性を確定

    // 基板上ステータスLED(赤)。ロード成否を内蔵SK6812と二重表示
    gpio_reset_pin(PIN_LED_STAT);
    gpio_set_direction(PIN_LED_STAT, GPIO_MODE_OUTPUT);
    stat_led(false);

    led_strip_config_t strip_cfg = {
        .strip_gpio_num = PIN_LED,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_cfg = { .resolution_hz = 10 * 1000 * 1000 };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led));
    set_led(32, 24, 0);  // 黄: ロード中

    esp_vfs_littlefs_conf_t fs_conf = {
        .base_path = "/littlefs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&fs_conf));

    // 前回選択ゲーム → 無ければ /game.nes
    char path[96] = "/littlefs/game.nes";
    FILE *f = fopen("/littlefs/autoload.txt", "r");
    if (f) {
        char name[64] = {0};
        if (fgets(name, sizeof(name), f)) {
            name[strcspn(name, "\r\n")] = 0;
            snprintf(path, sizeof(path), "/littlefs/%s", name);
        }
        fclose(f);
    }

    if (cart_load_game(path)) {
        set_led(0, 32, 0);  // 緑: RESET待ち
        stat_led(true);     // 基板LED点灯 = ロード完了、RESETを押す合図
    } else {
        ESP_LOGW(TAG, "no game loaded (%s)", path);
        set_led(32, 0, 0);  // 赤: 未ロード (Web UIからアップロード可)
        stat_led(false);
    }

    joypad_out_init();

    // ES8388はオプション実装。未実装・I2C応答なしでもカートリッジ機能は継続する。
    esp_err_t audio_err = audio_init();
    if (audio_err != ESP_OK) {
        ESP_LOGW(TAG, "audio disabled: %s", esp_err_to_name(audio_err));
    }

    web_server_start();

    vTaskDelete(NULL);
}
