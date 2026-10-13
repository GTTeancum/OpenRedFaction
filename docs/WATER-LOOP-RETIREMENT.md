# Water-loop retirement ownership

Source-written correction for the existing surface/underwater loop consumers,
based on published commit 6d1bf09b54625cade292445139cd9da0779634d4.
Parent confirmed that the relevant stop path remains present in its newer
source. No builds, syntax compilation, tests, runtime, images, haptics or
saved-HDD access were performed; parent owns integration and hourly stock64
compilation. Credits and all completed feature branches are untouched.

## Concrete defect and source evidence

The previous `scene_player_water_audio_stop()` first cleared the software
mixer voice, then called `campaign_audio_events.stop` (void), then always
discarded its public voice ID. The native callback in `audio.c` calls
`rf_xbox_audio_release_voice()`: a failed stop or 1000 ms stopped-state wait
returns RF_IO and leaves the created native voice and borrowed PCM alive.
The callback records that error but cannot return it to the water owner.
The old caller therefore forgot an unresolved borrower and allowed a new
water voice to replace it.

The existing declaration in `src/platform/xbox/audio.h` explicitly defines:
RF_OK means released; RF_NOT_FOUND means the complete handle owns no native
slot; RF_IO retains ownership and forbids freeing its PCM. Native lookup
matches the entire 32-bit handle, not a slot index. This correction uses that
existing result API directly on Xbox; no callback signature is changed.

Original water selection/gain evidence remains 429100/42939d and 431d1c, as
documented by the existing consumer. This change repairs port ownership; it
does not reinterpret original water eligibility, sound selection or gains.

## Ownership rules now implemented

- Capture the full handle immediately after successful start/bind, under the
  existing serialized mixer contract. Public IDs remain diagnostic only.
- Stop marks the loop pending and attempts native retirement first. On any
  result other than RF_OK/RF_NOT_FOUND, retain its full handle, pending bit,
  public ID, software loop slot, spatial record and sample identity.
- A retained software loop remains active/looping. `rf_audio_voice_start()`
  protects looping slots through `rf_audio_select_ordinary()`, and idle-bank
  eviction refuses active software borrowers. This pins the original slot
  while native ownership is unresolved, including across ordinary generation
  counter wrap. No unrelated sound can inherit that pinned slot.
- After confirmed native absence, software stop and spatial cleanup qualify
  the retained full generation before clearing anything. No retirement path
  re-resolves an old public ID or stops whatever currently occupies a slot.
- Retry pending retirements before normal water eligibility on the next
  ordinary water-audio tick, at most once per loop in that retry pass. Each
  attempt uses the existing bounded native API. Explicit close/reset can
  also retry. A failed retry returns before new water starts or gain changes.
- Replacement cannot overwrite pending ownership. Owner changes wait for
  close success. An unexpected immediate handle-capture failure is preserved
  as an unresolved error, rather than guessed from a later public-ID mapping.
- Close attempts both loops and returns the first failure; it clears shared
  owner/frame bookkeeping only after both retire. Reset clears sample IDs and
  diagnostics only after successful close.

## Caller integration and memory

The two internal static close/reset helpers now return status. No public API,
global audio callback contract, renderer, save codec or asset changes are made.
Exactly **12 persistent transient-owner bytes** are added: two uint32 handles
and one uint32 pending mask. The owner grows from 28 to 40 bytes, enforced by a
static assertion. There are no new allocations, PCM copies, images, handles to
archives, diagnostic arrays or queues; helper calls use scalar stack locals.

`scene_miner()` attempts water reset before resetting other scene owners.
`campaign_audio_open()` checks close before clearing the mixer/ID map or bank
setup. `campaign_close_movers()` checks water retirement at entry, before
destructors, callback reset, mixer reset or freeing bank PCM. Failure follows
the existing sticky `scene_session_retire_status` policy and retains the bank.
Full-session idle/reset admission also rejects active or pending water owners.

The relocation hook may still ignore close's return value: close itself keeps
all unresolved water ownership, and the next ordinary tick retries it. It
does not reject a completed world-load publication or add fallible work to a
save transaction. No device IDs, pending bits or retry state enter a save.

Parent integration must preserve its newer teardown/storage guards and merge
the water gate before any audio/mixer/bank destruction. Do not weaken or clear
the existing sticky retirement status. A later successful native shutdown may
make the full handle absent, but this patch does not declare the wider failed
session safe to reuse or introduce automatic recovery from other failures.

## Review scope

Source review covered both mixer-reset sites, water start/stop/gain/tick,
relocation, section entry, bank teardown and session-idle admission. Existing
native stop results and generation matching establish the ownership contract;
no runtime stop-timeout or audibility result is claimed. Non-Xbox staging keeps
its existing void callback behavior; Xbox alone uses the native result API.
