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
