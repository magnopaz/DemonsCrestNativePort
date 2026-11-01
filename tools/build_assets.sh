#!/usr/bin/env bash
# Rebuild every ROM-derived asset from scratch.
#
# Reproduces the documented asset state from:
#   - the player's own ROM (required)
#   - captures/graveyard/stage_600.*   (BG1/OBJ atlases, player idle, all 113 rooms)
#   - captures/anim/                   (player/fireball/HUD clips)
#   - captures/save*/ , captures/nb/*  (per-area BG2 backdrops)
#
# Post-extraction patches that cannot come from the ROM tables are applied
# afterwards, in order, and are idempotent:
#   apply_boss_table.py   -> boss objects from tools/check_bosses.py's table
#   apply_door_table.py   -> save-observed doors (if present)
#
# Usage:
#   tools/build_assets.sh [--rom PATH] [--out DIR] [--no-captures]
#
# Everything it writes is git-ignored. Re-running it never needs a recomp
# capture as long as captures/ is populated; delete captures/ to force a full
# oracle pass.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ROM="${ROM:-$ROOT/../roms/Demon's Crest (USA).sfc}"
OUT="${OUT:-$ROOT/assets/generated}"
DUMP="${DUMP:-$ROOT/captures/graveyard}"
PREFIX="${PREFIX:-stage_600}"
SKIP_CAPTURES=0

while [ $# -gt 0 ]; do
  case "$1" in
    --rom) ROM="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --no-captures) SKIP_CAPTURES=1; shift ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done

cd "$ROOT"
need() { [ -e "$1" ] || { echo "build_assets: missing $1" >&2; exit 1; }; }
need "$ROM"
need "$DUMP/$PREFIX.vram.bin"

echo "== 1/6 rooms, atlases, campaign =="
python3 tools/dc_extract/extract.py --rom "$ROM" --dump "$DUMP" --prefix "$PREFIX" \
    --area 1 --screens 9,10 --full-room --out "$OUT"

echo "== 2/6 player / fireball / HUD animations =="
if [ -d "$ROOT/captures/anim" ]; then
  python3 tools/dc_extract/extract_anim.py --dump "$ROOT/captures/anim" --out "$OUT" --clean
else
  echo "   captures/anim missing; animations fall back to player_idle.png"
fi

echo "== 3/6 font + endings =="
if [ -d "$ROOT/captures/font" ]; then
  python3 tools/dc_extract/extract_font.py --dump "$ROOT/captures/font" --out "$OUT"
else
  echo "   captures/font missing; run tools/dc_extract/extract_font.py on a menu capture"
fi
python3 tools/dc_extract/build_endings.py --out "$OUT"

echo "== 4/6 boss objects from the check table =="
python3 tools/dc_extract/apply_boss_table.py --rooms "$OUT"

echo "== 5/6 talkable city NPCs =="
python3 tools/dc_extract/apply_npc_table.py --rooms "$OUT"

echo "== 6/6 save-observed doors (optional) =="
if [ -f tools/dc_extract/apply_door_table.py ]; then
  python3 tools/dc_extract/apply_door_table.py --rooms "$OUT"
else
  echo "   apply_door_table.py not present yet; ROM transitions only"
fi

if [ "$SKIP_CAPTURES" -eq 0 ]; then
  echo "== verify: room/BG2 coverage =="
  python3 - "$OUT" <<'PY'
import json, sys
from pathlib import Path
out = Path(sys.argv[1])
rooms = sorted(out.glob("room_area*.json"))
with_bg2 = 0
for path in rooms:
    doc = json.loads(path.read_text())
    bg2 = [b for b in doc["bg"] if b["name"] == "bg2"]
    if bg2 and any(t & 0x3FF for t in bg2[0]["tilemap"]):
        with_bg2 += 1
print(f"rooms={len(rooms)} with_non_empty_bg2={with_bg2}")
PY
fi

echo "build_assets: done -> $OUT"
