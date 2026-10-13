# Player Rocket lock and guided-flight save continuation

Source-written against a6cd3de2 on October 12, 2026. This is a bounded RFAP10
implementation, not original RF.exe save-format reconstruction. No compilation,
functional test, XEMU session, HDD access, image, fixture, or Git publication was
performed. Parent owns integration and the hourly compile/package/startup smoke;
functional verification belongs to testers.

## Implemented continuation

Ordinary composed-world saves can retain a Rocket Launcher alternate hold, its
scan/acquisition deadline, completed lock, alternate input latch and primary
trigger cooldown. Each live guided rocket retains its exact already-paid flight
slot, position, velocity, remaining lifetime, liquid flags and orientation plus
a typed authored target identity. Inventory remains solely in RFPL; restore does
not launch another rocket, debit ammunition, replay a shot sound, reacquire a
target, or apply damage.

The existing gameplay reconstruction is unchanged: source4a63f9/4ad470 arms on
alternate press;4acec0..4ad0ff scans and fires on release;4ad110/4ad240 selects a
living visible target within range100 and the30-degree half-cone;4ad0b5 uses a
2750ms acquisition and75ms rescan (165 and5 simulation ticks in this port).
4acfa5..4acfff and4c6fca..4c7064 ground the already-implemented guidance wakeup
and speed-preserving turn. RFAP10 only continues those scene-owned behaviors.

## Wire contract and capacity

The existing RFAP header remains16 bytes and rows remain112 bytes. RFAP1–9 are
still read using their existing row policies. Producers select10 only when one
of the new rows is present; this does not change RFEN or the monitor schema.

- Kind23 is a singleton local-player controller. Slot and source UID are zero.
  Offsets12/16 name target kind/UID;20/24/28 are active/alternate-held/locked;
  32/36 hold remaining scan/acquisition frames;40/44/48/52 hold the existing
  primary trigger cooldown/remaining/delay/held tuple. Bytes56–111 are zero.
- Target kinds are1 NPC,2 passive vehicle and3 authored turret. Kind0 is allowed
  only for a controller without a target and requires UID0.
- Kind24 binds one paid kind3 flight slot to target UID at8 and target kind at12.
  Bytes16–111 are zero. A binding without the same active kind3 slot, duplicate
  binding or controller, invalid target identity, or mismatched trigger tuple
  fails closed. RFAP10 also requires kind3's reserved tail word to remain zero.
- The local-player role is rebound; no source/target generation handle or pointer
  is serialized. Guided flight shape is checked against the real Rocket radius,
  lifetime, liquid flags, speed and orthonormal basis, preserving actual bits.

The extension is bounded by one controller plus50 guidance bindings: at most
5712 additional wire bytes. The shared restore stage gains652 bytes of fields (plus ABI alignment), and the
live lock state grows by8 bytes. The existing110524-byte whole-save cap and2MiB restore
budget remain unchanged. If the full composed save does not fit, the save refuses;
no row is dropped, flight made ballistic, pool shortened or capacity enlarged.
Controller rows demand the Rocket player view; paid-flight rows demand only the
shared Rocket resources, independent of selected/owned player equipment.

## Ownership and publication

Capture verifies full registry/entity identities and current living/visible
eligibility. NPC targets additionally join authored, nonretired campaign history.
Read resolves a typed UID to its current constructor identity without using its
current health as the saved lifetime. Full-world admission then joins the exact
candidate NPC registration/retirement/health/visibility, RFVA2 passive vitals, or
authored RFTU turret lifetime. Active holds also join the candidate's selected,
owned Rocket Launcher and refuse holstered, transformed, cutscene or seated
player ownership. Existing world/body/support and other transient admissions
remain in effect.

A stack-owned save-only service receipt spans the existing combat-input call,
including lock sweeps, guidance movement/contact callbacks, flight publication
and its ammo debit. Resets never clear it. Reentrant capture refuses during this
interval; candidate preparation is not vetoed by the outgoing service owner.

Player Rocket flights, target bindings, trigger and hold publish together only
after final storage success, following outgoing transient resets. Preparation
and failed storage close cannot install new guided ownership. Resumed flight
loops follow publication; no launch audio is replayed. Direct in-session loader
replacement of an outgoing active/guided timeline remains refused. Ordinary
QuickLoad creates a fresh scene and can restore the new state.

## Input safety

Xbox chord/pause/hotplug suppression produces neutral gameplay input. A narrow
latest-poll native receipt distinguishes that zero from a real alternate release;
its state is invalidated on entry/open/close and all early-return paths. The
native gameplay provider is wrapped by player_poll_paced, so the receipt is tied
to the opened native input owner, not an incorrect direct poll-pointer match.
Replay does not open that owner and keeps its input ABI and ordinary behavior.

A loaded or input-suppressed active hold freezes its remaining acquisition until
an effective LT-held sample appears. The HUD says HOLD LT TO RESUME LOCK. That
sample rearms the retained hold; a subsequent real release uses the existing
cooldown/ammo/collision path. Synthetic zero therefore cannot fire a loaded or
just-saved shot. Primary takeover and the established invalid-owner/reload/death
cancellations still cancel the hold normally.

## Deliberate boundaries

Generated turret heads and promoted/active vehicle-host identities have no lane
in this extension and refuse capture; no raw handle is accepted as a substitute.
A target that dies/retires/hides before capture refuses until the existing next
simulation tick drops that guidance. Runtime target loss still permanently makes
the paid flight ballistic. Standalone RFCP/composed special-weapon checkpoints,
occupied-vehicle capture and direct replacement of live guided state retain their
existing refusal. This work does not expand player movement/save admission or
alter guidance fidelity, target scanning, damage, collision or ammunition rules.

The implementation is source-review-ready only. Broad saved gameplay behavior,
lock resumption, target lifetime, resource pressure and controller interaction
remain unverified and belong to tester functional coverage.
