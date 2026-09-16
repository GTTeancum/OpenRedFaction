# Installed authored source identity capture probe — 2026-09-16

`tools/check_installed_authored_identity.c` assembles `rf_geomod_authored_identity_input` from the actual installed ctf06 loader view and assets. Primary verification passed on installed data: two materials and19 references, with deterministic digest, missing/duplicate rejection and pixel sensitivity. No emulator or runtime scene changes.

Run from repository root with the existing matching Release core library containing `geomod_authored_identity.c`:

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cl /nologo /std:c11 /W4 /MD /D_CRT_SECURE_NO_WARNINGS /O2 /Iinclude tools\check_installed_authored_identity.c /Foartifacts\authored-post-live\installed-identity.obj /Feartifacts\authored-post-live\source-identity-probe.exe build\pc\Release\rf_core.lib /link /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'"
artifacts\authored-post-live\source-identity-probe.exe Installed_Game > artifacts\authored-post-live\installed-identity.log 2>&1
```

The successful terminal marker is `PASS installed_authored_identity`, preceded by `INSTALLED_SOURCE_SHA256`. Nonzero exit/missing terminal marker means capture remains unverified. All material/reference captures are printed to make ownership failures inspectable.

Capture details:

- Uses the original compiled section bytes retained by `rf_geometry_open`, raw editor section0x2000000 and original unremapped source/window/neighbor loader views. Source operation2 comes from the existing bounded loader's explicit UID94 flags0 requirement, matching original44d870; the trailing editor property is not mistaken for operation.
- Scans maps1/maps2/maps3/maps4/maps_en archives for each required compiled texture name. Exactly one asset member must own the name. Missing or ambiguous ownership prints `ASSET_OWNERSHIP` and fails. There is no missing checkerboard, optional material substitution, guessed archive precedence, image reduction or renderer-slot mapping.
- Decodes actual TGA/VBM content with the shared image decoder, then copies logical pixels through `rf_image_pixel` to remove platform swizzle/pitch identity. Unsupported animated/encoded assets remain explicit decoder failures. Member names are canonicalized to lowercase/forward slash.
- Deduplicates origin references only if owner/token/material agree. It verifies each reference's raw compiled authored-token word+24 against the owned loader origin before capturing collision filters and chart ownership. Explicit UINT32_MAX hidden faces are reported, not assigned arbitrary visible-face charts.
- Captures untouched RGB source lightmaps through `rf_lightmap_rgb_open`; converts each used original image losslessly to RGBA with constant alpha255 (format6, four bytes/pixel) for the helper's logical image representation. No 1555 floor quantization or dynamic light baking occurs. The full original image plus exact projection is included; chart names `ctf06.rfl/chart/<owner>/<source>` are stable authored identities, not current image/mapping handles.
- Reads the raw96-byte mapping's image word and rejects missing/out-of-range ownership **before** calling the existing mapping decoder, which otherwise models original image0 fallback. The probe must never silently fingerprint image0 for an invalid chart reference. Explicitly absent face mappings are unlit records.
- Repeats the actual digest, rejects missing and duplicate reference rows while preserving output, and confirms one changed real texture byte changes identity. It restores that byte afterward. These are assertions awaiting the parent run, not claimed test results.

Scope: installed source-domain capture prerequisite only. It neither serializes RFDS2 nor measures scene integration memory. The standalone process holds the source/editor/lightmap inputs and logical copies until hashing; production should capture identity before source close and release temporary repacking buffers. Raw RGB-to-RGBA conversion and naming must remain identical in PC/Xbox scene adapters for cross-platform digests. Template/generated-rock/admission domains and final published/composed-room digest domains remain separate.

Verified source digest: `4c74d0a864ac2f303620f5001c8f748a6a52b5e1bb7f65b510b2a7f40505c3b2`. The original installed-identity executable name triggered Windows installer detection; use the neutral name and explicit asInvoker manifest above. The probe needs no elevation.
