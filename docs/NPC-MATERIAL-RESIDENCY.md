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

These are PC probe measurements; native runtime accounting remains to verify.
The loader is not yet connected to campaign ownership or NPC rendering.
