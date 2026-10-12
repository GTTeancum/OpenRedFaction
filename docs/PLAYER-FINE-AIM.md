# Player Fine Aim first pass

Source-written 2026-10-12 against frozen b304abcd803d65c7264330194fd52de5e630bead.
This successor is staged outside the active repository. The parent owns
integration after the frozen 16:00 batch, compilation and Xbox runtime checks.

## Grounded missing control

The installed levels1.vpp train02_text.tbl entry 34 asks for the Fine Aim
($FINE_AIM$) button for more precise aiming and says it is helpful with scoped
weapons. Installed tables.vpp strings.tbl entry 154 names Fine Aim Mode.
The current Xbox frontend had no binding or response for that action and the
HUD explicitly rendered UNBOUND. Existing supported scoped zoom is separately
owned by alternate fire, so relabeling LT would not fill this gap.

These original text records establish the purpose, not a numerical response,
free-aim region, aim assist, toggle or original-console button. Read-only
inspection of RF.exe did not establish the relevant retail scalar/state path.
No retail implementation equivalence is claimed. In particular, unrelated
player+0xfb0 display/look branches are not used as Fine Aim evidence.

## Available native input

The actual parent toolchain's bundled SDL2 include/SDL_gamecontroller.h declares
SDL_CONTROLLER_BUTTON_RIGHTSTICK. Its src/joystick/xbox/SDL_xboxjoystick.c
maps XINPUT_GAMEPAD_RIGHT_THUMB (0x0080) to raw button 9; the original-Xbox
mapping in src/joystick/SDL_gamecontroller.c maps rightstick:b9. The project's
input.c did not read either stick-click button. Right-stick click therefore
has a source-confirmed route without stealing an existing action or modifier.
This is SDK/source evidence, not a physical controller check.

## Bounded implementation

- Hold right-stick click for 50% of the otherwise effective normalized look
  command on both axes. Release immediately restores normal response. This
  multiplier and hold policy are chosen Xbox first-playable defaults.
- Apply after radial deadzone, sensitivity, invert Y and axis saturation.
  At sensitivity 200%, a saturated axis changes from +/-1 to +/-0.5, rather
  than saturating again and losing the precision effect.
- Existing on-foot scope scaling composes later in actor_listener_pose.
  LT/alt_fire, scope edge state, projection, reticles, damage, spread, aiming
  targets and other button values are untouched. Mounted look consumers can
  use the same precision modifier; movement/steering command fields are not
  rewritten.
- INPUT_FINE_AIM joins the existing action mask before attach/pause suppression.
  A controller attached while holding the click requires release; pause-held
  actions similarly require release on resume. Missing controller, save/load
  chord and pause returns still publish a zero input. No extra SDL event pump.
- No retained fine-aim toggle, allocation, shared input field, save field,
  replay change, build-list entry or gameplay scene callback is introduced.
  Existing scene suppression for death/cutscene/defuse remains authoritative.
- The Xbox authored token becomes HOLD RIGHT-STICK CLICK. PC stays UNBOUND;
  no PC input work or runtime claim is made.

## Integration and proportional validation

The runtime change is limited to src/platform/xbox/input.c. The only shared
code intersection with the pending Message History slice is the token table
in scene_hud_control_text.inc. The supplied after-message-history patch keeps
its START / MESSAGE HISTORY route; do not replace that file with the baseline
variant after Message History is integrated. Documentation and the top TODO
entry also have an explicit composed successor.

No build, test, fixture, game execution, emulator, campaign traversal, image,
HDD access, upload, cleanup or active-repository edit was performed. Static
source review only. The parent can include a bounded ordinary controller
comparison in a later scheduled stock64 batch: equal look input with/released
click at 100% and 200%; unchanged movement/actions; LT remains independent;
held-click attachment/pause release suppression. This is a suggested check,
not a newly authored fixture or claimed result. Tester feel remains open.
