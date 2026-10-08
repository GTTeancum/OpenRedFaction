# Player motion residency

Status, 2026-10-08 after 16:00: source-written against frozen a653795.
No build, test, emulator, image or host input was run by this helper. The parent
owns the next consolidated stock-64-MiB Xbox validation. No FPS gain is claimed.

## Source finding and measured boundary

The parent's 16:00 run measured 17.35086 FPS; its pre-camera phase still averaged
4.5 ms. The earlier player-room/diagnostic patch is already in that build.
This finding does not isolate how much of that phase is archive I/O.

The player state-set loader calls rf_motion_file_open and retains an archive
pointer, without binding resident payloads. animation_run passes those files to
the ordinary bone evaluator. motion_read therefore reaches rf_vpp_read, which
performs fseek plus fread for every track header, key search and key decode.
NPC playback already has its own bounded clip-residency owner; player playback
has no such owner in a653795.

Read-only directory/table inspection of installed inputs identifies the base
miner stand clip as ult2_stand.rfa (3,820 bytes) and attack_stand as
ult2_attack_stand.rfa (3,732 bytes). Both rigs use the existing 25-bone model.
These are asset/source facts, not a runtime profile or speedup measurement.

## Implementation and invariants

animation_motion_residency.inc adds an optional owner local to animation_run:

- Only campaign-player streams use it. It is independent of diagnostic flags,
  visibility, input, particular level names and benchmark frame numbers.
- The same live active-slot list chooses which authored files become resident.
  All active slots are retained, even zero-weight ones the evaluator can read.
- It copies the whole immutable source entry and uses existing
  rf_motion_file_bind_memory header/size validation. The exact same sampler,
  key selection, interpolation, controller, phase, generation and pose math run.
- Sampled descriptors are private copies. The caller's state-set descriptors,
  resource references, input files and original archive ownership are unchanged.
- The payload ceiling is 128 KiB; there are 23 metadata entries. Metadata owner
  size is published with sizeof (4,052 bytes on the 32-bit Xbox ABI). The initial
  stand/attack pair together needs only 7,552 payload bytes. Allocation is lazy.
- If more space is required, only inactive clips may be evicted. Current-frame
  descriptors cannot be freed after they are selected. Missing capacity,
  allocation/read/bind failure keeps the original archive-backed path. Failed
  allocation or I/O is not retried every frame for the unchanged descriptor.
- A changed form replaces source-file identities and releases their private
  copies before selecting the next set. The ordinary neutral class/sphere/eye
  preparation remains unchanged. No model data is skipped.
- Normal return, requested stop and all error exits release every payload and
  the owner before archives are closed. There is no cross-stream static cache.

The model sampler's separate same-call track-reuse patch is compatible: this
changes the backing storage behind its existing file API, not sample arithmetic.

## Telemetry and pending verification

rf_animation_motion_residency[8] contains current payload bytes, peak payload
bytes, successful loads, resident active selections, archive fallbacks,
evictions, the payload ceiling and owner bytes. Current bytes return to zero on
cleanup; peak and counters remain available afterward.

The parent should inspect the same original L1S1 neutral-spawn FPS and
pre-camera timing, plus payload peak/fallback counts, in the scheduled batch.
Motion/sphere/camera parity can use existing checksummed cases; player-form
replacement, movement and lifecycle behavior still require ordinary validation.
No new test fixture was created. Bone-order micro-optimization and a tiny audio
pow deferral were set aside because the measured remaining player phase and
concrete archive-read path are higher priority.
