# Living Jeep drivers across vehicle switching

A stationary Jeep can now retain its original living NPC driver when the
player leaves the gunner seat and selects another Jeep. Previously switching
rejected every nonempty driver slot, because a passive chassis had no physical
seat endpoints. The change preserves the real authored driver relationship;
it does not create passengers or add another autonomous vehicle solver.

The passive owner now has canonical driver and two occupant words. Typed seat
resolution verifies the UID, full generation handle, registry pointer, damage
owner, Jeep resource and unattached ownership. Passive chassis keep their
existing kind11 registry owner and have no generic entity view. The actor's
binding, parent link and AI state survive the same assignment-only registry
exchange that already transfers empty vehicles. Returning to a driven Jeep
preflights interface_2 and restores its NPC driver before ordinary gunner Use.

Only the exact living authored driver in slot0 gains this exception. Slot1
must be empty, and occupied transfers are restricted to Jeep-to-Jeep. Retained
or pending movement/AI routes, moving/spinning chassis, group attachments,
unrelated riders/targets, stale or duplicate identities, pending combat,
frozen admission and failed real clearance still block switching. Occupied
passive Hide remains unsupported. Driver collision exclusions require the
reciprocal seat relationship, not merely a stored driver word.

## Saves and release

Existing RFNS/RFVA/RFSW wire layouts are retained. Source-validated boot hints
construct inactive identity placeholders for saved passive seats. Staging
requires the exact admitted stationary RFSW Jeep and living visible RFVA2
owner, with matching saved pose and authored interface_1. NPC pose/support/AI
checks and candidate-world hull checks precede publication. Only the exact
own-driver overlap is permitted; world, movers, props, player, other NPCs,
the selected host and other visible passive hulls remain obstacles. Visible
wrecks are retained. Final RFNS assignment writes the canonical passive slots,
independently of the later parked-bank allocation transfer.

If that parked host is destroyed, its living driver uses the actual parked
Jeep pose and class hull for the existing bounded safe-exit search. Transit
excludes only that wreck and includes the unrelated selected chassis; final
full-body placement must clear both. A blocked exit retains the seat and
retries. The stationary passive owner contributes zero linear/angular exit
momentum. No selected-host pose or velocity is borrowed. This remains the
port's practical egress policy, not exact original ejection parity.

## Stock64MiB cloud evidence

PASS: `artifacts/xemu/passive-jeep-driver-20261007-222142/report.json`.
The CTF06 fixture retains the complete original miner7646→Jeep7629 records
except declared transforms, and adds the existing empty Jeep copy915200.
There are no route or AI-mode events. One ordinary saved Delay later Slays A.

- Source350 frames: ordinary gunner boarding, real gunfire, exit, A→B
  selection and an occupied-B save. The living miner remains in parked A with
  exact reciprocal slots, parent, full handle, AI13 and authored tag pose.
  A/B ammunition is996/998, with independent RNG state. Both chassis retain
  health400/armor0; the miner retains health1/armor1.
- Return140 frames: fresh load reconstructs passive A and its driver without
  setup/rebinding, then ordinary exit from B and return to A's gunner seat
  preserve ownership, ammunition and real full-body exit clearance.
- Wreck320 frames: a second fresh boot reads the same save. The retained
  Delay destroys only parked A; at frame252 its living driver exits safely,
  releases the exact passive slots and enters Waiting2. All nine released,
  host and angular velocity words are zero. B remains alive and occupied.
  Final actual-sphere gaps are0.41047m from A and1.03734m from B, with ordinary
  full-body world/obstacle admission passing.

The17,152-byte payload is SHA256
`502a6be0d112b6e8e693de9b2d782d3699de4a80930d2879af77989e902b9482`.
It contains RFSW1, RFNS1, RFVA2, RFVC3, RFNC10 and RFEC3. Both loads reject
all22 isolated malformed seat candidates without mutating live owners or the
original admitted stages. Full registry handles stay stable within each run;
fresh boot rebuilds generations from saved authored UIDs.

Saved poses match the exact final source frame349 and restored frame0.
Parked A and its seated NPC remain bit-exact. Active B continues its ordinary
rigid settling: measured maximum horizontal change0, vertical0.00014067m and
basis-component change0.00020770, within explicit0.1mm/1mm/0.001 limits.
The earlier `221456` FAIL is retained: it incorrectly compared active B's
save with frame348, before the final rigid step. Gameplay was unchanged by
the observation correction. Final validators also require a living player.

Endpoint free pages are5635/5474/5474. All39 staged disc inputs restore
byte-for-byte. The nine actual prelaunch XBE/PE/map copies retain individual
hashes; all tested PEs are
`b7262f0d1f8d00a4998e2c49c98fbb1a272e929e5130032891a5360fcf235015`.
The PE reserves128KiB of stack; the native malformed audit's compiled frame is
4444bytes before saved registers/calls, not a measured runtime high-water mark.

```sh
bash tools/build-xbox.sh
python3 tools/xemu_passive_jeep_driver.py
```

No independently moving NPC vehicle AI, cross-class occupied transfer, extra
passengers, current-session restore, blocked/crowded wreck runtime matrix,
audio/visual parity or campaign progression is established by these checks.
Overall implementation remains approximately88%, vehicles approximately96%,
as rough first-playable estimates. Concurrent per-owner vehicle simulation
remains substantial separate work.
