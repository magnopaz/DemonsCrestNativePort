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
    ap.add_argument("--sources", default="save_observed,notes",
                    help="comma separated edge sources to merge")
    ap.add_argument("--trust-notes", action="store_true",
                    help="drop extracted transitions that contradict a note "
                         "edge from the same room")
    args = ap.parse_args()

    graph = json.loads(args.graph.read_text())
    sources = {s.strip() for s in args.sources.split(",") if s.strip()}
    added = 0
    for edge in graph["edges"]:
        if edge.get("source") not in sources:
            continue
        if edge.get("to") is None or int(edge["to"]) < 0:
            continue
        src, dst = int(edge["from"]), int(edge["to"])
        if edge.get("x") is None or edge.get("y") is None:
            if edge.get("source") == "notes":
                path = args.rooms / f"room_area{src}.json"
                if path.is_file():
                    data = json.loads(path.read_text())
                    moved = False
                    for tr in data.get("transitions", []):
                        if int(tr.get("next_area", -1)) != dst:
                            tr["next_area"] = dst
                            tr["source"] = "notes_repointed"
                            moved = True
                            break
                    if moved:
                        path.write_text(json.dumps(data))
                        print(f"{src}->{dst}: re-pointed an extracted door "
                              f"(no save position for this one)")
                        added += 1
                        continue
            print(f"skip {edge.get('from')}->{edge['to']}: no door position")
            continue
        path = args.rooms / f"room_area{src}.json"
        if not path.is_file():
            print(f"skip {src}->{dst}: no {path.name}")
            continue
        data = json.loads(path.read_text())
        transitions = data.setdefault("transitions", [])
        if args.trust_notes:
            note_targets = {int(e["to"]) for e in graph["edges"]
                            if e.get("source") == "notes"
                            and int(e.get("from", -1)) == src
                            and e.get("to") is not None}
            kept = []
            for tr in transitions:
                target = int(tr.get("next_area", -1))
                if target in note_targets or target < 0:
                    kept.append(tr)
                else:
                    print(f"{src}: dropping extracted {src}->{target} "
                          f"(notes say {sorted(note_targets)})")
            transitions[:] = kept
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
    print(f"doors added: {added} (sources: {sorted(sources)})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
