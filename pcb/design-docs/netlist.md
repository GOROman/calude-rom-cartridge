# ネットリスト設計 (回路図作成の元データ)

全体は5Vロジックドメイン。ESP32(3.3V)出力はHCT入力(VIH=2.0V)に直結。

## 制御ネット

| ネット | 生成元 | プル | 意味 |
|---|---|---|---|
| MODE | ESP32 GPIO8 | 10k PD | H=RUN / L=LOAD (電源投入直後は必ずLOAD) |
| MODE_N | U16 74HCT04 (MODEを反転) | — | 全541/245の/OE系イネーブル (L=RUN有効) |
| /OE_595 | ESP32 GPIO5 | 10k PU | 595出力イネーブル (起動前は必ず無効) |
| /WE_PRG | ESP32 GPIO6 | 10k PU | PRG SRAM書込パルス |
| /WE_CHR | ESP32 GPIO7 | 10k PU | CHR SRAM書込パルス |

## ゲーティング (U15 74HCT32 + U16 74HCT04)

| ネット | 論理 | 用途 |
|---|---|---|
| nRW | NOT R/W_BUF (U16) | リード判定 |
| /OE_PRG | /ROMSEL_BUF OR nRW | PRG SRAM /OE と U13(245) /OE |
| /OE_C245 | PA13_BUF OR /RD_BUF | CHR側 U14(245) /OE |
| /CE_PRG | = /ROMSEL_BUF | PRG SRAM /CE |
| /CE_CHR | = PA13_BUF | CHR SRAM /CE (パターンテーブルはA13=0) |
| /OE_CHRSRAM | = /RD_BUF | CHR SRAM /OE |

LOAD中(バッファhi-Z)のバッファ出力ネットのプル設定:

- /ROMSEL_BUF: 10k PD → LOAD中 /CE_PRG=L(書込可能) ✔
- R/W_BUF: 10k PD → nRW=H → /OE_PRG=H(SRAM出力オフ) ✔
- PA13_BUF: 10k PD → /CE_CHR=L(書込可能) ✔
- /RD_BUF: 10k PU → CHR出力オフ ✔

## PRG側 (CPUバス)

- U9 74HCT541: エッジ A0-A7 → PRG_A0-A7。/OE1=/OE2=MODE_N
- U10 74HCT541: エッジ A8-A14, R/W, /ROMSEL → PRG_A8-A14, R/W_BUF, /ROMSEL_BUF。/OE=MODE_N
- U13 74HCT245: B側=PRG_D0-D7、A側=エッジ D0-D7。DIR=B→A固定、/OE=/OE_PRG
- U1 M68AF127B: A0-A14=PRG_A*、A15/A16=GND、DQ0-DQ7=PRG_D*、E1=/CE_PRG、E2=+5V、G=/OE_PRG、W=/WE_PRG

## CHR側 (PPUバス)

- U11 74HCT541: エッジ PA0-PA7 → CHR_A0-A7。/OE=MODE_N
- U12 74HCT541: エッジ PA8-PA12, PA13, /RD → CHR_A8-A12, PA13_BUF, /RD_BUF。/OE=MODE_N
- U14 74HCT245: B側=CHR_D0-D7、A側=エッジ PD0-PD7。DIR=B→A固定、/OE=/OE_C245
- U2 M68AF127B: A0-A14=CHR_A*、A15/A16=GND、DQ0-DQ7=CHR_D*、E1=/CE_CHR、E2=+5V、G=/OE_CHRSRAM、W=/WE_CHR
- J3 DEBUG 60PIN: J1の1〜60番ピンを同番号で1:1接続するデバッグ用メスコネクタ。ケース組み込み時DNP
- PPU /WR (47): SRAMに非接続 (CHR-ROM相当、書込不可)

## 595チェーン (LOAD時のアドレス/データ注入)

チェーン順 (ESP32 SER→): [DATA] → [ADDR下位] → [ADDR上位]。24bit送出(MSBファースト)後の配置:

- チェーンA (PRG): U3=DATA (QH=D7…QA=D0) → U4=ADDR下位 (QH=A7…QA=A0) → U5=ADDR上位 (QH=未使用, QG=A14…QA=A8)
- チェーンB (CHR): U6=DATA → U7=ADDR下位 → U8=ADDR上位 (同構成、A13/A14未使用)
- SRCLK(GPIO3)/RCLK(GPIO4)は両チェーン共有、SER_A=GPIO1、SER_B=GPIO2
- 全595 /OE=/OE_595、/SRCLR=+5V
- 595出力はSRAMのアドレス/データネットに直結 (541/245とはMODEで排他)

## エッジその他

- CIRAM /CE (48) — PPU /A13 (49) 直結
- CIRAM A10 (18) — 3パッド半田ジャンパ — PA10 (V) / PA11 (H)
- 音声: 45 — 0Ω — 46
- /IRQ (15), M2 (32): 未接続 (M2はテストパッドのみ推奨)
- +5V (30, 31要確認) → 5Vプレーン → M5Stamp S3 5V入力
- GND (1, 16)

## GreenPAK / OPMレジスタ捕捉

U21 (SLG46826G) はCPUのOPMレジスタ書込みを捕捉し、M5Stamp S3から
インシステム設定を書き換えられるようにする。

- U21 VDD/VDD2 = +5V、GND = GND、C28=100nF
- 入力: D0-D7、A0、R/W、M2、/RESET、/OPM_CS
- 出力予約: OPM_IRQ、OPM_WR_STB
- /OPM_CSは$5000-$5FFFを外部ロジックでデコードして生成する
- GreenPAK I2C側: GPAK_SDA_5V / GPAK_SCL_5V
- M5Stamp側: GPIO40(SDA) / GPIO41(SCL)
- Q1/Q2=BSS138、R26-R29=4.7kΩで5V↔3.3V双方向レベル変換する
- PT8211は制御バスを使用しないため、GreenPAKがI2Cバスを単独使用する

## PT8211 16-bit DAC

- U20 = PT8211-S-TP、+5V単電源、SOP-8
- シリアル入力 = BCK / WS / DINの3線。16-bit Japanese format（LSB-justified）で駆動
- pin1 BCK = I2S_BCLK_MCU、pin2 WS = I2S_LRCK_MCU、pin3 DIN = I2S_DOUT_MCU
- pin4 GND、pin5 VDD = +5V、pin6 LCH、pin7 NC、pin8 RCH
- VDDはC21=100nFとC27=10µFでデカップリング
- OUTL / OUTRは各20kΩでモノラル加算し、1µFでAC結合してAUDIO46へ入力
- R14のAUDIO45-AUDIO46間0Ωパススルーは維持する

## J2: 拡張ポート(DA15)ケーブル用コネクタ (BLEパッド注入)

| J2 | 信号 | DA15 | レベル |
|---|---|---|---|
| 1 | GND | 1 | — |
| 2 | OUT0 | 12 | 5V→分圧(10k/20k)→ESP32 GPIO9 |
| 3 | JOY1 /D1 | 13 | ESP32 GPIO11 → 直結(3.3V=TTL H) |
| 4 | /OE joy1 | 14 | 5V→分圧→ESP32 GPIO10 |
| 5 | +5V | 15 | 未接続可 |

## パスコン

各IC 100nF (15個)、5Vエントリに22µF、M5Stamp近傍に10µF。
