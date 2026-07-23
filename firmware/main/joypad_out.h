#pragma once
#include <stdint.h>

// 標準コントローラのボタンビット (シフト順: A,B,Select,Start,Up,Down,Left,Right)
#define JOY_A      0x01
#define JOY_B      0x02
#define JOY_SELECT 0x04
#define JOY_START  0x08
#define JOY_UP     0x10
#define JOY_DOWN   0x20
#define JOY_LEFT   0x40
#define JOY_RIGHT  0x80

// 拡張ポート(DA15)経由の拡張コントローラ ($4016 D1) エミュレーションを開始する。
// OUT0 ストローブでラッチ、$4016 リード(/OE)の立上りで次ビットへシフト。
void joypad_out_init(void);

// 現在のボタン状態をセットする (BLEコントローラ等の入力元から呼ぶ)
void joypad_out_set_buttons(uint8_t buttons);
