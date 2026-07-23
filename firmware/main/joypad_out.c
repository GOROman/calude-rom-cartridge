// ファミコン拡張ポート (DA15) の拡張コントローラエミュレーション
//
// 4021 シフトレジスタ相当の振る舞いを GPIO 割り込みで再現する:
//   - OUT0 (DA15 pin12) H の間はボタン状態を常時ラッチ、最初のビット(A)を出力
//   - $4016 リード = /OE (DA15 pin14) の立上りで次ビットへシフト
//   - データは Joypad1 /D1 (DA15 pin13) へ出力。物理コントローラ(D0)とは独立
//
// 注意: 本体は /OE アサート時点のデータを読むため、シフトは必ずリード完了後
// (立上りエッジ) に行う。次のリードまでは最短でも数µs あり、IRAM ISR の
// 応答で間に合う。取りこぼしが出る場合は 74HC165 実装に切替える (docs参照)。

#include "joypad_out.h"
#include "pins.h"

#include "driver/gpio.h"
#include "esp_attr.h"

static volatile uint8_t s_buttons;   // 論理1=押下
static volatile uint8_t s_shift;     // 出力中のシフトレジスタ
static volatile int s_bit;

static void IRAM_ATTR isr_out0(void *arg)
{
    if (gpio_get_level(PIN_JOY_OUT0)) {
        // ストローブH: ラッチし直し、先頭ビット(A)を提示
        s_shift = s_buttons;
        s_bit = 0;
        gpio_set_level(PIN_JOY_D1, s_shift & 1);
    }
}

static void IRAM_ATTR isr_oe1(void *arg)
{
    // $4016 リード完了 → 次ビットへ (9回目以降は 1 を返すのが実機挙動)
    if (s_bit < 7) {
        s_bit++;
        gpio_set_level(PIN_JOY_D1, (s_shift >> s_bit) & 1);
    } else {
        gpio_set_level(PIN_JOY_D1, 1);
    }
}

void joypad_out_init(void)
{
    gpio_config_t out = {
        .pin_bit_mask = 1ULL << PIN_JOY_D1,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out);
    gpio_set_level(PIN_JOY_D1, 0);

    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_JOY_OUT0) | (1ULL << PIN_JOY_OE1),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    gpio_config(&in);

    gpio_install_isr_service(0);
    gpio_set_intr_type(PIN_JOY_OUT0, GPIO_INTR_POSEDGE);
    gpio_set_intr_type(PIN_JOY_OE1, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(PIN_JOY_OUT0, isr_out0, NULL);
    gpio_isr_handler_add(PIN_JOY_OE1, isr_oe1, NULL);
}

void joypad_out_set_buttons(uint8_t buttons)
{
    s_buttons = buttons;
}
