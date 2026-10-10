# Shared continuous-charge persistence

Source-written against f595620ae6a9f9ada44596e9961d3cafd3519e3d, outside the active tree. Requires the matching shared-player runtime/resource patch. No compilation, syntax check, test, fixture, gameplay run, image, or active-tree edit was performed. Parent integration is after the 18:00 batch and requires joint independent source review.

## Format and exact meaning

RFAP version5 adds one conditional kind11 row to the existing112-byte row format. Header words at +4/+8/+12 remain reserved zero. Kind11 identifies the singleton local-player role, never an invented UID or saved registry handle. Its32-byte payload is:

| Offset | Value |
|---|---|
|16|signed current state -1..2|
|20|signed pending state -1..2|
|24|exact binary32 blend duration +0 or0.2|
|28|exact binary32 elapsed|
|32|signed f90 remaining: -1 unset,0 expired,1..25 future milliseconds|
|36|signed episode catalog weapon ID, -1 absent|
|40|episode mode,0 or0x20 structurally; this slice admits only primary Flame0|
|44|signed retained FP profile: -1 none,0..17 existing handheld slots,18 Jeep|
|48..111|reserved zero|

The exact constructor tuple is current/pending=-1, duration/elapsed=+0, f90=-1, profile=-1, episode=-1/mode0. Only that tuple may omit kind11; an explicit constructor row is noncanonical. Existing independent projectile/controller rows may therefore keep their earlier RFAP version. A state0/1-only blend is still material and is saved. No has2, armed, selected-model or future-deadline save veto is introduced.

NaN, infinity, negative or oversized elapsed and negative zero are rejected. Active duration is exactly0.2f (0x3e4ccccd), pending is0..2 and elapsed is less than duration. Inactive duration requires pending=-1. Completed elapsed is retained exactly through the fixed60Hz completion overshoot, bounded by0.2f + scene_step_seconds. Current=-1/pending=-1 requires elapsed+0. A source state2 may survive selection of a table without loop2 or explicit unarmed; no selected-table state-count inference is made.

The last FP profile is portable resource provenance, not an ammo weapon ID. It is independently checked against actual validated mesh/motion metadata, even when the profile is no longer owned or selected. This matters for Remote Charge Detonator and Machine Pistol Special, whose view and inventory mappings differ. All19 profiles are bounded/admitted by the runtime resource adapter; Jeep needs no bit outside the existing18-slot view-demand mask. A retained profile does not demand a full rendered weapon allocation. Existing Flame episode rows still demand their ordinary Flame resources without granting inventory.

## Ownership joins and admission

An active saved episode is restricted to the actual catalog Flamethrower ID and primary mode0. It must agree exactly with kind9 ignition1 or2, including ignition1 before the0.10s start delay, and with the candidate player's selected, owned Flame weapon. Ignition0 cannot carry a receipt. Neither pair2 nor positive loaded ammo is inferred or required. Non-Flame active episodes remain explicitly unsupported; existing generic held/fire/cooldown and accepted-contact guards are not relaxed.

The new owner is admitted privately using the candidate RFPL player and source-qualified profile metadata. It snapshots the current tuple, full player handle, and runtime owner epoch, then revalidates that exact endpoint before publication. No raw handle, epoch, frame reservation, callback reservation, borrowed asset pointer, or call-local debit weapon is serialized. The runtime publisher rebinds the actual registered candidate player and creates a fresh epoch without replaying fire, debit or ignition audio.

The shared f90 uses original wrap-aware timer helpers. Expired absolute times canonicalize to relative0, while unset remains-1. Positive remaining time is at most25ms: Riot uses25ms, Flame uses20ms, and a Riot deadline legitimately survives selection of Flame. Restore rebases once against the destination gameplay clock. There is no fractional arithmetic reconstruction, catch-up, or ammunition transfer.

## Legacy compatibility

RFAP1..4 and absence predate this owner. A legacy kind9 with nonzero gas debt, ignition, delay or active-stream flag is unconvertible and returns explicit unsupported before any scene replacement. Ignition1 with zero debt is still ambiguous. The rejected debt is never discarded or rounded. Independently encoded reload, canister and damage-cadence state remains admissible when those continuous fields are zero.

For quiescent legacy data, the absent pair/f90/profile is initialized to the new-player constructor tuple as an explicit port compatibility policy. This is not a claim to recover original historical pair state or parity from loaded ammo. New RFAP5 kind9 retains its former gas word as reserved zero; it is not repurposed.

Cheap wire/legacy preflight runs after source validation in QuickLoad, before level-transition acceptance and teardown of the old scene. It also runs at the beginning of fresh world load, before terrain/world mutation. It uses the already opened immutable catalog directly, because boot resource demand precedes cached campaign_flame_id initialization. Full resource/player/world admission remains in the fresh loader. Save-only live guards are never imported into candidate preparation.

## Publication and scope

The sole full world loader runs on fresh frame0. Existing QuickLoad accepts replacement only after its read/preflight/storage close succeeds; failed preflight/close retains the old running scene. After full candidate validation, a narrow shared-owner suspension spans all pre-close selection, mode, possession, reset and vehicle-publication callbacks. These callbacks cannot change the tuple or its epoch while the candidate is private. The original failure status is preserved.

Flame's controller/canister assignment is moved from early RFAP assignment to the final-success finish. It publishes immediately before the shared episode/pair/f90, while suppression is still active; audio reconstruction follows. On failed close, neither of these coupled new owners publishes. An absent component successfully clears Flame through its ordinary reset and publishes constructor charge state. No whole-world rollback is claimed: existing vehicle publication and other world assignments retain their preexisting fresh-load failure behavior.

Standalone RFCP contains no lane for this owner. Live RFCP export and in-session restore reject nonconstructor or busy state before mutation; genuinely constructor-empty state remains admissible. Candidate-only scope checks are not changed. RFPL/RFWM formats and unrelated action admission are unchanged.

Genuine accepted scene/player replacement keeps the existing integer inventory carry and constructs a fresh shared pair/f90/profile/receipt at the actual new owner, as an explicit port policy. Same-level QuickLoad restores the exact lane and never substitutes this section policy. Ordinary selection and full FP-only reset retain f90. Cross-section timing continuity is deferred; no RFCH carry lane was invented.

## Bounds and memory

The fixed row size remains112B. RFAP theoretical row capacity increases153 to154 (8 enemy grenades +8 enemy rockets +50 player rockets +8 grenades +4 Fusion +8 Flame canisters +64 burning +4 singleton controllers), so its theoretical maximum grows17,152 to17,264B, exactly112B. A formerly absent RFAP additionally needs its16B header and the RFWC prefix extension320 to332B:140B total worst new-envelope growth. An existing RFAP adds only112B.

The codec adds no global state array and no heap allocation. Its local candidate contains two32B tuples, one8B runtime epoch and three4B words:84B field storage, with compiler tail alignment charged by actual sizeof (typically84B on32-bit,88B on64-bit). That exact sizeof is deducted from the existing2MiB loader staging budget with a checked subtraction. Runtime metadata costs belong to the companion resource patch. The RFAP stage allocation does not grow.

Stock RF_CHECKPOINT_FILE_MAX remains110,524B. The existing expanded-profile override262,144B is unchanged and is not the stock budget. Valid large scenes may still fail RF_RANGE when the total envelope no longer fits; neither transport capacity nor staging capacity was enlarged. Source/catalog hashes are unchanged.

## Source and tool audit

Original source evidence and independent ownership review are in the supplied riot-battery source contract and shared-owner design review. Principal owners are4aa230/4aa560 (pair),4aaf77 (periodic debit),4a5910 (selected host),4ae0d0/4ae122 (model binding),4adba0 (FP reset), and Flame41a870/41a8f0 (ignition receipt). The original Flame SP table gives clip100, drain2.0s and delay0.10s.

All executable RFAP version checks are in the diagnostic adapter and now accept conditional5. The unrelated GeoMod RFAP digest tag is unchanged. Existing xemu_ai_projectile_save.py, xemu_fusion_flight_save.py, xemu_flame_save.py and xemu_burning_save.py are old exact-format probes, not gameplay decoders. Their fixed version/row assertions may require a parent-authorized focused update for new captures; they were not changed or run in this source-only slice. No new fixture is supplied.
