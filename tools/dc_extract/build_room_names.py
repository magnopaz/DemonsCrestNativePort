#!/usr/bin/env python3
"""Build the room name table from the map-named level saves.

The user's saves are named `lvl_<level>_<map-name>[_role]`, where `<map-name>`
matches a file in docs/FullMaps/images. Reading the area id out of each save's
probe gives the name -> area mapping (and with it the level grouping), which the
area graph and the map-size checks need.

Roles: (none) = entrance, `before_door`, `e1`/`e2` = enemies in the room,
`boss`, `boss_dead`.

Usage:
  python tools/dc_extract/build_room_names.py \
      --probes <...>/oracle_probe --maps docs/FullMaps/images \
      --out assets/generated/room_names.json
"""

from __future__ import annotations

import argparse
import difflib
import json
import re
from pathlib import Path

PATTERN = re.compile(
    r"^(?:save_)?lvl_(?P<level>[IVX]+)_(?P<name>[a-z0-9-]+?)"
    r"(?:_(?P<role>before_door|boss_dead|boss|e\d+))?$")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--probes", type=Path, required=True)
    ap.add_argument("--maps", type=Path,
                    default=Path("docs/FullMaps/images"))
    ap.add_argument("--out", type=Path,
                    default=Path("assets/generated/room_names.json"))
    args = ap.parse_args()

    map_names = [p.stem.replace("demons-crest-", "").replace("-snes-map", "")
                 for p in sorted(args.maps.glob("*.webp"))]
    table: dict[str, dict] = {}
    unmatched: list[str] = []

    infos = sorted(set(args.probes.glob("save_lvl_*/info.json")) |
                   set(args.probes.glob("lvl_*/info.json")))
    for info in infos:
        stem = info.parent.name
        m = PATTERN.match(stem)
        if not m:
            continue
        level = m.group("level")
        raw = m.group("name")
        role = m.group("role") or "entrance"
        match = difflib.get_close_matches(raw, map_names, n=1, cutoff=0.8)
        if not match:
            unmatched.append(f"{stem} ({raw})")
            continue
        map_name = match[0]
        data = json.loads(info.read_text())
        area = int(data["area"])
        entry = table.setdefault(str(area), {
            "area": area, "level": level, "map": map_name,
            "roles": {}, "saves": [],
        })
        entry["roles"].setdefault(role, []).append(stem)
        entry["saves"].append(stem)

    args.out.write_text(json.dumps(table, indent=1))
    print(f"named areas: {len(table)}")
    for area, e in sorted(table.items(), key=lambda kv: int(kv[0])):
        roles = ", ".join(sorted(e["roles"]))
        print(f"  area {int(area):3d}  {e['level']:<4s} {e['map']:<44s} [{roles}]")
    if unmatched:
        print("unmatched save names:")
        for name in unmatched:
            print("  ", name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
