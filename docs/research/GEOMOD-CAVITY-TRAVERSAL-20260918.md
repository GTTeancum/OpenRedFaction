# Ordinary player traversal of a cavity crater

`tools/check_cavity_traversal.py` creates an ordinary single rocket cut at wallX-33,Y0.5,Z8, walks off the authored walkway toward the wall and jumps once. The unchanged source66 radius4 developer hardness patch is used; no collision override, teleport, host input or synthetic support is installed.

The intact-wall control uses identical aim/movement/jump inputs without firing. Its body stops at(-32.395847,-1.118479,8), outside the wall. With the crater, the player settles at(-33.392170,0.228207,7.917564), behind the original wall plane. The3818-byte checkpoint is written there. Reloading and ordinary backward movement returns the body to(-25.603197,-1.118479,7.917564). The entire resulting checkpoint equals uninterrupted play, and the player survives in all cases.

PC inside/returned framebuffers were inspected independently. The inside view shows the close rock interior; the return view shows the crater in the wall, retained floor, surrounding wall, launcher and HUD. Audio was not assessed. Assertions use body state, cut count, player survival, intact control and exact checkpoint equality; the screenshot alone does not prove traversal.

This tests one low crater and its standing save. It does not prove broad tunnel clearance, arbitrary slope traversal or complete world geometry support. The high three-shot depth test remains separate from this low opening. No new image is published to GitHub.

Xbox entry: `artifacts/xemu/render-20260918-001022`,1200frames,77checks passing,4024free pages at endpoint. The Xbox-created3818-byte save matches PC exactly. Native inside framebuffer inspected. The harness restored its disc and closed the emulator.

Xbox-created-save walk-out: `artifacts/xemu/render-20260918-001235`,301frames,76checks passing,4072free pages at endpoint. Native returned framebuffer inspected. Output equals the uninterrupted PC3818-byte checkpoint. Disc restored and emulator closed. These are endpoint free-page measurements, not peak-memory guarantees.
