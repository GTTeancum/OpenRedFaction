# Generated Auto Turret native check

`tools/xemu_auto_turret.py` is prepared but has not been executed. It copies the complete installed `levels1.vpp/L3S2.rfl` Auto Turret base UID1994 record (offset1942104,209 bytes), preserving its authored80 health and every non-transform byte. CTF06 contains this single authored base and **no authored Head record**. Head creation, class loading, registration, attachment, activation and fire must come from normal runtime code.

The base is staged6 metres along the clear fixture axis and0.8 metres sideways, at the CTF06 settled player-body floor height observed in the prior seat save. This is a fixture placement, not a claim that the original base model has the player's origin convention. Original CTF06 scripts/triggers are removed; pickups remain. The harness runs120 neutral frames, reads a live frame30 probe and a final sample, and restores staged files in `finally`. No synthetic damage/readiness flag, host input, images or campaign traversal is used.

Checks require exactly one created head, role1 under base UID1994, distinct valid registry handles, sentinel authored UID/seed values, intact base linkage, orientation-lock flag0x100, positive head health and the unchanged base health. Head position must match the evaluated skeletal `interface_1` world position, while successful publications preserve an independently aimed head basis. Actual acquisition, turning, shots and damaging player hits are required, alongside zero base handheld shots, no native errors and available stock64MiB memory. A successful model submission alone is insufficient. Player death is permitted only in this attack harness.

The generated include now exposes these counters, which survive teardown and reset at the first factory attempt of the next generated level:

| `rf_scene_turret_generated_stats[8]` index | Meaning |
|---|---|
|0|Successful head creations|
|1|Successful attachment-position publications|
|2|Publications whose position changed|
|3|Publications preserving a head basis different from the current base basis|
|4|Successful ready queries (normally one per combat tick)|
|5|Base-origin coupled-death dispatches|
|6|Head-origin coupled-death dispatches|
|7|Factory/publish/death-callback errors reaching instrumented failure paths|

Early argument/identity rejections still return ordinary errors to the caller; this counter is not an exhaustive error ledger.

| `rf_scene_turret_generated_probe[32]` indices | Last successful publication |
|---|---|
|0–1|Base UID, generated role|
|2–3|Base and head registry handles|
|4–6|Head authored UID, seed index, linked host handle|
|7–8|Base/head health float bits|
|9–11|Base AI flags7d0, head object flags7c, interface tag|
|12–14|Published head world position float bits|
|15–17|Evaluated interface world position float bits|
|18–26|Independently retained head basis float bits|
|27–29|Published base position float bits|
|30|Living registered generated-head count at publication, excluding dead owners and reserved slots|
|31|Last publication/callback status|

The probe retains the last live sample during teardown; it is not a post-close registry inventory. The runtime must publish after evaluated base animation and before head combat/contact/draw. The position-change counter is reported but not required: a genuinely static authored idle pose must not be called broken merely because no artificial animation was forced. A zero count leaves motion over an animated transition unverified. Save/load, either-side death, visual animation appearance and audible output remain separate work.

## Native result (2026-09-30)

`artifacts/xemu/auto-turret-20260930-134316/report.json` passed120 frames on stock64MiB: one generated head,119 attachment publications,65 position changes,89 independent-aim publications,11 shots/hits, no base handheld shots, and3517 free pages. Player health went from100 to-9.6. Disc inputs restored. This establishes generated ownership/attachment/combat from guest state; no visual/audio, coupled-death or save/load claim. After this run, translation publication was moved before projectile/NPC contacts; the ordering change needs the next focused check.
