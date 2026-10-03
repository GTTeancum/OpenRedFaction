# Same-class vehicle switching

The current implementation permits ordinary Use to transfer the active runtime between nearby parked, unoccupied Drillers, APCs or Jeeps of the same class. Each authored vehicle retains its UID and generation handle; checked registry exchange changes which runtime represents it. A bounded level-local bank retains rigid state, ammunition, firing cadence/random state and aiming state. Passive health and damage remain authoritative while parked.

Admission rejects occupied or linked owners, active or authored movement routes, moving chassis, pending firing/effects, grouped hosts and obstructed boarding paths. Cross-class switching, submarines and Fighters remain open. These are explicit first-pass constraints, not complete campaign vehicle support.

Ordinary saves and direct in-session restores reject switched-owner state until multi-owner persistence is implemented. Loading an older save through a fresh scene remains available because teardown clears the parked bank.

Validation: the checked registry-exchange unit passes and the NXDK Xbox build succeeds. Stock-64-MiB Xbox report `artifacts/xemu/vehicle-switch-20261003-165718/report.json` passes 480 frames: three ordinary boardings, two exits, two switches, stable handles, exact parked pose/vitals and independently spent ammunition retained on return. The fixture restores the disc afterward. Driller/APC switching is compiled but not separately exercised. No visual or campaign-playthrough claim is made.
