#!/bin/zsh
# EasyEDAプロジェクト(.eprj2)の保存を検知して自動コミット/プッシュする監視スクリプト。
# 使い方: リポジトリルートで  ./pcb/.watch-commit.sh &   (バックグラウンド実行)
# 停止:  pkill -f watch-commit.sh
set -u
cd "$(dirname "$0")/.." || exit 1
TARGET="pcb"
hashof() { find "$TARGET" -name '*.eprj2' -type f -exec shasum {} \; 2>/dev/null | shasum | awk '{print $1}'; }
last="$(hashof)"
echo "[watch-commit] 監視開始: $TARGET/*.eprj2"
while true; do
  sleep 5
  cur="$(hashof)"
  if [ "$cur" != "$last" ]; then
    last="$cur"
    sleep 1  # 書き込み完了を待つ
    if [ -n "$(git status --porcelain "$TARGET")" ]; then
      ts="$(date '+%Y-%m-%d %H:%M:%S')"
      git add "$TARGET"/*.eprj2
      git commit -q -m "pcb: save $ts" \
        -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>" && \
        git push -q 2>/dev/null
      echo "[watch-commit] committed at $ts"
    fi
  fi
done
