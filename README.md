# SpaceTravel

Seed-driven **endless procedural space world** in **Unreal Engine 5.8**, built by a Claude engineer
paired with GPT **Sol** (procedural-world advisor · image generation · image-recognition critique).
Epic `epic-733b165bd7`. World first, features later.

## Project shape (S1 walking skeleton)

- **C++ project** `SpaceTravel.uproject`, Engine 5.8. Targets: `SpaceTravel` (Game), `SpaceTravelEditor` (Editor).
- **Minimal plugin set** (not `AllToolsets`): `ModelContextProtocol`, `ToolsetRegistry`, `EditorToolset`,
  `Niagara`, `NiagaraToolsets`, `PythonScriptPlugin`, `EditorScriptingUtilities`.
- **Unreal MCP** autostarts an in-editor server at `http://127.0.0.1:8000/mcp`
  (`Config/DefaultEditorPerProjectUserSettings.ini` → `bAutoStartServer=True`).
- **git-lfs** routes `.uasset/.umap/.png/.tga/.exr/.hdr/.fbx/.wav/.psd` (see `.gitattributes`);
  `Binaries/ Intermediate/ Saved/ DerivedDataCache/` ignored. Provenance `*.manifest.json` sidecars stay in git (text).
- Generated assets: Sol `image_gen` PNG + `<basename>.manifest.json` → `Content/Generated/Source/` → imported to `/Game/Generated/`.

## Static [VERIFY@S1] resolved against the install (`C:\Program Files\Epic Games\UE_5.8`)

| Item | Finding | Evidence |
|---|---|---|
| UE version | 5.8.0, CL 55116800, `++UE5+Release-5.8` | `Engine/Build/Build.version` |
| Experimental toolset count | **27** `.uplugin` under `Plugins/Experimental/Toolsets/` | `ls .../Toolsets/*/*.uplugin` |
| `CaptureViewport` return | `FViewportCapture.Image` = base64-encoded PNG (empty on failure); no filename/resolution args | `EditorToolset/.../EditorAppToolset.h:164,170,246` |
| MCP settings class | `ModelContextProtocolEngine.ModelContextProtocolSettings` (port 8000, path `/mcp`) | scenic_world `DefaultEditorPerProjectUserSettings.ini` |
| GPU / VRAM | RTX 4050 Laptop, **6141 MiB** (budget 6 GB, not 4 GB) | `nvidia-smi` |

Items needing a **running editor** (MCP reachability, live `CaptureViewport`, flight, benchmark) are proven in S1 phases B–E.

## License
Apache-2.0 (see `LICENSE`).
