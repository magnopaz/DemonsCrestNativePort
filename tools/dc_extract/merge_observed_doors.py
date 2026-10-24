#!/usr/bin/env python3
"""Merge save-observed door edges into the room JSONs.

`build_area_graph.py --probes` records doors seen in the user's level saves. The
port needs them in `room_area<N>.json` to walk them, so this tool appends a
transition for each observed edge (deduplicated by destination area).

Usage:
  python tools/dc_extract/merge_observed_doors.py \
      --graph assets/generated/area_graph.json --rooms assets/generated
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--graph", type=Path,
                    default=Path("assets/generated/area_graph.json"))
    ap.add_argument("--rooms", type=Path, default=Path("assets/generated"))
    args = ap.parse_args()

    graph = json.loads(args.graph.read_text())
    added = 0
    for edge in graph["edges"]:
        if edge.get("source") != "save_observed":
            continue
        src, dst = int(edge["from"]), int(edge["to"])
        path = args.rooms / f"room_area{src}.json"
        if not path.is_file():
            print(f"skip {src}->{dst}: no {path.name}")
            continue
        data = json.loads(path.read_text())
        transitions = data.setdefault("transitions", [])
        if any(int(t.get("next_area", -1)) == dst for t in transitions):
            print(f"{src}->{dst}: already present")
            continue
        transitions.append({
            "next_area": dst,
            "spawn_id": int(edge.get("spawn_id", 0)),
            "pad_mask": 255,
            "x": int(edge["x"]),
            "y": int(edge["y"]),
            "half_w": int(edge.get("half_w", 12)),
            "half_h": int(edge.get("half_h", 40)),
            "type": -1,
            "source": "save_observed",
        })
        path.write_text(json.dumps(data))
        added += 1
        print(f"{src}->{dst}: door added at ({edge['x']},{edge['y']})")
    print(f"doors added: {added}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
