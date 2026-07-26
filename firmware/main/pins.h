#pragma once

// M5Stamp S3 ピン割当
// --- SRAM ロード系 (74HCT595 チェーン) ---
#define PIN_SER_A    1   // PRG チェーン シリアルデータ
#define PIN_SER_B    2   // CHR チェーン シリアルデータ
#define PIN_SRCLK    3   // シフトクロック (両チェーン共有)
#define PIN_RCLK     4   // ラッチクロック (両チェーン共有)
#define PIN_OE_595   5   // 595 /OE (active-low, 共有)
#define PIN_WE_PRG   6   // PRG SRAM /WE
#define PIN_WE_CHR   7   // CHR SRAM /WE
#define PIN_MODE     8   // L=LOAD / H=RUN (基板プルダウン)

// --- 拡張ポート ジョイパッドエミュレーション (J2 → DA15 ケーブル) ---
#define PIN_JOY_OUT0 9   // DA15 pin12 OUT0 ストローブ (5V→分圧入力)
#define PIN_JOY_OE1  10  // DA15 pin14 /OE ($4016リード, 5V→分圧入力)
#define PIN_JOY_D1   11  // DA15 pin13 Joypad1 /D1 データ出力 (3.3V直=TTL H)

// --- PCM5102A 3線I2S DAC ---
#define PIN_I2S_BCLK 13  // PCM5102A pin13 BCK
#define PIN_I2S_LRCK 15  // PCM5102A pin15 LRCK
#define PIN_I2S_DOUT 39  // PCM5102A pin14 DIN

// --- GreenPAK書き換え用I2C ---
#define PIN_I2C_SDA  40
#define PIN_I2C_SCL  41

// --- 基板上ディスクリートLED ---
#define PIN_LED_STAT 14  // 基板上ステータスLED(赤, GPIO High=点灯)。D2/R16経由
// 電源LED(緑, D1)は+5Vに直結で常時点灯、GPIO不要

// --- 内蔵 ---
#define PIN_LED      21  // SK6812 (M5Stamp内蔵、シェルで隠れる場合の代替が基板LED)
#define PIN_BTN      0   // BOOTボタン
