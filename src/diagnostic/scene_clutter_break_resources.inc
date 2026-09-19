#ifndef RF_SCENE_CLUTTER_BREAK_EFFECTS_RESOURCES_INC
#define RF_SCENE_CLUTTER_BREAK_EFFECTS_RESOURCES_INC
/* Include after scene_stream definition, before particle bitmap resolver.
 * Two demand-loaded effect families follow scene ownership. All resources bounded and closed
 * during scene teardown, after particle users cease. */
typedef struct scene_clutter_break_effects_owner {
    char foley[64];rf_explosion_definition definition;rf_explosion_materials materials;
    struct {rf_explosion_clock clock;uint32_t slots[6];} instances[8];
} scene_clutter_break_effects_owner;
static scene_clutter_break_effects_owner *scene_clutter_break_effects[2];
static void scene_clutter_break_effects_close(void)
{
    uint32_t kind;
    for(kind=0;kind<2;kind++)if(scene_clutter_break_effects[kind]){
        rf_explosion_materials_close(&scene_clutter_break_effects[kind]->materials);
        free(scene_clutter_break_effects[kind]);scene_clutter_break_effects[kind]=NULL;
    }
}
static int scene_clutter_break_effects_open(const char *tables_path,rf_vpp *maps,uint32_t map_count,uint32_t kind)
{
    rf_vpp tables={0};rf_vclip_definition clip;scene_clutter_break_effects_owner *owner;int status;
    if(kind>=2)return RF_RANGE;if(scene_clutter_break_effects[kind])return RF_OK;
    owner=calloc(1,sizeof(*owner));if(!owner)return RF_IO;
    status=rf_vpp_open(&tables,tables_path);
    if(!status)status=rf_vclip_definition_load(&tables,kind?"oil drum explode":"yellboom",65536,&clip);
    if(!status)status=rf_explosion_definition_load(&tables,clip.explosion,256*1024,&owner->definition);
    if(!status&&clip.has_foley)memcpy(owner->foley,clip.foley,sizeof(owner->foley));
    rf_vpp_close(&tables);
    if(status)printf("CLUTTER_BREAK_EFFECT_DEFINITION_ERROR %d\n",status);
    if(!status && (owner->definition.recipe.central_count>(kind?3u:6u) || owner->definition.recipe.central_random!=0))status=RF_FORMAT;
    /* Only central emitters are submitted here; do not load unused trail textures. */
    owner->definition.resolved&=(1u<<owner->definition.recipe.central_count)-1;
    /* Shared loader resolves/deduplicates the named emitter bitmap resources.
     * 128KiB material ceiling, in addition to the small owner/metadata. */
    if(!status)status=rf_explosion_materials_open(&owner->materials,&owner->definition,maps,map_count,128*1024);
    if(status)printf("CLUTTER_BREAK_EFFECT_MATERIAL_ERROR %d\n",status);
    else printf("CLUTTER_BREAK_EFFECT_MATERIALS %u %u\n",owner->materials.count,owner->materials.resident_bytes);
    if(status){rf_explosion_materials_close(&owner->materials);free(owner);return status;}
    scene_clutter_break_effects[kind]=owner;return RF_OK;
}
/* Texture index carried under the dedicated 0x40000000 particle bitmap tag. */
static const rf_particle_animation *scene_clutter_break_effects_image(uint32_t index)
{
    uint32_t kind=index>>8;index&=255u;
    if(kind>=2||!scene_clutter_break_effects[kind]||index>=scene_clutter_break_effects[kind]->materials.count)return NULL;
    return scene_clutter_break_effects[kind]->materials.animations+index;
}
#endif
