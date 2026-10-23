#!/usr/bin/env python3
"""Deterministic native regression checks for the Demon's Crest fan port.

Runs the built game in its headless DC_RUN_FRAMES mode, writes trace CSVs,
and asserts the recovered physics/object/HUD invariants. It does not touch
the recomp or ROM: local oracle comparison stays a separate workflow.

Usage:
  python3 tools/run_regression.py
  python3 tools/run_regression.py --binary build/demonscrest --assets assets/generated

Exit codes: 0 = pass, 1 = failure, 77 = assets/binary missing (ctest skip).
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--binary", type=Path,
                    default=ROOT / "build" / "demonscrest")
    ap.add_argument("--assets", type=Path,
                    default=ROOT / "assets" / "generated")
    return ap.parse_args()


def wrapper(binary: Path) -> list[str]:
    cmd = [str(binary)]
    # Always prefer a virtual display on Linux: CI and desktop sessions may
    # advertise DISPLAY without a usable GL context for the test binary.
    if sys.platform.startswith("linux") and shutil.which("xvfb-run"):
        cmd = ["xvfb-run", "-a", *cmd]
    return cmd


def run_demo(base: list[str], out: Path, demo: int, frames: int,
             env_extra: dict[str, str] | None = None) -> dict[str, Path]:
    player = out / f"demo{demo}_player.csv"
    objects = out / f"demo{demo}_objects.csv"
    projectiles = out / f"demo{demo}_projectiles.csv"
    env = dict(os.environ)
    env.update({
        "DC_DEMO": str(demo),
        "DC_RUN_FRAMES": str(frames),
        "DC_NO_AUDIO": "1",
        "DC_TRACE_CSV": str(player),
        "DC_TRACE_OBJECTS": str(objects),
        "DC_TRACE_PROJECTILES": str(projectiles),
    })
    if env_extra:
        env.update(env_extra)
    print(f"regression: running demo {demo} ({frames} frames)", flush=True)
    try:
        proc = subprocess.run(base, cwd=ROOT, env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, timeout=120)
    except subprocess.TimeoutExpired:
        print(f"regression: TIMEOUT demo {demo} ({frames} frames)",
              flush=True)
        raise
    if proc.returncode != 0:
        raise RuntimeError(f"demo {demo} exited {proc.returncode}\n{proc.stdout}")
    return {"player": player, "objects": objects, "projectiles": projectiles}


def csv_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def close(a: float, b: float, eps: float = 0.01) -> bool:
    return abs(a - b) <= eps


def main() -> int:
    args = parse_args()
    binary = args.binary.resolve()
    assets = args.assets.resolve()
    if not binary.is_file():
        print(f"regression: missing binary {binary} (build first)", file=sys.stderr)
        return 77
    needed = ["room.json", "player_anim.json", "fireball_anim.json",
              "hud_bar.png", "hud_slots.png"]
    missing = [name for name in needed if not (assets / name).is_file()]
    if missing:
        print(f"regression: missing assets {missing}; run the extract steps",
              file=sys.stderr)
        return 77

    base = wrapper(binary)
    failures: list[str] = []

    def check(ok: bool, message: str):
        if not ok:
            failures.append(message)

    with tempfile.TemporaryDirectory(prefix="dc-regression-") as td:
        out = Path(td)

        # ---- Step 3: walk / tap-jump / hold-jump / hover replay ----
        d6 = run_demo(base, out, 6, 603)
        rows = csv_rows(d6["player"])
        check(len(rows) == 603, f"demo6: expected 603 frames, got {len(rows)}")
        if rows:
            first = rows[0]
            check(close(float(first["player_x"]), 332.0),
                  "demo6: spawn X is not 332")
            check(close(float(first["player_y"]), 164.0),
                  "demo6: spawn Y is not 164 (slope snap)")
            check(first["hp"] == "4" and first["max_hp"] == "6",
                  "demo6: initial HP is not 4/6")
            xs = [float(r["player_x"]) for r in rows]
            ys = [float(r["player_y"]) for r in rows]
            check(close(min(xs), 332.0), "demo6: walk-left bound changed")
            check(close(max(xs), 347.383, 0.02),
                  "demo6: 11-frame walk distance changed")
            check(close(max(ys), 164.0, 0.01), "demo6: slope descent changed")
            check(min(ys) < 93.0, "demo6: hold-jump apex is too low")
            check(all(float(r["vy"]) <= 6.01 for r in rows),
                  "demo6: terminal fall clamp exceeded")
            hover = rows[235:256]
            if hover:
                y0 = float(hover[0]["player_y"])
                check(all(close(float(r["player_y"]), y0, 0.001)
                          for r in hover),
                      "demo6: hover did not freeze vertical motion")
                check(all(r["state"] == "3" for r in hover),
                      "demo6: hover state was not stable")

        # ---- Step 4: fireball spawn/velocity/pillar despawn ----
        d7 = run_demo(base, out, 7, 120)
        proj = csv_rows(d7["projectiles"])
        check(len(proj) > 0, "demo7: no projectiles spawned")
        first_shot = [r for r in proj if r["frame"] == "49"]
        check(bool(first_shot), "demo7: expected a shot at frame 49")
        if first_shot:
            p = first_shot[0]
            check(close(float(p["x"]), 369.0) and close(float(p["y"]), 152.0),
                  "demo7: fireball spawn offset changed")
            check(close(float(p["vx"]), 3.0), "demo7: fireball velocity changed")
        check(not any(float(r["x"]) > 384.0 for r in proj
                      if float(r["vx"]) > 0),
              "demo7: right shot passed through the stone pillar")

        # ---- Step 6: urn -> bouncing heart -> heal ----
        d9 = run_demo(base, out, 9, 180)
        rows9 = csv_rows(d9["player"])
        objs9 = csv_rows(d9["objects"])
        hp9 = [int(r["hp"]) for r in rows9]
        check(max(hp9) > min(hp9), "demo9: heart pickup did not heal HP")
        pickups = [r for r in objs9 if r["type"] == "2"]
        check(bool(pickups), "demo9: no pickup object was spawned")
        if pickups:
            ys = [float(r["y"]) for r in pickups]
            vys = [float(r["vy"]) for r in pickups]
            check(min(ys) < 105.0, "demo9: heart pop-up arc is too short")
            check(max(ys) <= 128.01, "demo9: heart fell below the pillar top")
            check(any(close(v, -3.0, 0.01) for v in vys),
                  "demo9: heart did not bounce at the 3.0/frame decrement")

        # ---- Campaign: crest-form cycling (Crest system) ----
        d11 = run_demo(base, out, 11, 180,
                       {"DC_AREA": "1", "DC_CAMPAIGN": "1",
                        "DC_UNLOCK_ALL": "1"})
        rows11 = csv_rows(d11["player"])
        forms = []
        for r in rows11:
            f = int(r.get("form", 0))
            if not forms or forms[-1] != f:
                forms.append(f)
        check(len(set(forms)) >= 3,
              f"demo11: Crest forms did not cycle (got {forms})")
        check(4 in forms or 5 in forms,
              "demo11: Ultimate/Air forms were never selected")

        # ---- Campaign: Area 0 boss gate -> Area 17 transition ----
        d12 = run_demo(base, out, 12, 1200,
                       {"DC_AREA": "0", "DC_CAMPAIGN": "1",
                        "DC_BOSS_DEFEATED": "1"})
        rows12 = csv_rows(d12["player"])
        areas12 = []
        for r in rows12:
            a = int(r["area"])
            if not areas12 or areas12[-1] != a:
                areas12.append(a)
        check(areas12 and areas12[0] == 0,
              f"demo12: campaign did not start in Area 0 ({areas12})")
        check(17 in areas12,
              f"demo12: boss-gate transition to Area 17 missing ({areas12})")

        # ---- Campaign: direct multi-row area loads ----
        for area in (2, 3):
            d = run_demo(base, out, 2, 20, {"DC_AREA": str(area)})
            rows_a = csv_rows(d["player"])
            check(bool(rows_a) and int(rows_a[0]["area"]) == area,
                  f"area {area}: room_area{area}.json was not loaded")
            if rows_a:
                check(float(rows_a[0]["camera_x"]) >= 0.0,
                      f"area {area}: camera X is negative")

        # ---- Campaign: title -> New Game -> Area 0 ----
        d13 = run_demo(base, out, 13, 120, {"DC_TITLE": "1"})
        rows13 = csv_rows(d13["player"])
        check(bool(rows13) and int(rows13[0]["area"]) == 0,
              "demo13: title NEW GAME did not enter Area 0")

        # ---- Save/options persistence: autosave, then resume ----
        save_path = out / "dc_save.json"
        save_path.unlink(missing_ok=True)
        run_demo(base, out, 12, 200,
                 {"DC_AREA": "0", "DC_CAMPAIGN": "1",
                  "DC_BOSS_DEFEATED": "1", "DC_SAVE_PATH": str(save_path),
                  "DC_SAVE_FORCE": "1"})
        check(save_path.is_file(), "save: autosave did not write a save file")
        if save_path.is_file():
            data = json.loads(save_path.read_text())
            check(int(data.get("version", 0)) == 2, "save: unexpected version")
            resumed = out / "resume_player.csv"
            env = dict(os.environ,
                       DC_RESUME="1", DC_RUN_FRAMES="2", DC_NO_AUDIO="1",
                       DC_SAVE_PATH=str(save_path), DC_SAVE_FORCE="1",
                       DC_TRACE_CSV=str(resumed))
            proc = subprocess.run(base, cwd=ROOT, env=env,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True,
                                  timeout=120)
            check(proc.returncode == 0, "save: resume run failed")
            rows_s = csv_rows(resumed) if resumed.is_file() else []
            check(bool(rows_s), "save: resume produced no trace")
            if rows_s:
                first = rows_s[0]
                check(int(first["max_hp"]) == int(data["max_hp"]),
                      "save: max_hp was not restored")
                check(int(first["gold"]) == int(data["gold"]),
                      "save: gold was not restored")
                check(int(first["area"]) == int(data["area"]),
                      "save: area was not restored")

        # ---- Campaign: Somulo defeat ends the area ----
        d14 = run_demo(base, out, 12, 300,
                       {"DC_AREA": "0", "DC_CAMPAIGN": "1",
                        "DC_BOSS_HP": "1"})
        rows14 = csv_rows(d14["player"])
        check(bool(rows14) and len(rows14) < 300,
              "demo12/boss: Somulo defeat did not leave GAME_PLAYING")

        d10 = run_demo(base, out, 10, 260,
                       {"DC_START_X": "900", "DC_START_Y": "160"})
        rows10 = csv_rows(d10["player"])
        hp10 = [int(r["hp"]) for r in rows10]
        check(any(hp10[i] < hp10[i - 1] for i in range(1, len(hp10))),
              "demo10: enemy contact did not damage the player")
        check(any(hp10[i] > hp10[i - 1] for i in range(1, len(hp10))),
              "demo10: no healing pickup was collected")
        enemies = [r for r in csv_rows(d10["objects"]) if r["type"] == "3"]
        last_by_slot = {}
        for r in enemies:
            last_by_slot[r["slot"]] = int(r["frame"])
        check(bool(last_by_slot), "demo10: no enemy objects spawned")
        check(any(frame < 259 for frame in last_by_slot.values()),
              "demo10: no enemy died during the run")

    if failures:
        print("regression: FAIL")
        for message in failures:
            print(f"  - {message}")
        return 1
    print("regression: PASS (demos 6,7,9,10,11,12,13, boss defeat + multi-area loads)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
