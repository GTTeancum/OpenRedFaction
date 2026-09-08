# Xbox-first reconstruction boundaries

The current executables are diagnostics. They do not load or play the campaign.

The shared C/C++ core owns archive access, reconstructed behavior, resource
accounting, world state, simulation, and renderer-facing data. Xbox and PC adapters
own filesystem integration, clocks, input, audio output, display, and GPU commands.
No original Windows import, absolute function call, or original process is required
by reconstructed runtime code. Original addresses belong in analysis evidence.

Use explicit-width serialized fields; do not serialize pointers or native structs.
Build PC as 32-bit x86 initially to expose width assumptions. A PC graphics backend
must exercise the same materials, resource formats, visibility, and budgets as Xbox.
The diagnostic PC executable is currently a console asset tool, not that renderer.

## Memory

64 MiB is the total physical pool shared by CPU and GPU. The original PE declares
29,917,184 bytes of image space, including a 28,260,480-byte virtual data section.
That declaration does not measure resident gameplay usage; identify its large
arrays and distinguish necessary live state from fixed-capacity PC allocations.

Provisional planning envelope, to be replaced with measured peak accounting:

| Use | MiB |
| --- | ---: |
| Kernel, drivers, executable, stacks, runtime overhead | 12 |
| Video surfaces and GPU command/storage overhead | 6 |
| World geometry, collision, visibility, Geo-Mod state | 14 |
| Textures and lightmaps | 16 |
| Actors, animation, scripts and gameplay state | 6 |
| Audio buffers and decode working memory | 4 |
| Streaming/decompression transients | 2 |
| Uncommitted safety margin | 4 |
| Total | 64 |

These are initial engineering budgets, not allocations or proof of fit.
Track peaks including fragmentation and contiguous GPU allocations. Do not cache
whole VPPs: the largest installed archive alone exceeds the hardware's memory.
The initial VPP reader keeps a small handle and a single directory entry on stack;
stdio has its own implementation-defined overhead. Lookup is linear for now.

## Graphics and validation

Start with the Xbox NV2A/pbkit capabilities and conventional textured geometry,
lightmaps, vertex lighting, alpha blending and period-appropriate effects. Choose
the final backend after the first level's material and geometry audit. Resource
changes must retain PS2 visual parity; the smaller PS2 resource memory does not
establish that unmodified PC asset layouts fit Xbox memory.

A provisional 640x480 presentation and 30 fps target provide a starting point,
not evidence of PS2 parity or an accepted final performance specification.
Compare matched scenes, materials, lighting, effects, actors, HUD and destruction.
XEMU must use 64 MiB; prepare actual hardware tests after emulator validation.
Only native emulator captures, logs and process-contained test harnesses are used.
