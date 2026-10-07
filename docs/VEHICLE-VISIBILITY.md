# Parked vehicle visibility

## Integrated behavior

Passive vehicle owners now honor authored hidden state and ordinary `UnHide`
events. The normal `Invert` → `UnHide` OFF path hides them again. The callback
requires the exact registered owner and full handle, changes only object flag
`0x4000`, and does not resurrect dead/destroyed owners or change their health,
faction, pose, physics, attachment, ammunition or identity.

That live flag gates passive rendering, actor/projectile hull admission, direct
damage, support/push paths and wreck-exit obstacles. Existing blast, NPC target
and entry checks already use it. Visible wreck behavior is retained; this change
does not equate hidden with destroyed.

Homing and ordinary boarding now follow the live flag instead of permanently
rejecting an initially hidden authored seed. Potential hidden-owner profile
packs are prepared under the existing optional 4 MiB budget and memory reserve.
Save boot still validates source identity, class, seat, group and authored vitals,
but allows an originally hidden owner that was legitimately revealed and boarded.

No save format changed. RFVA2 already stores/restores `damage.object_flags`,
and RFSW restores those authoritative flags into parked control state. The
same-class test continues to use RFSW1, RFVA2 and RFVC3.

## Stock 64 MiB cloud Xbox evidence

The final source was built with NXDK/Clang 19 and run serially in Linux cloud
XEMU 0.8.136. Both runs verified exactly 67,108,864 bytes of RAM. No PC gameplay,
campaign route, image capture or host input was used.

The disposable CTF06 fixture uses the established two-Jeep geometry. Jeep A is
original UID 7629; B is a copy with fixture UID 915200, staged hidden and hostile.
Normal event dispatch and in-game Use/fire inputs provide the transitions.

- Source, 420 frames: B starts hidden. At frames 120, 240 and 300 it is revealed,
  hidden and revealed. Each callback retains the full owner handle and health
  400, changes only `0x4000`, and changes actual homing admission 0→1→0→1.
- Live rendering/hull evidence: the hidden sample has no passive submission or
  hull enumeration despite 122 ground-query opportunities. The revealed sample
  has one admitted passive model, four batches, 63 actor-hull queries and 15
  projectile-hull queries. These are admission observations, not a visual or
  actual hull-contact claim.
- Ordinary Use promotes revealed B at frame 310. Parked A is hidden at frame
  360. The 17,304-byte ordinary save retains hidden A, occupied visible B,
  health 400 for both and independent ammunition 996/998.
- Fresh boot, 140 frames: B restores occupied without another visibility event;
  A restores hidden. After normal exit and Use, the player reboards visible B
  without promoting hidden A. UID identity persists; full registry handles are
  rebound on fresh boot and remain stable within each run.
- From the restored live probe at frame 42 through frame 111 and the endpoint,
  A's hull-query/hit counters do not grow while ground-query opportunities grow
  from 43 to 152 to 181. A remains excluded from rendering. Endpoint free pages
  are 5,948 for the source and 5,788 after fresh load.

The independent reviewer reran the validator against the original runtime
results and payload and checked negative mutations of flags, handles and hull
counters. Seven host transport tests, Python parsing, strict Xbox build and
diff checks pass. The stack reserve and listed compiler chain subtotals are
unchanged; these subtotals are not a whole-program stack proof.

### Validation corrections

The original two harness failure reports are preserved. The source expectation
incorrectly used event type 1 for the setup Delay; setup telemetry reports the
actual Delay type 48. The fresh-load assertion incorrectly required lifetime
hull counters to be zero, although startup before restoration contributed six
queries/four hits. The corrected test requires no growth after the first restored
live sample, with continuing ground-query opportunities. No gameplay code or
recorded runtime data changed for these corrections. A separate validated report
records the successful checks against the retained source/load evidence.

## Reproduce and limits

```sh
bash tools/build-xbox.sh
python3 -B tools/xemu_vehicle_visibility.py
```

The harness supports `--prepare-only` and `--resume-saved-run` for source evidence
reuse. Original inputs remain read-only; generated fixtures and compact reports
are under ignored `artifacts/` paths. Disc configuration is restored after each
batch.

Generic `Switch` routing and visibility of an already-active vehicle remain
outside this parked-owner slice. Moving/grouped support paths have consistent
source gates but were not independently exercised here. No new guided-shot,
visible appearance, audio, dead-owner resurrection or broad campaign claim is
made. Overall working-alpha and vehicle estimates remain approximately 88% and
95%, respectively.
