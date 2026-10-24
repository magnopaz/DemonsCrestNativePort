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
        outside = False
        if edge.get("x") is not None and edge.get("y") is not None:
            room_path = args.rooms / f"room_area{src}.json"
            if room_path.is_file():
                room = json.loads(room_path.read_text())
                world = room.get("world", {})
                w = int(world.get("width") or 0)
                h = int(world.get("height") or 0)
                outside = bool(w and (edge["x"] < 0 or edge["x"] >= w or
                                      edge["y"] < 0 or edge["y"] >= h))
        if edge.get("x") is None or edge.get("y") is None or outside:
            if edge.get("source") == "notes":
                path = args.rooms / f"room_area{src}.json"
                if path.is_file():
                    data = json.loads(path.read_text())
                    transitions = data.setdefault("transitions", [])
                    extracted = [tr for tr in transitions
                                 if tr.get("source") not in
                                 ("notes", "notes_repointed", "save_observed")]
                    # Unambiguous only: a room with several extracted doors may
                    # use them for side paths, so do not steal one of them.
                    if len(extracted) == 1:
                        tr = extracted[0]
                        if int(tr.get("next_area", -1)) != dst:
                            tr["next_area"] = dst
                            tr["source"] = "notes_repointed"
                            path.write_text(json.dumps(data))
                            print(f"{src}->{dst}: re-pointed the room's only "
                                  f"extracted door")
                            added += 1
                            continue
                    # Otherwise place the note door, clamped into the world.
                    world = data.get("world", {})
                    w = int(world.get("width") or 256)
                    h = int(world.get("height") or 256)
                    cx = min(max(int(edge.get("x") or w - 32), 16), w - 16)
                    cy = min(max(int(edge.get("y") or h // 2), 16), h - 16)
                    if any(int(tr.get("next_area", -1)) == dst for tr in
                           transitions):
                        continue
                    transitions.insert(0, {
                        "next_area": dst, "spawn_id": 0, "pad_mask": 255,
                        "x": cx, "y": cy, "half_w": 12, "half_h": 40,
                        "type": -3, "source": "notes_clamped",
                    })
                    path.write_text(json.dumps(data))
                    print(f"{src}->{dst}: added a clamped note door at "
                          f"({cx},{cy}) (level-space x was {edge.get('x')})")
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
            # Note only: overlapping doors are resolved by order, not deletion.
            for tr in transitions:
                if int(tr.get("next_area", -1)) != dst and \
                        abs(float(tr.get("x", -999)) -
                            float(edge.get("x", 0))) <= 24.0 and \
                        abs(float(tr.get("y", -999)) -
                            float(edge.get("y", 0))) <= 44.0:
                    print(f"{src}: note door {src}->{dst} overlaps extracted "
                          f"{src}->{tr.get('next_area')}; note goes first")
        if any(int(t.get("next_area", -1)) == dst for t in transitions):
            print(f"{src}->{dst}: already present")
            continue
        transitions.insert(0, {
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
