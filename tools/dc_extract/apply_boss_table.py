#!/usr/bin/env python3
"""Insert the known boss objects into the generated room documents.

`check_bosses.py` already carries the authoritative area -> (name, species,
flags, x, y) table (recovered from save states and probes).  A clean
`extract.py` run does not produce those objects, because the retail object
lists in the ROM do not include the boss entries for every room.  This tool
applies the table so a fresh extraction reaches the documented state:

  python tools/dc_extract/apply_boss_table.py --rooms assets/generated

It is idempotent: an existing boss with the same species is replaced in place,
anything else is appended.  Run it after `extract.py` and before the regression.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from check_bosses import BOSSES  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rooms", type=Path, default=ROOT / "assets" / "generated")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    changed = 0
    for area, entries in sorted(BOSSES.items()):
        path = args.rooms / f"room_area{area}.json"
        if not path.is_file():
            print(f"area {area}: missing {path.name}, skipped")
            continue
        doc = json.loads(path.read_text())
        objects = doc.setdefault("objects", [])
        expected = {species for _n, species, _f, _x, _y in entries}
        # Areas 52/54 shipped a placeholder boss ($96) at the real boss position.
        # Any boss species the table does not list for this area is stale.
        stale = [i for i, obj in enumerate(objects)
                 if obj.get("type") == "boss" and obj.get("species") not in expected]
        for i in reversed(stale):
            print(f"area {area}: dropping stale boss {objects[i].get('species'):#04x}")
            del objects[i]
        for name, species, flags, x, y in entries:
            entry = {"type": "boss", "species": species,
                     "flags": 0 if flags is None else flags, "x": x, "y": y}
            replaced = False
            for i, obj in enumerate(objects):
                if obj.get("type") == "boss" and obj.get("species") == species:
                    if flags is None:
                        entry["flags"] = obj.get("flags", 0)
                    objects[i] = entry
                    replaced = True
                    break
            if not replaced:
                objects.append(entry)
            print(f"area {area} ({name}): species {species:#04x} at {x},{y} "
                  f"flags={entry['flags']} ({'replaced' if replaced else 'added'})")
            changed += 1
        if not args.dry_run:
            path.write_text(json.dumps(doc, indent=1))
    print(f"{changed} boss entries applied to {len(BOSSES)} areas"
          f"{' (dry run)' if args.dry_run else ''}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
