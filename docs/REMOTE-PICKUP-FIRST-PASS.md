# Remote charges in ordinary levels

Remote Charge and Remote Charge Detonator now form an independent sparse resource pair. Authored pickups, Give_Item_To_Player events and imported ownership request both views while retaining one weapon ownership ID and one reserve. The deployed charge model, explosive definition and impact resources load before use. Planted charges update while another weapon is selected; slots8/9 bypass ordinary magazine/hitscan handling. Scene startup clears old charges, while frame-zero checkpoint handling stays after player import to preserve existing restored-charge behavior.

The non-DEV PC fixture `tools/check_remote_pickup.py` retains original CTF06 spawn, geometry,506 props and pickups. A real Remote Charges grant supplies3; a normal cycle selects charge slot8, input120 releases a charge at141, and it attaches by the planted endpoint. Cycling to detonator slot9 and firing300 detonates it. Counts are exactly one start/launch/attachment/detonation and zero live charges afterward, with reserve2 and no status errors. PC planted/detonated images were inspected; the latter shows the detonator and explosion smoke. Audio was not auditioned.

The first load exceeded the shared20MiB image cap (21,908,036bytes). Ordinary nonvehicle player levels now use the same8MiB gameplay reservation as DEV, leaving a4MiB world-material budget from the12MiB allowance. The existing filtered fallback reduces this CTF06 world's textures to128px (1,536,976resident bytes). This affects other ordinary player levels when needed and trades texture sharpness for usable gameplay within stock64MiB. Neither the total image cap nor Xbox memory target increases; a future demand-sized partition can retain more sharpness where headroom permits.

Focused paired-demand tests and PC/NXDK builds pass. Native run `artifacts/xemu/render-20260922-174127` passes360 frames and all harness comparisons without DEV mode. Exact remote counters match PC: one start, launch, attachment and detonation; zero live charges/errors. The inspected framebuffer shows the detonator and explosion smoke. Stock64MiB retains3623 free pages (14.15MiB); original disc restoration passes. Full campaign persistence/terrain, broader moving-host behavior, world pickup approaches and presentation refinement remain open.

An image-free ordinary save/load continuation now preserves a planted charge on unchanged static geometry. The non-DEV CTF06 fixture grants charges, throws one at frame120, saves at260 with a276-byte remote section, quick-loads at280, switches to detonator and fires at340. `tools/check_ordinary_remote_reload.py` repeats the PC case; stock64MiB Xbox run `artifacts/xemu/render-20260926-153502/report.json` passes380 frames with matching save/load status, charge detonation, zero remaining live charges and exact PC/Xbox ammunition. The ordinary snapshot is17440 bytes, and the minimum sampled Xbox headroom is3088 pages (12.06MiB). That check covered static geometry only; see the staged-host restore update below for NPC/mover admission. Active GeoMod terrain and broader campaign saves remain open. No image was captured.

## Active nano-shield contact

Stock64MiB Xbox now consumes remote charges that contact Capek's active shield
without attaching, exploding or draining armor. With shield OFF, another charge
attaches and detonates through the normal input/service path. The isolated
110-frame check stages releases/aim and initial armor, then exercises actual
flight, host binding and detonation. It records two launches, one absorption,
one attachment, one detonation and no live charges afterward. See
[Nano-shield contacts](NANO-SHIELD-CONTACT-FIRST-PASS.md) for evidence and limits.
Moving-host save/reload is addressed by the staged-host restore below.

## Ordinary world restore of attached hosts

Ordinary world loads now resolve a bound charge against the prepared NPC or
mover pose that the transaction will publish. Previously the remote preflight
used the current live pose, then a blanket gate rejected all host-bound charges.
The new path retains durable UID-to-runtime-handle validation and transports the
saved local offset/orientation through the candidate host transform. All host
and charge admission remains before publication; NPCs and movers publish before
the prepared charges. The save format and legacy player-only restore path are
unchanged. Missing, retired or invalid hosts still reject the transaction.

The bounded Xbox check is `tools/xemu_remote_host_save.py`: stage a real flight
onto an authored NPC, write an ordinary world save, boot again and load, then
select the detonator and fire. It checks the saved host UID, restored attachment
position, absence of replacement launches and one detonation with no live charge.
Stock64MiB report
`artifacts/xemu/remote-host-save-20260930-110757/report.json` passes: the ordinary
world snapshot is30536 bytes, including one276-byte remote component; a fresh
boot restores one bound charge on UID8359 at the saved position, with no new
launch or attachment, then exactly one detonation and zero live charges. The
load retains4538 free pages (17.73MiB); original disc flags are restored. No PC
runtime, image capture or campaign traversal was used. The shared weapon fixture
also now leaves reserve-only grenade/remote magazine counts at zero so it can
produce valid player checkpoints.

Mover-bound transport is compiled but has not
received a separate live save/load check; dead-host transitions, fragment hosts,
and wider encounters remain outside this first pass.
