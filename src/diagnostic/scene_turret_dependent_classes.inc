/* Explicit dependent class metadata for original423986 Auto Turret Head.
 * Include after campaign_seeds and entity_assets declarations, before loaders.
 * Call immediately after seeds_open, before any borrowed class pointers/catalogs.
 * Does not create records, actors, models, or fabricated record_index values. */
enum {SCENE_TURRET_DEPENDENT_CLASS_LIMIT=640};
typedef struct scene_turret_dependent_class_state {
    rf_entity_seeds *seeds;
    uint32_t active,added,authored_count,head_class,accounted_bytes,peak_bytes;
    rf_entity_physics_config head_physics;
} scene_turret_dependent_class_state;
static scene_turret_dependent_class_state scene_turret_dependent;
static const char scene_turret_auto_base_name[]="Auto Turret";
static const char scene_turret_auto_head_name[]="Auto Turret Head";

/* A missing record index is legal only for this explicitly retained dependency.
 * Caller never indexes records with the returned generated class's sentinel. */
static int scene_turret_class_name(const rf_entity_seeds *seeds,uint32_t index,const char **name)
{
    const rf_entity_seed_class *c;
    if(!seeds || !name || !seeds->classes || index>=seeds->class_count)return RF_RANGE;
    c=seeds->classes+index;
    if(c->record_index<seeds->records.count) {
        if(!seeds->records.items || !seeds->items || seeds->items[c->record_index].class_index!=index)return RF_FORMAT;
        *name=seeds->records.items[c->record_index].record.class_name;return RF_OK;
    }
    if(c->record_index==UINT32_MAX && scene_turret_dependent.active && scene_turret_dependent.added &&
       scene_turret_dependent.seeds==seeds && scene_turret_dependent.head_class==index) {
        *name=scene_turret_auto_head_name;return RF_OK;
    }
    return RF_FORMAT;
}
static int scene_turret_class_is_dependent(const rf_entity_seeds *seeds,uint32_t index)
{
    return scene_turret_dependent.active && scene_turret_dependent.added &&
        scene_turret_dependent.seeds==seeds && scene_turret_dependent.head_class==index;
}
static int scene_turret_dependent_read_class(const void *text,uint32_t bytes,rf_entity_seed_class *out)
{
    rf_entity_seed_class c={0};rf_entity_assets assets;const char *name=scene_turret_auto_head_name;int status;
    c.record_index=UINT32_MAX;
    status=rf_entity_vitals_config_read(text,bytes,name,&c.vitals);if(status)return status;
    status=rf_entity_eye_limits_read(text,bytes,name,&c.eye_limits);if(status)return status;
    status=rf_entity_unholster_delay_read(text,bytes,name,&c.unholster_delay);if(status)return status;
    status=rf_entity_rotation_values_read(text,bytes,name,&c.rotation);if(status)return status;
    status=rf_entity_corpse_config_read(text,bytes,name,&c.corpse);if(status)return status;
    status=rf_entity_damage_factors_read(text,bytes,name,c.damage_factors);if(status)return status;
    status=rf_entity_class_physics_read(text,bytes,name,&c.physics);if(status)return status;
    status=rf_entity_lod_distances_read(text,bytes,name,&c.lod);if(status)return status;
    status=rf_entity_assets_read(text,bytes,name,"",&assets);if(status)return status;
    memcpy(c.model,assets.model,sizeof(c.model));status=rf_entity_model_kind(c.model,&c.model_kind);if(status)return status;
    {const char *model="turret_top01.v3d";
     if(c.model_kind!=1 || rf_emitter_name_lookup(&model,1,c.model)!=0 || c.corpse.model[0])return RF_FORMAT;}
    *out=c;return RF_OK;
}
/* budget covers existing seeds, retained sidecar, replacement class allocation
 * while the old array is live, and one scratch buffer. Caller seeds preserved
 * on failure. Same authored class indices, records and seed count survive. */
static int scene_turret_dependent_classes_open(rf_entity_seeds *seeds,rf_vpp *tables,uint32_t budget)
{
    scene_turret_dependent_class_state next={0};rf_entity_seed_class definition,*classes=NULL;
    rf_vpp_entry entity,material;void *text=NULL;uint64_t peak,retained,replacement=0;
    uint32_t i,base_present=0,head=UINT32_MAX,scratch;int status;
    if(!seeds || !tables || scene_turret_dependent.active ||
       seeds->class_count>SCENE_TURRET_DEPENDENT_CLASS_LIMIT ||
       (seeds->class_count && !seeds->classes) ||
       (seeds->records.count && (!seeds->records.items || !seeds->items)))return RF_RANGE;
    for(i=0;i<seeds->records.count;i++) {
        const char *name=seeds->records.items[i].record.class_name;
        const char *base=scene_turret_auto_base_name,*child=scene_turret_auto_head_name;
        if(seeds->items[i].class_index>=seeds->class_count)return RF_FORMAT;
        if(rf_emitter_name_lookup(&base,1,name)==0)base_present=1;
        if(rf_emitter_name_lookup(&child,1,name)==0) {
            uint32_t cls=seeds->items[i].class_index;
            if(head!=UINT32_MAX && head!=cls)return RF_FORMAT;head=cls;
        }
    }
    if(!base_present)return RF_OK;
    next.active=1;next.seeds=seeds;next.authored_count=seeds->class_count;
    next.added=head==UINT32_MAX;next.head_class=next.added?seeds->class_count:head;
    if(next.added && seeds->class_count==SCENE_TURRET_DEPENDENT_CLASS_LIMIT)return RF_RANGE;
    status=rf_vpp_find(tables,"entity.tbl",&entity);if(status)return status;
    status=rf_vpp_find(tables,"materials.tbl",&material);if(status)return status;
    scratch=entity.size>material.size?entity.size:material.size;
    if(!entity.size || !material.size)return RF_FORMAT;
    if(next.added)replacement=(uint64_t)(seeds->class_count+1)*sizeof(*classes);
    retained=(uint64_t)seeds->resident_bytes+sizeof(next)+(next.added?sizeof(*classes):0);
    peak=(uint64_t)seeds->resident_bytes+sizeof(next)+replacement+scratch;
    if(retained>budget || peak>budget || peak>UINT32_MAX)return RF_RANGE;
    text=malloc(scratch);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entity,0,text,entity.size);if(status)goto done;
    status=scene_turret_dependent_read_class(text,entity.size,&definition);if(status)goto done;
    next.head_physics.authored=definition.physics;
    status=rf_entity_sphere_declarations_read(text,entity.size,scene_turret_auto_head_name,&next.head_physics.spheres);if(status)goto done;
    status=rf_vpp_read(tables,&material,0,text,material.size);if(status)goto done;
    status=rf_entity_material_read(text,material.size,definition.physics.material,&next.head_physics.material);if(status)goto done;
    if(next.added) {
        classes=malloc((size_t)replacement);if(!classes){status=RF_IO;goto done;}
        if(seeds->class_count)memcpy(classes,seeds->classes,seeds->class_count*sizeof(*classes));
        classes[seeds->class_count]=definition;
    }else {
        const char *actual;status=scene_turret_class_name(seeds,head,&actual);if(status)goto done;
        {const char *wanted=scene_turret_auto_head_name;
         if(rf_emitter_name_lookup(&wanted,1,actual)!=0 || seeds->classes[head].model_kind!=1){status=RF_FORMAT;goto done;}}
    }
    /* Publication is last: all failing work completed, no borrowed class pointer
     * may exist yet. Allocation ownership transfers to ordinary seeds_close. */
    if(next.added) {
        free(seeds->classes);seeds->classes=classes;classes=NULL;++seeds->class_count;
        seeds->resident_bytes+=(uint32_t)sizeof(*seeds->classes);
    }
    next.accounted_bytes=sizeof(next);next.peak_bytes=(uint32_t)peak;
    if(peak>seeds->peak_bytes)seeds->peak_bytes=(uint32_t)peak;
    scene_turret_dependent=next;status=RF_OK;
done:
    free(classes);free(text);return status;
}
/* Existing core loader remains authoritative for all authored motion classes.
 * It sees an authored-prefix view, never a fabricated Head seed. The dependent
 * static class receives only default weapons/groups and inert motion indices. */
static int scene_turret_dependent_base_motions_open(const rf_entity_seeds *seeds,
    rf_vpp *tables,rf_vpp *motions,uint32_t budget,rf_entity_base_motions *out)
{
    rf_entity_seeds authored;rf_entity_base_motions loaded={0};rf_entity_state_set *combined=NULL,*head;
    rf_vpp_entry entry;void *text=NULL;uint64_t replacement,peak,retained;uint32_t i;int status;
    if(!out || out->classes || out->class_count || out->resident_bytes || out->peak_bytes ||
       out->weapons.count || out->weapons.primary_count || out->groups || out->group_count)return RF_RANGE;
    if(!scene_turret_dependent.active || !scene_turret_dependent.added)
        return rf_entity_base_motions_open(seeds,tables,motions,budget,out);
    if(seeds!=scene_turret_dependent.seeds || seeds->class_count!=scene_turret_dependent.authored_count+1 ||
       scene_turret_dependent.head_class!=seeds->class_count-1)return RF_FORMAT;
    authored=*seeds;authored.class_count=scene_turret_dependent.authored_count;
    status=rf_entity_base_motions_open(&authored,tables,motions,budget,&loaded);if(status)return status;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)goto done;
    replacement=(uint64_t)seeds->class_count*sizeof(*combined);
    retained=(uint64_t)loaded.resident_bytes+sizeof(*combined);
    peak=(uint64_t)loaded.resident_bytes+replacement+entry.size;
    if(!entry.size || peak>budget || peak>UINT32_MAX || retained>budget){status=RF_RANGE;goto done;}
    combined=calloc(seeds->class_count,sizeof(*combined));text=malloc(entry.size);
    if(!combined || !text){status=RF_IO;goto done;}
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status)goto done;
    head=combined+scene_turret_dependent.head_class;
    for(i=0;i<23;i++)head->states[i]=-1;
    for(i=0;i<45;i++)head->actions[i]=-1;
    status=rf_entity_weapon_groups_read(text,entry.size,scene_turret_auto_head_name,&loaded.weapons,head->weapon_groups);if(status)goto done;
    status=rf_entity_default_weapons_read(text,entry.size,scene_turret_auto_head_name,&loaded.weapons,&head->default_weapons);if(status)goto done;
    /* Static model, like core model_kind!=2 path: no skeletal motions/groups. */
    if(head->default_weapons.primary!=rf_weapon_name_find(&loaded.weapons,"Vauss") ||
       head->default_weapons.primary<0 || head->default_weapons.secondary!=-1){status=RF_FORMAT;goto done;}
    memcpy(combined,loaded.classes,loaded.class_count*sizeof(*combined));free(loaded.classes);
    loaded.classes=combined;combined=NULL;loaded.class_count=seeds->class_count;
    loaded.resident_bytes=(uint32_t)retained;if(peak>loaded.peak_bytes)loaded.peak_bytes=(uint32_t)peak;
    free(text);*out=loaded;return RF_OK;
done:
    free(combined);free(text);rf_entity_base_motions_close(&loaded);return status;
}
/* Ordinary seed teardown frees appended classes. Reset only after that teardown;
 * resource/actor users and any copied names must already be closed. */
static int scene_turret_dependent_classes_close(void)
{
    if(scene_turret_dependent.active && scene_turret_dependent.seeds &&
       (scene_turret_dependent.seeds->classes || scene_turret_dependent.seeds->class_count))return RF_RANGE;
    memset(&scene_turret_dependent,0,sizeof(scene_turret_dependent));return RF_OK;
}
