#ifndef RF_RESOURCE_BUDGET_H
#define RF_RESOURCE_BUDGET_H
/* Shared initial campaign limits, not a claim that every level fits.
 * L1S2 world materials peak at 9,367,476 bytes; reserve room for actor skins.
 * Xbox textures share CPU/GPU storage. Lightmaps remain separately bounded.
 * All other allocations and page rounding must still fit stock 64 MiB. */
#define RF_CAMPAIGN_MATERIAL_BUDGET (12u*1024u*1024u)
#define RF_CAMPAIGN_LIGHTMAP_BUDGET (4u*1024u*1024u)
#define RF_CAMPAIGN_IMAGE_BUDGET (RF_CAMPAIGN_MATERIAL_BUDGET+RF_CAMPAIGN_LIGHTMAP_BUDGET)
#endif
