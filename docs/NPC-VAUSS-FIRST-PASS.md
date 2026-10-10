# Ordinary NPC Vauss first pass

Status: source-written on 2026-10-10 and parent-integrated after independent
source review found no material blocker. Compilation and action runtime remain
unverified. No build, syntax check, test, PC/Xbox/emulator run, route, fixture,
inventory grant, input change, image or original-asset mutation was performed
for this slice.

## Missing playable consumer and actual owners

The ordinary primary selector admits six legacy slots. The extra selector
admits Machine Pistol, HMG, scoped AR and Undercover handgun. Neither admits
Vauss. The separate Tankbot selector checks the exact Tankbot Chaingun identity.
Consequently an otherwise live ordinary NPC holding its real Vauss cannot reach
normal primary combat, although mounted Vauss already has a working consumer.

Read-only original archive decoding establishes the two relevant placed owners:

- `levels2.vpp/L9S4.rfl` UID964, file-relative record offset2227896, length209.
- `levels2.vpp/L9S4.rfl` UID965, file-relative record offset2228105, length209.
- Both are `Meca Turret`, primary `Vauss`, secondary `none`, friendliness0,
  health30, armor override-1, creation flags0, raw AI mode byte1 and raw attack
  style byte2. Raw authored bytes are not runtime enum values.
- `entity.tbl:4144–4187` supplies `mft2.vcm`, robot-fly movement, class armor0,
  allowed Laser/Vauss, default Laser, no default secondary, no force-launch
  override and use-none. These are ordinary NPC bodies, not player mounts.

The installed supply catalog resolves Vauss to weapon18 and turret_ammo to
pool15. These are source observations, never constants in the implementation.
Actual supply mapping remains name/catalog-based.

Existing `rf_weapon_startup_grant_sp` first acquires and fills Meca's class
default Laser. `scene_npc_loadout_apply` then acquires the authored primary
Vauss, preserves other owned primaries, fills its actual mapped reserve from
capacity170 and clears the secondary category for `none`. Its loaded magazine
is zero. Thus both owners have the source-derived selected/owned Vauss and
170 finite reserve rounds; they retain their independent default Laser
ownership/reserve. This is a source-based construction result, not an observed
live inventory or successful level-admission claim.

The inspection evidence is retained outside tracked source in
`npc-vauss-next/evidence/original-vauss.json` and the numbered table excerpts.
The original L9S4 entry SHA-256 is
`7534a2369b33054ba83f23b4410e3bd492bb0774ab51ae8f5339800fd9a7bbba`;
weapons.tbl is
`5f93d97b44342df4d0734cbbd67953569a8abe49967874d1854b11f1b26cb8db`.

## Authored primary and finite acceptance

`weapons.tbl:1709–1766` defines exact Vauss:

- continuous_fire / from_eye; Flags2 undeviating; bullet damage kind1
- turret_ammo; maximum reserve170/170; no Clip Size or Reload Time
- AI attack range30/30, fire wait0.08 seconds, AI spread1.5/1.5 degrees
- damage100, primary AI damage scale0.2, one projectile and no burst declaration
- piercing enabled, power0.1; independent projectile collision radius0.02
- Launch Vauss5 Fire; ordinary dry Gun Hit Default/Flesh/Metal/Rock/Water groups

`scene_ai_vauss_primary.inc` loads the exact primary through the existing
continuous-weapon reader, requiring primary catalog identity, finite mapped
ammo, no clip/reload, bullet kind, one projectile and no burst. It never aliases
a player slot, creates a mounted host or broadens another weapon's selector.
Both ordinary combat and opposed acquisition require the owner's exact selected
Vauss and actual ownership.

Readiness requires zero loaded rounds, a valid actual ammo mapping and
nonnegative finite reserve. It cancels a previous magazine reload without
moving ammo. Zero reserve returns the existing exhausted event3; it does not
replenish, decrement below zero or create a reload. The existing fallback policy
may choose a genuinely owned supported replacement. This slice does not extend
that policy to select the retained default Laser; a Meca with no eligible
fallback therefore remains dry on Vauss.

Each accepted ordinary automatic shot retains one `campaign_enemy_cadence` call;
queued Shoot_Once preserves its existing cadence bypass. Both retain one
`rf_weapon_consume_shot` debit, one spread evaluation and one fire presentation.
Original accepted-fire4267c4–4267c6 calls the finite debit4257c0/4c86e0; the latter
selects mapped reserve for clipless guns. The shared port primitive already
implements that ownership. Vauss's authored count1 leaves no automatic burst
remaining and produces `ceil(0.08 * 60) = 5` ticks under the existing60Hz policy.
The first contact's AI base is100×0.2=20 before target-specific modifiers.
No Tankbot16-shot cadence or Tankbot burst audio is borrowed.

## Shared actor continuation, distinct identity

`scene_ai_actor_penetration.inc` extracts the existing admitted Tankbot
continuation algorithm into neutral names. The only algorithm-entry change is
that exact weapon admission belongs to the two small wrapper functions rather
than a Tankbot-only check inside the shared core. The core retains bullet kind,
weapon bounds, authored enable and finite-input checks. The Tankbot wrapper
remains a live caller; the Vauss wrapper independently requires exact admitted
Vauss. No dormant legacy implementation or duplicate continuation algorithm
is left compiled.

The parent captures the accepted full source handle, intended target, weapon,
fresh AI base, damage kind, authored enable/power and original eye before fire
presentation. Dispatch uses this snapshot after the existing spread sample,
not a subsequently changed held weapon. Only authored-enabled Tankbot/Vauss
enters the shared continuation; disabled piercing retains ordinary resolution.

The original object policy remains the evidence already documented in
SNIPER-PENETRATION.md, HANDHELD-ACTOR-PENETRATION.md,
MOUNTED-ACTOR-PENETRATION.md and TANKBOT-ACTOR-PENETRATION.md:

- RF.exe4c9b48–4c9b73 computes each body's damage from fresh base and current
  remaining/initial power, never an already-attenuated previous damage amount.
- 4c9b75–4c9bde spends0.2×actual enclosing body radius, lets the exhausting
  contact receive damage, and advances the ray from entry by that radius.
- The existing bounded helper retains up to32 visited full generation handles.
- Player-first/strict-nearer NPC actor/shield selection, source-related
  exclusion and full-generation revalidation stay unchanged.
- Every continuation re-arbitrates vehicle, clutter, rubble and world cover on
  the complete original ray. Body-radius advance cannot skip terminal cover.
- Physical and Nano shields use the accepted unattenuated base and consume the
  accepted contact, including a breaking hit. No power is spent for shields.
- NPC death entry and actual player/vehicle damage accounting stay separate;
  the original intended target cannot manufacture a player-damage observation.

At nominal power0.1 a body of radius0.5 or larger exhausts the shot immediately,
while still receiving the first hit. This is not a promise of visible
multi-actor penetration through ordinary human-sized targets.

Laser's two player-liveness references are renamed to the neutral shared
helper because they already used this same predicate. Its projectile,
acceptance, contacts and transient save policy are otherwise untouched.

## Authored audio and existing lifecycle

The independent original table/audio evidence is:

- weapons.tbl1732 selects Vauss5 Fire.
- foley.tbl2115–2116 maps it to vauss_05.wav, near10, gain0.95.
- bluebeard.bty17928–17935 marks that waveform looping from0.
- Original41a9bd guards the retained actor+81c voice;41aa3d–41aa95 selects and
  retains positional Launch, and41aee3–41aef7 closes it at firing reset.

The existing `scene_npc_loop_audio` mapper gains exact admitted Vauss as its
third primary. It shares its existing accepted-shot start, actual cooldown
retention, full source/voice-generation checks, spatial update and pass-end
closure. It still suppresses queued single/no-animation loop playback rather
than inventing a one-shot conversion or arbitrary release deadline. Missing
or malformed audio is optional and cannot reject gameplay. Actor action2 Foley
remains independently invoked once by ordinary fire presentation; Meca authors
an empty fire_stand sound and its real mft2_attack motion.

The existing dry-impact allowlist gains exact Vauss. Its five groups already
occur among the admitted ordinary guns, so normal table contents deduplicate
through the same resident bounded bank. Caps, headroom, rollback, nonloop
requirements and optional-failure policy are unchanged. Calls follow actual
accepted body or terminal cover contacts; world/underwater parity and impact
Vclips remain separate. Mounted Vauss callbacks are not rewired.

All voice lifecycle and successful-load close paths already belong to the NPC
loop owner. There is no new allocation, voice pool, timer, persistent projectile
or save field. Existing inventory stores the real reserve; existing NPC combat
rows store due/burst values. No synthetic projectile save row is introduced.

## Honest first-pass boundaries

This is the existing synchronous ordinary NPC ray with current eye origin,
coarse body spheres and existing directional physical-shield queries. It does
not reconstruct the authored275-unit/s projectile flight,0.5-second lifetime,
tracer, exact eye/muzzle animation transform, ricochet or world-metal exit.
Undeviating is not used to suppress the authored1.5-degree AI spread. Shared
sight, turning, pursuit and firing-cone policy remains the existing practical
port policy rather than claimed retail AI parity.

Parent owns active scene integration and the scheduled Xbox batch. Original
neutral startup/resource admission can establish only that the level and
metadata load; it cannot demonstrate actual attacks, finite exhaustion,
penetration/cover/shield ordering, audible loop shutdown or save restoration.
Those remain unverified until an authorized bounded check or tester feedback.
