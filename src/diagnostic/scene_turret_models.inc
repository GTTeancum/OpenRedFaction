/* Authored static turret models and their replacement bodies. Included after
 * campaign_seeds and scene_stream declarations. Geometry/tags/material rows are
 * level-owned; texture pixels transfer to the ordinary scene materials owner. */
enum {SCENE_TURRET_MODEL_LIMIT=6,SCENE_TURRET_CLASS_LIMIT=3,
      SCENE_TURRET_MODEL_BUDGET=2*1024*1024};
typedef struct scene_turret_model_resource {
    char name[64];
    rf_static_render_resource render;
    rf_model_collision_resource collision;
    rf_static_model_tags tags;
    rf_model_materials materials;
    uint32_t base,textures;
} scene_turret_model_resource;
typedef struct scene_turret_model_pair {uint32_t class_index,live,dead;} scene_turret_model_pair;
static scene_turret_model_resource scene_turret_models[SCENE_TURRET_MODEL_LIMIT];
static scene_turret_model_pair scene_turret_model_pairs[SCENE_TURRET_CLASS_LIMIT];
static uint32_t scene_turret_model_count,scene_turret_model_pair_count;
uint32_t rf_scene_turret_resources[6]; /* models, classes, retained bytes, peak bytes, tags, spheres */
static void scene_turret_models_close(void)
{
    uint32_t i;
    for(i=0;i<SCENE_TURRET_MODEL_LIMIT;i++) {
        scene_turret_model_resource *r=scene_turret_models+i;
        rf_model_materials_close(&r->materials);
        rf_static_model_tags_close(&r->tags);
        rf_model_collision_resource_close(&r->collision);
        rf_static_render_resource_close(&r->render);
    }
    memset(scene_turret_models,0,sizeof(scene_turret_models));
    memset(scene_turret_model_pairs,0,sizeof(scene_turret_model_pairs));
    scene_turret_model_count=scene_turret_model_pair_count=0;
}
static int scene_turret_model_acquire(const char *authored,rf_vpp *meshes,
    rf_vpp *maps,uint32_t map_count,rf_model_file *file,uint32_t *used,uint32_t *index)
{
    scene_turret_model_resource *r;uint32_t i,peak;char name[64];int status;
    status=rf_model_compiled_filename(authored,name,".v3m");if(status)return status;
    for(i=0;i<scene_turret_model_count;i++) {
        const char *key=scene_turret_models[i].name;
        if(rf_emitter_name_lookup(&key,1,name)==0){*index=i;return RF_OK;}
    }
    if(i==SCENE_TURRET_MODEL_LIMIT || *used>=SCENE_TURRET_MODEL_BUDGET)return RF_RANGE;
    r=scene_turret_models+i;
    status=rf_model_file_open(file,meshes,name);if(status)return status;
    status=rf_static_render_resource_open(file,SCENE_TURRET_MODEL_BUDGET-*used+sizeof(r->render),&r->render);
    if(status)return status;*used+=r->render.allocated_bytes-sizeof(r->render);
    if(*used>SCENE_TURRET_MODEL_BUDGET)return RF_RANGE;
    status=rf_model_collision_resource_open(&r->collision,file,
        SCENE_TURRET_MODEL_BUDGET-*used+sizeof(r->collision));
    if(status)return status;*used+=r->collision.accounted_bytes-sizeof(r->collision);
    if(*used>SCENE_TURRET_MODEL_BUDGET)return RF_RANGE;
    status=rf_static_model_tags_open(file,SCENE_TURRET_MODEL_BUDGET-*used+sizeof(r->tags),&r->tags);
    if(status && status!=RF_NOT_FOUND)return status;
    if(!status)*used+=r->tags.allocated_bytes-sizeof(r->tags);
    if(*used>SCENE_TURRET_MODEL_BUDGET)return RF_RANGE;
    status=rf_model_materials_open_records(&r->materials,r->render.materials,r->render.material_count,
        maps,map_count,SCENE_TURRET_MODEL_BUDGET-*used+sizeof(r->materials));
    if(status)return status;
    peak=*used+r->materials.peak_bytes-sizeof(r->materials);
    if(peak>SCENE_TURRET_MODEL_BUDGET)return RF_RANGE;
    if(peak>rf_scene_turret_resources[3])rf_scene_turret_resources[3]=peak;
    *used+=r->materials.resident_bytes-sizeof(r->materials);
    if(*used>SCENE_TURRET_MODEL_BUDGET)return RF_RANGE;
    memcpy(r->name,name,strlen(name)+1);*index=scene_turret_model_count++;
    rf_scene_turret_resources[4]+=r->tags.count;rf_scene_turret_resources[5]+=r->render.sphere_count;
    return RF_OK;
}
static int scene_turret_models_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,rf_materials *materials)
{
    uint32_t cls,i,j,used=sizeof(scene_turret_models)+sizeof(scene_turret_model_pairs)+sizeof(rf_model_file);
    rf_model_file *file=NULL;rf_material *combined;int status=RF_OK;
    if(!meshes || !maps || !map_count || !materials || scene_turret_model_count || scene_turret_model_pair_count)return RF_RANGE;
    if(campaign_seeds.class_count && (!campaign_seeds.classes || !campaign_seeds.records.items))return RF_RANGE;
    memset(rf_scene_turret_resources,0,sizeof(rf_scene_turret_resources));
    for(cls=0;cls<campaign_seeds.class_count;cls++) {
        const rf_entity_seed_class *c=campaign_seeds.classes+cls;scene_turret_model_pair *pair;
        if(c->model_kind!=1)continue;
        if(c->record_index>=campaign_seeds.records.count){status=RF_FORMAT;goto fail;}
        /* The Auto Turret's static head has no authored replacement body. */
        {const char *names[3]={"Stationary Turret","Stationary Turret_Plain","Auto Turret Head"};
         if(rf_emitter_name_lookup(names,3,campaign_seeds.records.items[c->record_index].record.class_name)<0)continue;}
        if(scene_turret_model_pair_count==SCENE_TURRET_CLASS_LIMIT){status=RF_RANGE;goto fail;}
        if(!file){file=malloc(sizeof(*file));if(!file){status=RF_IO;goto fail;}}
        pair=scene_turret_model_pairs+scene_turret_model_pair_count;pair->class_index=cls;
        status=scene_turret_model_acquire(c->model,meshes,maps,map_count,file,&used,&pair->live);if(status)goto fail;
        pair->dead=UINT32_MAX;
        if(c->corpse.model[0]) {
            status=scene_turret_model_acquire(c->corpse.model,meshes,maps,map_count,file,&used,&pair->dead);if(status)goto fail;
        }
        ++scene_turret_model_pair_count;
    }
    /* All resource work completes before publishing texture slots. Pixel
     * storage moves to the level's existing materials lifetime, like pickups. */
    j=0;for(i=0;i<scene_turret_model_count;i++)j+=scene_turret_models[i].materials.textures.count;
    if((uint64_t)materials->count+j>RF_CAMPAIGN_TEXTURE_SLOTS){status=RF_RANGE;goto fail;}
    if(j) {
        combined=realloc(materials->items,(materials->count+j)*sizeof(*combined));
        if(!combined){status=RF_IO;goto fail;}materials->items=combined;
    }
    for(i=0;i<scene_turret_model_count;i++) {
        scene_turret_model_resource *r=scene_turret_models+i;rf_materials *textures=&r->materials.textures;
        r->base=materials->count;r->textures=textures->count;
        if(textures->count)memcpy(materials->items+materials->count,textures->items,textures->count*sizeof(*textures->items));
        materials->count+=textures->count;materials->loaded+=textures->loaded;
        materials->missing+=textures->missing;materials->allocated_bytes+=textures->allocated_bytes;
        free(textures->items);memset(textures,0,sizeof(*textures));
    }
    rf_scene_turret_resources[0]=scene_turret_model_count;rf_scene_turret_resources[1]=scene_turret_model_pair_count;
    rf_scene_turret_resources[2]=used-sizeof(rf_model_file);
    if(used>rf_scene_turret_resources[3])rf_scene_turret_resources[3]=used;
    free(file);return RF_OK;
fail:
    free(file);scene_turret_models_close();return status;
}
static int scene_turret_model_indices(uint32_t class_index,uint32_t *live,uint32_t *dead)
{
    uint32_t i;if(!live || !dead)return RF_RANGE;
    for(i=0;i<scene_turret_model_pair_count;i++)if(scene_turret_model_pairs[i].class_index==class_index) {
        *live=scene_turret_model_pairs[i].live;*dead=scene_turret_model_pairs[i].dead;return RF_OK;
    }
    return RF_NOT_FOUND;
}
static int scene_turret_model_tag(uint32_t index,const char *name,const float position[3],
    const float basis[9],float transform[12])
{
    int32_t tag;int status;
    if(index>=scene_turret_model_count || !name)return RF_RANGE;
    status=rf_static_model_tags_find(&scene_turret_models[index].tags,(rf_model_name){name,strlen(name)},&tag);
    return status?status:rf_static_model_tag_place(&scene_turret_models[index].tags,tag,basis,position,transform);
}
