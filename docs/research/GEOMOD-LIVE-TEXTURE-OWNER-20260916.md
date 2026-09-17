# Live crater base texture owner

Both settled checkpoint replays retain exactly the installed rock02 pixels in
the scene's actual material owner. No material replacement, brightness gain,
channel correction or mip generation follows from this evidence.

`RF_REPLAY_TERRAIN_MATERIAL_AUDIT` is an opt-in PC diagnostic. At replay cleanup
it exports the retained substrate material ID, width, height, original format,
byte count and logical RGBA pixels. RFT1 consists of magic plus five little-endian
32-bit fields followed by RGBA8. Unsupported packed images fail explicitly.
The diagnostic is excluded from Xbox compilation and performs no allocation.
It does not intercept draws or read GPU memory.

`tools/check_geomod_material_owner.py` restores the selected checkpoint, executes
32 neutral process-local frames and exports the actual image, physical mesh and
live atlas. It independently decodes the installed uncompressed TGA, honoring
horizontal and vertical orientation, and compares all RGBA bytes. It also checks
every generated physical face and every chart for the same material ID and
checks that chart face-reference totals equal generated face count.

| Replay | Material | Generated faces | Maps | Live bytes matching asset |
|---|---:|---:|---:|---:|
| fixed16 |49|1486|1267|262144|
| swept14 |49|1128|1086|262144|

Both images are256x256, source format6. RGBA SHA256:
`ee9eddbff96108ca000351e02673d7b82b65b7d44e61da8538c1be6fafcfb258`.
Installed rock02.tga SHA256:
`b09c580e4aba00db4a35dbfc6ee7a7ab5d7da2159eb10d366d46e55a8679b6b1`.
Mean RGB is43.434967,40.311310,39.127991 out of255. Alpha matches255 throughout.
Reports with executable/checkpoint hashes are in
`artifacts/geomod-material-owner/report.json` and
`artifacts/geomod-material-owner-sweep/report.json`.

Validation: default and expanded PC targets build successfully with existing
scene warnings; both checkpoint audits pass. No rendering behavior changed.
The resulting frames were not visually reviewed for this numeric ownership
check; it makes no new visual, audio, gameplay or Xbox acceptance claim.

Existing recovered sampler research already establishes linear min/mag,
repeating base UV and clamped lightmap UV. The current backends use one mip
level; original rock02 mip generation remains conditional on unresolved load
policy. Mips affect minification, not a missing brightness multiplier. Combined
with the idle lightmap audit, current evidence rules out wrong CPU base pixels
and uninitialized CPU lightmaps in these two fixtures. Actual Xbox upload/
binding and original later lighting lifecycle remain open boundaries.
