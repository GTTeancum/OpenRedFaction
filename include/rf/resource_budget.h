#ifndef RF_RESOURCE_BUDGET_H
#define RF_RESOURCE_BUDGET_H
/* Shared initial campaign limits, not a claim that every level fits.
 * L1S2 world materials peak at 9,367,476 bytes; reserve room for actor skins.
 * Xbox textures share CPU/GPU storage. Lightmaps remain separately bounded.
 * All other allocations and page rounding must still fit stock 64 MiB. */
#define RF_CAMPAIGN_TEXTURE_SLOTS 512u
#define RF_CAMPAIGN_MATERIAL_BUDGET (12u*1024u*1024u)
#define RF_CAMPAIGN_LIGHTMAP_BUDGET (4u*1024u*1024u)
/* The combined renderer also references separately owned NPC, clutter,
 * weapon and pickup images. This check does not allocate/copy their pixels.
 * L2S3 references18,677,764 bytes including lightmaps and fallback; the old
 *16MiB sum rejected already loaded assets before any GPU allocation. */
#define RF_CAMPAIGN_IMAGE_BUDGET (20u*1024u*1024u)
/* Opt-in player shield view profile; still subject to stock64MiB admission. */
#define RF_PLAYER_SHIELD_IMAGE_BUDGET (21u*1024u*1024u)
#endif
