# Actor section persistence

Living actor health and armor now survive section backtracking, using the
existing level/UID persistence slots. The appended fixed vitals owner costs24KiB;
no per-hit or per-transition allocations, runtime handles or pointers are saved.
Retirement still takes precedence over old living values. Tests cover wounded
restoration, identical UIDs in different sections, untouched first visits and
retirement. All37 PC tests pass. The actor-vitals replay shoots guard8456 before
explicit L1S1->L1S2->L1S1 exits and retains80.8 health/79.2 armor. This is staged
encounter/exit coverage; full natural route and complete actor-state persistence
remain open. Position/orientation, allegiance, inventory and dropped items are
not included. Native harness compares saved vitals against PC; return-exit
staging now permits no pickup UID on both platforms. Reproduce PC coverage with
python tools/replay_actor_vitals.py. Evidence:artifacts/actor-vitals.
Stock64MiB XEMU replay `replay-20260914-120730` passes240 frames and both
section transitions, with saved actor vitals exactly matching PC (including
guard8456 at80.8 health/79.2 armor). NXDK build and restoration pass.


Native reproduction:

```powershell
python tools/xemu_replay_check.py artifacts/actor-vitals/inputs.bin --campaign-spawn --level L1S1.rfl --actor-uid 8456 --exit-uid 9019 --return-exit-uid 9346 --seconds 240
```

## Mission state

Actor section persistence now includes allegiance and the authored hidden/
invulnerable object bits0x4000/4, alongside health/armor. This adds16KiB of fixed
storage (40KiB total for vitals plus mission fields), with no transient handles,
physics flags or animation state retained. Restore publishes flags consistently
to object/view/room owners. Retirement still wins. Tests exercise real mission
setters before capture, restore, unchanged first visits, section separation and
preservation of unrelated construction flags. All37 PC tests pass. A controlled
L1S1 round trip compares authored Make_Invulnerable9668 against a control:
actor9643 retains flag4 versus0, with allegiance1 in both. The guard damage
regression remains80.8 health/79.2 armor. Replay setup now permits existing
Make_Invulnerable and Set_Friendliness runtime events without bypassing delays.
Reproduce:python tools/replay_actor_vitals.py --mission-event. Full natural
traversal, actor placement/inventory and remaining world state remain open.
Evidence:artifacts/actor-vitals/mission-*.
Stock64MiB XEMU replay `replay-20260914-121653` passes240 frames and both
section transitions with exact PC mission-state/vitals comparisons, including
actor9643 allegiance1 and invulnerability flag4. NXDK restoration succeeds.


Native verification uses the same command above with `--setup-uid 9668`.
