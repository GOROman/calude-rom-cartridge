# PCB

基板設計・製造・ケース関連のファイルは、このディレクトリに集約しています。

## 構成

| ディレクトリ | 内容 |
|---|---|
| `easyeda/` | 現行EasyEDAプロジェクトの書き出し |
| `easyeda/archive/` | 旧EasyEDAプロジェクト |
| `design-docs/` | BOM設計表、ネットリスト仕様、PCB作業状況 |
| `footprints/` | カスタムフットプリント資料 |
| `fabrication/` | Gerber、実装BOM、Pick & Place |
| `case/` | ケースSTL、USB開口生成スクリプト |

## 現行ファイル

- EasyEDAプロジェクト: `easyeda/calude-rom-cartridge.epro2`
- Gerber: `fabrication/calude-rom-cartridge-PCB2-gerber.zip`
- 実装BOM: `fabrication/calude-rom-cartridge-PCB2-bom.csv`
- Pick & Place: `fabrication/calude-rom-cartridge-PCB2-pick-and-place.csv`

製造データはEasyEDAのPCB2から直接生成しています。BOMとPick & Placeは
EasyEDA標準のUTF-16LE形式です。

## 注意

現在のGerberは設計スナップショットです。発注前にDRC、未配線、部品重なり、
エッジフィンガーの向き、板厚1.2mm、金端子処理を再確認してください。

PCB2の60ピン端子は全端子を同じ幅（63 mil）に統一し、先端を左右対称の
テーパー形状にしています。端子への引き込みは垂直・24 milで、挿入側の
左右基板角はR60 milです。端子15・47はNCのため引き込み配線はありません。
