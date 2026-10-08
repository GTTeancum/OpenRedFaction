# Duplicate renderer resets, with full pre-reset lifetime guards

Post16 source candidate against frozen a653795. Not in that hourly input.
No build, test or emulator run. No overlap or FPS improvement is claimed.

## Material review correction

The initial candidate tried to remove pre-reset full-GPU-idle guards because
NXDK `pb_reset` waits for DMA Get to reach the recycled buffer head. Source
review found its void timeout path: `pb_jump_to_head` in the bundled
`lib/pbkit/pbkit.c:1536` prints a warning and breaks after100 kernel ticks even
if Get has not reached the head. Treating a return as proof of command-memory
consumption would be unsafe. That candidate was never integrated or run.

The corrected code retains `while(pb_busy()) {}` before every bounded
particle/HUD reset. No asynchronous command-buffer reuse is introduced.
Standalone calls and command-audit completion behavior remain unchanged.

## Remaining narrow reduction

The frame previously fully drained and reset at particle end, then immediately
fully drained and reset again at HUD begin. Particle end now just clears its
private state. HUD begin still performs the full idle guard and reset before
any further commands are written. Particle errors explicitly fully drain
before returning, so no error path can retire an in-use image owner.

Similarly, the explicit ordinary geometry fence immediately before particle
begin is redundant with particle begin's existing full pre-reset fence. The
standalone and command-audit fence is retained. Final HUD end still fully
waits for all drawing before capture publication, swap, simulation or resource
mutation, but does not perform an unnecessary reset. Its final64-fan segment
is bounded well below512KiB; the swap adds only a few commands, and the next
frame keeps its ordinary reset.

All GPU states, draws, shader work, texture/vertex lifetimes, image contents,
visibility, simulation, resolution, pacing and presentation counts are
unchanged. The64-fan bounds remain. `rf_xbox_particle_batch[1]` counts actual
command-buffer resets. The expected effect is modest command-management work
reduction, pending measurement. The stronger overlapping-GPU proposal remains
rejected without a checked DMA-consumption contract. Frozen16:00 a653795
remains untouched.
