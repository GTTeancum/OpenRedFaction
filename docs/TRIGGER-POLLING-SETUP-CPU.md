# Trigger polling setup CPU

Source-written 2026-10-08 against d0a1edc for the parent-owned 19:00 Xbox
batch. No helper build, test, emulator run or new gameplay fixture. The
18:00 neutral L1S1 stock-64-MiB event phase was 2.740 ms; that includes more
than trigger contact and is the previous baseline, not this change's gain.

## Existing source and object evidence

The scene visits the player followed by every eligible NPC, in order, and
visits each trigger for each actor. Before contact, the existing path calls
`rf_trigger_contact_filter_authored`, writes a temporary four-field filter,
then calls `rf_runtime_trigger_contact_cached`. That wrapper resolves the
handle, copies the four-byte object kind, writes an eight-field gate and
calls the internal poll. The poll calls eligibility and then dwell.

The existing 18:00 `event.obj` confirms separate calls for registry lookup,
the four-byte `memcpy`, internal contact polling, eligibility, the cooldown
timer query and dwell. It also retains out-of-line float classifiers for
the memory-backed sphere/box input validation. This is inspection of the
previously built object, not post-change compiler or runtime verification.

## Bounded implementation

- `rf_runtime_trigger_contact_authored_cached` combines authored filtering
  and runtime contact without constructing the temporary filter or gate.
  It still resolves the full generation-qualified handle on every visit and
  checks the resulting object kind. A fixed-size builtin representation copy
  retains alias safety while removing the freestanding four-byte libc call.
- The common actor-filter predicates are shared with the existing public
  eligibility API. All low-byte tests, signed count/limit comparisons, flag
  gates, attachment checks and cooldown queries remain live. For filter 2,
  the same first matching resolved link word supplies membership. Missing
  link storage still fails before the runtime lookup. A valid link search
  is unnecessary only when the current common state already rejects contact.
- Rejected contacts still call the unchanged dwell routine immediately. Its
  finite-duration guard, positive-delay clearing, timer semantics and error
  behavior remain. Accepted contacts use the existing exact geometry cache
  and sphere/box routines, followed by the same dwell routine. There is no
  cached eligibility, actor state, deadline or activation result.
- The uncommon player-use reach/ray branch keeps the previous separate
  filter preparation and runtime API. This preserves its earlier filter
  errors relative to reach/occlusion checks as well as interaction behavior.
  The scene also preserves malformed filter-link rejection before polling
  telemetry, as in the previous separate preparation call.
- Sphere and box memory-input validation use `rf_finite_float`: seven input
  checks for sphere contact and 24 for box contact. These accept/reject the
  same binary32 values. Computed-difference/corner checks, float/double
  expressions, cast/store boundaries, collision helpers and tolerances are
  unchanged.

No owner layout, save format, allocation, cache capacity, trigger ordering,
callback ordering or simulation cadence changes. Trigger counters, airlock
checks, dispatch and activation bookkeeping remain at the same scene points.
The scene edit is confined to `campaign_actor_trigger_contacts`; NPC movement,
pose, eye and lifetime code are untouched.

## Parent validation still required

The scheduled batch must compile the shared core and Xbox scene seam. Inspect
the resulting fused function for removal of filter/gate packing and redundant
call boundaries, and contact routines for the intended input-classifier
reduction. Existing trigger eligibility/contact/dwell and authored-contact
probes cover the shared predicates and geometry if included in that batch;
the new fused entry point itself has not been executed by this helper.

Compare the unchanged neutral-spawn L1S1 profile and actual presented FPS.
The phase result cannot isolate this patch from other integrated work, and
neither source inspection nor the number of avoided calls predicts its gain.
No generated artifacts or cleanup work were produced by this helper.
