# Head-only scripted Auto Turret removal

The existing gameplay implementation already admits a generated head in `campaign_remove_object` through `scene_turret_generated_remove`: reason2/handled1 hides and detaches the head while preserving its health and registration, keeps the base alive and registered, and suppresses orphaned firing. The new isolated helper exercises this branch through a real registered type2 Remove_Object event. It does not duplicate the retirement implementation or add a new authored child UID.

At frame60, `scene_turret_generated_remove_fixture` resolves `(baseUID,role1)` into the actual generated head handle. It creates a temporary registered event with one resolved object link, fires `rf_runtime_event_fire` through the existing campaign trigger backend, then unregisters that temporary event before returning. No object health is written and no Slay/death code is invoked. The resolved runtime link is diagnostic staging, not a claim that an installed RFL event names the UID-less child.

## Integration

Include new `scene_turret_generated_remove_fixture.inc` beside the other turret diagnostic fixture includes, after generated retirement APIs and `campaign_remove_object` are defined. Call `scene_turret_generated_remove_fixture(frame)` beside the existing fixture tick and propagate failure. Expose `extern uint32_t rf_scene_turret_head_remove_uid;` to Xbox main. Read optional `D:\\campaign-auto-head-remove.bin` as exactly one nonzero uint32, following campaign-turret-test.bin; zero/default disables all diagnostic work. Add the new flag to native harness FLAGS. No change to normal runtime removal or retail event resolution is required.

Parent runs `python tools/xemu_auto_turret_head_remove.py` after serial integration. It reuses the real L3S2 base1994 copied to isolated CTF06, enables only this new flag, executes120 neutral frames and samples after75, then restores all disc controls in finally. Source-only prepared; no native result claimed.

Assertions: exactly one type2 event and one head-only retirement, zero completed base unregisters, base80 health/registered/not retired/not hidden, head60 health/dead0/registered/hidden/detached, readiness cleared and base catatonic, head target cleared, and no further head shots/draw submissions after removal. Coupled-death, shared base-damage, Slay and death-effect counters remain zero; all runtime errors remain zero. Head-only saves and visual/audio output are separate work. No images, host input, PC runtime or campaign routes are used.

The24-word probe layout is documented beside `rf_scene_turret_head_remove_test` in the include. It samples surviving base state each frame after removal so stable head registration alone cannot conceal a removed or damaged parent.

## Xbox result

The parent integrated the fixture and stock-64-MiB XEMU passed `artifacts/xemu/auto-turret-head-remove-20260930-145349/report.json`: one real type2 runtime Remove_Object event hid/detached the head, preserved the living registered base and the head’s health, and stopped further head shots without invoking death effects. Free pages: 3514. Save continuity and visual output are outside this check.
