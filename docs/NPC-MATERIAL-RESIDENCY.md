# Shared NPC material residency

`rf_entity_materials_open` owns flat material instances, appearance offsets and
one texture table across authored appearances. The indices in each material
refer to this shared table. Primary substitutions follow appearance metadata;
secondary maps retain the disk names. First case-insensitive name occurrence
wins, with archives searched in caller order. This is port resource ownership,
not a claim to reconstruct the original allocation or runtime skin switching.

The loader reads the original 84-byte SUBM records and uses the recovered
`rf_model_material_from_disk` conversion (RF.exe 53ae5f). All retained state
survives closure of model, appearance and archive inputs. Peak budget includes
temporary raw records/name pointers/handle maps as well as retained images and
material arrays. Allocator overhead and fixed decoder stack are excluded.
Static base-mip images only; animation and additional mip residency remain open.

## Verification

`python tools/verify_npc_materials.py` checks the first three opening levels
against `artifacts/npc-texture-residency.json` and independently decoded pixels.
It verifies every appearance offset, primary and secondary handle, all 200
material-record bytes after remapping, scalar arrays and image hashes. The C
probe also checks exact peak budget, one byte short, missing-image cleanup,
repeated close and reading retained data after closing inputs.

| Level | Appearances | Materials | Images | Resident bytes | Peak bytes |
|---|---:|---:|---:|---:|---:|
| L1S1 | 12 | 143 | 51 | 3,853,776 | 3,868,076 |
| L1S2 | 9 | 108 | 44 | 2,649,276 | 2,660,076 |
| L1S3 | 10 | 101 | 44 | 2,709,068 | 2,719,168 |

These are standalone PC probe measurements. The L1S1 campaign/native check below also includes retained appearance metadata.
The loader is connected to campaign ownership; NPC rendering remains open.

## Campaign / native verification

Campaign startup retains appearances and shared materials with a 4 MiB material
load budget, then releases both in campaign teardown. `rf_scene_npc_materials`
records appearance/material/image counts, resident/peak bytes including the
retained appearance maps, binding hash, pixel bytes and logical pixel hash.
The pixel walk uses `rf_image_pixel`, so swizzled Xbox storage is compared with
PC row-major storage in identical logical order. Binding hashes include actor
appearance indices, material offsets, all material record bytes and owned arrays.

`artifacts/xemu/replay-20260911-090328/report.json` passes the 180-frame L1S1
door/audio replay on 64 MiB XEMU. All eight telemetry words match PC:
`12, 143, 51, 3860424, 3874724, 60337271, 3818496, 4177010851`.
The existing NPC startup/geometry and door/audio checks also pass. These totals
cover NPC appearance/material ownership, not the entire game memory footprint.
No new rendered NPCs are claimed by this residency verification.

## Shared model emission

`rf_preview_model_emit` now supplies the clipping, triangle emission and preview
vertex conversion previously embedded in diagnostic animation. It takes an
already processed resident batch and caller-owned scratch, performs no allocation
or archive reads, and retains the model-local material index for appearance
mapping. The existing animation renderer uses the same helper.

`artifacts/npc-render-refactor/report.json` compares a saved pre-change PC
executable with this path over the 180-frame door replay. Every telemetry line
and the final framebuffer bytes match. The existing placed-stream test also
passes translated/rotated equivalence, moving placement and full culling.
This preserves the current diagnostic screen/depth/color policy; original
lighting and render-state parity remain separate work. Campaign integration
still needs shared scratch, actor iteration, LOD selection, appearance mapping
and correct ownership of the renderer's combined image table.

Native follow-up `artifacts/xemu/replay-20260911-090838/report.json` passes
the same 180-frame door/audio replay on stock 64 MiB XEMU. Both builds and all
nine CTest checks pass after extraction. No new visual is introduced.
