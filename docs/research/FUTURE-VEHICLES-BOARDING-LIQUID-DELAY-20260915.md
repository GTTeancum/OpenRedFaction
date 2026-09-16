# Vehicle boarding: shield delay and liquid admission

Priority: P1 for functional vehicle entry, ahead of implementation. Original RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

## Executed evidence

`tools/future_re/vehicle_boarding_liquid.py` executes original cdecl `4a1970(player*,actor*)` through the pre-attachment boundary4a1dd8. It supplies preceding eligibility queries and target/entity resolution, records428f00 and5231e0 services, and executes the actual timestamp setter4fa360 and room-liquid predicate4ce080. All384 combinations pass: vehicle/turret use kind, Riot Shield selected, already-holstered flag, water-only class, room present, liquid present, host below/on/above surface and local/nonlocal player. Results are in `boarding-liquid.json`. This is no claim of rendered boarding, actual audio or full holster animation.

## State policy

At4a1d52, actor weapon class+2a4 is compared to global85cce4, the Riot Shield ID independently identified in DEATH-LIFECYCLE.md. If it matches and actor+810 bit800 is clear, original calls428f00(actor), invokes thiscall4fa360 with ECX=player+bc and delay500, then returns without entry. The fixture clock5a3ed8=1000 produces timestamp1500. Even a dry water-only vehicle takes this delay first. The test records the holster request, not its animation internals. This is a Riot Shield rule, not a blanket heavy-weapon rule.

At4a1d8d, host class+724 bit40 controls water-only admission. If clear, room and liquid status do not block. If set,40a490(ECX=host) reads the host's first word (room pointer); null rejects. Then4ce080(ECX=room,host+3c) admits exactly when room+184 liquid byte is nonzero and host position Y at+40 is at or below room+0c + room+188. Tested surface=15 with Y14,15,16. This is the vehicle origin, not the player's feet or camera. Rejection falls back to4c0100(actor,1).

An admitted water-only vehicle invokes5231e0("generic") only for the nonnull global local player7c75d4. This report records that call without assigning an unverified audio/environment effect. All admitted cases stop at4a1dd8 before event pulse, local-position capture, seat attachment or control handoff.

## Authored examples and implementation

The established authored Driller01 in L1S2 (UID8122) and APC in L1S3 (UID9627) are concrete use-kind1/movement5 entry targets. Their recovered class flag words561161 and557569 both have water-only bit40 clear; consequently their entry must not depend on a room liquid plane. No authored submarine seat/run is asserted by this fixture.

Preserve class flags from src/core/entity_assets.c and the shared entity model in src/core/entity.c; compose this admission stage at the live use-action boundary in src/diagnostic/scene.c before committing actor-host ownership. The port can represent a pending shield holster explicitly, while keeping the 500-unit clock semantics consistent with the existing gameplay clock. Do not apply liquid restrictions to all vehicles or test player position instead of host origin.

Still open: full428f00 holster state/animation, duration units in the future vehicle driver, automatic retry versus another use press, actual water-only campaign vehicle instances, post-attachment controls/camera and vehicle weapons. No source implementation was changed.
