#!/usr/bin/env bash
# Build the offline .spc renderer against the oracle's SPC700 + S-DSP cores.
#
# The renderer is a dev-time asset tool: it links snesrecomp's spc.c/dsp.c/apu.c
# (the same cores the behavioural oracle runs) plus tools/audio/spc_stubs.c,
# which no-ops the observability hooks.  Nothing from this build ships in the
# game; the game only ever loads the rendered WAV/OGG.
#
# Usage:
#   tools/audio/build_spc_render.sh [--recomp DIR] [--out PATH]
#
# RECOMP_DIR overrides the oracle checkout, like the tools/oracle/*.py tools.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RECOMP="${RECOMP_DIR:-$ROOT/../DemonsCrestSNESRecomp}"
OUT="$ROOT/build/spc_render"

while [ $# -gt 0 ]; do
  case "$1" in
    --recomp) RECOMP="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    *) echo "build_spc_render: unknown option $1" >&2; exit 2 ;;
  esac
done

SRC="$RECOMP/snesrecomp/runner/src"
for f in "$SRC/snes/spc.c" "$SRC/snes/dsp.c" "$SRC/snes/apu.c"; do
  [ -f "$f" ] || { echo "build_spc_render: missing oracle source $f" >&2; exit 1; }
done

mkdir -p "$(dirname "$OUT")"
CC="${CC:-gcc}"
$CC -O2 -std=c11 -o "$OUT" \
  "$ROOT/tools/audio/spc_render.c" \
  "$ROOT/tools/audio/spc_stubs.c" \
  "$SRC/snes/spc.c" "$SRC/snes/dsp.c" "$SRC/snes/apu.c" \
  -I"$SRC" -I"$SRC/snes" -lm
echo "build_spc_render: $OUT"
