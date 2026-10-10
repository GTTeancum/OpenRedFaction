# Authored sound-only prop breaks

Source-written on 2026-10-09 from parent `db5aa27f`, in an isolated worktree. The
worker changed the two existing clutter-break includes and this note; the
parent-owned `scene.c` definition-load hook is supplied separately. No build,
test, original-code execution, new fixture, emulator run, capture, cleanup or
remote publication was performed. Compilation and runtime behavior remain
unverified for the parent's consolidated batch.

## Original data establishes a sound-only effect

Direct, read-only inspection of installed `tables.vpp` found vclip slot 62,
`grate break`, containing empty `$Flags` and only `$Foley Sound: "grate break"`.
It has no VFX filename, VBM filename, central-explosion recipe or particle
block. The existing `rf_vclip_definition_read` defaults its damage to zero.
There is no missing visual decoder for this specific vclip.

The existing case-insensitive Foley lookup resolves that name to `Grate Break`,
whose single authored sample is `Metal_hit_Metal3.wav`, near distance 5 and
volume 0.9. Read-only `audio.vpp` member inspection found a 19,666-byte RIFF
file with 19,554 bytes of mono, 16-bit, 11,025-Hz PCM data, using ordinary WAV
format 1 already handled by the audio bank. No sample extraction or playback
was performed.

`Duct Grate Cover` has life 30, `collide_object`, this explosion name and debris
velocity 1. Its vclip damage remains zero, so this change creates no blast.
Separate authored mesh debris is not established by a vclip sound: the `grate`
class names `grate_debris.v3d`, while `Duct Grate Cover` has no explicit debris
filename. Existing generic model-break/debris behavior remains outside this
bounded sound-only change. No substitute explosion, particles or fragments are
created.

## Existing-owner integration

- The existing class loader already resolves each class's explosion through
  the original 64-slot vclip name catalog, using ASCII-insensitive first-match
  lookup, then loads each used definition once for its damage. The scene hook
  additionally binds sound-only metadata during that same load. It neither
  reparses per prop nor hardcodes `grate break` or its spelling. Capitalized
  authored `Grate Break` references therefore reach the same vclip identity.
- `scene_clutter_break_foley_bind` accepts only complete sound-only definitions:
  an authored Foley field, zero flags and damage, no VBM/VFX/explosion resource
  and no particle block. It resolves the actual Foley name through
  `rf_foley_find` (original `434cb0`). Missing names retain the ordinary absent
  result. Unsupported visual effects do not acquire a partial audio fallback.
- A fixed 64-entry table retains the resolved group plus one, with zero meaning
  absent. This adds exactly 256 bytes of scene metadata, no per-class or
  per-placement bytes and no allocation. Teardown clears the table alongside
  the existing clutter-break effect owners, before the borrowed Foley owner
  is released. Repeated close is harmless.
- The existing deferred break pass consumes its current one-shot
  `break_pending`, validates the retained generation/health/class and gates on
  the current class's 50-ms timer. It calls the existing `combat_sound` path
  with the bound canonical Foley name and the same transformed class-offset
  argument. `combat_sound` currently uses a nonspatial player-style sound
  request even for existing prop breaks; this inherited routing is preserved,
  and positional-audio fidelity is not claimed.
  An `else` keeps yellboom/oil-drum audio exclusively in their existing visual
  owners. Their materials, instances, particle tags and emission budgets are
  unchanged.
- Audio is still selected, lazily admitted and started through the existing
  sound bank, voice service and combat audio RNG. The 1,280-KiB bank ceiling is
  unchanged; a cold grate sample costs 19,666 bytes inside it. Existing reload
  and idle-eviction safeguards remain authoritative. Missing or exhausted
  audio cannot undo death, delay removal or trigger an audio retry.

## One-shot and continuation boundary

Only existing ordinary lethal damage sets `break_pending`; subsequent damage
rejects retired bit 2. The deferred pass clears the request before attempting
sound. Scripted removal clears it without creating a health break, and ordinary
checkpoint restoration explicitly clears it. A loaded dead prop therefore does
not replay its break. The existing shared-class cooldown can suppress a second
same-class death within 50 ms; this change does not promise one audible sound
for every death or change that policy. It admits at most one sound request for
an accepted pending health break.

The conditional `CLUTTER_BREAK_FOLEY` trace records UID, frame, vclip and Foley
group for a request, not successful output. Existing combat-audio counters and
native voice/output evidence remain the authority for playback. The original
L3S1 grate/contact/removal check can exercise this route in the parent's planned
batch, but neither audible output nor stock-64-MiB runtime cost is verified here.

No damage, retirement, collision, checkpoint format, GeoMod, model-switch or
prop-debris owner was changed. Broader prop visuals and active-effect saves are
not claimed complete.
