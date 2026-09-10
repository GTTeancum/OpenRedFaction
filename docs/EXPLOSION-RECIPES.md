# Explosion recipe reconstruction

Original RF.exe SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

`tools/verify_explosion_defaults.py` executes original `48dd90` with parser,
string-copy and emitter-name lookup return values supplied. Its 256 cases cover
0, 1, 2 and 6 central emitters and all combinations of six optional fields.
This verifies loader writes and branches, not the original parser or live effects.

The original definition has stride 0x98 at 75e520; active definition index is
75ec44. The six central handles start at +2c, process bytes at +44, minimum-size
floats at +4c, play-time factors at +64 and central count at +7c. Process bytes
are stored without an additional boolean normalization. Absent minimum size is
zero; absent play-time factor is FLT_MAX (0x7f7fffff).

The central random-position factor at +90 is a single shared float. Each central
entry overwrites it, including writing zero when its optional field is absent.
With no central entries, it is untouched. It must not become an independent
per-emitter value in the reconstruction.

Sparks handle +80 defaults to -1 and count +84 to zero. A present sparks block
resolves the emitter and reads its required integer count. Head handle +24 and
tail handle +28 default to -1. Head time +8c and random-position factor +94 are
untouched when the head is absent; with a head, time is required and absent
random-position factor becomes zero. Explosion play time is at +88.

Missing central emitter resolution branches to the fatal diagnostic at 48dff5;
that branch is not executed by the verifier. Optional sparks/head/tail lookup
failure behavior and the general emitter lookup remain separate recovery work.
Six slots follow from record offsets, not a verified runtime overflow guard.

The authored charge_explode vclip selects rocket hit, whose central emitters
are ordered: explosion main part 3, explosion boom, flamethrower fire_large,
explosion flare, explosion main_smoke center, flamethrower_explode_large. Its
sparks emitter is explosion random bits 2, with count 20. The bounded recipe reader now retains this metadata. Resolved emitter
ownership is implemented; live execution remains unimplemented.

The shared recipe reader is verified against all nine authored recipes on PC
and compiled NXDK. Optional absent fields are zeroed only in owned metadata
and marked absent; they are not asserted to be original runtime defaults.
The reader rejects a seventh central emitter and preserves output on errors.

Resolved metadata owns nine fixed emitter slots: central 0..5, sparks 6, head 7,
and tail 8. A bit mask identifies resolved slots. Archive loading reuses one
scratch buffer and releases it before returning; no archive pointers remain.
All nine installed recipes resolve successfully. Missing central names return
an error; missing optional names remain unresolved, without creating particles.

Native 64-MiB XEMU replay 20260910-081353 hashes all nine resolved definitions
and the charge_explode vclip against PC. Original emitter lookup 497550, its
string helper 5001d0 and CRT comparator pass 146 cases. The optional recipe
reference explosion random bits 2 is absent from installed emitters.tbl and
returns -1 in the original lookup. Rocket hit therefore resolves central slots
0..5 only (mask 63), not its sparks slot. The initial harness expectation of
mask 127 failed and was corrected after this original-code verification.

Creation entry 48e640 is called from the code_explode path in 4c16e0.
The span 48e7c5..48e879 skips central emitters below their minimum size
(including unordered comparisons), and scales min/max velocity, min/max
radius and min/max life by explosion size. It also multiplies the shared
central random-position factor by size. All these outputs match the shared
owned-copy helper in 600 original/PC/NXDK fixtures. This does not yet create
emitters, sample positions or execute the explosion update loop.

Update function 48e290 adds frame delta 5a4014 to elapsed +20 before work.
Central slot processing requires a live pointer, process byte exactly 1, and
elapsed strictly below play_factor * size. Recipe expiry uses elapsed strictly
greater than play_time, after central processing, and releases all live central
emitters. Shared central-only timing matches 700 original update fixtures
(164 expirations) with process/release callbacks intercepted without mutation.
Trail updates, callback mutation, active-list ownership and scene phase remain
outside this helper; callers must process selected slots before releasing them.

Emitter creation 497ca0 calls 497020; the central update calls 4972f0.
Alternating phase duration 496f60 chooses on/off timing by nonzero enabled
byte, consumes one draw through 504db0/57312d, and computes
(base - variance) + 2 * (draw / 32768) * variance before rounding to float
and clamping to 0.1f. Duration and RNG state match in 1140 PC/NXDK cases
against these unchanged original functions with only the CRT thread pointer
supplied. This helper does not yet construct or process an emitter.

Emitter update 4972f0 first checks the low byte of global enable at 59fd1c.
With alternation flag 0x20, it adds frame delta, toggles when elapsed reaches
or exceeds duration, discards all overshoot and calculates one new duration.
Only the enabled low byte is changed; a single update never toggles twice.
Enabled continuous emitters (flag 4) bypass the timer query. Other enabled
emitters request emission only when the timer query returns a nonzero low
byte. Disabled emitters neither query the timer nor request emission.

The shared clock/actions helper matches 2560 original/PC/NXDK cases with
592 toggles and 609 emission requests. Fixtures cross global enable, flags,
enabled byte, delta and timer result independently, with varying clock bounds.
The original phase-duration and RNG routines execute unchanged. The timer
result is supplied, emission is intercepted without mutation, and parent
lookup returns null. Phase calculation precedes timer query and emission;
the original parent lookup follows them. This verifies decisions, state and
RNG advancement, not live particle execution. Parent attachment, emission
callback effects and spawn-timer reset must be integrated separately.
