#!/usr/bin/env python3
"""S1 vertical-slice flight + first benchmark harness (drives PIE via MCP).

StartPIE (in-viewport) so ASpaceGameMode spawns the pawn + streamer and the scripted flight runs;
capture frames at intervals; poll nvidia-smi for peak board VRAM; StopPIE. The [flight]/[bench] lines
land in Saved/Logs/SpaceTravel.log (read separately). Frames -> Saved/Captures.
"""
from __future__ import annotations
import sys, os, json, time, base64, subprocess, datetime
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_client import MCP, _result

A = "EditorToolset.EditorAppToolset"
OUT = r"C:\Projects\SpaceTravel\Saved\Captures"
ANN = {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0, "maxLabelDistance": 0, "classFilter": None, "maxLabels": 0}


def vram_used_mib():
    try:
        r = subprocess.run(["nvidia-smi", "--query-gpu=memory.used", "--format=csv,noheader,nounits"],
                           capture_output=True, text=True, timeout=10)
        return int(r.stdout.strip().splitlines()[0])
    except Exception:
        return -1


def tcall(m, tool, args):
    r = _result(m.call_toolset(A, tool, args))
    if r.get("isError"):
        raise SystemExit(f"{tool} failed: {r['content'][0]['text'][:300]}")
    return json.loads(r["content"][0]["text"])


def capture(m, label):
    cam = tcall(m, "GetCameraTransform", {})["returnValue"]
    inner = tcall(m, "CaptureViewport", {"captureTransform": cam, "annotations": ANN, "bShowUI": False})["returnValue"]
    raw = base64.b64decode(inner["image"]["data"])
    os.makedirs(OUT, exist_ok=True)
    ts = datetime.datetime.now(datetime.UTC).strftime("%Y%m%dT%H%M%SZ")
    p = os.path.join(OUT, f"capture_{ts}_{label}.png")
    open(p, "wb").write(raw)
    return p, inner["cameraLocation"]


def main():
    m = MCP(); _result(m.initialize())
    base_vram = vram_used_mib()
    print(json.dumps({"baseline_vram_mib": base_vram}))
    running = tcall(m, "IsPIERunning", {})["returnValue"]
    if running:
        tcall(m, "StopPIE", {}); time.sleep(2)
    # Start PIE in the level viewport; warm up 6s (skips shader-compile hitches).
    tcall(m, "StartPIE", {"options": {"bSimulate": False, "playMode": "PlayMode_InViewPort", "warmupSeconds": 6}})
    print("PIE started")
    peak = base_vram
    frames = []
    marks = {8: "flight-early", 20: "flight-recenter", 34: "flight-far-near"}
    t0 = time.time()
    captured = set()
    while True:
        el = time.time() - t0
        peak = max(peak, vram_used_mib())
        for at, lab in marks.items():
            if el >= at and lab not in captured:
                try:
                    p, loc = capture(m, lab)
                    frames.append({"t": round(el, 1), "label": lab, "path": p, "camloc": loc})
                    print("captured", lab, p)
                except SystemExit as e:
                    print("capture skip", lab, e)
                captured.add(lab)
        if el >= 38:
            break
        time.sleep(1)
    peak = max(peak, vram_used_mib())
    tcall(m, "StopPIE", {})
    print(json.dumps({"peak_vram_mib": peak, "baseline_vram_mib": base_vram,
                      "delta_vram_mib": peak - base_vram, "frames": frames}, indent=2))


if __name__ == "__main__":
    main()
