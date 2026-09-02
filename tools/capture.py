#!/usr/bin/env python3
"""CaptureViewport -> unique PNG on disk (the MCP->file->consult bridge, adversary #7).

The EditorToolset `CaptureViewport` tool returns the frame as an inline base64 PNG nested inside
the tool result, so a capture is useless to `consult(images=[...])` until it is a real file. This
helper drives the running editor's MCP server and writes a uniquely named PNG under
Saved/Captures/ (git-ignored — captures are evidence, not source), plus a `.cam.json` sidecar with
the exact camera pose/FOV so the frame carries the Shape-2 evidence bundle (seed/pose reproducibility).

Learned live in S1 (EditorAppToolset quirks, UE 5.8):
- `CaptureViewport` marks captureTransform / annotations / bShowUI optional in its schema but the
  implementation rejects the call unless ALL THREE keys are present. So we always send the current
  camera transform (from GetCameraTransform) and a fully-disabled annotation config.
- The result is `{content:[{type:"text","text":"<json-string>"}]}` where the text is a JSON string
  whose `returnValue.image.data` is the base64 PNG (`returnValue.image.mimeType` = image/png).

Usage:
    python tools/capture.py                         # capture from the current viewport camera
    python tools/capture.py --label r0-loop         # name the file
    python tools/capture.py --out-dir Saved/Captures
"""
from __future__ import annotations

import argparse
import base64
import datetime as _dt
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mcp_client import MCP, _result  # noqa: E402

TOOLSET = "EditorToolset.EditorAppToolset"
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"
# grid off (gridSpacing 0), labels off (maxLabelDistance 0) -> clean frame for critique
_ANNOTATIONS_OFF = {"gridSpacing": 0, "gridExtent": 0, "gridHeight": 0,
                    "maxLabelDistance": 0, "classFilter": None, "maxLabels": 0}


def _text_result(m: MCP, tool: str, args: dict) -> dict:
    res = _result(m.call_toolset(TOOLSET, tool, args))
    if res.get("isError"):
        raise SystemExit(f"{tool} failed: {res['content'][0]['text'][:400]}")
    return json.loads(res["content"][0]["text"])


def capture(out_dir: str, label: str | None, show_ui: bool) -> str:
    m = MCP()
    _result(m.initialize())
    cam = _text_result(m, "GetCameraTransform", {})["returnValue"]
    args = {"captureTransform": cam, "annotations": _ANNOTATIONS_OFF, "bShowUI": show_ui}
    rv = _text_result(m, "CaptureViewport", args)["returnValue"]
    img = rv["image"]
    raw = base64.b64decode(img["data"])
    if not raw.startswith(PNG_MAGIC):
        raise SystemExit(f"CaptureViewport returned non-PNG data (mime={img.get('mimeType')})")

    os.makedirs(out_dir, exist_ok=True)
    ts = _dt.datetime.now(_dt.UTC).strftime("%Y%m%dT%H%M%SZ")
    stem = f"capture_{ts}" + (f"_{label}" if label else "")
    path = os.path.abspath(os.path.join(out_dir, stem + ".png"))
    with open(path, "wb") as fh:
        fh.write(raw)
    # Shape-2 evidence sidecar: the pose the frame was rendered from.
    cam_meta = {
        "png": os.path.basename(path),
        "cameraLocation": rv["cameraLocation"],
        "cameraRotation": rv["cameraRotation"],
        "cameraFOV": rv["cameraFOV"],
        "captured_utc": ts,
        "mimeType": img.get("mimeType"),
    }
    with open(path[:-4] + ".cam.json", "w", encoding="utf-8") as fh:
        json.dump(cam_meta, fh, indent=2)
    print(json.dumps({"ok": True, "path": path, "bytes": len(raw),
                      "cameraFOV": rv["cameraFOV"]}, indent=2))
    return path


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", default=os.path.join(PROJECT_ROOT, "Saved", "Captures"))
    ap.add_argument("--label", default=None)
    ap.add_argument("--show-ui", action="store_true", help="include gizmos/UI in the frame")
    args = ap.parse_args(argv[1:])
    capture(args.out_dir, args.label, args.show_ui)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
