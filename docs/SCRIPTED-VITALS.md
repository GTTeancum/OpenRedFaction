# Scripted Heal and Armor

Authored Heal and Armor events now dispatch signed amounts to linked NPCs and
the optional player target through shared runtime callbacks, including delayed
activation. Class limits clamp values; negative amounts support authored training
and campaign reductions. Dead actors are not resurrected; newly lethal NPC
reductions enter the existing death path. Unsupported non-NPC objects remain
unimplemented. Original evidence:Heal4bb0d0, Armor4bb170, heal helper48a510;
signed event+2b8 amounts and optional-player byte+2bc. This is a practical shared
implementation, not complete original helper equivalence. All37 PC tests pass,
covering delayed dispatch, linked/player selection, caps, reductions and stale
handles. The controlled9959/9960 replay retains half-second delays and produces
71.2 final health versus61.6 in control with the same8 enemy hits. No natural
trigger traversal claim. Evidence:artifacts/script-vitals; reproduction:
python tools/replay_script_vitals.py.
Stock64MiB XEMU replay `replay-20260914-123601` passes240 frames with
exact PC combat and player-vitals comparisons:8 hits and71.2 final health.
NXDK build and staging restoration succeed.


Factory4b69d0 maps type13 to constructor4be630/vtable5899bc and type14 to4be650/vtable5899cc. The handlers traverse links before the optional player application. Heal calls48a510; Armor adds and clamps against the entity class armor. Player healing receives an additional zero-to-class-health clamp. The shared first pass supports registered NPC/player owners, clamps both fields to nonnegative class limits and uses100 when a class limit is negative/nonfinite. Original generic-object healing and resurrection are not claimed.

The added backend is required for delayed13/14 processing; absent backends retain unsupported-event reporting. Normal event scheduling and link propagation remain in place.

Native reproduction:

```powershell
python tools/xemu_replay_check.py artifacts/script-vitals/inputs.bin --campaign-spawn --level L1S1.rfl --actor-uid 8456 --setup-uid 9959 9960 --seconds 240
```
