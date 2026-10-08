# Authored vehicle weapon admission

The parent's12:00 UTC combined c29d6f NXDK build passes. Runtime verification
remains pending. No new tests, fixtures, emulator runs or images were made.

Original instance setup464010 applies primary/secondary strings after class
startup422360. The existing reconstructed class parser records defaults from
41bf57..41c073; the existing NPC override adapter records that empty/unknown
names inherit, a recognized override selects that weapon, and explicit `none`
clears its category ownership/selection while retaining dormant ammunition. Vehicles now use the same source distinction.

The concrete missing case is original L13S3 Fighter8955: both instance weapon
strings are `none`, despite Fighter01 class defaults being Fighter Minigun and
Fighter Rocket. The previous selected-host resource loader granted900/20
rounds and permitted fire unconditionally. L12S1 Fighter9700 and L1S3 APC26 explicitly disable
their secondary as well. A hardware definition/capacity is not weapon ownership.

`scene_vehicle_loadout_resolve` reads the loaded class default IDs, translates
through catalog names, then applies the original instance strings. It returns
both actual selected IDs and the set of implemented class-specific firing
adapters. A known alternate weapon with no adapter retains its identity but
cannot fire the old class gun. Unknown names retain the class default rather
than becoming explicit none. There is no table reload or allocation per tick.

Integration covers ordinary Driller/APC/Jeep/submarine/Fighter profiles:

- Initial active-host construction publishes the resolved weapon IDs and
  clears unsupported trigger/spin-up state without changing ammunition or definitions.
- Every firing path has a source-loadout gate. A disabled Driller cannot cut;
  disabled miniguns, mortar/rockets and torpedoes cannot launch or debit ammo.
- First-time and returning vehicle switches normalize the candidate's own
  loadout before transactional publication. One owner's weapons do not become
  another owner's grants. Both enabled and dormant ammunition remain unchanged.
- RFSW decoding validates raw data before publishing source-derived selection.
  Ordinary and direct selected-host restore normalize afterward as well.
  Existing older saves may contain900/20 dormant rounds; those rounds remain
  intact but cannot authorize fire from explicitly disabled weapons. No format changes
  are needed because ownership comes from the source-identity-pinned record.
- The vehicle HUD omits unavailable weapon ammunition. Existing in-flight
  projectile stepping/damage continues; the gate only controls new firing.
- Scripted Fighter Attack now uses this same resolver to admit its primary,
  preserving the original9700 minigun/secondary-none loadout.
- Explicit enemy-free developer vehicles without authored UIDs retain their
  existing developer supply policy. This does not become a campaign fallback.

Autonomous secondary selection and actual firing adapters for arbitrary
instance replacement guns remain implementation work. The distinct boss
profile is handled separately and is not treated as an ordinary Fighter here.

Correction after the12:00 build: the first implementation incorrectly cleared
unavailable reserves. Source review of464010 and the existing NPC loadout
composition shows that `none` leaves ammo intact. The revised code retains all
dormant rounds and gates selection/new firing instead; that revision awaits
the13:00 build. No new runtime result is claimed.
