#ifndef RF_SCENE_MEDIC_SYRINGE_OWNER_INCLUDED
#define RF_SCENE_MEDIC_SYRINGE_OWNER_INCLUDED
/* Original423a85..423ad7 creates syringe type4 for medic1;420f90 poses hand1.
 * This resource owner is deliberately separate from authored clutter rows.
 * No free-world drop, pickup UID, reserve inference, or death-time deletion. */
static struct {
    campaign_clutter_model model;rf_clutter_shared_static_model shared;
    rf_model_materials materials;uint32_t class_index,ready,bytes,peak,body_bytes;
} scene_medic_syringe_resource;
static rf_object_list scene_medic_syringe_candidates;
uint32_t rf_scene_medic_syringe[12]; /* created,posed,damage,deleted,closed,draws,bytes,peak,fault,parent,child,state */
static int scene_medic_syringe_qualified(const campaign_npc_body *owner)
{
    uint32_t slot,cls;const char *name;
    if(!owner||!campaign_npc_bodies||owner<campaign_npc_bodies||owner>=campaign_npc_bodies+campaign_npc_body_count)return 0;
    slot=(uint32_t)(owner-campaign_npc_bodies);if(slot>=campaign_seeds.records.count)return 0;
    cls=campaign_seeds.items[slot].class_index;if(cls>=campaign_seeds.class_count)return 0;
    name=campaign_seeds.records.items[slot].record.class_name;
    return name&&rf_emitter_name_lookup(&name,1,"medic1")==0;
}
static int scene_medic_syringe_parent_live(const campaign_npc_body *owner)
{
    return owner&&owner->registration.view==&owner->view&&
        rf_object_registry_lookup(&campaign_registry,owner->registration.handle)==&owner->registration&&
        rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)==&owner->view;
}
static int scene_medic_syringe_child_valid(const rf_clutter_base_owner *child)
{
    return child&&scene_medic_syringe_resource.ready&&
        child->state.token==(uint32_t)(uintptr_t)&child->state&&child->state.class_index==(int32_t)scene_medic_syringe_resource.class_index&&
        child->state.definition==campaign_clutter_classes.items+scene_medic_syringe_resource.class_index&&
        child->attachment.model==(uint32_t)(uintptr_t)&scene_medic_syringe_resource.shared&&
        rf_object_registry_lookup(&campaign_registry,child->state.handle)==&child->state;
}
static int scene_medic_syringe_present(const campaign_npc_body *owner)
{
    const scene_medic_syringe *s;
    if(!scene_medic_syringe_qualified(owner)||!scene_medic_syringe_parent_live(owner))return 0;
    s=&owner->syringe;
    return !s->fault&&!s->unavailable&&s->state!=2&&s->parent==&owner->registration&&
        s->parent_handle==owner->registration.handle&&scene_medic_syringe_child_valid(s->child)&&
        !(s->child->state.flags&2u)&&isfinite(s->child->state.health)&&s->child->state.health>0;
}
static int scene_medic_syringe_close_check(const rf_clutter_base_owner *child,const rf_object_list *list)
{
    if(!child)return RF_OK;
    if(!scene_medic_syringe_child_valid(child)||!scene_medic_syringe_resource.shared.references||
       child->allocated_bytes>scene_medic_syringe_resource.body_bytes||
       !list||!list->count||child->state.link.next||child->state.link.previous||
       !child->object_link.next||!child->object_link.previous||
       child->object_link.next->previous!=&child->object_link||
       child->object_link.previous->next!=&child->object_link)return RF_FORMAT;
    return RF_OK;
}
static int scene_medic_syringe_child_close(rf_clutter_base_owner **child,rf_object_list *list)
{
    uint32_t bytes=*child?(*child)->allocated_bytes:0;
    int status=scene_medic_syringe_close_check(*child,list);if(status)return status;
    if(bytes>scene_medic_syringe_resource.body_bytes)return RF_FORMAT;
    if(*child)++rf_scene_medic_syringe[4];
    status=rf_clutter_shared_static_base_close(&scene_medic_syringe_resource.shared,child,&campaign_registry,list);
    if(!status)scene_medic_syringe_resource.body_bytes-=bytes;
    return status;
}
static int scene_medic_syringe_lane_valid(uint32_t state,float health)
{
    uint32_t bits;memcpy(&bits,&health,4);
    if(state>2)return RF_FORMAT;
    if(state==1)return isfinite(health)&&health>0&&health<=50.0f?RF_OK:RF_FORMAT;
    return bits?RF_FORMAT:RF_OK;
}
/* Parent render source may be a retained dying actor OR transferred corpse.
 * The model owner is stable, but a corpse must independently match its UID. */
static int scene_medic_syringe_parent_pose(campaign_npc_body *owner,int32_t tag,
    float transform[12],uint32_t *room,uint32_t *flags)
{
    uint32_t slot=(uint32_t)(owner-campaign_npc_bodies),i;rf_entity_pose *pose=NULL;
    rf_corpse_surface_source source={0};campaign_model_owner *render;int status;
    if(slot>=campaign_model_owner_count||!scene_medic_syringe_parent_live(owner))return RF_NOT_FOUND;
    render=campaign_model_owners+slot;status=campaign_model_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    *flags=owner->object_flags;
    if(render->owned){
        for(i=0;i<RF_CORPSE_CAPACITY;i++)if(campaign_live_corpses.pool.active_mask&(1u<<i)){
            const rf_corpse *corpse=&campaign_live_corpses.slots[i].corpse;
            if(corpse->update.model==slot+1&&corpse->uid==(uint32_t)campaign_seeds.records.items[slot].record.uid){
                *flags=corpse->update.fade.object_flags_7c;break;
            }
        }
        if(i==RF_CORPSE_CAPACITY)return RF_NOT_FOUND;
    }
    source.model=slot+1;memcpy(source.position,render->owned?render->position:owner->published,12);memcpy(source.basis,render->basis,36);
    status=campaign_file_tag_pose(&source,tag,transform);if(status)return status;
    *room=render->room==UINT32_MAX?0:render->room+1;return RF_OK;
}
static int scene_medic_syringe_child_create(campaign_npc_body *owner,float health,
    uint32_t budget,scene_medic_syringe_candidate *candidate)
{
    rf_clutter_create_descriptor d={0};const rf_clutter_class *cls;const rf_entity_material *material;
    float transform[12],coefficients[3];uint32_t room,flags;uint64_t predicted;int status;
    if(!scene_medic_syringe_resource.ready||!campaign_surface_palette)return RF_NOT_FOUND;
    cls=campaign_clutter_classes.items+scene_medic_syringe_resource.class_index;
    predicted=sizeof(rf_clutter_base_owner)+(uint64_t)scene_medic_syringe_resource.model.resource.sphere_count*sizeof(rf_physics_sphere);
    if(cls->flags&6u)predicted+=(uint64_t)(scene_medic_syringe_resource.model.resource.sphere_count?scene_medic_syringe_resource.model.resource.sphere_count:1)*sizeof(rf_physics_sphere);
    if(scene_medic_syringe_resource.body_bytes>128u*1024u||
       predicted>128u*1024u-scene_medic_syringe_resource.body_bytes||predicted>budget)return RF_RANGE;
    status=rf_scene_corpse_file_tag(candidate->parent_slot+1,"hand_right",0,&candidate->hand_tag);if(status)return status;
    status=scene_medic_syringe_parent_pose(owner,candidate->hand_tag,transform,&room,&flags);if(status)return status;
    material=campaign_surface_palette->materials+cls->material;
    coefficients[0]=material->elasticity;coefficients[1]=material->friction;coefficients[2]=material->density;
    d.model=cls->model;d.kind=cls->model_kind;d.material=cls->material;d.identifier=-1;d.radius=cls->radius;
    d.flags=(cls->flags&6u)?0x20u:0u; /* Original410560: can_carry alone supplies no collision. */
    memcpy(d.position,transform+9,12);memcpy(d.matrix,transform,36);
    status=rf_clutter_shared_static_base_open(&scene_medic_syringe_resource.shared,&d,
        &campaign_registry,&scene_medic_syringe_candidates,&campaign_clutter_uid_cursor,room,0,1,coefficients,
        budget,&candidate->child);if(status)return status;if(!candidate->child)return RF_NOT_FOUND;
    scene_medic_syringe_resource.body_bytes+=candidate->child->allocated_bytes;
    /* Keep the generic runtime UID from the allocator. It is never used as an
     * authored key, trigger target, RFNC UID, or substitute pickup identity. */
    candidate->child->state.class_index=(int32_t)scene_medic_syringe_resource.class_index;
    candidate->child->state.definition=campaign_clutter_classes.items+scene_medic_syringe_resource.class_index;
    candidate->child->state.name="";candidate->child->state.flags|=0x4000u;
    if(cls->flags&1u)candidate->child->state.flags|=0x1000u;
    status=scene_clutter_damage_bind(&campaign_registry,candidate->child,
        campaign_clutter_damage_profiles+scene_medic_syringe_resource.class_index,&candidate->damage);
    if(status){(void)scene_medic_syringe_child_close(&candidate->child,&scene_medic_syringe_candidates);return status;}
    candidate->child->state.health=health;
    candidate->allocated_bytes=candidate->child->allocated_bytes;candidate->peak_bytes=candidate->child->peak_bytes;
    ++rf_scene_medic_syringe[0];return RF_OK;
}
static int scene_medic_syringe_capture(const campaign_npc_body *owner,uint32_t *state,float *health)
{
    const scene_medic_syringe *s;int status;
    if(!owner||!state||!health)return RF_RANGE;
    if(!scene_medic_syringe_qualified(owner)){*state=0;*health=0;return RF_OK;}
    s=&owner->syringe;if(s->fault)return s->fault;
    status=scene_medic_syringe_lane_valid(s->state,s->health);if(status)return status;
    if(s->child){
        if(s->parent!=&owner->registration||s->parent_handle!=owner->registration.handle||
           s->parent_slot!=(uint32_t)(owner-campaign_npc_bodies)||!scene_medic_syringe_parent_live(owner)||
           !scene_medic_syringe_child_valid(s->child))return RF_FORMAT;
        if(s->child->state.flags&2u){*state=2;*health=0;return RF_OK;}
        if(!isfinite(s->child->state.health)||s->child->state.health<=0||s->child->state.health>50)return RF_FORMAT;
        if(s->child->state.health!=50||s->state==1){*state=1;*health=s->child->state.health;return RF_OK;}
    }else if(!s->unavailable&&s->state!=2)return RF_NOT_FOUND;
    *state=s->state;*health=s->health;return RF_OK;
}
static void scene_medic_syringe_restore_discard(scene_medic_syringe_candidate *candidate)
{
    int status;if(!candidate)return;
    status=scene_medic_syringe_child_close(&candidate->child,&scene_medic_syringe_candidates);
    if(status){rf_scene_medic_syringe[8]=(uint32_t)status;return;}
    memset(candidate,0,sizeof(*candidate));
}
static int scene_medic_syringe_restore_prepare(campaign_npc_body *owner,uint32_t state,float health,
    uint32_t retired,uint32_t budget,scene_medic_syringe_candidate *out)
{
    scene_medic_syringe_candidate value={0};int status;
    if(!owner||!out||out->prepared||out->child||retired>1)return RF_RANGE;
    status=scene_medic_syringe_lane_valid(state,health);if(status)return status;
    if(!scene_medic_syringe_qualified(owner))return state?RF_FORMAT:RF_NOT_FOUND;
    status=scene_medic_syringe_close_check(owner->syringe.child,&campaign_clutter_objects);if(status)return status;
    value.parent=&owner->registration;value.parent_handle=owner->registration.handle;
    value.parent_slot=(uint32_t)(owner-campaign_npc_bodies);value.state=state;value.health=health;
    value.retired=retired;value.hand_tag=-1;
    if(!retired){
        if(!scene_medic_syringe_parent_live(owner))return RF_NOT_FOUND;
        if(state!=2){status=scene_medic_syringe_child_create(owner,state==1?health:50,budget,&value);if(status)return status;}
    }
    value.prepared=1;*out=value;return RF_OK;
}
/* RFNC calls this on its private candidate BEFORE assigning any parent rows.
 * Parent retirement may unregister the source afterwards; final publication
 * relies on this preflight and permits that deliberate transition. */
static int scene_medic_syringe_restore_validate(const campaign_npc_body *owner,
    const scene_medic_syringe_candidate *candidate)
{
    const rf_object_link *link;uint32_t i;int status;
    if(!owner||!candidate||!candidate->prepared||candidate->parent!=&owner->registration||
       candidate->parent_slot!=(uint32_t)(owner-campaign_npc_bodies)||
       candidate->parent_handle!=owner->registration.handle||candidate->retired>1)return RF_FORMAT;
    status=scene_medic_syringe_lane_valid(candidate->state,candidate->health);if(status)return status;
    status=scene_medic_syringe_close_check(owner->syringe.child,&campaign_clutter_objects);if(status)return status;
    if(candidate->retired||candidate->state==2)return candidate->child?RF_FORMAT:RF_OK;
    if(!scene_medic_syringe_parent_live(owner)||!candidate->child||
       candidate->hand_tag<0||!(candidate->child->state.flags&0x4000u)||
       candidate->child->state.health!=(candidate->state==1?candidate->health:50.0f))return RF_FORMAT;
    status=scene_medic_syringe_close_check(candidate->child,&scene_medic_syringe_candidates);if(status)return status;
    link=scene_medic_syringe_candidates.sentinel.next;
    for(i=0;i<scene_medic_syringe_candidates.count&&i<RF_OBJECT_CAPACITY;i++){
        if(link==&candidate->child->object_link)return RF_OK;
        if(!link||link==&scene_medic_syringe_candidates.sentinel)return RF_FORMAT;
        link=link->next;
    }
    return RF_FORMAT;
}
/* Caller has completed every fallible world publish and final storage close.
 * Prepare validated both allocations; no load/current-timeline state is mixed.
 * A failed prepare/close path must discard its candidate and leave this unused. */
static void scene_medic_syringe_restore_publish(campaign_npc_body *owner,scene_medic_syringe_candidate *candidate)
{
    scene_medic_syringe value={0};int status;
    if(!owner||!candidate||!candidate->prepared||candidate->parent!=&owner->registration||
       candidate->parent_slot!=(uint32_t)(owner-campaign_npc_bodies)){rf_scene_medic_syringe[8]=RF_FORMAT;return;}
    status=scene_medic_syringe_child_close(&owner->syringe.child,&campaign_clutter_objects);
    if(status){owner->syringe.fault=status;rf_scene_medic_syringe[8]=(uint32_t)status;return;}
    value.child=candidate->child;value.damage=candidate->damage;value.parent=candidate->parent;
    value.parent_handle=candidate->parent_handle;value.parent_slot=candidate->parent_slot;
    value.state=candidate->state;value.health=candidate->health;value.hand_tag=candidate->hand_tag;
    value.unavailable=candidate->retired;
    if(value.child){rf_object_list_remove(&scene_medic_syringe_candidates,&value.child->object_link);rf_object_list_append(&campaign_clutter_objects,&value.child->object_link);}
    owner->syringe=value;memset(candidate,0,sizeof(*candidate));
    if(value.child)value.child->state.flags=(value.child->state.flags&~0x4000u)|(owner->object_flags&0x4000u);
}
static int scene_medic_syringe_open(campaign_npc_body *owner)
{
    scene_medic_syringe_candidate candidate={0};rf_entity_pose *pose=NULL;char held[2][RF_LEVEL_NAME_CAPACITY];
    uint32_t slot;int status;
    if(!scene_medic_syringe_qualified(owner))return RF_OK;
    slot=(uint32_t)(owner-campaign_npc_bodies);
    status=rf_level_entity_held_clutter_read(campaign_seeds.records.items+slot,held);if(status)return status;
    for(uint32_t hand=0;hand<2;hand++){const char *name=held[hand];if(rf_emitter_name_lookup(&name,1,"none")!=0)return RF_NOT_FOUND;}
    status=campaign_model_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    status=campaign_model_pose_ready(slot,pose);if(status)return status;
    status=scene_medic_syringe_restore_prepare(owner,0,0,0,64*1024,&candidate);if(status)return status;
    status=scene_medic_syringe_restore_validate(owner,&candidate);if(status){scene_medic_syringe_restore_discard(&candidate);return status;}
    scene_medic_syringe_restore_publish(owner,&candidate);return owner->syringe.fault;
}
/* Drop_Weapon4b9c7c deletes both held slots even when the actor has no gun.
 * Next scheduler-boundary resource close keeps same-frame hit receipts valid; state2 is the
 * permanent absent lane. Neither reserve exhaustion nor initial death calls it. */
static int scene_medic_syringe_delete_validate(const campaign_npc_body *owner)
{
    if(!scene_medic_syringe_qualified(owner))return RF_OK;
    if(owner->syringe.fault)return owner->syringe.fault;
    return owner->syringe.child&&!scene_medic_syringe_child_valid(owner->syringe.child)?RF_FORMAT:RF_OK;
}
static int scene_medic_syringe_delete(campaign_npc_body *owner)
{
    scene_medic_syringe *s;int status=scene_medic_syringe_delete_validate(owner);if(status)return status;
    if(!scene_medic_syringe_qualified(owner))return RF_OK;s=&owner->syringe;
    if(s->fault)return s->fault;
    if(s->child&&!scene_medic_syringe_child_valid(s->child))return RF_FORMAT;
    if(s->state!=2){++rf_scene_medic_syringe[3];s->state=2;s->health=0;}
    if(s->child){s->child->state.flags|=2u;s->damage.break_pending=0;}
    return RF_OK;
}
static int scene_medic_syringe_unavailable(campaign_npc_body *owner)
{
    uint32_t state;float health;int status;
    if(!scene_medic_syringe_qualified(owner))return RF_OK;
    status=scene_medic_syringe_capture(owner,&state,&health);if(status)return status;
    status=scene_medic_syringe_child_close(&owner->syringe.child,&campaign_clutter_objects);if(status)return status;
    owner->syringe.state=state;owner->syringe.health=health;owner->syringe.unavailable=1;return RF_OK;
}
static void scene_medic_syringe_visibility(campaign_npc_body *owner)
{
    scene_medic_syringe *s=&owner->syringe;
    if(scene_medic_syringe_child_valid(s->child))s->child->state.flags=(s->child->state.flags&~0x4000u)|(owner->object_flags&0x4000u);
}
static int scene_medic_syringe_tick(void)
{
    uint32_t i;int status;
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;scene_medic_syringe *s=&owner->syringe;
        float transform[12];uint32_t room,flags;
        if(!s->child||s->unavailable)continue;
        if(s->state==2){status=scene_medic_syringe_child_close(&s->child,&campaign_clutter_objects);if(status)return status;continue;}
        if(s->parent!=&owner->registration||s->parent_handle!=owner->registration.handle||!scene_medic_syringe_child_valid(s->child))return RF_FORMAT;
        {rf_entity_pose *pose=NULL;status=campaign_model_pose(i,&pose);if(status)return status;
         if(pose){status=campaign_model_pose_ready(i,pose);if(status)return status;}}
        status=scene_medic_syringe_parent_pose(owner,s->hand_tag,transform,&room,&flags);
        if(status==RF_NOT_FOUND){
            /* Explicit port resource policy: no available actor/corpse pose.
             * This is not evidence of original death-time syringe deletion. */
            status=scene_medic_syringe_unavailable(owner);if(status)return status;continue;
        }
        if(status)return status;
        status=scene_clutter_pose_publish(s->child,transform+9,transform,room);if(status)return status;
        s->child->state.flags=(s->child->state.flags&~0x4000u)|(flags&0x4000u);
        ++rf_scene_medic_syringe[1];
    }
    return RF_OK;
}
static int scene_medic_syringe_damage(uint32_t actor,float amount,int32_t kind)
{
    campaign_npc_body *owner;scene_medic_syringe *s;scene_clutter_damage_live_result result;int status;
    if(actor>=campaign_npc_body_count)return RF_RANGE;owner=campaign_npc_bodies+actor;s=&owner->syringe;
    if(s->unavailable||s->parent!=&owner->registration||s->parent_handle!=owner->registration.handle||
       !scene_medic_syringe_parent_live(owner)||!scene_medic_syringe_child_valid(s->child))return RF_NOT_FOUND;
    status=scene_clutter_damage_receive_live(&campaign_registry,s->child,
        campaign_clutter_damage_profiles+scene_medic_syringe_resource.class_index,&s->damage,amount,kind,0,&result);if(status)return status;
    if(result.applied){++rf_scene_medic_syringe[2];s->state=result.retired?2:1;s->health=result.retired?0:s->child->state.health;}
    if(result.retired){++rf_scene_medic_syringe[3];s->damage.break_pending=0;}
    return RF_OK;
}
static rf_clutter_base_owner *scene_medic_syringe_child_at(uint32_t actor)
{
    scene_medic_syringe *s;
    if(actor>=campaign_npc_body_count)return NULL;s=&campaign_npc_bodies[actor].syringe;
    if(s->unavailable||s->parent!=&campaign_npc_bodies[actor].registration||s->parent_slot!=actor||
       s->parent_handle!=campaign_npc_bodies[actor].registration.handle||
       !scene_medic_syringe_parent_live(campaign_npc_bodies+actor)||!scene_medic_syringe_child_valid(s->child))return NULL;
    return s->child;
}
static int scene_medic_syringe_children_close(void)
{
    uint32_t i;int status;
    for(i=0;i<campaign_npc_body_count;i++){
        status=scene_medic_syringe_child_close(&campaign_npc_bodies[i].syringe.child,&campaign_clutter_objects);if(status)return status;
    }
    return scene_medic_syringe_candidates.count?RF_RANGE:RF_OK;
}
static void scene_medic_syringe_resources_close(void)
{
    if(scene_medic_syringe_resource.shared.references||scene_medic_syringe_resource.body_bytes||scene_medic_syringe_candidates.count){rf_scene_medic_syringe[8]=RF_RANGE;return;}
    rf_model_materials_close(&scene_medic_syringe_resource.materials);
    rf_model_collision_resource_close(&scene_medic_syringe_resource.model.collision);
    rf_static_render_resource_close(&scene_medic_syringe_resource.model.resource);
    memset(&scene_medic_syringe_resource,0,sizeof(scene_medic_syringe_resource));
}
static int scene_medic_syringe_resources_open(rf_vpp *models,rf_vpp *maps,uint32_t map_count)
{
    const uint32_t budget=256*1024;rf_model_file *file=NULL;rf_clutter_class *cls;
    uint32_t i,found=0;uint64_t bytes=sizeof(scene_medic_syringe_resource),peak;int status;
    if(scene_medic_syringe_resource.ready)return RF_RANGE;
    for(i=0;i<campaign_seeds.records.count;i++){
        const char *name=campaign_seeds.records.items[i].record.class_name;
        if(rf_emitter_name_lookup(&name,1,"medic1")==0){found=1;break;}
    }
    if(!found)return RF_OK;
    rf_object_list_init(&scene_medic_syringe_candidates);
    for(i=0;i<campaign_clutter_classes.count;i++)if(rf_emitter_name_lookup(&campaign_clutter_classes.items[i].name,1,"syringe")==0)break;
    if(i==campaign_clutter_classes.count||!campaign_clutter_damage_profiles)return RF_NOT_FOUND;
    cls=campaign_clutter_classes.items+i;
    if(cls->model_kind!=1||cls->life!=50||cls->material!=rf_clutter_material_index("metal")||(cls->flags&0x1ffu)!=0x80u||
       cls->material>=10||cls->emitter_count||cls->sound!=-1||cls->explosion!=-1||cls->glare!=-1||cls->rod!=-1)return RF_FORMAT;
    scene_medic_syringe_resource.class_index=i;
    status=rf_model_compiled_filename(cls->model,scene_medic_syringe_resource.model.name,".v3m");if(status)goto fail;
    file=malloc(sizeof(*file));if(!file){status=RF_IO;goto fail;}bytes+=sizeof(*file);
    if(bytes>=budget){status=RF_RANGE;goto fail;}
    status=rf_model_file_open(file,models,scene_medic_syringe_resource.model.name);if(status)goto fail;
    status=rf_static_render_resource_open(file,(uint32_t)(budget-bytes),&scene_medic_syringe_resource.model.resource);if(status)goto fail;
    bytes+=scene_medic_syringe_resource.model.resource.allocated_bytes;
    if(bytes>=budget){status=RF_RANGE;goto fail;}
    status=rf_model_collision_resource_open(&scene_medic_syringe_resource.model.collision,file,(uint32_t)(budget-bytes));if(status)goto fail;
    bytes+=scene_medic_syringe_resource.model.collision.accounted_bytes;if(bytes>=budget){status=RF_RANGE;goto fail;}
    status=rf_model_materials_open(&scene_medic_syringe_resource.materials,file,maps,map_count,(uint32_t)(budget-bytes));if(status)goto fail;
    peak=bytes+scene_medic_syringe_resource.materials.peak_bytes;
    if(peak>budget){status=RF_RANGE;goto fail;}
    bytes+=scene_medic_syringe_resource.materials.resident_bytes;bytes-=sizeof(*file);
    scene_medic_syringe_resource.shared.filename=scene_medic_syringe_resource.model.name;
    scene_medic_syringe_resource.shared.resource=&scene_medic_syringe_resource.model.resource;
    scene_medic_syringe_resource.ready=1;scene_medic_syringe_resource.bytes=(uint32_t)bytes;
    scene_medic_syringe_resource.peak=(uint32_t)peak;rf_scene_medic_syringe[6]=(uint32_t)bytes;rf_scene_medic_syringe[7]=(uint32_t)peak;
    free(file);return RF_OK;
fail:free(file);scene_medic_syringe_resources_close();return status;
}
#endif
