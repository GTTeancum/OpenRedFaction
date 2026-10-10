# Capek Cane: optional contact and expiry audio

Source-written outside the active checkout and rebased on
`b2590497bab418615feb43b4437adb41f4f9f689`. Initial source staging used
`e601eef22349d0dc6f62a4a6b77e1625d5b824e9` while queued08 was frozen. The
parent08:00 build failed on a preexisting Snake vehicle-material declaration
boundary; b2590497 repairs that boundary behind the primitive material query
reused below. This slice was not included in that build. No compilation,
syntax check, test, XEMU/PC run, gameplay fixture, route, grant, forced event,
image or original-input change was performed by this worker. Parent owns
integration and the next scheduled Xbox validation. Playback, audible output
and stock64MiB admission remain unverified.

## Original evidence

Read-only `Installed_Game/RF.exe`, SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- Physical-shield contact calls presentation4c5820 at4c4f9b before durability
  subtraction4c4ff0 and shield-break handling4c501b. It returns4c5023 without
  forwarding the breaking hit into body/radial damage.
- Entity dispatcher4c5a35 resolves the captured target; unresolved identity
  returns4c5a48–4c5a51. Nano4c5a90–4c5b5d returns before ordinary direct,
  radial and impact sound, including the hit that depletes Nano.
- Ordinary entity direct damage4c6132 precedes dry material resolution4c6463,
  sample-minus-one Default fallback4c64d9–4c64eb and positional5056a0 at4c6509.
  Cane has no blast radius. No audio-driven damage or Nano prepass is needed.
- Shield/world presentation4c5912–4c5996 reads the retained material and
  chooses its dry/underwater table through4c98b0. That predicate accepts ONLY
  damage kinds1/2. Cane energy5 therefore uses dry sound even if a liquid
  flag were present. Spit's AP liquid-suppression policy must not be copied.
- Homing expiry is audible presentation, not an ordinary object impact:
  synthesized homing0x800000 passes mask0x814000 at4c6bb5 into4c6bd3;
  radius0 skips radial damage at4c6c55;4c6cb9–4c6cc8 copies projectile3c to
  its presentation point1b4;4c6cd5 selects Default material0;4c6cf9 calls
  presentation4c5820. This slice includes that optional sound only. It does
  not add an expiry collision, blast, crater, damage or visual effect.

`tables.vpp/weapons.tbl`, offset1013760, size108890, line1930 declares the
sole Cane impact group as Default `Riot Impact`. Other dry and all underwater
slots remain undeclared. This is distinct from `Riot Impact Default`,
`Riot Impact Flesh`, `Riot Impact Metal` and `Riot Impact Rock`.

`tables.vpp/foley.tbl`, offset645120, size82283, lines1673–1678 maps
`Riot Impact` to four samples, each near6/gain0.9. Read-only audio archive
headers show unsigned8-bit mono22050Hz PCM:

- Riot_Jolt_01.wav: offset244350976,22186 file bytes
- Riot_Jolt_02.wav: offset244373504,12972 file bytes
- Riot_Jolt_03.wav: offset244387840,16788 file bytes
- Riot_Jolt_04.wav: offset244406272,13234 file bytes

The total is65180 retained file bytes, within64KiB. The existing bank retains
complete WAV files and already supports8-bit PCM. `bluebeard.bty`15253–15275
has these four entries without loop flags; the loader rejects looping
metadata rather than inventing a loop for terminal feedback.

## Bounded admission and ownership

The existing shared impact catalog, Foley RNG, sound bank and generation-
qualified positional starter are reused. A separate4-new-row/64KiB optional
append runs once at the existing load-stage `cane_visual_needed` site,
immediately after Cane flight-audio admission. That demand already observes
placed living, registered, selected-owned Cane actors, including hidden
authored owners. It does not grant or activate a weapon or actor. Levels with
no selected-owned Cane request no PCM. Later scripted acquisition on an
initially unqualified level remains silent until a subsequent normal load.

The append requires the already-transferred `campaign_audio_archive`; the
local archive at this site is `meshes.vpp` and must never be passed. No new
archive, parse, declaration, bank allocation or generic-bank refactor is
introduced. Only the four existing Foley declarations can gain optional WAV
residency. Existing resident rows are reused, deduplicated and pinned.

Admission validates the exact default group, absent other dry/underwater
slots, four-sample group envelope, sample IDs and nonlooping metadata. It
sizes only previously unretained samples as additional bank bytes and
preserves256KiB shared-bank headroom. All reloads finish before publishing
any new pin/readiness. Failure unloads only newly loaded unpublished samples
and truncates only this append's array tail. Conventional/Spit support,
previous PCM, pins and aggregate resident counters remain unchanged. Failure
is presentation-only and cannot reject a level, flight, contact or expiry.

This append depends on successful existing conventional impact admission;
it is not an independent fallback bank. The original conventional192KiB and
Spit288KiB admission transactions and ordering are unchanged. Cane admission
runs later, after other startup optional audio and its flight sample, so it
can refuse for headroom even when all assets exist. Final Xbox admission is
unverified. The shared close/reset owner restores all appended eviction flags
before bank teardown; no extra sound lifetime, checkpoint data or reset hook
is needed. Maximum new fixed bookkeeping is the20-byte diagnostic array;
sample rows reuse the existing64-row storage.

## Retained contact and expiry consumers

`scene_ai_contact_material.inc` extracts the existing Spit identity/material
reader unchanged into a shared include after `scene_ai_laser.inc`. The vehicle
branch calls the repaired `scene_driller_projectile_material` primitive;
private later-defined vehicle runtime types stay outside this early include.
The reader has no family readiness, liquid policy, sweep, Nano prepass, sound/RNG or damage.
Spit's existing wrapper keeps its exact readiness and world/shield AP
passage suppression before calling it. Spit direct/radial/audio ordering and
expiry remain unchanged.

Cane contact captures this read-only qualification before damage can retire
an owner. It retains body collision material, clutter owner material,
qualified vehicle/turret material and physical shield material. Missing
metadata explicitly chooses Default through material-1. Detached solid
pieces retain their existing batch/piece liveness check and explicit Default
because the public contact view does not expose material. No new world or
mover hit is invented: Cane's existing object-only sweep remains unchanged.

A qualified physical shield plays before durability and then calls the
shared direct consumer exactly once. An ordinary qualified contact calls
that same consumer once, then plays only if it succeeded, did not consume
the shot through Nano, and did not replace the timeline. The breaking Nano
hit remains silent. Lethal, immune and source-retired contacts retain their
captured material/weapon identity; no positive-health-damage requirement is
added. Stale identities fail optional qualification. The existing consuming
physical-shield and qualified direct-damage behavior remains authoritative.

The actual terminal-kind2 branch requests Default0 at the copied final
`shot.flight.position`, not the zeroed contact payload. Terminal state has
already been published inactive and its flight loop has already retired.
There is no contact-material lookup, damage dispatch or nearby-actor query
for expiry. Epoch checks stop old work after either sound callback resets
the timeline. Repeated frames cannot repeat terminal feedback because the
pool slot is inactive before callbacks.

Both paths resolve an authored material sample and retry the same dry
Default only for sample-1. Cane normally falls back from physical material
to its sole Default group; physical material indices are not catalog slots
that should be relabeled Flesh. Expiry explicitly supplies catalog Default0.
The existing starter checks pin membership and residency first. It performs
no per-terminal archive access or PCM load. Native voice admission remains
the backend's best-effort behavior and cannot veto gameplay.

`rf_scene_cane_impact_audio[5]` records admission ready, new sample rows,
retained file bytes, status and selected-owned demand. Existing shared impact
diagnostics record requested weapon/material/group/sample/voice and point;
their request counter now also includes Cane expiry. Actual contacts and
expiry remain distinguished by existing `rf_scene_ai_cane[1]` and `[2]`.

Separate NanoAttackHit Vclip/visual presentation, warmup effects, exact global
sound-RNG interleaving, bit-exact original collision ordering and runtime
listening remain outside this slice. No speculative visual replacement or
new gameplay fixture was created.
