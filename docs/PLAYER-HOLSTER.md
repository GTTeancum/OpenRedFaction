# Player Holster first pass

Source-written 2026-10-12 against3d3eb4c95bf99618cf1f176aea01a069587f790e.
Staged outside the active repository for parent integration and the next hourly
Xbox batch. No compilation, tests, gameplay fixture, emulator, saved-HDD access,
campaign route, image or upload was performed. Runtime is unverified.

## Authored purpose and original ownership

Read-only supplied RF.exe1.20 NA (documented SHA256
b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836)
and installed archive records establish:

- levels1.vpp/train02_text.tbl entries11/12 describe inconspicuous carrying
  and explicitly ask the Holster ($HOLSTER$) button to holster and unholster.
  tables.vpp/strings.tbl entry150 names Holster;809 names Holster Weapon.
-4a6672..4a6699 calls428e60 and then428e90 when holstered or428f00 otherwise.
  Earlier checks exclude linked/vehicle forms. The installed PC binding and
  retail-console button are not inferred from this dispatcher.
-428f00 rejects absent/already-holstered/negative-primary owners and a local
  busy timer.428f4c..55 ORs entity810 with0x800. It calls local FP reset4aa0b0,
  generic weapon reset41ae70 and clears two delayed actor timers. It does not
  remove the selected weapon or its inventory/ammunition.
-428e90 requires bit0x800 and ownership of the selected primary (with its
  explicit paired alias), clears the bit and invokes local draw4aa140.
-428e60 is the fire predicate; linked kind4 is exempt. NPC authored type64's
  additional7d0 bit0x200 is distinct and is not applied to the player.

This first pass uses the recovered posture bit and owned-selection split.
Instant hide/idle redraw, busy-action refusal and the Xbox mapping below are
explicit port policy. No retail draw/holster animation, timing or new AI
inconspicuousness/disguise policy is claimed. Existing disguise compromise,
scripted attacks and enemy awareness are unchanged.

## Xbox binding and input lifetime

Click left stick to toggle. The bundled nxdk SDL2 header declares
SDL_CONTROLLER_BUTTON_LEFTSTICK. SDL_xboxjoystick.c maps LEFT_THUMB0x0040 to
button8; SDL_gamecontroller.c's Original Xbox mapping uses leftstick:b8.
No existing input.c action read that button. Right-click Fine Aim, B crouch,
movement, triggers, shoulders/D-pad cycle, Start menus and save/load remain
owned by their existing actions. $HOLSTER$ expands to LEFT-STICK CLICK on Xbox;
PC stays UNBOUND.

A new trailing rf_scene_input.holster boolean leaves the old48-byte input
prefix intact. Existing RFI records are unchanged: Xbox clears the entire
struct before reading the old record, and PC allocates zeroed structs before
reading their old prefixes. No RFI7, replay fixture or PC binding is added.

INPUT_HOLSTER participates in existing hotplug/pause release suppression.
The save/load chord also latches a simultaneously held Holster action until
release. The scene samples only its held value before modal/life filtering so
a held click cannot become a fresh request on modal exit. Actual toggle uses
the filtered input after the existing combat-frame and mounted ownership
checks. Busy/dead/unarmed/mounted clicks are consumed, never queued. Frame0
consumes the initial held sample. No additional SDL pump is introduced.

## Ready versus holstered

entity810 bit0x800 is the single retained live posture; the damage mirror
changes only that bit. campaign_explicit_unarmed continues to mean absence of
a selected owned weapon, never holstered carrying. Inventory, magazines,
reserve, selected slot, completed Machine Pistol mode and Undercover suppressor
attachment stay intact.

- A fresh click toggles a living, registered, on-foot owned selection with its
  loaded view. Active forms can use the control without losing their mode.
- The click declines during conventional/Fusion/Flame reloads, queued bursts,
  pending MP/Undercover custom transitions, unreleased Grenade/Remote/Flame
  throws, Remote followup selection, delayed Riot/shield contacts, Rocket lock,
  or active/shared FP2 charge tail. Release and click again after it settles.
  It never cancels/refunds the accepted operation or weakens save admission.
- While holstered, primary, alternate and reload are masked. They do not draw
  or fire. Toggle again, or cycle to an owned available weapon, to draw.
  Cycling also draws when the current weapon is the only eligible one.
- Drawing consumes weapon controls still held across the change until release;
  no same-frame shot, reload, scope toggle or suppressor transition is created.
  A simultaneous Holster request wins over manual cycle for that sample.
- Generic campaign_select_primary preserves posture. Scripted replacement and
  inventory fallback therefore cannot silently draw. Existing armed pickup
  admission declines selection while holstered, but the real grant/refill,
  retirement, notice and audio remain. Full Strip already clears the bit and
  explicitly unarms; a later real pickup follows its ordinary unarmed policy.
- FP weapon and Undercover attachment renderers hide the view, HUD omits armed
  ammunition/reticle, scope/scanner retire, and the holstered shield neither
  intercepts nor bashes. Durability remains owned. Fine Aim remains a look
  modifier and does not draw the weapon.
- Previously released rockets, grenades, canisters, Fusion flights and Remote
  charges remain with their existing world owners. Their ticking/lifetime and
  ammunition are untouched. Conventional cooldown ages while holstered;
  special controllers retain their ordinary no-input cooldown processing.
- Mounting consumes Holster clicks and preserves the carried posture without
  changing mounted weapons. Dismount returns to that posture. Death keeps its
  existing life/recovery owner; in-place restart copies the level-entry view,
  and section restart/new-game construction remains fresh.

## Transition and save continuity

In-memory section handoff retains one exported posture word and one pending
word alongside the existing validated player export/import. Before teardown,
copying that word requires exact selected weapon, catalog, vitals and inventory
matching against the existing export. No inventory copy, pointer or registry
handle is added. Explicit fresh-player/reset and disk RFCP import clear this
lane; ordinary import first assigns its accepted carry then lets a present
RFWM record take precedence. Generic selection never erases that assignment.

RFWM keeps32 bytes and its existing checksum, catalog, RNG and shield fields:

- flags bit3 (value8): RF_WEAPON_MODES_PLAYER_HOLSTERED
- Encoder emits version3 only when this flag is set; ready players still emit
  their existing version1 or2, byte layout unchanged.
- Versions1/2 keep their strict3/7 flag masks and decode ready. Version3 admits
  bits0..3, requires the Holster flag, and retains the same finite positive
  shield/zero-reserved rules. Other versions, unknown flags, bad checksums,
  wrong lengths and impossible posture/ownership pairings fail closed.
- RFWM3 is readable by this successor. Older executables reject it rather than
  silently drawing. Old RFWM saves remain readable. There is no row size,
  storage budget, RFPL or top-level world-format expansion.
- Legacy RFCP profiles with no RFWM reject live holstered capture. Loading
  them clears posture through the existing fresh import lane. No lossy save
  or invented weapon deletion is used to bypass that limit.
- RFWM capture requires the retained posture be settled. Candidate preparation
  requires a selected owned weapon. Existing resource/mode/shield checks stay.
  World prepublication additionally rejects holstered RFAP pending reload,
  ignition, throw/burst or FP2/active-episode work; Remote preflight rejects
  pending throws/delayed selection. Independent flights/cooldowns remain valid.
- RFWM publication only assigns bit/mirror/input edges; it runs no action,
  inventory grant, animation, firing or new fallible service. Existing restore
  transaction order and rollback/error policy are unchanged. Holstered shield
  selected state is false, and ordinary scope reconciliation sees the flag.

## Proportional parent validation

Source review only. At the scheduled stock64 batch, compile the composed
successor and, if an ordinary permitted controller observation is available,
compare ready/holstered/drawn weapon and shield presentation, held inputs,
cycle, scope and an already available special-mode selection. Busy operations
should decline a click without losing ammo or deferred actions. Pause/hotplug
and a simultaneous save/load chord should require release. Persisted tests
remain subject to the parent's protected-save authorization and existing
transaction/resource gates. No new campaign route or per-slice fixture is
required or supplied. Retail animation, feel and AI refinements remain tester
follow-up, separate from the code implementation claim.
