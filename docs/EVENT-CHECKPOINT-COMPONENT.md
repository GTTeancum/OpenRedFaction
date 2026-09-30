# Event checkpoint component

RFEC3 preserves one supported event's represented runtime fields. It is not ordinary-scene save admission. The composer must capture associated world state, preflight every component, rebuild references, publish state and remove retired owners from the registry before gameplay resumes. Restore itself dispatches no effects or sounds.

Section history now preserves `Remove_Object` retirement of scripted event
owners, including types with no timer or threshold history. It keys each event
by section and authored UID, records a retirement bit in RFCH4, and removes
the rebuilt owner from the new section's registry before startup actions run.
The late full history restore remains idempotent for that owner.

A bounded stock-64-MiB Xbox L1S1→L1S2→L1S1 run retired authored startup
Strip_Player_Weapons event UID8366 through Remove_Object UID8630 before leaving.
The process-local fixture bypassed UID8630's authored two-second delay solely
to place the retirement before the frame60 exit. On return, the auto trigger
encountered one unresolved target and the first-entry inventory guard recorded
zero repeated strip callbacks. The run completed240 frames with3,949 free pages
(`artifacts/xemu/pending-section-event-20260930-010950`). Other removed object
classes and natural timing remain open.

RFCH4 also freezes a pending event's remaining delay at section exit, retaining
its activation mode and source/actor as sentinel tags or authored UIDs. A
revisited section resolves UIDs to fresh handles before rescheduling. Completed
generic events clear their old pending history so they cannot fire twice on a
later return. RFCH1..3 decode remains supported. Missing source/actor identity
still rejects the handoff; active effects already produced by startup before
history restoration are not rolled back.

NXDK and shared PC compilation pass. A bounded 64-frame stock-64-MiB Xbox
ordinary L1S2 save/reload with authored Message UID9727 pending passes. The
saved RFCH4 row contains 8,950 ms remaining, mode1 and sentinel source/actor;
reload leaves 3,339 free pages (`artifacts/xemu/native-world-20260930-003325`).
An additional bounded Xbox-only run activates that event with live authored
event handles UID9728 and UID9732. Its RFCH4 row contains source/actor tags2
and those respective UIDs, and ordinary reload succeeds with3,339 free pages
(`artifacts/xemu/native-world-20260930-004000`). This establishes real-handle
UID serialization and ordinary reload admission. Live reference state after
ordinary reload remains unverified.

A separate bounded stock-64-MiB Xbox section-return run activated L1S2's
2.5-second Message UID9725 with live source/actor event handles UID9728/9732,
forced the authored L1S2→L1S1 exit at frame60, and returned through the
authored L1S1→L1S2 exit at frame180. At frame210 the rebuilt pending event had
both registered references (probe mask15). At frame330 its timer had cleared
while both references remained live (mask7); Message UID9725 had presented by
frame360. The run finished with3,466 free pages and restored its test-disc
flags (`artifacts/xemu/pending-section-event-20260930-004726`). This covers
one real-handle section return and delayed dispatch, not every event type or
missing-reference policy.

Read-only inspection of installed `levels1.vpp/L1S1.rfl` found 184 events across 32 types. All 32 types now have event-local field representation. This is static inventory coverage, not a claim that every gameplay event is implemented or that a full scene save has passed.

Supported state includes common flags/mode and delayed-dispatch remaining time; distinct zero/UINT32_MAX references or UID-mapped source/actor handles; retirement and death latches; cyclic timer count/enabled/remaining time; threshold latches; Switch disabled/activation count; and UnHide cooldown plus pending on/off requests. Authored cycle period/limit/unlimited, Switch limit/mode/unlimited and threshold must match. The source identity covers other authored settings. Expired deadlines become zero remaining time; disabled deadlines remain disabled. Remaining times rebase onto the restored clock. The diagnostic death stamp remains the saved observation stamp.

`rf_event_checkpoint_external_requirements(type)` exposes associated NPC, audio, visual, damage, world, inventory, goal and level-transition requirements. These are categories the composer must inspect, not assertions that an effect is active. Inert records can survive without inventing an effect. The mandatory scene-adapter admission callback must independently establish that external state is captured or safely settled.

Play_Sound (6 L1S1 records) now dispatches on/off actions to a bounded positional voice service, including delayed dispatch. Its event-local requirement is `RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO`; pending deadlines are preserved, but an already playing voice and its phase are not serialized or restored. The first pass uses the authored first distance value as near range and sample metadata for looping; exact flag, attachment and pitch behavior remains unverified. Black_Out_Player (1) now draws a timed full-screen blackout using its first authored value as seconds, including on/off dispatch; the active timer is not serialized or restored, so its event checkpoint still carries `RF_EVENT_CHECKPOINT_EXTERNAL_UNIMPLEMENTED`. Look_At (5) now issues on/off gaze commands to linked NPCs, and Explode (6) dispatches radial damage and bounded named visuals; their active scene state is not serialized, so the external-unimplemented checkpoint requirement remains. Music_Start (3) and Music_Stop (1) now play one streamed stereo track and fade it over the authored stop value; their event-local requirement is `RF_EVENT_CHECKPOINT_EXTERNAL_AUDIO`, while the active track, playback position and fade are not serialized. Continuous_Damage (2 records) is implemented through the damage backend; it has no additional event-owned repeat timer. Its common deadline/flags/refs survive, while victim health, burning and other damage effects require their own components.

Read-only stable mapping callbacks convert real source/actor handles into authored UIDs and resolve them to fresh handles. Zero and UINT32_MAX retain distinct sentinel tags. Missing mappings reject before publication. Convenience wrappers admit sentinel references only. The caller must resolve UIDs for the correct level/category and define an explicit policy for removed referenced objects. No raw runtime handle is serialized.

RFEC3 remains 192 bytes with little-endian fields. Header 0..63 contains magic/version/length/FNV checksum (checksum bytes treated as zero), UID, type, identity32 and reserved8. Payload offsets: 64 flags, 68 mode, 72 death-fired, 76 diagnostic death-time; 80..100 cycle remaining/period/limit/count/enabled/unlimited; 104 threshold, 108 threshold-fired; 112 retired; 116/120 source tag/value; 124/128 actor tag/value; 132 reserved; 136..152 Switch disabled/limit/unlimited/activations/mode; 156 UnHide remaining; 160 common delayed-dispatch remaining; 164 UnHide on; 168 UnHide off; 172/176 Countdown_Reaches armed/fired (type 84 only); 180..191 reserved. Tags 0/1 represent zero/UINT32_MAX with zero value; tag2 holds UID. Inapplicable fields are zero, except common remaining time is -1 when disabled. RFEC2 is accepted with inactive common delay and no pending UnHide requests; RFEC1 rejects.

Focused test source retains cycle/Switch/retirement checks and adds actual rf_event_activate delayed scheduling, save/rebase, rf_event_tick delivery with regenerated references, pending UnHide delivery, invalid remaining-time atomic rejection and RFEC2 compatibility. This extension was not built or run by its author; parent integration owns validation. No original executable execution or screenshots were used.

Parent validation: focused event codec and scene adapter checks pass, including delayed dispatch and pending UnHide delivery; shared PC and NXDK builds pass. Unmodified L1S1 captures all184 event-local packets, while full scene save/load remains unimplemented.
