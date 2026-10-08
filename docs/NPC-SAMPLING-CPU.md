# NPC motion sampling CPU

Source-written 2026-10-08 against a39fc09. No helper builds, tests or emulator
runs. Parent owns the next consolidated Xbox validation. No FPS gain claimed.

## Existing evidence

The parent's 17:00 original L1S1 stock-64-MiB neutral-spawn run records 3.489 ms
in NPC pose advance/evaluation, with 29 shared-pose hits and six misses in the
sampled tick. This is the prior baseline, not a post-change measurement.

Inspection of the existing Xbox motion_file.obj shows nine out-of-line
four-byte memcpy calls in each decoded position key, plus a resident-payload
copy. Resident binary-search probes and track descriptors also copy into
temporary byte arrays. The existing model object retraces every bone's whole
parent chain and rescans all bones once per depth to construct evaluation order.

## Bounded changes

- Resident motion reads return an immediate borrowed byte view after the same
  offset/size guard. The archive fallback retains the same read calls, offsets,
  sizes and caller-owned scratch. No pointers survive the decode and no new
  lifetime/cache ownership is introduced. All metadata and key guards remain.
- Float decoding spells its fixed representation copy as __builtin_memcpy on
  Clang/GCC, preserving alias safety and raw binary32 values while allowing the
  freestanding Xbox compiler to omit the nine libc calls. Other compilers keep
  the previous memcpy implementation.
- Bone ordering memoizes each completed depth within the call, detects cycles
  with an in-progress marker, and uses stable counting order. The output is
  still increasing depth followed by increasing original bone index. Invalid
  parents/cycles still fail before publishing any order. All 256 bones remain
  supported. Scratch grows by 512 bytes, with no persistent storage/allocation.
- Within one playback evaluation, bit-identical consecutive envelope arrays
  reuse the last successful bone-weight result. Active slots, ticks, primary
  slot and looping mask are unchanged throughout that call. Every new track is
  still read/validated and every selected bone is still sampled, blended,
  composed, overridden and generation-stamped normally. This adds 320 bytes of
  call-local envelope storage; it has no cross-frame state.

The transform, interpolation and motion arithmetic functions are unchanged.
Their existing classifier calls and x87 spill boundaries stay in place. This
does not approximate trigonometry, suppress actors/geometry, skip simulation
or animation updates, alter sharing keys, or change the 64 MiB target.

## Parent validation still required

Inspect the newly built Xbox motion_file object for removal of resident/decode
memcpy calls. Existing model_tests bone-order cases, motion_file_probe resident
and archive modes, and shared pose/model probes are relevant if included in the
scheduled batch. Compare actual presented FPS and the same NPC pose phase on
the unchanged neutral L1S1 scene. Source/object inspection alone cannot quantify
the gain. No generated artifacts were produced by this helper.
