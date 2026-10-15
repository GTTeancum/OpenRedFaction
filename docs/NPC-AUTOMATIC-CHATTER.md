# Automatic NPC persona chatter

Update15:08 UTC: compiled in670bdf36 and included in the passing Actual15
ordinary startup check. Broader interaction/audio/save functionality remains
tester-owned; earlier source-only wording below is historical.

Source-written October15 after user authorization to continue NPC interactions
between menu builds. Native compilation and tester functional coverage pending.

RF.exe403a24/403a2a services Timeout then Humming. Persona430570/430630 resolves
$Humming: and $Timeout:, then434da0 Foley selection and4299b0 actor voice.
Installed nonmedic bindings name9 groups/39 real WAVs. Missing authored groups
remain silent; no name corrections or invented lines.

Humming403db0 uses Waiting2/Wandering4, alive humanoid, no target/current speech,
814bit1/810bit0x02000000 exclusions. Constructor402ef5 schedules60000..89999ms;
403e65 adds selected sample duration on rearm. Timeout403ea0 requires authored
permission, live holstered nonattached player and squared distance<4 (radius2).
It arms6000..9999ms, retains armed timer when out of range, and clears before
rechecking range on expiry. Neither voice failure nor absent assets retries
every frame; Humming rearms after each attempt.

A retained-v180 accessor resolves final17-byte switches5==1 (Timeout permission),
6==1 (no Humming),13!=0 (ordinary Use/Timeout exclusion), matching4648be,4648e0,
464951. It does not mutate gameplay flags. The last exclusion is not applied
to direct medic40a670 admission.

Use/Humming/Timeout share three ten-bit lanes in the existing4-byte/class
metadata; Use6 medics retain Use/Gone/Ignore and are excluded from this scheduler.
An optional24-byte/actor sidecar holds two chatter deadlines, immutable flags,
full identity and the additional Use-facing target/deadline described in
NPC-USE-FACING.md. Admission charges it inside the unchanged640-KiB NPC cap;
if no room or optional allocation fails, chatter stays disabled. Actor/body
and checkpoint ABI/static allowances remain unchanged. No count or PCM/voice
limit rises. The sidecar retires with bodies and resets on identity changes.

Successful world publication/storage close resets presentation timers and stops
existing optional voices; rejected loads retain old timers/voices. No new save
format or exact original timer continuation is claimed. Existing simulation
clock and wrap-safe timer helpers drive the scheduler, not render cadence.

Conservative port coexistence additionally suppresses cutscene/endgame, hidden,
AI-suppressed, attached, combat-target and scripted movement/look/shoot/animation
owners. This protects operational behavior; it is not claimed as extra original
gating. Qualified Use-facing/default-patrol pause is now included in the compiled
NPC-USE-FACING.md slice. Speech gestures remain disabled pending residual save
ownership. Broader functional checks belong to testers.
