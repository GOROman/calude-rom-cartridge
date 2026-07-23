# アーキテクチャ設計

## 方式判断

ESP32-S3によるカートリッジバスの直接エミュレーションは不成立:

- NROMは CPU側 (A0-A14, D0-D7, R/W, /ROMSEL, M2 ≈26本) + PPU側 (PA0-PA13, PD0-PD7, /RD ≈23本) の**約49信号が同時に必要**。M5Stamp S3の使用可能GPIOは約23本。
- バス応答は /ROMSEL 立下りから ~300-400ns 以内。PPU側は独立した並行フェッチ。RP2040系 (PicoROM等) はPIO+オーバークロック+30GPIOで成立しているが、ESP32-S3にPIO相当はなく、WiFi/RTOS併走で2系統の決定的応答は不可能。

→ **SRAMロード&ハンドオーバー方式**を採用。

## ブロック図

```
             ファミコン 60ピンエッジ (2×30, 2.54mm)
   CPU側                                    PPU側
    A0-A14,/ROMSEL,R/W →[74HCT541×2]→+       PA0-PA12 →[74HCT541×2]→+
    D0-D7 ←[74HCT245]←+              |       PD0-PD7 ←[74HCT245]←+  |
                      |              |                           |  |
               +------+--------------+--+              +---------+--+----+
               | U1 PRG SRAM AS6C62256  |              | U2 CHR SRAM     |
               +-----------+------------+              +--------+--------+
                           |                                    |
              [74HCT595×3 チェーンA]                [74HCT595×3 チェーンB]
                           \____________    ____________________/
                                        \  /
                          M5Stamp S3 (3.3V, 使用GPIO ~10本)
                          SER_A/SER_B, SRCLK, RCLK, /OE_595,
                          /WE_PRG, /WE_CHR, MODE, LED, BTN
```

- ミラーリング: CIRAM A10 (pin18) ← 半田ジャンパ → PA10 (V) / PA11 (H)
- CIRAM /CE (pin48) ← 直結 → PPU /A13 (pin49) ※NROM慣例
- 音声: pin45 — 0Ωブリッジ — pin46
- 電源: 本体+5V → SRAM/74HCTレール + M5Stamp 5V入力(内蔵3.3Vレギュレータ)

## 動作モード

`MODE` 線1本で切替(抵抗でLOAD側にプル → ESP32起動前は必ず不活性):

- **LOAD**: コンソール側541/245無効(本体から見てhi-Z=オープンバス、衝突なし)。595チェーンがSRAMのアドレス+データを駆動、ESP32が/WEをパルス。
- **RUN**: 595 /OE=H (hi-Z)。コンソール側バッファ有効。SRAMの/CE//OEは 74HCT32 で /ROMSEL+R/W (PRG) / PPU A13+/RD (CHR) からゲート。

## 起動シーケンス

1. 電源ON。MODE=LOAD(プルダウン)。本体CPUはオープンバスのゴミを実行するが電気的衝突なし。
2. ESP32起動(~300ms)→ LittleFSマウント → .nesのiNESヘッダ解析。
3. PRG 32,768バイト(16KBイメージは2回ミラー書込)をチェーンAへ: 15bitアドレス+8bitデータをシフト→ラッチ→/WE_PRGパルス。CHR 8,192バイトをチェーンBへ。シフトクロック≤10MHzで計~100-200ms。
4. MODE=RUN → LED緑。
5. **ユーザーが本体RESETを押す** → CPUが$FFFCをSRAMから読みゲーム起動。
6. RUN中もESP32は生存(将来: WiFi UI)。別ゲーム選択時は MODE=LOAD→再ロード→RESET再押下。

## 信号設計

ピンマッピング (NESdev Cartridge connector 準拠):

- CPU: GND=1/16, A11-A0=2-13, R/W=14, /IRQ=15(未使用), /RD(PPU)=17, M2=32, A12-A14=33-35, D7-D0=36-43, /ROMSEL=44, +5V=30
- PPU: CIRAM A10=18, PA6-PA0=19-25, PD0-PD3=26-29, /WR=47, CIRAM /CE=48, PPU /A13=49, PA7-PA12=50-55, PA13=56, PD7-PD4=57-60
- 音声: 45→46 0Ωブリッジ。pin31: 5Vプレーンへ0Ωオプション。

ゲーティング (74HCT32):

- PRG SRAM: /CE = /ROMSEL(バッファ後)、/OE = /ROMSEL OR R/W反転考慮 OR (NOT MODE) — $8000+への書込衝突防止のためR/Wを含める。/WE = ESP32 (プルアップ)。
- CHR SRAM: /CE = PPU A13 OR (NOT MODE)、/OE = PPU /RD、/WE = ESP32。PPU /WR はSRAMに非接続(CHR-ROM相当)。
- 全体5Vロジック。ESP32(3.3V)→HCT入力は VIH=2.0V なので直結可。シフト線は短く+直列33Ω。
- パスコン: 各IC 100nF、エッジ付近にバルク22µF。

## ESP32 ピンバジェット

| 信号 | 本数 |
|---|---|
| SER_A, SER_B | 2 |
| SRCLK, RCLK (チェーン共有) | 2 |
| /OE_595 (共有) | 1 |
| /WE_PRG, /WE_CHR | 2 |
| MODE | 1 |
| LED (内蔵SK6812=G21), BTN | 1 |
| 予備 | ~11 |

## 部品表(主要)

| Ref | 部品 | LCSC候補 |
|---|---|---|
| U1,U2 | AS6C62256-55 (32K×8 SRAM, 2.7-5.5V, 55ns) | C5569983 (DIP) / SOP版要検索。大容量化はAS6C1008 C1523885 |
| U3-U8 | 74HCT595 ×6 | 74HCT595D系 |
| U9-U12 | 74HCT541 ×4 | |
| U13,U14 | 74HCT245 ×2 | |
| U15 | 74HCT32 | |
| M1 | M5Stamp S3 | ユーザー支給 |

**HCT必須**(HC不可): 3.3V駆動のVIH互換のため。

## 基板外形 (NESdev実測)

- 外形 **90 × 46.1 mm**、板厚 **1.2mm** (1.6mmはコネクタを痛めるため不可)
- エッジフィンガー: 2×30、2.54mmピッチ、信号パッド幅1.6mm/電源2.6mm、挿入深さ10.7mm、パッド上端〜基板端3mm、レジスト開口4mm
- JLCPCB: ENIG以上(理想: 金フラッシュ+30°ベベル)。フィンガー1mm以内に部品/ビア禁止、フィンガーは一辺のみ。
- 参照: DameNoSupaplex/famicom-cartridge-nrom (KiCad)、EasyEDAコミュニティ FAMICOM CARTRIDGE PCB フットプリント
- M5Stamp S3 (~24×20×4.5mm) はシェルの厚い側キャビティに配置。樹脂シェルなのでWiFi透過OK。

## WiFi 転送 (実装済み)

ESP32-S3 が softAP (SSID: `FC-CART`) + HTTPサーバを起動。`http://192.168.4.1/` で:

- `.nes` ファイルのアップロード (LittleFS 4MBパーティションに保存、NROM数十本分)
- ゲーム一覧から選択 → その場でSRAM再ロード → 本体RESET押下で切替
- 選択したゲームは `autoload.txt` に記憶し次回電源ONで自動ロード

## Bluetooth コントローラ対応 (拡張ポート経由)

**カートリッジ端子にはコントローラ信号が存在しない**($4016/$4017は本体内部でデコードされ、シリアルデータは本体内蔵ラッチ/拡張ポートから供給される)。よってコントローラ入力の注入は **DA15拡張ポート** 経由で行う(8BitDoレシーバと同方式)。

構成: カートリッジ基板上に J2 コネクタ(5線)を設け、DA15プラグへのケーブルで接続。

| J2信号 | DA15ピン | 方向 | レベル対策 |
|---|---|---|---|
| GND | 1 | — | — |
| OUT0 (ストローブ) | 12 | 本体→ESP32 | 抵抗分圧 5V→3.3V |
| /OE joy1 ($4016リード) | 14 | 本体→ESP32 | 抵抗分圧 5V→3.3V |
| Joypad1 /D1 (データ) | 13 | ESP32→本体 | 3.3V直結 (TTL VIH=2.0Vで成立) |
| +5V | 15 | — | (給電はカートエッジ側を使用、未接続可) |

動作: 4021シフトレジスタ相当をGPIO割り込みでエミュレート(`joypad_out.c` 実装済み)。OUT0でボタン状態をラッチ、$4016リードの立上りエッジで次ビットへシフト。データは拡張コントローラ扱い($4016 D1)なので物理コントローラ(D0)と衝突しない。ゲーム側は標準+拡張の両対応読取りが一般的。

- リスク: ISR応答遅延(~1-2µs)による取りこぼし。問題があれば74HC165(物理シフトレジスタ)+パラレル駆動に切替(フォールバック設計)。
- BLE入力元: **Bluepad32** (ESP-IDFコンポーネント統合、BLEゲームパッド: Xbox Series等) を `joypad_out_set_buttons()` に接続する。ESP32-S3はBLEのみ(BT Classic非対応)のため、Classic専用パッド(Switch Pro等)は不可。統合はv1.2マイルストーン。

## タイミングマージン

RUN経路: 541 (~9ns) + SRAM (55ns) + 245 (~9ns) ≈ **75ns** ≪ バジェット ~350ns。十分。

## リスクと対策

| リスク | 対策 |
|---|---|
| ロード中の本体ゴミ実行 | 全バッファhi-Z(衝突なし)+「LED緑でRESET」フロー |
| $8000+書込衝突 | PRG /OE に R/W をゲート |
| PPU /WR によるCHR破壊 | CHR /WE をESP32+プルアップで常時H |
| シェル個体差 | 発注前に実物シェルで実測確認 |
| SRAM読み戻し検証不可 | アップロード時CRC+控えめなシフトクロック |

## 参考資料

- https://www.nesdev.org/wiki/Cartridge_connector
- https://www.nesdev.org/wiki/Famicom_cartridge_dimensions
- https://github.com/DameNoSupaplex/famicom-cartridge-nrom
- https://github.com/wickerwaka/PicoROM (RP2040 ROMエミュレータ先行事例)
- https://jlcpcb.com/help/article/jlcpcb-gold-fingers
