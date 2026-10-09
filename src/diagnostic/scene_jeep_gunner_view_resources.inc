#ifndef RF_SCENE_JEEP_GUNNER_VIEW_RESOURCES_INCLUDED
#define RF_SCENE_JEEP_GUNNER_VIEW_RESOURCES_INCLUDED
/* A mounted view has its own owner; Jeep Gun is never a handheld inventory
 * slot. Authored weapons.tbl resolves fp_jgun.vcm and its idle/fire MVFs to
 * the ordinary V3C/RFA reader. No attachment/rest mesh substitutes for it.
 * One owner serves every admitted Jeep in the scene, with no gameplay-time
 * archive reads or allocations. Images transfer once to the scene owner. */
typedef struct scene_jeep_gunner_view {
    rf_player_weapon *weapon;
    uint32_t base,textures,merged,resident_bytes,peak_bytes;
    uint32_t active,host,shots,pose_frame,steps;
} scene_jeep_gunner_view;
static void scene_jeep_gunner_view_close(scene_jeep_gunner_view **owner)
{
    scene_jeep_gunner_view *o;if(!owner || !(o=*owner))return;
    rf_player_weapon_close(&o->weapon);free(o);*owner=NULL;
}
static int scene_jeep_gunner_view_open(rf_vpp *tables,rf_vpp *meshes,rf_vpp *motions,
    rf_vpp *maps,uint32_t map_count,uint32_t budget,scene_jeep_gunner_view **owner)
{
    scene_jeep_gunner_view *o=NULL;rf_weapon_view_definition definition;int status;
    if(!tables || !meshes || !motions || !maps || !map_count || !owner || *owner ||
       budget<sizeof(*o)+sizeof(definition)+128*1024)return RF_RANGE;
    status=rf_weapon_view_load(tables,"Jeep Gun",128*1024,&definition);if(status)return status;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;o->host=UINT32_MAX;
    status=rf_player_weapon_open_view(meshes,motions,maps,map_count,&definition,
        budget-sizeof(*o)-sizeof(definition),&o->weapon);if(status)goto fail;
    o->resident_bytes=sizeof(*o)+o->weapon->resident_bytes;
    o->peak_bytes=sizeof(*o)+sizeof(definition)+o->weapon->peak_bytes;
    if(o->peak_bytes<sizeof(definition)+128*1024)o->peak_bytes=sizeof(definition)+128*1024;
    *owner=o;return RF_OK;
fail:scene_jeep_gunner_view_close(&o);return status;
}
/* All fallible admission precedes image ownership transfer. The view already
 * passed its own loader budget. At this point the scene also references NPC,
 * clutter and weapon banks, so the INITIAL world-material cap is inapplicable.
 * Retain the texture-slot/overflow bounds; the renderer independently enforces
 * RF_CAMPAIGN_IMAGE_BUDGET on combined image payloads before GPU publication. */
static int scene_jeep_gunner_view_merge(scene_jeep_gunner_view *o,rf_materials *scene,
    uint32_t capacity)
{
    rf_materials *bank;rf_material *combined;
    if(!o || !o->weapon || o->merged || !scene)return RF_RANGE;
    bank=&o->weapon->materials.textures;
    if(!bank->count || !bank->items || scene->count>capacity || bank->count>capacity-scene->count ||
       bank->allocated_bytes>UINT32_MAX-scene->allocated_bytes ||
       bank->allocated_bytes>o->resident_bytes)return RF_RANGE;
    combined=realloc(scene->items,(scene->count+bank->count)*sizeof(*combined));if(!combined)return RF_IO;
    scene->items=combined;o->base=scene->count;o->textures=bank->count;
    memcpy(combined+scene->count,bank->items,bank->count*sizeof(*combined));
    scene->count+=bank->count;scene->allocated_bytes+=bank->allocated_bytes;
    scene->loaded+=bank->loaded;scene->missing+=bank->missing;
    o->resident_bytes-=bank->allocated_bytes;
    free(bank->items);memset(bank,0,sizeof(*bank));o->merged=1;return RF_OK;
}
#endif
