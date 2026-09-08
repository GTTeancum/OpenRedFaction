# Model-instance materials

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
