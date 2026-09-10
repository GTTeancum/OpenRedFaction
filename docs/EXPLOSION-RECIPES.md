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
