# Xbox startup Load Game

Source implementation, October12,2026, based on3fa74fdc. Uncompiled and unrun;
only the parent-owned hourly stock64 batch may establish compilation/runtime.
No build, test, emulator, saved-HDD access, host disk-image operation, image,
repository mutation or upload was performed by these implementation lanes.
The protected saved-consumer backup/durability/admission requirements remain.

## Tester-facing behavior

The startup menu now has New Game, Options and Load Game. Existing New Game and
Options indices0/1 are preserved; Load Game is2. Load Game opens a source page:
Manual Save, Autosave and Back. D-pad up/down chooses; a fresh A on a source
starts that explicit load. B, Start or Back returns to the startup menu.

Opening/drawing/navigating the page never scans storage. The two sources have
independent generation counters; this menu does not claim to choose the newest
across them, auto-continue, or silently fall back from one source to the other.
Each selected source retains its existing two-slot newest-valid selection.

After A, a loading/validation screen appears before the synchronous source
preflight. If that screen cannot draw, storage work never begins. The outcomes
are deliberately separate from the terminal session error:

- Missing manual/autosave: explicit source-specific notice, staying on the page.
- Incompatible/damaged save: notice and actual RF status, staying on the page.
- Read/source-data error: notice and actual RF status, staying on the page.
- Failed native close or session retirement: cleanup-failed/reboot-required
  screen; New Game, retry and source changes remain unavailable.
- READY: leave the frontend for the existing cold scene construction and full
  saved-world restore. READY means source preflight passed, not restore success.

Moving to another row clears the prior source notice; Back leaves it behind.
After recoverable rejection, another load needs a new A edge. Existing controller
hotplug/rearm policy applies. Merely reconnecting or holding A never selects a
source. Options and difficulty behavior from the first batch remain unchanged.

## Storage, compatibility and publication

The new scene_frontend_load.inc helper is copied unchanged from the separately
reviewed continuation lane. It requires idle ownership, performs the existing
session reset before reading, and opens only the explicitly selected ordinary
manual or autosave base with writable0. It uses the existing payload cap,
prior-envelope validator, world decoder, level source identity, catalogue hash,
full snapshot source validation and continuous-charge preflight. It changes no
save version, writer, slot transport, generation policy or compatibility gate.

Temporary payload/catalogue/level owners are released before native close.
Successful close precedes publishing the saved level, live-load flag and source
selector. A failed close retains the exact static native storage owner and
latches the existing retirement failure, so it cannot become a recoverable
missing/corrupt notice or orphan the mount.

The catalogue is separately zero-initialized and loaded through the same128KiB
rf_weapon_supply_load path and whole-catalogue FNV used by campaign construction.
Source identity still binds the complete RFL entry and tables.vpp plus catalogue
hash; it does not newly prove all model/motion/texture/audio assets compatible.

## Direct cold routing

player_frontend returns a caller-owned64-byte admitted level name. The tester
loop passes it to player_campaign_run after resetting only platform counters.
There is no session reset or New Game difficulty setter after READY. The cold
route checks tester mode, bounded termination and an armed ordinary load, then
calls rf_level_campaign_open on that exact level instead of opening L1S1 first.
It uses the same archive owner, normal section construction and final teardown.
It creates no fake scene_stream, inventory handoff or level-transition request.

The normal boot resource-demand and frame-zero world loader reread the explicit
source and perform their existing full admission/transaction. Inherited timing
allows a constructor-world frame0 step/presentation before restore, which
publishes before the next frame1 input; this is not a before-any-tick claim.
RFCH history
publication owns saved difficulty; the process-local New Game seed does not
replace it. No slot pinning was added. A resource/world-restore failure after
READY keeps the established terminal session-failure policy, never silently
starts a fresh game or offers retry over partially owned construction.

Diagnostic/non-tester launches pass NULL and retain their previous direct
archive/level path. The optical-fixture and replay launch policy is unchanged.

## Receipts and verification boundary

rf_xbox_frontend_menu still records page/selection/NewGame seed. Page0 is root,
1 difficulty,2 Options and3 Load Game. The load page rows are manual0,auto1,back2.
rf_xbox_frontend_load records explicit attempt count, selected source, helper
outcome and RF status. No attempt is inferred when count0. Frontend trace phase9
means the synchronous source preflight is active; READY is not a restored-world
receipt. Existing starts now includes either an accepted New Game or READY load.

Source-only review covers call signatures, explicit selection/notice flow,
close-before-arm, exact helper integration, direct archive routing, the absence
of post-READY reset/seed calls, and saved-history difficulty publication. No
compile or gameplay result is claimed. The parent should compile this together
with the first options batch at the scheduled hour. Saved-source functional
checks require the independently approved/protected disk conditions; they are
not authorized by adding this UI. Source-page navigation alone performs no
saved storage access and may share the bounded options check if useful.
