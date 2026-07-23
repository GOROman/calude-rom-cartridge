#pragma once

// softAP (SSID: FC-CART) + HTTP サーバを起動する。
//   GET  /        — ゲーム一覧とアップロードフォーム
//   POST /upload  — .nes ファイルを LittleFS へ保存 (ファイル名は X-Filename ヘッダ)
//   POST /select  — body のファイル名をロードして RUN へ (要 RESET 押下)
void web_server_start(void);
