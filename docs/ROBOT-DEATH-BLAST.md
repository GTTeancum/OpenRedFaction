# Authored clipless robot death finalization

The Drone/Tankbot source implementation was independently reviewed against
`59fc9e0b8a5ace01b9e515fc920aa3f0ae11b205` and parent-integrated after the noon batch.
The exact Spike extension was independently source-reviewed against
`3d9798272e57f5b051019b95f9b19c9e85bba0b7` and parent-integrated. All added terminal branches remain uncompiled and runtime-unverified.
No builds, tests, fixtures, runtime runs, worktrees or active-repository edits
were performed during implementation/review. Parent owns the next scheduled Xbox batch.

## Original evidence and bounded scope

Installed `entity.tbl` Tankbot (line 4028) and Drone (line 4086) declare
`DeathSnd: "Big Bot Death"`, `Explode Anim: "drone explode"`, radii 10 and 4,
and no death animation or replacement corpse model. Installed `vclip.tbl`
line 234 gives that clip damage 100, the generic explosion recipe and
`Medium Explosion` Foley. Known placed consumers are Drone UID844 in L3S4
and Tankbot UID10696 in L7S4. These are authored source facts, not new tests.

Spike declares `DeathSnd: "Spike Death"`, `Explode Anim: "yellboom_metal"`
and radius 0.9, with no death action or replacement corpse model. The Vclip
omits `$Damage`; original RF.exe 4c14e9–4c1505 initializes that field to zero,
and the existing `rf_vclip_definition_read` likewise zero-initializes it.
The shared wrapper's clip-damage × radius × multiplier therefore stays zero,
not Drone's 100 or any positive Spike blast. Existing placement documentation
records Spike UID5034 in L7S1; UID10810/10878/11127 in L7S4; and
UID977/978/1140 in L9S4 (`SPIKE-TRIBEAM-FIRST-PASS.md`). L17S1 has no Spike.
No new campaign scan, route or synthetic encounter was performed.

Original damage feedback at 41a505 calls 4196f0 only when incoming damage /
class health exceeds .001 and damage kind is not 10. At 419706–419736 the
base class death descriptor gates the effective class sound, actor+7d4 is
the spatial position at 41973f–419751, and actor810 bit4 suppresses repeats.
This sound is not an unconditional finalizer effect. The candidate adds only
these exact three classes to the existing fatal RF_DAMAGE_PAIN_SOUND consumer.
It uses copied audio state and latches bit4 before optional playback callbacks.
Ordinary nonfatal pain and death-action audio are unchanged.

41ee40 permits finalization with action824 == -1. In 418f80, 48ab40 marks
removal first; 4190f9–419108 clears the action and invokes 419420 when the
class explosion ID exists. The no-action/no-replacement-model corpse branch
then creates no corpse. This is immediate source finalization for the absent
action; no timer or animation-completion predicate is required or invented.
41942c–419489 copies published position plus basis × class offset and calls 436490 with radius and multiplier1. 436490 requests
the named effect first, multiplies clip damage × radius × multiplier at
4364da/4364e7, and submits kind3/sourceUINT32_MAX radial damage at
436606–43660d. There is no GeoMod call in this chain.

Thus the radial request magnitudes are 400 for Drone and 1000 for Tankbot,
before the port's existing shared cover/falloff, shield and damage gates.
The candidate deliberately does not call the player-attributed blast wrapper.
Spike submits the same immutable zero-magnitude radial request after retirement
and optional presentation. The existing shared radial service returns before
victim/shield/cover callbacks when damage is nonpositive, so this extension adds
no damage or physical push. That retained port policy is not a claim of exact
original zero-damage radial/force parity. Telemetry slot 4 counts radial requests,
including Spike's zero request; it must not be read as a damaging-pulse count.

## Runtime ownership

Required gameplay metadata is loaded at the existing tables/catalog stage,
independently of optional PCM and vclip-material admission. The exact authored
names/radii/damage and no-replacement profile are validated. Only present
Tankbot, Drone or Spike records require metadata. The existing shared
four-clip/512KiB visual owner remains the cosmetic budget, and a visual refusal cannot discard
a death's terminal removal or authored radial request.

A 64-byte runtime receipt belongs to each allocated NPC, with full registry
handle, UID, class, persistence slot, transformed position, actual fatal
health, immutable damage/radius/clip/Foley values and FIFO sequence. This
avoids a separate finite queue losing a fatal event. The immutable profile
bank is three small rows; no RFNC format changes are made.

Publication is immediately after successful `combat_death_start` death-motion
publication. This is slightly later than the preliminary death-entry proposal:
the existing shared motion owner has now established action -1 for absent
clips, existing finite weapon-drop handling has completed, and no-corpse
eligibility can be proved without predicting the animation result. Before this
extension, Spike fell through to `campaign_live_corpse_create`, which rejects
action -1. With no owned corpse, the separate corpse-expiry path could not
retire that source; retained dead-pose capture also correctly requires a valid
death action. Exact profile admission fixes the missing terminal consumer
without relaxing either guard. All live combat call sites reach this function only for accepted death entry. This
point still does not remove, blast or call presentation callbacks. Existing
lethal callers retain their owner pointers safely until returning.

An outer actor-step boundary after combat/events/impact/liquid/NPC work drains
at most 32 receipts in FIFO admission order. Chained deaths enqueue their own
receipts and never recursively drain; excess work remains for the next tick.
The receipt is consumed before callbacks. A persistent in-progress guard and
timeline epoch stop recursive drains and old-timeline continuation.

The drain exactly requalifies the original owner and no-corpse state, then
uses `campaign_remove_object` before optional effect/Foley and the guarded
shared source-aware radial service. It never calls `scene_corpse_source_retire`,
whose already-released-corpse-model contract does not apply. Ordinary removal
unregisters the physical owner and cancels executable orders. Actual fatal
health, current armor, mission state and current drop tombstones are retained;
the ordinary removal owner's temporary health zero is not saved as the fatal
value. Inventory and weapon selections are not fabricated or erased.

An accepted receipt also survives an intervening authored Remove_Object. The
same allocated row/epoch, copied UID/class/persistence, retained full damage
handle, absent registry/entity handle, closed body and retired key qualify
that tombstone. This handles the already-removed source without replaying
removal or silently cancelling the already-accepted blast. Any other identity
mismatch fails visibly. No low-index registry lookup authorizes a new owner.

The current production Drone/Tankbot/Spike fire paths do not set embedded firing
active/audio/effect tokens. The terminal adapter explicitly rejects such
unsupported outstanding tokens rather than wiping them to satisfy saves.
Their separate live loop audio and prepaid launches retain their existing
death/removal ownership. Existing Spike death/removal hooks already cancel only
its prepaid pending TriBeam windup, with no refund; already released flights
retain their independent source and save-only pending ownership. No extra
projectile reset or inventory mutation is introduced. Original dying-update
reset of future embedded weapon tokens remains outside this profile until its real teardown callbacks
are connected.

## Save and failure behavior

Actual world saves, NPC exports and composed legacy player saves reject while
receipts are pending, being dispatched, or faulted. No gate is added to shared
candidate capture, load preparation or existing RFNC validation. A partial
radial error is never retried: the consumed receipt and visible fault remain,
and saving cannot misrepresent a partial pulse as a completed terminal state.

After successful terminal removal and radial-request consumption, the existing
retired RFNC row is authoritative; restoring it emits no death effects. Retirement is
source-justified finalization, not a workaround for the valid clip requirement
on retained dead poses. Existing unrelated RFNC guards, including active firing,
attachments and orientation, remain strict and may still reject their own
unsupported state. In particular, the pre-existing body_pitch_roll guard
still rejects a pitched/rolled Drone even after retirement; this change does
not claim universal retired-Drone save support.

Pending receipts reset only on NPC owner teardown, completed NPC component
restore, or final successful world publication/storage closure. Failed load
preparation or closure preserves them. There is no frame0 reset, because
startup events can produce deaths before the first combat tick. A player-only
life reset does not silently discard an already-paid NPC death in the retained
NPC world.

## Explicit limits and verification status

This is a bounded first-playable adapter, not full generic 418f80 integration.
Other exploding classes, attachments and player detachment, replacement corpses,
optional corpse emitters, exact original callback timing,
RNG sequence and the unclassified 41948e–4194c8 class-name branch remain separate.
The existing shared cover/falloff implementation is reused without expanding
its fidelity claims. Optional visual particles begun at the outer boundary
advance on the next normal particle tick.

The 13:00 UTC parent-coordinated batch remains committed to the actual saved-HDD
regression. Drone/Tankbot/Spike terminal branches will have compilation evidence
only if integrated; there is no established ordinary robot-fatal runtime case for this slice.
Death sound eligibility, authored zero/positive radial magnitudes, attribution,
chaining, optional-resource refusal, terminal retirement and pending/settled save behavior remain source-only
and unverified. A later proportional ordinary case must be grounded before
runtime coverage is scheduled; this candidate adds no fixtures or test matrix.
