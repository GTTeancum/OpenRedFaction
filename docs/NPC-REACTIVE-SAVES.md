# Reactive NPC combat saves

RFNC12 gives reactive NPC combat its own discriminator, `combat.active=4`.
This records a nonzero authored NPC target UID and restores the existing live
`combat_scripted=2` mode. Authored Attack remains mode1 in the scheduler and
mode2 on disk. Loading reactive combat must not turn it into an authored order
that resists ordinary retaliation or releases differently.

The600-byte row and40-byte combat extension are unchanged. The writer emits12
only when reactive rows are present; otherwise generated-head Attack still
uses11 and ordinary rows use10. Readers retain versions1 through11 and reject
mode4 in pre12 data. Reactive targets cannot be zero, UINT32_MAX or self UID.

Capture requires a real registered, living NPC target and retains the existing
fire/reload deadlines, burst count, reload weapon and shared spread RNG. Load
joins the target UID against the pending living RFNC records and resolves its
current registry generation before publication. The assignment phase restores
mode2 exactly. NPC and player placement, animation, inventory, resource and
pending-effect checks still apply. Active reactive pursuit uses the existing
movement extension rather than a new navigation representation.

Generated-head and passive-vehicle reactive targets remain outside this new
mode; their authored Attack support is unchanged. Pain/burn/effect admission
and ordinary load-placement limits remain those of the existing save path.
The added84-byte diagnostic snapshot copies two restored rows plus total count
without changing gameplay, and reconnects the existing live pair probe.

Focused codec cases cover mixed reactive/generated-head version selection,
roundtrip preservation, old-version rejection and invalid target IDs. They
are prepared source tests; native validation is recorded separately below.


## Stock64MiB Xbox continuation (2026-10-03)

PASS: `artifacts/xemu/npc-opposed-save-20261003-135013/report.json`.
The source runs80frames with ordinary Set_Friendliness delayed one second.
Both actors acquire each other naturally before saving, with no shots fired.
RFNC12 records reciprocal target UIDs914200/914201, fire deadlines11/12frames,
RNG1, full authored health70/armor30 and16 loaded rounds each.

Fresh load120frames restores these exact fields into reactive mode2. No setup
or sight acquisition repeats. Four real shots land four hits, one actor dies,
and the survivor releases its target with no further shots after the observed
death. Player health remains100. The ordinary save is17392bytes, loader staged
memory805492bytes, and minimum endpoint5950 free pages (23.24MiB).

NXDK build and original-disc restoration pass. No PC runtime or images were
used. This is a quiet initial-reaction save followed by real combat; active
reload, in-progress pain and moving-pursuit continuation were not separately
exercised. Visual/audio output is unverified. Codec test additions remain
unrun; the ordinary native run exercises the real RFNC12 encoder and decoder.
