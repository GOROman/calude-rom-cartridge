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

// --- 内蔵 ---
#define PIN_LED      21  // SK6812
#define PIN_BTN      0   // BOOTボタン
