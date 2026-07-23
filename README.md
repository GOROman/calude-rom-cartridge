# Famicom ROM Cartridge (M5Stamp S3 + NROM)

ファミコン実機で動作するカートリッジ基板。物理ROMの代わりに **M5Stamp S3 (ESP32-S3)** を搭載し、電源投入時にROMイメージをSRAMへ転送してファミコンにバスを明け渡す「SRAMロード&ハンドオーバー方式」を採用。

## 使い方 (UX)

1. カートリッジを挿してファミコンの電源を入れる
2. ESP32がSRAMへROMをロード(約1秒、完了でLED緑点灯)
3. 本体の **RESETボタンを押す** → ゲーム起動

※カートリッジ端子にはカート側から操作できるリセット線が無いため、この手順が必要。

## 構成

- マッパー: **NROM** (PRG 32KB + CHR 8KB)
- ROM相当: AS6C62256 SRAM ×2 (PRG/CHR)
- ロード経路: 74HCT595 シフトレジスタチェーン ×2
- バス分離: 74HCT541/245 バッファ、74HCT32 ゲーティング
- 外形: 純正シェル互換 90×46.1mm、板厚1.2mm、60ピンエッジ

## 機能

- **WiFi転送**: softAP (`FC-CART`) + Web UI (`http://192.168.4.1/`) で .nes アップロード/ゲーム切替
- **Bluetoothコントローラ**: 拡張ポート(DA15)経由でパッド入力を注入(BLEパッド → Bluepad32 統合予定)。カートリッジ端子にはコントローラ信号が無いため J2→DA15 ケーブルを使用

## ファームウェア開発 (ESP-IDF)

```sh
cd firmware
idf.py set-target esp32s3
idf.py build flash monitor
```

## ディレクトリ

- `docs/` — アーキテクチャ設計、寸法、発注仕様
- `hardware/` — EasyEDA Pro プロジェクト・フットプリント資料
- `firmware/` — ESP32-S3 ファームウェア (PlatformIO)

詳細は [docs/architecture.md](docs/architecture.md) を参照。
