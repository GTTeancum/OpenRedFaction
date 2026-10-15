# Medic persona responses

Update15:08 UTC: compiled in670bdf36 and included in the passing Actual15
ordinary startup check. Broader interaction/audio/save functionality remains
tester-owned; earlier source-only wording below is historical.

Source-written after the October15 user authorized NPC interactions during menu
build waits. Native compilation and functional coverage are pending.

RF.exe40a670 selects depleted/missing-syringe response before full-health response,
then treatment. Persona functions4305a0/4305d0/430600 select Use Key, Healing Gone,
and Heal Ignore respectively; actor4299b0 owns all three voice routes.

Installed medic1 selects persona medic. Its groups are medic use (9 samples),
medic gone (5), and medic usenw (6). No names or translations are invented.
The existing four-byte/class metadata allocation stores three ten-bit group+1
lanes: zero means absent, groups0..639 remain inside the existing Foley cap.
Class640, metadata2560-byte ceiling,30 actor slots and the PCM bank cap are unchanged.

The parser publishes all three groups transactionally. Ordinary Use9 keeps its
existing eligibility gates; medic Use6 enters the shared optional actor-voice
helper only through its own qualified action. Busy scripted actor speech remains
protected. Full handles are rechecked after voice publication; successful-load
cleanup retains the existing origin semantics.

Treatment chooses a sample once. Original40a774..40a7aa uses
floor(max(action43 seconds, selected speech seconds)*1000+0.5)+750ms.
Unavailable PCM contributes zero; optional playback failure cannot cancel healing.
Installed success clips are1.091–1.811s versus action43 at4.966667s, retaining
its5717ms deadline. No persistent speech/save fields are introduced.
