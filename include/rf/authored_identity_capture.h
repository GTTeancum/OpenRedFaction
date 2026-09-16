#ifndef RF_AUTHORED_IDENTITY_CAPTURE_H
#define RF_AUTHORED_IDENTITY_CAPTURE_H
#include "rf/geomod_authored_identity.h"
/* Capture the bounded ctf06/UID94 source identity before original resources
 * close. Inputs remain borrowed; maps is an already-open contiguous array1..32
 * and rgb holds immutable ORIGINAL source RGB, never a dynamically lit atlas.
 * Asset materials must be the loader's unremapped compiled texture IDs.
 * Unique exact texture ownership is required across supplied maps; no archive
 * precedence guesses, missing pattern, reduction or mapping-image0 fallback.
 * Returns digest matching the installed capture probe's logical pixel/chart
 * convention. No console, scene mutation, archive reopen or retained owner.
 * Both outputs unchanged on any failure. All allocated scratch is released.
 * peak_bytes accounts context/arrays/editor/material/chart copies, temporary
 * decoder image rounded to4KiB, and8KiB conservative stack reserve including
 * the4096-byte image reader/SHA work. Excludes borrowed input owners and heap
 * allocator bookkeeping. Budget2MiB is the intended current profile cap.
 * Output/input buffers must be disjoint; caller retains valid immutable input
 * lifetimes and exclusive archive cursor access for the duration of capture. */
int rf_geomod_authored_identity_capture(const rf_level *,const rf_geometry *,
    const rf_geomod_authored_post_view *,rf_vpp *maps,uint32_t map_count,
    const rf_lightmap_rgb_owner *,uint32_t budget,unsigned char digest[32],uint32_t *peak_bytes);
#endif
