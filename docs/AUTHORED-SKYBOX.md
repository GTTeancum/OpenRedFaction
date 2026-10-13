# Authored sky rooms and Skybox_State59

Source-written on the published `6d1bf09b54625cade292445139cd9da0779634d4`
baseline. Parent owns the single hourly stock64 compile/startup smoke. No helper
build, syntax compilation, runtime test, image capture or saved-HDD access was
performed. Parent integration includes RFEN9 save continuation and type59 checkpoint admission; see AUTHORED-SKYBOX-SAVE.md. Both renderer and adapter remain uncompiled and runtime-unverified.

## Original evidence

Addresses below refer to the installed PC RF.exe and inform the reconstructed
shared C/Xbox implementation; the executable is neither patched nor shipped.

- Type59 factory table at 4b76f0 selects 4b72d0, constructor 4bec50, vtable
  589bbc. ON handler 4bccf0 copies event byte +2b8 to axis global 64601e and
  event float +2bc to rate global 646020. OFF is generic 4b9f80; the ordinary
  common link phase remains in force.
- 4334d1..43350b advances phase 646024 by positive rate times game delta
  5a4014, then calls 45e870. That function uses the float at 589428
  (0.01745329238474369 radians/degree), sin/cos and a lowercased x/y/z axis to
  build matrix 646028. Level initialization at 45c626 sets axis x and rate 0.
  A phase-reset instruction was not established; fresh-section phase 0 is an
  explicit port initialization policy.
- 431fb5..431ff8 supplies the rotation to world rendering at 4d45d0 only for
  positive rate. Nonpositive rate therefore draws an unrotated sky while the
  port retains the accumulated phase for a later positive rate.
- 4d4760 resolves the special room through 4d4b90/45ebb0. Loader 4f0300 maps
  file room byte28 to runtime room byte1; byte34 is the detail-room flag. The
  first primary sky room is selected. 4d463a..4d466a invokes 4d39b0, which
  centers on room bounds, saves rendering globals, installs the inverse sky
  matrix through 4fc930/40a3b0, draws the special room and restores globals.
  40a3b0 copies a matrix; it is not a camera-matrix multiply.
- A source-data scan of 66 L*.rfl campaign levels found three Skybox_State
  records: UID6676 in L17S1/L17S2/L17S3, text `z`, rate3, delay0 and no outgoing
  links. L17S1 has no primary sky room. L17S2 has room index162/UID21184 with
  129 faces/156 fan triangles; L17S3 has index128/UID3 with 128/152. These are
  input counts, not runtime coverage results.
- Those rooms reference Starbox01_01.tga, Starbox01_02.tga and
  Starbox01_03.tga from maps3.vpp, maps4.vpp and maps2.vpp respectively.
  Each source image is 256x256 RGB24, 196652 source bytes, 262144 decoded RGBA
  bytes. Existing world-material resolution/resizing still applies.

## Rendering and ownership

`scene_skybox.inc` borrows the immutable retained world geometry and scene
material owner after normal resource setup, before startup events. It selects
one authored room, validates the bounded face/material mapping, and retains
only face and material indices. Material descriptors are resolved again for
each draw, so scene material-array replacement does not leave image-descriptor
pointers dangling. Sky pixels remain owned by the existing material table;
there is no second decode, texture bank, native vertex allocation or archive.

The Xbox renderer draws a dedicated immediate sky pass after clear and before
ordinary world/models, allowing their transparency to blend over it. A private
camera copies the active viewing basis, centers at the authored sky bounds and
uses the existing projection/clipping helpers. Axis rotation changes relative
sky vertices, never the main camera or simulation. UVs come from actual sky
faces; white unlit modulation avoids the world-lightmap brightness path.

`RF_SKYBOX_MODE` is an exact world-only blend-disabled, depth-test/no-write
mode. The sink supplies constant far depth16777214. The existing particle
batch owns immediate GPU commands. Both success and failure drain the sky
pass before returning or restoring the ordinary vertex/fragment/depth state.
Normal world geometry then reinstalls vertex arrays and texture stages.
Retained and CPU-projected world paths omit authored sky-room faces; movers
and collision ownership remain unchanged.

The owner closes during existing scene teardown. A nonempty owner blocks a
new scene and full-session idle/reset rather than silently discarding borrowed
identity. Parent must retain its newer pre-close/storage uncertainty guards
when integrating the teardown hunk.

### Additional resource bound

| Resource | Additional cost |
| --- | ---: |
| 1024 face/material pairs | 8192 bytes |
| Remaining owner metadata on 32-bit Xbox | 72 bytes |
| Diagnostic words and event callback/context | 72 bytes |
| Metadata rounded up to 4096-byte pages | 12288 bytes |
| Fixed stack/scratch allowance | 8192 bytes |
| Total charged allowance | **20480 bytes (20 KiB)** |
| New images, heap blocks, GPU mesh or archive handles | **0** |

The compile-time charge is bounded by64KiB. The loops have fixed limits of
1024 faces, 128 corners/face and 12 projected vertices; no recursive work or
variable-sized stack buffers are introduced. Three full-size sample sky
textures total786432 decoded bytes, already resident in the world material
owner; diagnostics report their actual deduplicated resident bytes after any
existing texture resizing. The draw gate checks at least4096 native free
pages (16MiB) **after renderer/world allocations**; it allocates nothing and
does not change persistent state when presentation is temporarily suppressed.

## Parent checkpoint integration contract

Native struct values, not an on-disk struct dump:

```c
typedef struct rf_scene_skybox_state {
    uint32_t version, present, level_identity, room_uid, source_identity, axis;
    float rate, phase;
} rf_scene_skybox_state;
int rf_scene_skybox_capture(int32_t now, rf_scene_skybox_state *out);
int rf_scene_skybox_validate(const rf_scene_skybox_state *candidate);
void rf_scene_skybox_restore(const rf_scene_skybox_state *candidate, int32_t now);
```

The eight32-bit words occupy32 bytes. Encode integer words explicitly in the
parent codec and encode floats by their IEEE binary32 bits. No pointer,
resource handle, event queue or game-clock value is persisted.

| Word | Meaning / required value |
| --- | --- |
| 0 | Version1 |
| 1 | present0/1: valid resident authored sky source, not temporary draw admission |
| 2 | Level identity: FNV-1a of exact entry name bytes then four timestamp bytes |
| 3 | Selected authored room UID, or UINT32_MAX when absent |
| 4 | Source identity: FNV-1a of room header, eligible face headers, texture names, positions and UVs; zero when absent |
| 5 | ASCII x, y or z (120..122) |
| 6 | Finite rate, degrees/second, [-3600,3600]; positive means rotation active |
| 7 | Finite accumulated phase, degrees, [0,360) |

Canonical absence is version1/present0 with the current target level identity,
room UID UINT32_MAX, source identity0, axis x, rate0 and phase0. This also covers
optional resource admission failure; a candidate from a valid sky target must
not silently load into an unavailable sky target. Rotation activation needs
no separate boolean: it is exactly present && rate>0. Positive rate is the
original condition; the finite3600 bound is a port guard, not an original limit.

Capture samples the wrap-safe game clock into a temporary value, without
advancing live phase/clock. Failure preserves output. A backward/replaced
clock more than half RF_TIMER_PERIOD away is refused; ordinary wrap works.
Validate is read-only and checks exact version, presence and source/level/room
identity against the prepared target plus numeric/canonical domains. It
returns RF_OK, RF_FORMAT for a mismatched/malformed candidate, or RF_NOT_FOUND
when no scene is prepared. Capture can also return RF_RANGE for clock input.

Parent sequence: prepare the target owner; capture defaults if handling an
older format; decode and validate the candidate and a game-clock value in
[0,RF_TIMER_PERIOD]; complete all fallible world preparation and storage close;
then call restore while that same target remains alive. Restore is **void,
non-failing and assignment-only**: it publishes the already validated state
and rebases the private clock to now. Do not call it on failed reads, cleanup
failure, before validation, against a different owner or with invalid now.
It performs no allocation, codec work, event replay or resource publication.

Fresh-section setup resets axis x/rate0/phase0, resolves presence/identity,
then ordinary startup events run. An ON command integrates elapsed time at
the old rate before replacing axis/rate; OFF has no inverse. No-sky or invalid
axis/rate commands leave presentation state unchanged and return success so
ordinary authored link chaining continues. Parent owns older-save policy and
final RFEN support classification; event59 remains marked externally
unimplemented in this branch until that persistence join is complete.

## Honest first-pass limits and integration notes

- Actual authored geometry and textures are used, but exact original sky
  projection, lighting, portal eligibility and overlapping sky-surface depth
  behavior are not claimed. This pass draws the room as an unlit far backdrop;
  ordinary world depth occludes it. Runtime visual behavior remains unverified.
- Only the first primary sky room is supported; optional resources missing or
  over bounds leave the ordinary clear backdrop, with diagnostics. Unsupported
  event payloads and absent sky rooms preserve event chains.
- The published baseline lacks the parent's newer live-monitor renderer. This
  pass serves the main view; a monitor camera sky hook is not implemented here.
- Merge renderer admission with the accepted PlayVclip opaque mode: preserve
  its depth4 test/write behavior and this distinct depth1 no-write sky mode;
  disable blending for either exact mode. Keep all existing error fences.
- `rf_scene_skybox[16]` records status, charged bytes, face/triangle counts,
  borrowed image bytes, free pages, ON/refused ON counts, ticks, draws,
  submitted triangles/vertices, captures, restores, clock rebases and resource
  refusals. It is diagnostic only and is not a persistence codec.

No downloaded implementation or third-party code was added. Original inputs
and generated evidence remain outside tracked source.
