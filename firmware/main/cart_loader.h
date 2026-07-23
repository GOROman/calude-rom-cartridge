#pragma once
#include <stdbool.h>

// GPIO をバス不活性状態で初期化する (起動直後に必ず呼ぶ)
void cart_loader_init(void);

// path の iNES ファイルを解析し、595 チェーン経由で PRG/CHR SRAM へ書き込み、
// 完了後 MODE=RUN へハンドオーバーする。成功で true。
bool cart_load_game(const char *path);
