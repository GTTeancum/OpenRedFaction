/* Generated Auto Turret heads. NOT integrated by this include alone.
 * Include after turret owner, NPC body and campaign_file_tag_pose declarations.
 * Parent reserves stable turret slots and a real dependent class, then calls
 * factory before combat_open. No fabricated RFL rows or authored child UIDs.
 * Original factory423970..4239d4; linked pose487630; death418f80. */
enum {SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD=1,
      SCENE_TURRET_GENERATED_BASE_DIED=1,SCENE_TURRET_GENERATED_HEAD_DIED=2};
typedef struct scene_turret_generated_key {uint32_t base_uid,role;} scene_turret_generated_key;
typedef struct scene_turret_generated_binding {
    scene_turret_generated_key key;
    uint32_t base_slot,base_handle,head_handle;
    int32_t tag;
    uint32_t death_dispatched;
} scene_turret_generated_binding;
static scene_turret_generated_binding scene_turret_generated[SCENE_TURRET_OWNER_LIMIT];
static uint32_t scene_turret_generated_count;
/* Counters survive teardown for native result collection; first factory resets.
 * Probe live count means living registered generated heads, not reserved slots. */
uint32_t rf_scene_turret_generated_stats[8]; /* created,published,moved,preserved independent aim,ready ticks,base deaths,head deaths,errors */
uint32_t rf_scene_turret_generated_probe[32]; /* See docs/AUTO-TURRET-CHECK.md. */

static int scene_turret_generated_is_base(uint32_t seed)
{
    const char *name="Auto Turret";
    return seed<campaign_seeds.records.count && campaign_seeds.records.items &&
        rf_emitter_name_lookup(&name,1,campaign_seeds.records.items[seed].record.class_name)==0;
}
static int scene_turret_generated_required(uint32_t *count)
{
    uint32_t n=0,i;if(!count)return RF_RANGE;
    if(campaign_seeds.records.count && !campaign_seeds.records.items)return RF_RANGE;
    for(i=0;i<campaign_seeds.records.count;i++)if(scene_turret_generated_is_base(i))++n;
    if(n>SCENE_TURRET_OWNER_LIMIT)return RF_RANGE;
    *count=n;return RF_OK;
}
static scene_turret_generated_binding *scene_turret_generated_find(uint32_t head)
{
    uint32_t i;for(i=0;i<scene_turret_generated_count;i++)
        if(scene_turret_generated[i].head_handle==head)return scene_turret_generated+i;
    return NULL;
}
/* Full registry identity check protects reuse after unregister/reload. */
static campaign_npc_body *scene_turret_generated_base(const scene_turret_generated_binding *b)
{
    campaign_npc_body *base;
    if(!b || b->base_slot>=campaign_npc_body_count || !campaign_npc_bodies ||
       !scene_turret_generated_is_base(b->base_slot) ||
       (uint32_t)campaign_seeds.records.items[b->base_slot].record.uid!=b->key.base_uid)return NULL;
    base=campaign_npc_bodies+b->base_slot;
    if(base->registration.view!=&base->view || base->registration.handle!=b->base_handle ||
       rf_object_registry_lookup(&campaign_registry,b->base_handle)!=&base->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)b->base_handle)!=&base->view)return NULL;
    return base;
}
static int scene_turret_generated_pose(const scene_turret_generated_binding *b,float transform[12])
{
    rf_corpse_surface_source source={0};campaign_model_owner *model;
    if(!scene_turret_generated_base(b))return RF_NOT_FOUND;
    if(!campaign_model_owners || b->base_slot>=campaign_model_owner_count)return RF_RANGE;
    model=campaign_model_owners+b->base_slot;
    if(model->owned)return RF_NOT_FOUND; /* Corpse now owns the former base model. */
    source.model=b->base_slot+1;
    memcpy(source.position,model->position,12);memcpy(source.basis,model->basis,36);
    return campaign_file_tag_pose(&source,b->tag,transform);
}
/* owner_index is a zeroed, already allocated slot within scene_turret_count.
 * This stable address must never move once rf_entity_view_register publishes it.
 * head_class is retained class metadata; config is parsed Auto Turret Head.
 * No authored-vitals overrides apply to the generated child. */
static int scene_turret_generated_create(uint32_t owner_index,uint32_t base_slot,
    uint32_t head_class,const rf_entity_physics_config *config,int32_t vauss)
{
    scene_turret_generated_binding binding={0};scene_turret_owner *o;
    const rf_entity_seed_class *c;rf_entity_creation_vitals_state vitals={0};
    campaign_npc_body *base;float transform[12];uint32_t i;int status;
    if(!config || vauss<0 || vauss>=64 || !scene_turrets || owner_index>=scene_turret_count ||
       scene_turret_count>SCENE_TURRET_OWNER_LIMIT || !campaign_seeds.classes ||
       head_class>=campaign_seeds.class_count || !campaign_npc_bodies || base_slot>=campaign_npc_body_count ||
       !scene_turret_generated_is_base(base_slot) || scene_turret_generated_count>=SCENE_TURRET_OWNER_LIMIT)return RF_RANGE;
    if(!scene_turret_generated_count){memset(rf_scene_turret_generated_stats,0,sizeof(rf_scene_turret_generated_stats));
        memset(rf_scene_turret_generated_probe,0,sizeof(rf_scene_turret_generated_probe));}
    o=scene_turrets+owner_index;if(o->registration.view)return RF_RANGE;
    binding.key.base_uid=(uint32_t)campaign_seeds.records.items[base_slot].record.uid;
    binding.key.role=SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD;
    if(binding.key.base_uid==UINT32_MAX)return RF_FORMAT;
    for(i=0;i<scene_turret_generated_count;i++)if(scene_turret_generated[i].key.base_uid==binding.key.base_uid)return RF_FORMAT;
    binding.base_slot=base_slot;binding.base_handle=campaign_npc_bodies[base_slot].registration.handle;
    base=scene_turret_generated_base(&binding);if(!base)return RF_NOT_FOUND;
    if(base->damage.effects.health<=0 || (base->view.flags_810&1u))return RF_NOT_FOUND;
    status=rf_scene_corpse_file_tag(base_slot+1,"interface_1",0,&binding.tag);if(status)return status;
    status=scene_turret_generated_pose(&binding,transform);if(status)return status;
    c=campaign_seeds.classes+head_class;
    {const char *name="turret_top01.v3d";
     if(c->model_kind!=1 || rf_emitter_name_lookup(&name,1,c->model)!=0)return RF_FORMAT;}
    for(i=0;i<12;i++)if(!isfinite(transform[i]))return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(campaign_model_owners[base_slot].basis[i]))return RF_FORMAT;
    memset(o,0,sizeof(*o));o->uid=o->seed=UINT32_MAX;o->class_index=head_class;
    status=scene_turret_model_indices(head_class,&o->live_model,&o->dead_model);if(status)goto fail;
    vitals.object_flags=rf_entity_creation_object_flags(0,c->model_kind)|0x06000100u;
    if(!(c->physics.flags2&1u))vitals.object_flags|=0x20000u;
    rf_entity_creation_vitals(&vitals,&c->vitals,0);
    if(!isfinite(vitals.health) || vitals.health<=0 || !isfinite(vitals.armor)) {status=RF_FORMAT;goto fail;}
    o->model=o->live_model;o->material=config->material.index;o->surface=config->material;
    memcpy(o->position,transform+9,12);memcpy(o->basis,campaign_model_owners[base_slot].basis,36);
    o->damage.effects.health=vitals.health;o->damage.effects.armor=vitals.armor;
    o->damage.effects.class_health=c->vitals.health;o->damage.effects.class_armor=c->vitals.armor;
    o->damage.effects.class_flags_728=c->physics.flags2;
    o->damage.effects.affiliation=0; /* Original4239bb, not inherited from base. */
    o->damage.effects.voice=o->damage.responsible_handle=o->damage.burn_source=UINT32_MAX;
    memcpy(o->damage_factors,c->damage_factors,sizeof(o->damage_factors));
    o->view.handle=-1;o->view.type=0;o->view.class_type=(int32_t)c->physics.use_kind;
    o->view.flags_7c=vitals.object_flags;o->view.flags_7d0=0x100000u;
    o->view.linked_handle=(int32_t)binding.base_handle;
    o->view.weapons[0]=vauss;o->view.weapons[1]=-1;o->target=UINT32_MAX;
    status=rf_entity_view_register(&campaign_registry,&campaign_entities,&o->view,&o->registration);if(status)goto fail;
    o->damage.effects.handle=o->registration.handle;
    binding.head_handle=o->registration.handle;
    scene_turret_generated[scene_turret_generated_count++]=binding;
    ++rf_scene_turret_generated_stats[0];return RF_OK;
fail:
    ++rf_scene_turret_generated_stats[7];rf_scene_turret_generated_probe[31]=(uint32_t)status;
    memset(o,0,sizeof(*o));return status;
}
/* Publish only successfully constructed owners from the preallocated stable pool. */
static uint32_t scene_turret_generated_activation_allows(uint32_t head);
static int scene_turret_generated_open(int32_t vauss)
{
    uint32_t i;int status;
    if(scene_turret_generated_count)return RF_RANGE;
    memset(rf_scene_turret_generated_stats,0,sizeof(rf_scene_turret_generated_stats));
    memset(rf_scene_turret_generated_probe,0,sizeof(rf_scene_turret_generated_probe));
    for(i=0;i<campaign_seeds.records.count;i++)if(scene_turret_generated_is_base(i)) {
        campaign_npc_body *base;
        if(!campaign_npc_bodies||i>=campaign_npc_body_count)return RF_FORMAT;
        base=campaign_npc_bodies+i;
        if(base->damage.effects.health<=0||(base->view.flags_810&1u))continue;
        if(!scene_turret_dependent.active||scene_turret_count>=scene_turret_capacity)return RF_RANGE;
        ++scene_turret_count;
        status=scene_turret_generated_create(scene_turret_count-1,i,scene_turret_dependent.head_class,
            &scene_turret_dependent.head_physics,vauss);
        if(status){--scene_turret_count;return status;}
    }
    rf_scene_turret_owners[0]=scene_turret_count;return RF_OK;
}
/* Query is also the combat admission gate: attachment is neither possession nor
 * automatic readiness. Parent must drive the base's original AI-ready bit. */
static int scene_turret_generated_ready(uint32_t head,uint32_t *ready)
{
    scene_turret_generated_binding *b=scene_turret_generated_find(head);
    scene_turret_owner *o;campaign_npc_body *base;
    if(!ready)return RF_RANGE;*ready=0;if(!b)return RF_NOT_FOUND;
    o=scene_turret_lookup(head);base=scene_turret_generated_base(b);
    if(!o || !base || b->death_dispatched)return RF_OK;
    *ready=!o->dead && o->damage.effects.health>0 && base->damage.effects.health>0 &&
        !(o->view.flags_7c&(2u|0x4000u)) && !(base->view.flags_7c&(2u|0x4000u)) &&
        !(base->view.flags_810&1u) && (base->view.flags_7d0&1u) &&
        o->view.linked_handle==(int32_t)b->base_handle && scene_turret_generated_activation_allows(head);
    if(*ready)++rf_scene_turret_generated_stats[4];
    return RF_OK;
}
/* Call after base animation/published transform, before head targeting/contact.
 * Copy ONLY translation: original flag0x100 preserves independently aimed basis.
 * Missing/dead base is a lifecycle signal; never fall back to a guessed offset. */
static int scene_turret_generated_publish(uint32_t head)
{
    scene_turret_generated_binding *b=scene_turret_generated_find(head);
    campaign_npc_body *base;scene_turret_owner *o;float transform[12],basis_before[9];uint32_t i,live=0;int status;
    if(!b||scene_turret_generated_retired(head))return RF_NOT_FOUND;o=scene_turret_lookup(head);base=scene_turret_generated_base(b);
    if(!o || !base || b->death_dispatched || o->dead || base->damage.effects.health<=0 ||
       (base->view.flags_810&1u))return RF_NOT_FOUND;
    if(o->view.linked_handle!=(int32_t)b->base_handle)return RF_FORMAT;
    status=scene_turret_generated_pose(b,transform);if(status){++rf_scene_turret_generated_stats[7];
        rf_scene_turret_generated_probe[31]=(uint32_t)status;return status;}
    for(i=9;i<12;i++)if(!isfinite(transform[i]))return RF_FORMAT;
    memcpy(basis_before,o->basis,36);
    if(memcmp(o->position,transform+9,12))++rf_scene_turret_generated_stats[2];
    memcpy(o->position,transform+9,12);
    if(memcmp(basis_before,campaign_model_owners[b->base_slot].basis,36)&&!memcmp(basis_before,o->basis,36))
        ++rf_scene_turret_generated_stats[3];
    ++rf_scene_turret_generated_stats[1];
    for(i=0;i<scene_turret_generated_count;i++){
        scene_turret_owner *candidate=scene_turret_lookup(scene_turret_generated[i].head_handle);
        if(candidate&&!candidate->dead&&candidate->damage.effects.health>0)++live;
    }
    rf_scene_turret_generated_probe[0]=b->key.base_uid;rf_scene_turret_generated_probe[1]=b->key.role;
    rf_scene_turret_generated_probe[2]=b->base_handle;rf_scene_turret_generated_probe[3]=b->head_handle;
    rf_scene_turret_generated_probe[4]=o->uid;rf_scene_turret_generated_probe[5]=o->seed;
    rf_scene_turret_generated_probe[6]=(uint32_t)o->view.linked_handle;
    memcpy(rf_scene_turret_generated_probe+7,&base->damage.effects.health,4);
    memcpy(rf_scene_turret_generated_probe+8,&o->damage.effects.health,4);
    rf_scene_turret_generated_probe[9]=base->view.flags_7d0;
    rf_scene_turret_generated_probe[10]=o->view.flags_7c;rf_scene_turret_generated_probe[11]=(uint32_t)b->tag;
    memcpy(rf_scene_turret_generated_probe+12,o->position,12);memcpy(rf_scene_turret_generated_probe+15,transform+9,12);
    memcpy(rf_scene_turret_generated_probe+18,o->basis,36);memcpy(rf_scene_turret_generated_probe+27,campaign_model_owners[b->base_slot].position,12);
    rf_scene_turret_generated_probe[30]=live;rf_scene_turret_generated_probe[31]=0;return RF_OK;
}
static inline int scene_turret_generated_identity(uint32_t head,scene_turret_generated_key *key)
{
    scene_turret_generated_binding *b=scene_turret_generated_find(head);
    if(!key)return RF_RANGE;if(!b || !scene_turret_lookup(head))return RF_NOT_FOUND;
    *key=b->key;return RF_OK;
}
static inline int scene_turret_generated_resolve(scene_turret_generated_key key,uint32_t *head)
{
    uint32_t i;if(!head || key.role!=SCENE_TURRET_GENERATED_ROLE_AUTO_HEAD)return RF_RANGE;
    for(i=0;i<scene_turret_generated_count;i++)if(scene_turret_generated[i].key.base_uid==key.base_uid &&
        scene_turret_lookup(scene_turret_generated[i].head_handle)){
        *head=scene_turret_generated[i].head_handle;return RF_OK;}
    return RF_NOT_FOUND;
}
/* Parent performs ordinary coupled death side effects with these handles.
 * Base death -> set child life0 and route normal death; head death -> original
 * 1000 damage/source-1/kind-1 to base. Mark BEFORE callback to stop recursion.
 * Callback must not compact/free owners. At-most-once dispatch even on error:
 * return an error to abort the tick; never blindly retry a partial damage call. */
static int scene_turret_generated_death(uint32_t actor,
    int (*dispatch)(void *,uint32_t base,uint32_t head,uint32_t reason),void *context)
{
    uint32_t i;int status;if(!dispatch)return RF_RANGE;
    for(i=0;i<scene_turret_generated_count;i++) {
        scene_turret_generated_binding *b=scene_turret_generated+i;scene_turret_owner *o;
        uint32_t reason;
        if(actor!=b->head_handle && actor!=b->base_handle)continue;
        if(b->death_dispatched)return RF_OK;
        o=scene_turret_lookup(b->head_handle);if(!o)return RF_NOT_FOUND;
        reason=actor==b->head_handle?SCENE_TURRET_GENERATED_HEAD_DIED:SCENE_TURRET_GENERATED_BASE_DIED;
        b->death_dispatched=reason;o->view.linked_handle=-1;o->target=UINT32_MAX;o->fire_due=0;
        ++rf_scene_turret_generated_stats[reason==SCENE_TURRET_GENERATED_BASE_DIED?5:6];
        status=dispatch(context,b->base_handle,b->head_handle,reason);
        if(status){++rf_scene_turret_generated_stats[7];rf_scene_turret_generated_probe[31]=(uint32_t)status;}
        return status;
    }
    return RF_NOT_FOUND;
}
/* Bindings borrow owners. Reset AFTER turret unregister/close and BEFORE base
 * resources disappear; never use this to erase a live registration. */
static int scene_turret_generated_close(void)
{
    uint32_t i;for(i=0;i<scene_turret_generated_count;i++)
        if(scene_turret_lookup(scene_turret_generated[i].head_handle))return RF_RANGE;
    memset(scene_turret_generated,0,sizeof(scene_turret_generated));scene_turret_generated_count=0;return RF_OK;
}

/* Linked children must not acquire or shoot through their own parent body. */
static uint32_t scene_turret_generated_related(uint32_t a,uint32_t b)
{
    for(uint32_t i=0;i<scene_turret_generated_count;i++) {
        const scene_turret_generated_binding *v=scene_turret_generated+i;
        if((a==v->head_handle&&b==v->base_handle)||(b==v->head_handle&&a==v->base_handle))return 1;
    }
    return 0;
}
