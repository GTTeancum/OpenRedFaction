# Xbox gameplay button mapping

Source-written October 12, 2026, against `d2d37a53b6aca9737fe3c823adf5fc78af037b6a`
(the accepted credits successor to the lane's `f859c52d` starting source).
Staged outside the real repository. No compilation, test, fixture, emulator,
HDD operation, image, cleanup, upload or publication was performed. The parent
owns the hourly compile/package rollup and startup smoke; testers own gameplay
and persistence coverage. This is implementation, not a verified runtime result.

## Bounded first-playable controls

Options now adds **Button Mapping** at index 7, retaining indices 0–6. Its two
six-row pages expose ten gameplay actions followed by Reset Buttons and Back:

| Action | Default physical control |
| --- | --- |
| Fire | RT |
| Alt Fire | LT |
| Jump | A |
| Use | X |
| Reload | Y |
| Crouch | B |
| Next Weapon | Black |
| Previous Weapon | White |
| Fine Aim | Hold right-stick click |
| Holster | Left-stick click |

D-pad Up/Down chooses an action and automatically changes page at the boundary;
Left/Right cycles its control, and A advances to the next control. Choosing an
occupied control **swaps the two actions**, preserving a complete one-to-one
mapping. The screen states that rule before changes. There are no duplicate,
unbound, modifier, macro or multi-device entries. Fine Aim remains a held
50-percent look modifier; Holster remains the existing scene-owned edge/toggle.
The menu displays full stick-click names; compact weapon HUD labels use L3/R3.

Reset Buttons restores only this permutation. The existing Options Reset
Defaults also restores the default buttons together with Standard analog
layout, sensitivity 100, invert off and deadzone 18. Changes apply immediately,
survive controller/session open/close and End Session/New Game, and are saved
only by a fresh physical A edge on the existing **Save Settings** row. The saved
baseline comparison includes the permutation. Back/cancel is navigation, not
an undo or an implicit disk write. Startup restore includes saved mapping.

This is explicit Xbox port policy, not a recovered retail layout/editor. The
bundled NXDK SDL source confirms Original Xbox rightshoulder:b5 / leftshoulder:b4
and maps Black to right shoulder and White to left shoulder. Its existing trigger
threshold (3855) is unchanged. The change adds no controller backend, axis editor,
PC control remapping, gameplay/replay field or campaign-save codec.

## Reservations, suppression and action ownership

- Start pause/resume, Back+Start End Session, physical menu A accept/B cancel
  and D-pad navigation stay fixed. Options cannot select them as candidates.
- Physical Back+Y Save and Back+X Load remain independent of mapped Reload/Use.
  Their existing load-over-save priority and released/hotplug request admission
  remain in place. Remapping Use away from X never moves the Load chord.
- Any raw Back+X/Y chord suppresses gameplay. Its X/Y constituents and a
  simultaneously held mapped Holster are consumed through release. A remapped
  Fire/Alt on X/Y cannot escape when only Back is released. The separate
  chord-consumption latch does not reset a held save/load request each poll.
- D-pad Left/Right remain fixed previous/next-weapon aliases; opposing cycle
  directions cancel as before. D-pad Up/Down remain the fixed defuse controls.
  Black/White can be reassigned without removing the D-pad weapon route.
- Held-action suppression is now indexed by raw physical control, including
  both triggers, before mapping. Attach, disconnect replacement, section reopen,
  pause and resume still cannot convert held controls into new actions.
- A changed permutation adds a full-neutral gate: all candidate buttons,
  triggers, D-pad, Start and Back must be released before gameplay resumes.
  Menus remain usable while the gate waits. Merely disconnecting or closing the
  input owner does not satisfy neutral. The analog stick axes need not center.
- A rebind separately marks alternate samples suppressed until a fresh effective
  press of the newly mapped Alt control. This preserves the Rocket lock owner's
  existing suppression/rearm path; neutral after a rebind is not a release-fired
  shot. Paused polling remains USB-responsive without advancing simulation.
- Fine Aim still applies after normalized look saturation and composes with the
  existing scope owner. Holster keeps its busy/dead/mounted/cutscene admission,
  equipment ownership and same-size RFWM continuation. Those scene owners are
  not moved into the mapping UI or input adapter.

Authored action-token expansion now queries effective bindings when presenting
subtitles, Message History and credits. Rocket lock, remote detonator,
death/recovery and mission-failure hints also use the mapped action. The fixed
save/load hint remains physical. Original text, voice, timing and stored history
are not rewritten. PC label paths remain unchanged.

## Same-size RFCS3 and legacy compatibility

Each controller-settings slot remains exactly 36 bytes. Paths, little-endian
header/generation, checksum over bytes 0–31, slot parity and adjacent-generation
rules are unchanged. Only the version and word at bytes 28–31 gain a new case:

- RFCS1 still requires reserved word zero and decodes Standard/default buttons.
- RFCS2 still requires layout exactly 0 or 1 and decodes default buttons.
- RFCS3 requires word28 = `(permutation_rank * 2) + stick_layout`, strictly less
  than 7,257,600. The low bit is Standard/Southpaw and the rest is a rank from
  0 through 3,628,799. No unused upper bits are admitted.

The canonical order is the table above; its default control sequence is
RT, LT, A, X, Y, B, Black, White, right click, left click. The rank is the
lexicographic permutation rank: for each action count the later controls with
smaller IDs, then accumulate those digits in radices 10, 9, …, 1. Decoding selects
and removes one control from the remaining ordered pool using 9!, 8!, …, 0!.
Thus every admitted rank decodes to exactly one complete permutation. No
malformed duplicate assignment can reach the live setter through storage.
The live setter normalizes an out-of-range rank to defaults; storage rejects it.

Default buttons continue writing canonical RFCS1 for Standard and RFCS2 for
Southpaw, byte-identical at the same prior options and generation. RFCS3 with
rank zero is accepted and canonicalizes on the next explicit Save. Non-default
buttons write RFCS3. Mixed legacy/new neighboring records retain the existing
highest-generation selection and adjacency admission. Earlier executables
regard RFCS3 as unknown and refuse writing over it, preserving the newer data.

All existing invalid/unknown-file write refusals, last-good recovery notices,
exclusive older-target revalidation, newest-slot protection, full-byte writes,
flush/close/reread comparison, alias ownership, uncertain-close terminal latch,
generation-wrap refusal and publish-after-final-success order remain intact.
There is no file migration, truncation, silent repair or changed disk owner.

## Integration and limits

The new input option is one uint32 rank; the live decode cache is ten uint32s.
UI labels and action lookup tables are small static arrays. The two-page draw
reuses the existing procedural HUD, with bounded local text and pointer arrays,
no allocation, new image/texture owner or changed stock64 budget. The largest
mapping-row label is below the existing 640-pixel text width; the last footer
begins at y442. Rendered UI and physical-controller feel remain unverified.

The complete increment includes input/storage, Options and draw-only menu
wiring, live hint labels, this document and the top TODO milestone. Apply as
localized hunks; the accepted authored-credits implementation in scene.c must
remain intact. No tests or fixtures are supplied or requested in this lane.
