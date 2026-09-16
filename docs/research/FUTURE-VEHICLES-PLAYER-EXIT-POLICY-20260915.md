# Future vehicles: player exit policy

Priority P1: user-requested exit must respect lockout and collision failure before releasing player control. This boundary sits above the common seat helpers and differs from script-forced movement.

`python tools/future_re/vehicle_player_exit_gates.py` executes original4a1970's linked-host exit branch for64 combinations, stopping at successful-detach continuation4a1a00. Host lookup426fc0, detach4279d0, feedback505560 and fallback interaction4c0100 are explicit supplied boundaries. Checked executable SHA256 remains `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`. Output:artifacts/future-vehicles-re/player-exit-gates.json.

ABI4a1970: cdecl(player owner pointer, actor pointer). Actor+200=-1 takes the separate boarding/use branch; this report covers linked actors only.

- Nonzero multiplayer byte64ecb9 exits immediately without lookup/detachment or feedback.
- Resolve actor+200 through426fc0. Fixtures supply a valid host. Original null-host path falls into a later host dereference, so callers must maintain valid ownership; do not imitate that unsafe dereference in the port.
- Host+814 bit0x80 blocks exit.
- Otherwise host+1a8 bit0x4000 combined with signed host+1380<0 blocks exit. These fields are reported by verified offsets; no unverified timer meaning is assigned to1380.
- Either policy block sets **global local player**7c75d4's flags+10 bit0x800, even if the passed player differs. Existing flag bits are preserved. The event/campaign worker owns identifying the consumer semantics of this bit.
- Feedback505560(2,0,0,1.0) occurs at this point only if the passed player equals the local player. The code then falls through to interaction fallback4c0100(actor,1), with4c04e0 and possibly further feedback if fallback rejects. The fixture supplies successful first fallback; its actual interaction effects are outside scope.
- Allowed exit calls4279d0(actor) and **checks AL**. False returns immediately. True enters4a1a00 continuation. This preserves the five-candidate clearance veto proven in EXIT-ADMISSION; no silent host clearing on blocked geometry.

## Following continuation, inspected not executed here

On detach success, actor+1434/+1440 vectors are reset from class+6c/+78. If429990 classifies the old host as a vehicle, actor+87c becomes host+87c+host+864 and actor+868 becomes host+880+host+868; host vectors+144/+15c/+150 are zeroed. Vehicle/turret cases may invoke428e90 when actor weapon2a4 equals global85cce4. Exact view/weapon meanings and that helper's body remain to integrate; these offsets must not be casually relabeled as a complete camera restoration.

## Script coordination and authored examples

Campaign worker independently inspected two callers which ignore4279d0 AL: Teleport_Player4b9864 and cutscene45bc94. Their forced script policies must remain separate from this player-use policy; the campaign worker owns executable caller coverage and reports. An exit helper returningfalse does not mean every original caller stops.

Authored use-kind1 examples remain L1S2 Driller01 UID8122 and L1S3 APC UID9627. Event-controlled never-leave/try-exit behavior is important for actual campaign vehicle segments, but this report does not claim these particular placements contain those event links; authored event examples are being recovered by the campaign worker.

Implementation should expose player exit policy in a vehicle-use service above shared seat mutation and collision candidate selection. Preserve flags/event notification and rejection semantics, then explicitly restore movement/view/weapon state only on success. Current standalone gameplay has no full vehicle possession loop. No shared source edits, builds or emulators.
