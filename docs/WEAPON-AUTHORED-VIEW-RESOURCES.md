# Authored first-person view resources

Status, 2026-10-08 after 22:00: source-written against b84d973. No helper
build, test, emulator, image, cleanup or desktop automation was run. The parent
owns the 23:00 consolidated stock-64-MiB Xbox check. No visual-match claim is
made by this change.

## Concrete defect

The first-person resource parser retained the mesh and four clip names, but
discarded `$1st Person FOV:` and `$1st Person offset:`. The resource owner then
discarded the definition after loading. Scene placement therefore had no
authored metadata and used a mixture of fitted per-slot cameras and manually
copied values. Several disagree with the installed table, including the normal
pistol, Assault Rifle, Riot Stick, Shotgun, Rocket Launcher and Grenade.

`rf_weapon_view_definition` and `rf_player_weapon` now each retain
`float position[3],fov`. Position is the normal camera-local model offset, with
its authored signs, and FOV is in degrees. The owner copies both once during
resource admission. They do not enter or modify bone poses, motion roots or
prepared skinning matrices. Scene placement is a separate change consuming
these retained values.

## Original and installed evidence

Read-only disassembly of the supplied RF.exe identifies:

- 4c305e..4c3083: optional first-person mesh at descriptor +40.
- 4c3084..4c30a9: optional normal FOV at descriptor +74; absent values use the
  90-degree float at 59613c.
- 4c30a9..4c30ce: a distinct split-screen FOV at descriptor +78.
- 4c30ce..4c30e0: required normal offset parsed into descriptor +4c.
- 4c30e5..4c30f7: distinct split-screen offset at descriptor +58.
- 4aa762..4aa7a5: normal offset is copied, rotated by the entity eye basis and
  added to the eye position. This is whole-view placement, separate from
  third-person hand/grip attachment.
- 51ba30..51ba5e: skinning composes the stored bone transform with the current
  evaluated pose through 51c620. The existing shared implementation already
  follows this order; adding the table offset inside skinning would apply
  placement at the wrong layer.

The installed `tables.vpp/weapons.tbl` contains 19 first-person mesh
declarations. Every one has a normal offset and FOV. Representative exact
normal values are:

| Weapon | Offset X, Y, Z | FOV |
| --- | --- | --- |
| 12mm handgun | -0.110, -0.140, -0.342 | 65 |
| Undercover 12mm handgun | -0.110, -0.140, -0.342 | 65 |
| Assault Rifle | -0.064, 0.1, -0.28 | 65 |
| Riot Stick | -0.178, -0.418, 0.072 | 40 |
| Shotgun | -0.020, 0.056, 1.071 | 55 |
| Rocket Launcher | 0.05, 0.03, -0.08 | 70 |
| Grenade | -0.202, -0.336, 0.206 | 70 |

The ordinary pistol compatibility constructor uses its exact table values.
Assault Rifle's inverse model-space camera for the scene consumer is therefore
`{0.064,-0.1,0.28}`, independent of weapon slot or fixture identity.

Read-only model/clip directory and section inspection also confirms that the
loaded first-person rigs contain their own authored arm/weapon skeletons. For
example, Assault Rifle has 34 bones with root index 21 (`link-bdbn-root`), while
the pistol has 40 bones with root index 36 (`bdbn-root`). Their loaded clips have
the same respective track counts. Existing topological evaluation already
handles these nonzero root indices. No additional hand attachment, root-index
substitution or guessed mesh scale is introduced. This inspection is asset
evidence, not a playback or rendering test.

## Admission and compatibility

- Normal offset is required, matching the original parser's required-tag path.
  Duplicate, incomplete, nonnumeric or nonfinite vectors reject the candidate.
- Missing normal FOV uses 90. Explicit FOV must be finite and strictly between
  0 and 180 degrees so projection is defined. Duplicate FOV rejects the entry.
- Split-screen field names cannot overwrite normal metadata.
- The existing decimal parser is reused; no NXDK strtof/strtod stub is called.
- The reader stages the complete definition and preserves caller output on
  failure. Resource admission validates metadata before allocation and keeps
  its existing failure cleanup and budget accounting.
- Retention adds 16 bytes to each definition and owned view. Existing sizeof
  accounting includes the owner increase; there is no frame-time allocation,
  archive read or new retained animation payload.
- Existing resource-test sources now supply required normal offsets and check
  normal-versus-split-screen metadata, original FOV default, retained values and
  malformed/duplicate rejection. These checks have not been executed.

Authored sway, turn lag, run-state blending and broader animation transition
fidelity remain separate. No neutral placement offset is fitted to compensate
for those missing effects.
