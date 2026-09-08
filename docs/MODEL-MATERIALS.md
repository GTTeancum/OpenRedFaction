# Model-instance materials

`rf_model_material_initialize` reconstructs the complete 0x54a7c0 constructor
over a fixed-width 200-byte record. It sets the primary identifier and both
texture handles (+10/+44) to -1, RGBA bytes at +9 to ff, +78 to 15, +b4 to -1,
and the original zero-valued fields. It preserves all bytes the original
leaves untouched, including most names and unknown fields. Raw pointer-valued
slots are not native C pointers; material ownership is not established by
initializing this record. Never use the constructor as a live-material reset
or destructor.

The original color constructor 0x50cc00 and texture constructors 0x54a610
are effectively no-ops; vector construction still executes those callees.
`tools/verify_model_material_init.py` compares all 200 bytes across 1,000
zero-filled, ff-filled, patterned and random initial states. It executes
the complete original constructor, including SEH and vector/color helpers,
and checks return pointer, surrounding canaries and restored SEH head.
Win32 Release, four CTest cases and NXDK build pass. Instance allocation,
copying and texture binding remain open; no Xbox runtime integration or
new visual result is claimed for initialization alone.

The weapon presentation mode path reaches 0x48ab90, which finds a named
material override and calls 0x48ac00. That function obtains mutable instance
materials through 0x503650, then requests textures with 0x50f6a0(name,-1,1)
and stores each result at material +10 with a 200-byte stride. The loop is
limited by both override count and material count. Texture loading, mutable
material ownership and the binding loop are not reconstructed yet.

0x503650 allocates an instance array through 0x503730 when wrapper +50 is null,
sets byte +54 to one, and returns the array and the 0x503690 count. The new
`rf_model_material_count` reconstructs that count query over resolved views:

- Kind 1 returns zero if the static model's +50 is greater than one; otherwise
  it returns the material count at the referenced mesh +84.
- Kind 2 sums every submesh's +84 count through 0x4a76f0; wrapper +8 leads to
  the animated object, with submesh count +19bc and 148-byte records at +19c0.
- Kind 3 returns the direct model count at +88.
- Other kinds return zero.

Negative submesh counts perform no iteration. Signed direct values and
32-bit wrapping sums match the original; this query is not allocation-size
validation. The future allocation layer must validate counts and byte budgets
before reserving stock Xbox memory. An insufficient supplied submesh view
returns RF_RANGE with output unchanged.

`tools/verify_model_material_count.py` compares 2,000 full original executions
of 0x503690 and its unchanged 0x4a76f0 callee, with one added bounds rejection.
It checks that the original model views remain unchanged. Win32 Release,
four CTest cases and NXDK build pass. This query is not yet integrated with
instance allocation or the Xbox diagnostic; no new visual result is claimed.
