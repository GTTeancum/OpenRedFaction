# Crater codec bounds can change during CSG — 2026-09-15

`tools/verify_geomod_crater_bounds_lifetime.py` executes original4de27c..4de2a8 and complete4cf9a0 with actual extrema/padding/radius arithmetic. The vertex container and unrelated room refresh4ccf50 are supplied. Fixture solid is also installed as6460e8. Its bounds change from[-1,1] to vertex extrema(-2,-3,-4)/(5,6,7), padded by float0.0001. A seeded packed32-byte history record remains unchanged. JSON retains resulting bounds. Same RF.exe hash as preceding reports.

Static call chain:466c50 passes6460e8 as first4de530 argument;4de5a7 stores that argument into globalc968b4; the staged CSG worker passesc968b4 into4dd8c0. That function retains the solid argument in EDI. If current roomc9f630 is nonzero,4de2a1 passes EDI to4cf9a0 at4de2a3. The latter writes solid+48..5c from current vertex extrema and applies the padding.

Therefore the original geometry bounds used by4b5820/4b5900 are **not inherently immutable**. The executed rebuild does not re-encode existing records. A linear direct-call audit finds4b5820 calls at410fab,42b58d,42b69e,42b6b8,42e65b,4672ac,48ad94,4c0e63; none is in this CSG rebuild path. The crater admission write is4672ac. This is not a proof against every indirect or external writer, but no record-reencoding loop was identified.

Recommendation: retaining stable original bounds for the port's crater-history codec is a deliberate stability policy, not a proved match to mutable original bounds. It prevents a changing collision overlay from reinterpreting already-encoded centers. Ordinary enclosed cuts may leave original extrema effectively unchanged, but the code does not enforce that invariant for all geometry. This probe does not execute an entire original level blast or prove the frequency of the original failure in authored levels.

Primary review: retained and reran this executable probe successfully. The live port stores requested and adjusted centers separately and deliberately retains initial codec bounds; this evidence does not claim identical original mutable-bound behavior.
