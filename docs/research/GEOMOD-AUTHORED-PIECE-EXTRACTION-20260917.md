# Installed authored-post extraction probe

The standalone rf_check_authored_piece_extraction target reads ctf06.rfl from
Installed_Game/levelsm.vpp using the production authored-post loader, and uses
build/data/geomod-template.bin. It requires local game assets and is deliberately
not an unconditional CTest case. No original process or desktop interaction.

Run from the repository:

    build/pc/Release/rf_check_authored_piece_extraction.exe Installed_Game/levelsm.vpp build/data/geomod-template.bin

The real UID94 source bounds are(-5.25,-1.5,2.25)..(-4.75,2,2.75). Six independent
cases use blast radii1.05/1.5 at25%,50%,75% of post height, with an identity
orientation. Each successful first cut is followed by a second blast shifted
up0.15, then a checkpoint decode into a separate terrain owner.

All six initial and six follow-up cuts succeed under a1152KiB core budget.
Four initial cases emit one detached piece; two emit none. The emitted meshes
have54-80 corners and13-17 faces. Every emitted mesh builds an owned physics
batch. Resident batch bytes are130832-131528; each batch construction peaks at
296472 PC bytes. Initial core peak is1105564 PC bytes, concurrent with callback
work. The probe destroys batches after fingerprinting them, so reported batch
resident totals are not retained scene allocations.

All six reloads match retained terrain vertices/faces byte-for-byte. FNV digests
of detached mesh corners, faces, filters, body state and collision spheres match,
as do extraction counts and RNG. Digests are regression evidence, not a formal
proof of equality. Output: artifacts/geomod-postedit-re/authored-piece-extraction.txt.

The body's density2.5, elasticity0.5 and friction0.25 remain explicit probe
parameters; this does not validate the scene's material lookup. Generated
subdivision material0 is likewise a probe value, not a renderer-slot contract.
There is no live scene activation, body motion, draw or native acceptance here.
The next integration needs staged ownership across clone/decode/mutate/publish,
historical emission deduplication, material mapping, rendering and collision.
