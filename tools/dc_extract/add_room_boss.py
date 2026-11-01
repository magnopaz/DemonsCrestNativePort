#!/usr/bin/env python3
"""Add or replace boss objects in a generated room from a save/probe mapping.

Usage:
  python tools/dc_extract/add_room_boss.py --room assets/generated/room_area26.json \
    --bosses "123:1320:154,124:1380:154"

Each spec is species:x:y. Existing boss entries with the same species are
replaced in place; otherwise the entry is appended.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--room", type=Path, required=True)
    ap.add_argument("--bosses", required=True)
    args = ap.parse_args()
    data = json.loads(args.room.read_text())
    objects = data.setdefault("objects", [])
    for spec in [s for s in args.bosses.split(",") if s.strip()]:
        species, x, y = (int(v, 0) for v in spec.split(":"))
        entry = {"type": "boss", "species": species, "flags": 0,
                 "x": x, "y": y}
        replaced = False
        for i, obj in enumerate(objects):
            if obj.get("type") == "boss" and obj.get("species") == species:
                objects[i] = entry
                replaced = True
                break
        if not replaced:
            objects.append(entry)
        print(f"boss {species:#04x} at {x},{y} "
              f"({'replaced' if replaced else 'added'})")
    args.room.write_text(json.dumps(data, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
