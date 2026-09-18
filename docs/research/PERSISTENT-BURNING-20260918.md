# Persistent burning and flamethrower pickups

The previously empty combat_burn callback now owns a fixed64-record NPC burn pool. Existing damage-effect admission decides whether fire ignites an actor; the callback retains target/source identity. Burning continues independently of the selected weapon or trigger state, applies fire damage through normal NPC damage, and reaches the existing death animation/weapon-drop path. Death, removed/hidden identity, explicit extinguishing and water immersion retire the effect; scene teardown releases records before NPC storage is freed. Active burns block the current scoped checkpoint capture.

Damage rate uses class_health/6.5 per second, a deterministic midpoint of the retained42f1dc/rf_burn_owner_tick divisor5..8. Five-second life and quarter-second batches are explicit first-pass policies. Original admission continues excluding player ignition. Attached particle/audio presentation, full original duration/arbitration, terrain fire propagation, persistence across level transitions and native burn encounter coverage remain open. This implements NPC damage behavior, not complete visual burning fidelity.

Authored items.tbl classes flamethrower and Napalm now map to implemented slot10. The existing pickup path loads their static models, grants weapon/fuel, retires once and restores retirement. Campaign weapon cycling is still the existing four-slot policy, so collection alone does not establish full campaign weapon availability.

## Evidence

- Focused scene_burning test uses real entity damage/effect arithmetic and checks ignition, continued damage, expiry, death, water/explicit extinguishing and stale identity; passes.
- Installed-table pickup names/grants checks pass including flamethrower loaded100 plus Napalm reserve100 and no duplication after restored retirement.
- PC live burn-tail.bin holds fire230..279 and then releases through frame499. Eight primary damage contacts leave health32.307693 at frame278; subsequent burning reaches death with one ignition,11 burn pulses,one retirement,one burn death and one dropped weapon. Final native PC render inspected: NPC in death animation and dropped gun visible. The short prior replay's direct-hit health differences independently show interleaved burn damage.
- PC and Xbox builds pass. Initial Xbox warning about misleading indentation was fixed. No new XEMU burn scenario was run; prior flamethrower primary/alternate native replays establish those weapon paths only.

## First-pass attached sprites

A fixed192-link table now follows up to three actor-sphere surface anchors per live burn. Sprites reuse Fire01 and the existing standalone particle pool; private negative owner tags identify matching sprites after pool reuse. New births are capped at12 per frame, with no new textures or allocations. Missing visual resources skip presentation without disabling burn damage. Birth radius0.25..0.55 is a port presentation policy; body motion/room refresh and particle lifetime remain shared services.

PC300-frame flame replay:12 sprites spawned,141 follow updates,zero pool misses,three live attachments after trigger release. Capture inspected: small amber fire/glow regions remain attached to the actor, but the effect is faint and is not considered finished flame presentation. The500-frame continuation reaches burn death and reports36 births,456 follows,three explicit retirements,zero remaining attachments; inspected death frame has no lingering attached fire. Xbox build passes; native attached-burn presentation and movement across room boundaries have not been checked. Defer intensity/lifecycle polish while assembling missing gameplay systems.
