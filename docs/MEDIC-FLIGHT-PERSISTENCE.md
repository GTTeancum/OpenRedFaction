# Medic damage-flight checkpoint lane (source-only)

Baseline: `ef0612a6def69d194cfd434377cb25337a7af689`. Integration is after the parent's23:00 outcome and independent joint review. This stage changes no active repository files and has not been compiled, syntax-checked, tested or executed.

## Scope and interface

RFNC25 is conditional on a qualified flight continuation or the independent shared flight RNG receipt. Runtime owns the source-qualified L3S1 UID783 medic1 constructor, action service, shared types and all `scene.c` changes. Navigation owns pure graph/start admission and no-fail typed publication. This lane owns the13 files in `SOURCE-MANIFEST.sha256` and the additive public codec APIs:

- `rf_npc_checkpoint_encode_with_rng(identity,catalog,rows,count,rng,output,capacity,written)`
- `rf_npc_checkpoint_decode_with_rng(data,bytes,identity,catalog,rows,capacity,count,rng)`
- `rf_npc_checkpoint_record_validate(row,catalog)` for isolated row validation, without pretending a row is a complete shared snapshot.

Old ownership encode/decode refuse any new flight ownership or present singleton. Metadata-only preflight validates it; resource-demand and turret inspection explicitly call the aware decoder and intentionally do not publish its receipt. Inputs and outputs remain disjoint. Core outputs are committed only after complete validation.

The runtime export glue captures `scene_medic_flight_rng`, copies its two words into `rf_npc_checkpoint_rng`, and uses the aware encoder after ordinary row capture. All actual source/class/player/node joins remain scene responsibilities. No API invents a UID, stores a pointer/full handle on wire, performs a route search or consumes a random draw during admission/restoration.

## Wire layout

The64-byte header is unchanged in size. Version25 alone uses word20 as RNG presence0/1 and word60 as its exact uint32 value. Presence0 requires value0; presence1 admits every uint32 including0. Both words are covered by the existing component checksum. Versions1–24 retain their exact reserved-zero checks. A zero-row component can retain a singleton.

Each version25 row keeps the existing version23/24 base, variable payloads and276 bytes of fixed version13+ tails. A new optional flight tail follows those tails:

- Offset0: presence. Absent is4 bytes on wire, with the full decoded logical lane zero.
- Offsets4–8: entered and target kind NONE0 / actual candidate PLAYER1 / UNRESOLVED2.
- Offsets12–28: five signed relative millisecond deadlines250,294,4d4,1370,4d0.
- Offsets32–36: two visibility result bits.
- Offsets40–87: independent pair[2] and prefix[4], each `{kind, ordinal}`. AUTHORED1 uses an exact zero-based graph ordinal; NONE0 and OWN_START2 require ordinal0.
- Offsets88–100: prefix count, previous/current indices, relative wait134.
- Offsets104–140: exact binary32 turn[3], eye[3], bias and steering[3].
- Offsets144–156: approach, activity, start-valid and eye-valid.
- Offsets160–192: exact own-start position[3], query-point[3], radius, height and word040.
- Offsets196–204: exact retained actor command714[3].
- Offsets208–216 and220–228: exact angular velocity150/body.vector_c8 and torque174/body.vector_ec. These are entered-only; constructor rows encode canonical zeros and leave independently reconstructed ordinary physics untouched.

Present length is232 bytes including presence. The runtime snapshot is236 bytes because its target_handle word is runtime-only and always zero in the portable snapshot. Timers use -1 unset,0 already due and bounded future remaining; private staging rebases them through the existing timer API. Constructor due timers have proven source provenance. Unwritten start/eye coordinates are canonical positive-zero storage with explicit validity, rather than claimed source values. Meaningful finite float bits, including signed zero, are retained; absent storage requires canonical zero. Graph/start/source-class admission is repeated before publication. The bias is the actual approved constructor cycle value, independent of RNG receipt presence.

Any entered flight requires the singleton, because admitted onset necessarily drew. A declined request may retain a singleton without entered state, and retirement of the final medic does not remove it. Entered angular/torque exactly join the genuine RFNC14 angular/torque when that independently owned physics tail is present; no scripted marker is invented. Every real ordinary RFNC pain RNG and genuine RFAP burn RNG must exactly equal a present singleton. No synthetic pain-present row is created.

## Capture, candidate joins and publication

Capture preserves qualified constructor, active7, default2 remnants and registered inert dead-pose ownership, including exact command714. Current7 is admitted only with the qualified entered owner. Nonzero command admission is waived only after that exact flight snapshot is captured; existing actual body-velocity, script, resource, playback and unsupported-owner checks remain. Untouched qualified constructor metadata does not itself forbid an independently represented ordinary route. Retired rows have no flight continuation, under the runtime's actual action1 retirement boundary.

The private restore stage holds rebased snapshots and the shared candidate/before receipts. Its late player join runs only after real RFPL/environment/form candidate validation and binds the actual registered player; no historical raw handle is reused. Saved actor action, weapons, health, ordinary route/combat owners and existing class geometry are supplied to the pure runtime/nav admission. Full current owner/pose and actual history-before fingerprints remain mandatory. The final validation also checks the prepublication RNG receipt and player full registration identity.

Scope suspension begins immediately before the first existing world publication. New flight hooks/draws remain suspended through final storage close. Existing ordinary NPC AI/pain/playback and other world assignments keep their established fresh-world ordering. The genuine old qualified `navigation.start` is preserved in full across early ordinary navigation clearing/start writes. After successful close and history/ordinary-route publication, the exact ordinary start projection and constructor dimensions are reconstructed, then the saved typed flight start is published; invalid start metadata leaves the real constructor dimensions intact. No later route helper overwrites the flight start. Legacy absent live rows receive the explicit fresh-qualified constructor compatibility policy, preserving actual constructed bias and assigned saved action/ordinary commands; no historical flight or RNG parity is claimed. Legacy command714 is zero, as required by its old grammar. Retired absence stays absent.

Existing ordinary pain/burn assignment may write the same RNG before close. If a failed load had a preexisting flight singleton, its exact old value/seen receipt is restored before scope release. An old entered flight likewise restores its exact old angular/torque aliases on failure, since existing body assignment may have overwritten them. On success, entered flight publishes saved vectors; constructor/legacy preserves ordinary physics reconstruction. This is only preservation of the newly retained shared alias. It is not coherent old-world rollback: full world loading is a fresh-scene replacement, and preexisting early ordinary publication retains its established failure policy. Ordinary live QuickLoad preflight still closes before accepting replacement. Standalone NPC restoration refuses both new input ownership and a live entered flight or present shared flight singleton before staging. Untouched qualified constructor state remains eligible for legacy restoration; this guard does not apply to ordinary composed QuickLoad. Its admitted legacy validate/assign/finish path has no later fallible step. The frame0 RNG seed runs in combat_input_tick before the sole full world load; final singleton publication is not overwritten by a later frame0 seed.

## Fresh-section history policy

No RFCH version or payload is added. Only an authoritative validated owned current7, with no patrol/script route and an actual generic lane that is absent or exactly `{flags=1, action=2}`, receives the bounded projection. Section1 capture emits canonical default2 without mutating the live actor. Same-level sections0/2 omit that generic lane while RFNC25 retains exact7. The actual original route_before is still captured and unconditionally compared at restore validation; it also triggers the existing full owner/pose snapshot when present. Composed RFCH/RFNC current rows join exactly. No unrelated route/provenance or combat target is dropped. Fresh accepted scene/player replacement initializes the new flight/RNG owner under the explicit section policy, rather than carrying timing across RFCH.

## Memory and capacity accounting (32-bit target)

- Logical core record grows232 bytes; saved+before copies add464 bytes per NPC stage entry.
- Each entry also gains the236-byte private rebased snapshot, and its existing owner_before copy grows164 bytes with the runtime owner: total new entry cost864 bytes, before any target alignment (all additions are4-byte aligned).
- Stage header gains two8-byte RNG receipts and three4-byte control words:28 bytes.
- Runtime live NPC grows164 bytes; navigation adds no separate per-owner persistent buffer. Runtime owns its separate bounded global counters/scratch accounting.
- At the structural2048-row maximum, decoded rows grow475,136 bytes; entries grow1,769,472 bytes plus28 header; live actor storage grows335,872 bytes. These maxima are not an admission promise.
- Wire row maximum grows1768→2000, exactly232 bytes. The structural RFNC bound grows475,136 bytes. All-absent version25 tails cost4*N; when only the singleton promotes an older component, the existing version13–23 fixed tails also become present. Compared with version10, that is280*N total added fixed bytes; compared with version24,4*N. A qualified otherwise-basic row is600+276+232=1108 bytes. A singleton-only zero-row snapshot remains64 bytes.
- Stock `RF_CHECKPOINT_FILE_MAX` remains110524 bytes. Expanded profile's262144 override is untouched. Composed load budget remains2MiB. Actual admission may return RF_RANGE well below actor-store capacity.
- Stage and temporary decoded rows are charged through existing sizeof-based budgets. Preliminary resource-demand decoded scratch is now explicitly subtracted from that phase's remaining budget while resident. Generated-turret metadata scratch retains its existing explicit budget.
- Pure capture/translation adds one236-byte snapshot and one232-byte logical lane on the stack; the late candidate join uses one existing `campaign_npc_body` automatic copy. No additional heap arena, new resource demand or graph rebuild is introduced.

## Source-only audit and remaining limits

The version/span/hash/read/write paths, old wrapper refusal, complete-snapshot RNG equality, actual decoding callers, one-row validations, current-history joins, publication ordering and all row initializers were inspected. Existing row initializers are zero/designated or member assignment, not positional complete literals. Targeted older fixture readers remain intentionally version-specific and were not rewritten or executed. Header/source/catalog identities are unchanged except the explicit RFNC25 version and protected fields; no source/catalog hash was altered.

The look.clock_7b0 echo has no current gameplay reader and is not given a new persistence lane. No gameplay, route quality, movement/save round-trip, visuals or runtime success is claimed. The independent reviewer owns the final receipt. Full restore probes lacking the composed player join do not establish RFNC25 ownership admission; the ordinary full loader supplies that join.
