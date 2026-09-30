# Auto Turret base/head ownership

The installed campaign contains **19 Auto Turret bases and no authored Auto
Turret Head entities**. Supporting the static Head class only when it appears
in level records therefore does not make these campaign turrets functional.
The original entity factory creates their heads at runtime and attaches each
to an animated skeletal interface on its base.

This report is asset inspection and static binary analysis, supplemented by
previously recorded attachment execution evidence. No original game launch,
new binary execution probe, build, emulator run or image inspection was used.
Only this document was added for this task.

## Authored instances

Directly walked all 94 installed RFL records using the existing independent
`entity_rows` decoder from `tools/xemu_turret_combat.py` and structural section
walker from `tools/inspect_levels.py`. Archive counts were levels1:25,
levels2:24, levels3:19, levelsm:26. Class matching was case-insensitive.

| Archive | Level | Auto Turret base UIDs |
|---|---|---|
| levels1.vpp | L3S2 | 1994,1995 |
| levels1.vpp | L3S4 | 2080 |
| levels2.vpp | L11S1 | 9520,9521,9522,9523 |
| levels2.vpp | L7S2 | 4887,4889,4907,4911 |
| levels3.vpp | L13S1 | 8253,8262 |
| levels3.vpp | L13S3 | 8937 |
| levels3.vpp | L14S3 | 9619 |
| levels3.vpp | L15S1 | 8305 |
| levels3.vpp | L19S3 | 12035,12175,12176 |

The independent older `artifacts/entity-spawn-verification.json` also counts
19 bases and no Head class in its 66-level coverage. The fresh 94-file scan
includes multiplayer archives; none supplied additional bases or heads.

Installed entity.tbl distinguishes the two owners:

- Base: `auto_turret.vcm`, life 80, no primary weapon, no movement,
  `turt_stand.mvf` and `turt_attack_stand.mvf`, use-kind none.
- Head: `turret_top01.v3d`, life 60, Vauss primary, FOV 360, rotation rate 6 and
  acceleration 10, no authored replacement model, use-kind none. The entity
  class still carries the turret class flag.

These are separate damage/model owners, not two draw sections of one static
turret. The base is animated and the generated head is static geometry with
its own independently aimed transform.

## Runtime creation: exact original call site

Examined the local RF.exe directly with PE mapping and Capstone, checking
SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
The existing raw export `artifacts/analysis/rf_b8fb9ab4c9bf/422360.c.txt`
contains this branch but has unreliable reconstructed stack arguments;
the argument list below comes from the instructions themselves.

At 423970..423984, factory 422360 compares the selected class name with
`auto turret` (string 595aac) through 5001d0. On match:

1. 423986..42398b resolves `auto turret head` (595ab8) through 4251c0.
2. 423990..4239a4 recursively calls 422360 with seven arguments:
   resolved Head class, name pointer 62f468, base handle+2c,
   base position+3c, base orientation+48, creation flags 0, final argument -1.
   EBP is 0 and EBX is -1 at this branch. The name pointer addresses an empty
   string in the mapped binary; no authored head name or RFL UID is passed.
3. 4239b4..4239c1 forces Head friendliness +1f8 to 0 and sets object flag +7c
   bit 0100, independent of the base's friendliness.
4. 4239ae..4239d4 reads interface entry 0 from the base's +8cc array and calls
   `427240(base this, head handle, first interface tag)`.

The base builds that interface array earlier, at the 422c94 call to 4243c0.
That helper resolves `interface_1`, `interface_2`, etc. through 503220 until
lookup fails, creates 8-byte entries containing tag handle and occupant -1,
and preserves numeric interface order. Its raw export is
`artifacts/analysis/rf_b8fb9ab4c9bf/4243c0.c.txt`.

427240's previously executed contract establishes both directions:
Head host +200=base handle, Head tag +204=interface tag, base seat occupant=head
handle; it zeros child linear/angular velocity. It does not immediately
publish position. See [seat attachment](FUTURE-VEHICLES-SEAT-ATTACH-20260915.md).
The port should add allocation/lookup guards; the original branch assumes a
valid newly created head and a nonempty interface array.

## Real animated attachment, not a guessed offset

Directly inspected `meshes.vpp/auto_turret.v3c` (5967 bytes, one SUBM,
nine bones, two attachments). First-LOD attachment rows are:

| Name | Quaternion x,y,z,w | Local position | Parent bone |
|---|---|---|---:|
| eye | (-.707106709,0,0,.707106829) | (approximately 0,-.0206541661,.00000000454) | 5 |
| interface_1 | (-.707106709,0,0,.707106829) | (approximately 0,-.251986504,-.004755368) | 7 |

Bone 7 is `turt-bdbn-pole02`, parent 6 (`turt-bdbn-pole01`), then bone 2
(`turt-bdbn-base`), then root 8. The interface local position alone is not the
head's world offset. Compose the attachment with the **current evaluated
bone 7 matrix**, then the base's published model transform. The current shared
skeletal tag namespace assigns nine bone tags followed by attachment tags,
so this model's interface_1 resolves to tag 10 (9 + attachment index 1).

Existing `campaign_file_tag_find` and `campaign_file_tag_pose` already perform
this lookup and attachment/bone/world composition for retained skeletal model
owners. Reuse their typed data path or expose an appropriate typed wrapper;
do not use `rf_static_model_tags_open` for this skeletal base, since that
loader intentionally accepts static LOD flags only.

**The generated Head preserves its own orientation.** Creation sets flag 0100.
Previously joined original 4881a0→487630→5034f0→48a230 execution establishes
that flag's behavior: update attached position, current/pending physics
position and bounds; preserve all three child orientation matrices +48/+fc/+120.
Base use-kind is none, so tag placement uses its published basis +48, not the
special use-kind 4 eye basis. See [occupant pose evidence](secondary-re/vehicles-occupant-pose-20260916.md).

For this Head, move its position with the animated base tag while keeping
the combat orientation/world aim independent. Do not rotate the combat
sidecar mount every time the base moves or rotates. The generic moving-mount
note in the initial combat adapter document is not this orientation-locked
head's rule. Pose publication belongs after base animation/world placement
and before head eye, targeting, ray collision and draw.

## Activation and death coupling

402c20 initializes class-specific AI bits: actor +7d0 bit 00100000 for the Head
(403052), and 20000000 for the base (403076). At 401b64..401b9e, the Head branch
resolves its host and asks 408e90(host +2a0); a false result returns false from
the surrounding eligibility function. 408e90's AL result is host AI +530 bit 0,
equivalently base actor +7d0 bit 0. The following 401b9f..401bb4 branch excludes
the base's 20000000 bit from that eligibility path.

This proves a host activation dependency, but not the complete base alert
scheduler. 4073c0..40741a checks completion of action 28 (`idle_to_ready`),
calls 408ea0 to set AI +530 bit 0, sets actor +810 bit 10 and keeps the Auto Turret
base's primary weapon -1. A preceding 407375 call sets the same bit when that
ready action is absent. The authored base lists stand/attack_stand states,
not an idle_to_ready action. Do not invent a permanently inert base merely
because the optional action is absent. The original event/awareness path
that requests readiness still needs integration with the base owner.

Original death entry 418f80 supplies concrete linked lifecycle rules:

- 419019..419050: a dying Head with AI bit 00100000 resolves host +200 and
  dispatches 1000 damage to that host through 4892c0, source -1, kind -1.
- 41905a..419076: detach the dying child from a resolved host through 4279d0.
- 419089..4190c4: a dying owner walks interfaces; attached Auto Heads have
  life +34 set to 0 before 427380 releases each occupied interface.

The raw export `418f80.c.txt` and direct instructions agree. Current turret
damage replacement switching alone does not implement these coupled deaths.
A port callback must guard recursive/repeated death handling and clear both
binding directions before freeing either owner. Shared standard damage,
death events and model retirement still own their ordinary side effects.

## Minimal integration work

1. **Load the dependent class/model.** Seeing an Auto Turret seed must retain
   Head class metadata, `turret_top01.v3m`, materials, collision and tags even
   though no Head seed exists. The current model cache iterates seed classes;
   the current owner constructor iterates level records, so neither creates
   this dependency. A generated child is not a fabricated authored RFL row.
2. **Create one owned child per registered base.** Keep a validated base
   handle, interface tag and child handle. Initialize its own life 60/Vauss,
   original forced friendliness 0 and orientation-lock flag. Give it a stable
   port save key `(base authored UID, AutoHead role)` instead of inventing an
   adjacent UID that could collide with a real level object.
3. **Publish animated attachment position.** Query interface_1 from the live
   base pose, update child position/eye/contact bounds while preserving its
   independently aimed basis. Keep class-resource ownership alive until both
   actor teardown paths finish; handle missing/dead base deterministically.
4. **Admit attached-head combat deliberately.** The present
   `scene_turret_combat_tick` rejects every nonnegative `view.linked_handle`.
   A generated head needs a distinct typed attachment role; it is not a
   possessed turret or seated player. Enforce base readiness plus ordinary
   visibility/death gates, and leave the base unarmed. Avoid having generic
   skeletal NPC look/combat independently aim or fire the base.
5. **Wire bidirectional death and state.** Destroying the head must affect its
   base; destroying the base must retire its head. Preserve base animation,
   readiness, child life/aim/cadence and stable binding through saves. Existing
   RFTU1 rows assume authored turret UIDs and do not identify this generated
   child role, so saving it requires an explicit codec/binding extension.

First bounded acceptance should use one real base record, its actual animated
interface and one generated Head, checking one head per base, moving tag
position with independent head basis, activation/fire, either-side death,
and ordinary save/load without duplicate children. The current standalone
static turret checks cannot establish any of those Auto Turret behaviors.
