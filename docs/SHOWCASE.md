# README scene capture

The README image is a native 1920 x 1440 offline PC render, not an Xbox capture
or an enlarged 640 x 480 framebuffer. It uses original assets through the shared
C reconstruction. The camera faces the Live Mines guard-station doorway
(mover 8544), with a staged miner near it. This location is identified by the
visible sign; its equivalence to the requested main hall is not established.

`rf_scene_showcase_enabled` applies a static diagnostic placement: miner UID
9858 faces the camera, and both pairs of exit panels receive half their authored
endpoint displacement from L1S1 section 3000 (keys 8591..8594, 8603..8606).
It does not run NPC AI, door triggers, or navigation. All assets stay unchanged.
The default diagnostic profiles are unchanged when this mode is disabled.

Reproduce from the project root in PowerShell:

```powershell
cmake --build build/pc --config Release --target rf_pc_preview
$env:RF_PREVIEW_SCALE = '3'
./build/pc/Release/rf_pc_preview.exe --scene-showcase Installed_Game/levels1.vpp L1S1.rfl artifacts/showcase-hires.ppm Installed_Game/meshes.vpp Installed_Game/motions.vpp Installed_Game/tables.vpp 9858 Installed_Game/maps1.vpp Installed_Game/maps2.vpp Installed_Game/maps3.vpp Installed_Game/maps4.vpp Installed_Game/maps_en.vpp
python -c "from PIL import Image; Image.open('artifacts/showcase-hires.ppm').save('artifacts/showcase-hires.png')"
```

RF_PREVIEW_SCALE accepts 1..4 (640x480 to 2560x1920), preserving 4:3 framing.
Only the offline PC rasterizer's output grid changes. Geometry, UVs, material
sampling and lightmaps are unchanged; the default remains 640x480 for Xbox
pixel comparisons. High-resolution capture buffers are not Xbox allocations.
The stored capture used 3,434 world triangles and 437 visible actor triangles.
Current static-world backface rejection reduces submitted geometry; reproducing
the 1920x1440 image changes two pixels. The stored screenshot is retained.

The optional Xbox `showcase.flag` camera binding builds but has not been tested
in live XEMU. Other scene/actor streaming flags must be absent for a static
scene. The existing smoke harness does not yet recognize this profile's draw
counts; do not claim its standard scene validation covers this composition.
