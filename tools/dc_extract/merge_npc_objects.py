#!/usr/bin/env python3
"""Add talkable NPC placements from probe dumps into room JSONs.

The room extractor only records the area's object-list entries; scripted NPCs
(seen live in save probes, e.g. species $96 in the city) are missing. This tool
adds them at their probed positions so the port can talk to them.

Usage:
  python tools/dc_extract/merge_npc_objects.py --probe-root <...>/oracle_probe \
      --saves 0ba --rooms assets/generated
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

NPC_SPECIES = {0x96}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--probe-root", type=Path, required=True)
    ap.add_argument("--saves", required=True)
    ap.add_argument("--rooms", type=Path, required=True)
    args = ap.parse_args()

    for tag in [s.strip() for s in args.saves.split(",") if s.strip()]:
        src = args.probe_root / f"save{tag}"
        info_path = src / "info.json"
        if not info_path.is_file():
            print(f"skip save{tag}: no probe")
            continue
        info = json.loads(info_path.read_text())
        room = args.rooms / f"room_area{info['area']}.json"
        if not room.is_file():
            print(f"skip save{tag}: no {room.name}")
            continue
        data = json.loads(room.read_text())
        objects = data.setdefault("objects", [])
        added = 0
        for ent in info.get("species", []):
            species = int(ent["species"])
            if species not in NPC_SPECIES:
                continue
            if any(o.get("type") == "npc" and
                   int(o.get("species", 0)) == species for o in objects):
                continue
            objects.append({"type": "npc", "species": species, "flags": 0,
                            "x": round(ent["x"]), "y": round(ent["y"])})
            added += 1
        if added:
            room.write_text(json.dumps(data))
        print(f"save{tag}: area {info['area']} +{added} npc "
              f"(now {len(objects)} objects)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
