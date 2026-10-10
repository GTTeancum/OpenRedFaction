#ifndef SCENE_VEHICLE_AI_INCLUDED
#define SCENE_VEHICLE_AI_INCLUDED
/* Source chain: authored464996/4270a0 ->403a80/401de0 ->408ac0/action3
 * ->401580 primary +406330 secondary. Goto_Player action10 hands back only
 * after opposition/FOV/LOS. Shared geometry/ownership is real; steering and
 * homing remain bounded port policies. No fake skeletal vehicle actor. */
typedef struct scene_vehicle_ai_state {
    struct scene_vehicle_ai_state *next;
    uint32_t uid,handle,profile,target,target_kind,target_uid,engaged,scan_ticks;
    scene_vehicle_primary_state primary_fire;float missile_cooldown,unseen_seconds;
    int32_t primary_reserve,missile_reserve;
    scene_submarine_weapon_round bullets[48],missiles[4];
} scene_vehicle_ai_state;
typedef struct scene_vehicle_ai_owner {
    uint32_t uid,handle,profile,selected,allowed,alive,affiliation,slot,mask;
    const float *position,*basis;const scene_driller_resources *resource;
    rf_level_entity_ai authored;int32_t weapons[2];
} scene_vehicle_ai_owner;
static scene_vehicle_ai_state *campaign_vehicle_ai;
static scene_stream *scene_vehicle_ai_stream;
static scene_vehicle_primary_definition scene_vehicle_ai_minigun,scene_vehicle_ai_apc;
static scene_submarine_weapon_definition scene_vehicle_ai_missile[3];
static float scene_vehicle_ai_range[3],scene_vehicle_ai_blind_seconds[4];
static uint32_t scene_vehicle_ai_ready;
static scene_driller_resources *scene_vehicle_ai_torpedo;
static uint32_t scene_vehicle_ai_torpedo_base,scene_vehicle_ai_torpedo_textures;
static uint32_t scene_vehicle_ai_kind(uint32_t profile){return profile==2?2:profile==4?0:1;}
static uint32_t scene_vehicle_ai_blind_index(uint32_t profile){return profile==2?3:profile-4;}
static const scene_vehicle_primary_definition *scene_vehicle_ai_gun(uint32_t profile)
{return profile==2?&scene_vehicle_ai_apc:&scene_vehicle_ai_minigun;}
uint32_t rf_scene_vehicle_ai[8]; /* acquisitions,handoffs,primary,missiles,contacts,UID,target,status */
static scene_vehicle_ai_state *scene_vehicle_ai_find(uint32_t handle)
{for(scene_vehicle_ai_state *a=campaign_vehicle_ai;a;a=a->next)if(a->handle==handle)return a;return NULL;}
static uint32_t scene_vehicle_ai_primary_supply(uint32_t handle,int32_t *reserve)
{scene_vehicle_ai_state *a=scene_vehicle_ai_find(handle);if(!a)return 0;*reserve=a->primary_reserve;return 1;}
static uint32_t scene_vehicle_ai_has_state(void){return campaign_vehicle_ai!=NULL;}
static void scene_vehicle_ai_cancel(uint32_t handle)
{
    scene_vehicle_loop_audio_stop(handle,SCENE_VEHICLE_LOOP_AI);
    scene_vehicle_ai_state *a=scene_vehicle_ai_find(handle);if(!a)return;
    a->engaged=0;a->target=UINT32_MAX;a->target_kind=a->target_uid=0;a->unseen_seconds=0;
    a->primary_fire.held=0;a->primary_fire.warmup=0;
    /* Emitted rounds keep their source and flight; lost guidance coasts. */
}
static uint32_t scene_vehicle_ai_busy(uint32_t handle)
{
    const scene_vehicle_ai_state *a=scene_vehicle_ai_find(handle);if(!a)return 0;
    if(a->engaged)return 1;
    for(uint32_t i=0;i<48;i++)if(a->bullets[i].flight.active)return 1;
    for(uint32_t i=0;i<4;i++)if(a->missiles[i].flight.active)return 1;
    return 0;
}
static void scene_vehicle_ai_states_close(scene_vehicle_ai_state **head)
{
    /* Staged checkpoint candidates do not own live audio. */
    if(head==&campaign_vehicle_ai)scene_vehicle_loop_audio_stop_controller(SCENE_VEHICLE_LOOP_AI);
    while(*head){scene_vehicle_ai_state *next=(*head)->next;free(*head);*head=next;}
}
static void scene_vehicle_ai_close(void)
{scene_vehicle_ai_states_close(&campaign_vehicle_ai);scene_driller_resources_close(&scene_vehicle_ai_torpedo);scene_vehicle_ai_ready=0;scene_vehicle_ai_stream=NULL;}
static int scene_vehicle_ai_owner_read(scene_stream *s,uint32_t uid,scene_vehicle_ai_owner *out)
{
    scene_vehicle_ai_owner v={0};uint32_t count=0,cls;int32_t weapons[2];int status;
    if(!s||!out)return RF_RANGE;
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){v.slot=i;++count;}
    if(count!=1)return RF_NOT_FOUND;
    const rf_level_owned_entity *record=campaign_seeds.records.items+v.slot;
    v.profile=!strcmp(record->record.class_name,"APC")?2:!strcmp(record->record.class_name,"sub")?4:!strcmp(record->record.class_name,"Fighter01")?5:
        !strcmp(record->record.class_name,"masako_fighter")?6:0;
    if(!v.profile)return RF_NOT_FOUND;
    status=rf_level_entity_ai_read(record,&v.authored);if(status)return status;
    cls=campaign_seeds.items[v.slot].class_index;if(cls>=campaign_seeds.class_count)return RF_FORMAT;
    if(!v.authored.enabled&&!(campaign_seeds.classes[cls].physics.flags&0x10u))return RF_NOT_FOUND;
    status=scene_vehicle_loadout_resolve(uid,v.profile,weapons,&v.mask);if(status)return status;
    memcpy(v.weapons,weapons,sizeof(weapons));v.uid=uid;
    if(uid==(uint32_t)campaign_authored_vehicle_uid){
        scene_driller_runtime *r=s->driller_runtime;
        if(!r||rf_scene_vehicle_enabled!=v.profile||!scene_driller_damage_runtime_valid(&r->damage)||
           rf_object_registry_lookup(&campaign_registry,r->entry.host.handle)!=&r->entry.registration)return RF_NOT_FOUND;
        v.selected=1;v.handle=r->entry.host.handle;v.resource=s->driller;
        v.position=r->physics.state.position;v.basis=r->physics.state.orientation;
        v.affiliation=r->damage.damage.state.effects.affiliation;
        v.alive=r->entry.host.alive&&!r->damage.damage.destroyed&&r->damage.damage.state.effects.health>0&&!(r->damage.damage.object_flags&2u);
        v.allowed=v.alive&&scene_vehicle_physics_allows_control(s)&&!(r->damage.damage.object_flags&0x4000u)&&
            !r->entry.session.active&&r->entry.host.driver==UINT32_MAX&&r->entry.occupant==-1;
    }else{
        scene_passive_vehicle *p=NULL;
        for(uint32_t i=0;i<campaign_passive_vehicle_count;i++)if(campaign_passive_vehicles[i].uid==uid){if(p)return RF_FORMAT;p=campaign_passive_vehicles+i;}
        if(!p||p->object_kind!=11||scene_secondary_vehicle_profile(p->resource_kind)!=v.profile||
           rf_object_registry_lookup(&campaign_registry,p->handle)!=p)return RF_NOT_FOUND;
        v.handle=p->handle;v.resource=scene_secondary_vehicle_resource(s,v.profile);
        if(!v.resource)return RF_NOT_FOUND;
        v.position=p->pose.position;v.basis=p->pose.input_matrix;v.affiliation=p->damage.state.effects.affiliation;
        v.alive=!p->damage.destroyed&&p->damage.state.effects.health>0&&!(p->damage.object_flags&2u);
        v.allowed=v.alive&&scene_secondary_vehicle_empty(p)&&
            !(p->damage.object_flags&0x4000u)&&(!p->scripted_physics||(p->physics_body_flags&0x80000000u));
    }
    if(!v.resource)return RF_NOT_FOUND;*out=v;return RF_OK;
}
static uint32_t scene_vehicle_ai_opposed(uint32_t source,uint32_t target)
{
    static const uint8_t matrix[16]={0,0,1,1,0,0,0,0,1,0,0,1,1,1,1,1};
    return source<4&&target<4?matrix[source*4+target]:0;
}
static uint32_t scene_vehicle_ai_target_affiliation(uint32_t handle,uint32_t *affiliation)
{
    if(handle==campaign_player_object.handle){*affiliation=campaign_player_damage.state.effects.affiliation;return 1;}
    rf_damage_effect_state *v=scene_vehicle_allegiance_owner(handle);if(v){*affiliation=v->affiliation;return 1;}
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==handle){*affiliation=campaign_npc_bodies[i].damage.effects.affiliation;return 1;}
    return 0;
}
static int scene_vehicle_ai_sight(scene_stream *s,uint32_t source,uint32_t target,uint32_t hull,
    const float start[3],const float end[3],uint32_t *clear)
{
    scene_apc_primary_query query={s,source,UINT32_MAX};rf_weapon_flight_contact hit;float delta[3];uint32_t found;int status;
    for(uint32_t i=0;i<3;i++)delta[i]=end[i]-start[i];
    status=scene_apc_primary_sweep(&query,start,delta,0,&hit,&found);if(status)return status;
    *clear=!found||scene_vehicle_attack_contact_target(&hit,target,hull);return RF_OK;
}
static int scene_vehicle_ai_visible(scene_stream *s,const scene_vehicle_ai_owner *owner,uint32_t target,
    uint32_t fov,float point[3],float *distance,uint32_t *visible)
{
    float delta[3],dot=0,length=0;uint32_t hull,affiliation;int status;
    *visible=0;
    if(target==owner->handle||!scene_vehicle_attack_target(target,point,&hull)||
       !scene_vehicle_ai_target_affiliation(target,&affiliation)||!scene_vehicle_ai_opposed(owner->affiliation,affiliation))return RF_OK;
    /* The occupied hull, not its hidden rider, is the physical aim target. */
    if(hull!=UINT32_MAX){float health;scene_campaign_vehicle_target(s,hull,point,&health);}
    for(uint32_t i=0;i<3;i++){delta[i]=point[i]-owner->position[i];length+=delta[i]*delta[i];dot+=delta[i]*owner->basis[6+i];}
    length=sqrtf(length);if(!isfinite(length)||length<.001f)return RF_OK;*distance=length;
    if(fov){float cosine=cosf((float)owner->authored.fov_degrees*.01745329238474369f*.5f);
        if(dot/length<cosine)return RF_OK;}
    status=scene_vehicle_ai_sight(s,owner->handle,target,hull,owner->position,point,visible);return status;
}
static int scene_vehicle_ai_open(scene_stream *s,rf_vpp *tables,rf_vpp *meshes,rf_vpp *maps,uint32_t map_count)
{
    rf_weapon_primary_definition ai;uint32_t sub=0,fighter=0,apc=0,blind_loaded=0;int status;
    scene_vehicle_ai_stream=s;
    memset(scene_vehicle_ai_blind_seconds,0,sizeof(scene_vehicle_ai_blind_seconds));
    for(uint32_t i=0;i<campaign_seeds.records.count;i++){
        const char *name=campaign_seeds.records.items[i].record.class_name;
        apc+=!strcmp(name,"APC");sub+=!strcmp(name,"sub");fighter+=!strcmp(name,"Fighter01")||!strcmp(name,"masako_fighter");
        uint32_t profile=!strcmp(name,"APC")?2:!strcmp(name,"sub")?4:!strcmp(name,"Fighter01")?5:!strcmp(name,"masako_fighter")?6:0;
        if(profile){uint32_t index=scene_vehicle_ai_blind_index(profile);
            if(!(blind_loaded&(1u<<index))){status=rf_entity_blind_pursuit_load(tables,name,512*1024,&scene_vehicle_ai_blind_seconds[index]);if(status)return status;blind_loaded|=1u<<index;}}
    }
    if(apc){
        status=scene_vehicle_primary_load(tables,"APC Minigun",512*1024,&scene_vehicle_ai_apc);if(status)return status;
        status=rf_weapon_primary_load(tables,"APC Minigun",512*1024,&ai);if(status)return status;
        scene_vehicle_ai_apc.damage*=ai.ai_damage_scale[0];scene_vehicle_ai_range[2]=ai.ai_attack_range;
    }
    if(sub){
        rf_vpp_entry entry;void *text;
        status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
        if(!entry.size||entry.size>512*1024)return RF_RANGE;text=malloc(entry.size);if(!text)return RF_IO;
        status=rf_vpp_read(tables,&entry,0,text,entry.size);
        if(!status)status=scene_submarine_weapon_read(text,entry.size,&scene_vehicle_ai_missile[0]);free(text);if(status)return status;
        status=rf_weapon_primary_load(tables,"Torpedo",512*1024,&ai);if(status)return status;
        scene_vehicle_ai_missile[0].damage*=ai.ai_damage_scale[0];scene_vehicle_ai_range[0]=ai.ai_attack_range;
    }
    if(fighter){
        rf_vpp_entry entry;void *text;
        status=scene_vehicle_primary_load(tables,"Fighter Minigun",512*1024,&scene_vehicle_ai_minigun);if(status)return status;
        status=rf_weapon_primary_load(tables,"Fighter Minigun",512*1024,&ai);if(status)return status;
        scene_vehicle_ai_minigun.damage*=ai.ai_damage_scale[0];
        status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status;
        if(!entry.size||entry.size>512*1024)return RF_RANGE;text=malloc(entry.size);if(!text)return RF_IO;
        status=rf_vpp_read(tables,&entry,0,text,entry.size);
        if(!status)status=scene_fighter_rocket_read(text,entry.size,&scene_vehicle_ai_missile[1]);free(text);if(status)return status;
        status=rf_weapon_primary_load(tables,"Fighter Rocket",512*1024,&ai);if(status)return status;
        scene_vehicle_ai_missile[1].damage*=ai.ai_damage_scale[0];scene_vehicle_ai_range[1]=ai.ai_attack_range;
    }
    if(sub&&!s->submarine_torpedo){
        status=scene_submarine_weapon_model_open(meshes,maps,map_count,512*1024,&scene_vehicle_ai_torpedo);if(status)return status;
    }
    /* The shared impact image owner is demanded by all ready vehicle classes,
     * including an APC-only section with no handheld explosive resources.
     * Load its definition under that same demand; otherwise resolved remains
     * zero and the bounded material loader rejects it before allocating. */
    if(sub||fighter||apc){status=scene_rocket_definitions_open(tables);if(status)return status;}
    scene_vehicle_ai_ready=!!(sub||fighter||apc);return RF_OK;
}
static int scene_vehicle_ai_launch_bullet(void *context,uint32_t source,const scene_vehicle_primary_definition *definition,
    const float muzzle[12],const float direction[3],uint32_t *accepted)
{
    scene_vehicle_ai_state *a=context;scene_submarine_weapon_round value={0};uint32_t slot;int status;
    *accepted=0;for(slot=0;slot<48&&a->bullets[slot].flight.active;slot++){}if(slot==48)return RF_OK;
    status=rf_weapon_flight_launch(&value.flight,muzzle+9,direction,definition->speed,definition->lifetime,definition->radius);if(status)return status;
    value.source=source;value.driver=UINT32_MAX;memcpy(value.previous,muzzle+9,12);a->bullets[slot]=value;*accepted=1;return RF_OK;
}
static int scene_vehicle_ai_homing(void *context,uint32_t index,scene_submarine_homing_candidate *out)
{
    scene_vehicle_ai_state *a=context;scene_vehicle_ai_owner owner;float point[3];uint32_t hull,affiliation,wet=1;int status;
    (void)index;
    if(a->target==UINT32_MAX||!scene_vehicle_attack_target(a->target,point,&hull)||
       scene_vehicle_ai_owner_read(scene_vehicle_ai_stream,a->uid,&owner)||
       !scene_vehicle_ai_target_affiliation(a->target,&affiliation)||!scene_vehicle_ai_opposed(owner.affiliation,affiliation))return RF_OK;
    if(hull!=UINT32_MAX){float health;scene_campaign_vehicle_target(scene_vehicle_ai_stream,hull,point,&health);}
    if(a->profile==4){status=scene_submarine_weapon_muzzle_wet(scene_vehicle_ai_stream,point,&wet);if(status)return status;}
    out->handle=hull!=UINT32_MAX?hull:a->target;out->alive=out->hostile=1;out->submerged=wet;memcpy(out->position,point,12);return RF_OK;
}
static int scene_vehicle_ai_flights(scene_stream *s,scene_vehicle_ai_state *a,uint32_t frame)
{
    const scene_submarine_weapon_definition *definition=&scene_vehicle_ai_missile[scene_vehicle_ai_kind(a->profile)];int status;
    for(uint32_t slot=0;slot<52;slot++){
        uint32_t missile=slot>=48;scene_submarine_weapon_round *r=missile?a->missiles+slot-48:a->bullets+slot;
        scene_apc_primary_query query={s,r->source,r->driver};rf_weapon_flight_event event={0};float end[3];
        if(!r->flight.active)continue;
        if(missile){
            uint32_t target;rf_weapon_flight_liquid_event liquid;scene_submarine_projectile_query wet={s->collision,scene_apc_primary_sweep_all,&query};
            status=scene_submarine_homing_scan(r,definition,scene_step_seconds,scene_vehicle_ai_homing,a,1,scene_submarine_homing_sight,s,&target);if(status)return status;
            memcpy(r->previous,r->flight.position,12);
            for(uint32_t k=0;k<3;k++)end[k]=r->flight.position[k]+r->flight.velocity[k]*fminf(scene_step_seconds,(float)r->flight.remaining);
            if(a->profile==4)status=scene_submarine_projectile_step(r,scene_step_seconds,&wet,&r->liquid,0,-1,-1,&liquid);
            else{const rf_weapon_flight_liquid_policy policy={0,0,-1,-1};status=rf_weapon_flight_step_liquid(&r->flight,scene_step_seconds,&r->liquid,&policy,scene_apc_primary_sweep_all,&query,&liquid);}
            if(status)return status;event=liquid.terminal;
        }else{
            memcpy(r->previous,r->flight.position,12);
            for(uint32_t k=0;k<3;k++)end[k]=r->flight.position[k]+r->flight.velocity[k]*fminf(scene_step_seconds,(float)r->flight.remaining);
            status=rf_weapon_flight_step(&r->flight,scene_step_seconds,scene_apc_primary_sweep,&query,&event);if(status)return status;
        }
        if(event.kind==1){
            scene_apc_primary_round direct={0};direct.source=r->source;direct.driver=r->driver;
            direct.damage=missile?definition->damage:scene_vehicle_ai_gun(a->profile)->damage;direct.kind=missile?3:2;
            status=scene_apc_primary_damage(s,frame,&direct,&event.contact,r->previous,end);if(status)return status;++rf_scene_vehicle_ai[4];
            if(missile){float origin[3];for(uint32_t k=0;k<3;k++)origin[k]=event.contact.hit.point[k]+event.contact.hit.normal[k]*.01f;
                status=scene_impact_start_sized(s,&event.contact,frame,a->profile==4?2:1.5f);if(status)return status;
                scene_impact_sound(origin,frame);
                status=scene_explosion_blast_source(s,frame,origin,definition->damage,definition->blast,r->source,3);if(status)return status;
                status=scene_explosion_terrain(s,frame,&event.contact,definition->crater);if(status)return status;
            }
        }
    }
    return RF_OK;
}
static uint32_t scene_vehicle_ai_source_admits(uint32_t uid)
{
    for(uint32_t i=0;i<campaign_seeds.records.count;i++)if((uint32_t)campaign_seeds.records.items[i].record.uid==uid){
        const rf_level_owned_entity *r=campaign_seeds.records.items+i;rf_level_entity_ai ai;
        if(strcmp(r->record.class_name,"APC")&&strcmp(r->record.class_name,"sub")&&strcmp(r->record.class_name,"Fighter01")&&strcmp(r->record.class_name,"masako_fighter"))return 0;
        return !rf_level_entity_ai_read(r,&ai)&&ai.enabled&&ai.mode==1;
    }
    return 0;
}
static int scene_vehicle_ai_claim(scene_stream *s,const scene_vehicle_ai_owner *owner)
{
    scene_secondary_vehicle_runtime *r;uint32_t slot,allowed=0;int status;
    if(owner->selected||scene_secondary_vehicle_find(owner->handle))return RF_OK;
    if(scene_vehicle_switch_parked_uid(owner->uid))return RF_NOT_FOUND;
    for(slot=0;slot<campaign_passive_vehicle_count&&campaign_passive_vehicles[slot].handle!=owner->handle;slot++){}
    if(slot==campaign_passive_vehicle_count)return RF_NOT_FOUND;
    r=calloc(1,sizeof(*r));if(!r)return RF_IO;
    status=scene_secondary_vehicle_prepare(s,slot,0,r);if(status){free(r);return status;}
    if(scene_secondary_vehicle_flight_profile(r->profile)){
        if(r->profile==4)status=scene_submarine_water_body_path(&r->submarine,r->physics.collision,r->physics.sphere_count,
            r->physics.state.position,r->physics.state.orientation,r->physics.state.position,r->physics.state.orientation,&allowed);
        else status=scene_fighter_dry_body_path(&r->fighter,r->physics.collision,r->physics.sphere_count,
            r->physics.state.position,r->physics.state.orientation,r->physics.state.position,r->physics.state.orientation,&allowed);
        if(status||!allowed){free(r);return status?status:RF_NOT_FOUND;}
    }
    if(!campaign_secondary_vehicles){
        campaign_secondary_vehicles=calloc(campaign_passive_vehicle_count,sizeof(*campaign_secondary_vehicles));
        if(!campaign_secondary_vehicles){free(r);return RF_IO;}campaign_secondary_vehicle_capacity=campaign_passive_vehicle_count;
    }
    if(slot>=campaign_secondary_vehicle_capacity||campaign_secondary_vehicles[slot]){free(r);return RF_FORMAT;}
    campaign_secondary_vehicles[slot]=r;++campaign_secondary_vehicle_count;
    campaign_passive_vehicles[slot].physics_body_flags=r->physics.body_flags;
    campaign_passive_vehicles[slot].scripted_physics=r->scripted_physics;
    scene_vehicle_owner_collision_bind(s,1);return RF_OK;
}
static int scene_vehicle_ai_acquire(scene_stream *s,scene_vehicle_ai_state *a,const scene_vehicle_ai_owner *owner)
{
    uint32_t best=UINT32_MAX;float closest=scene_vehicle_ai_range[scene_vehicle_ai_kind(a->profile)],point[3];int status;
    /* Practical deterministic array order in place of the original linked list.
     * Same opposition/FOV/covered nearest-target admission; no player shortcut. */
    for(uint32_t i=0;i<1+campaign_npc_body_count+campaign_passive_vehicle_count;i++){
        uint32_t handle=i==0?campaign_player_object.handle:i<=campaign_npc_body_count?
            campaign_npc_bodies[i-1].registration.handle:campaign_passive_vehicles[i-1-campaign_npc_body_count].handle;
        uint32_t visible;float distance=0;
        status=scene_vehicle_ai_visible(s,owner,handle,1,point,&distance,&visible);if(status)return status;
        if(visible&&distance<closest){closest=distance;best=handle;}
    }
    if(best==UINT32_MAX)return RF_OK;
    status=scene_vehicle_ai_claim(s,owner);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=scene_vehicle_attack_target_key(best,&a->target_kind,&a->target_uid);if(status)return status;
    a->target=best;a->engaged=1;a->unseen_seconds=0;++rf_scene_vehicle_ai[0];return RF_OK;
}
static int scene_vehicle_ai_secondary_sight(void *context,const float start[3],const float end[3],uint32_t *blocked)
{
    scene_vehicle_ai_state *a=context;uint32_t clear,hull;float unused[3];
    if(!scene_vehicle_attack_target(a->target,unused,&hull)){*blocked=1;return RF_OK;}
    int status=scene_vehicle_ai_sight(scene_vehicle_ai_stream,a->handle,a->target,hull,start,end,&clear);
    if(!status)*blocked=!clear;return status;
}
static int scene_vehicle_ai_fire(scene_stream *s,scene_vehicle_ai_state *a,const scene_vehicle_ai_owner *owner,uint32_t frame)
{
    const scene_submarine_weapon_definition *definition=&scene_vehicle_ai_missile[scene_vehicle_ai_kind(a->profile)];
    float point[3],distance=0,muzzle[12],delta[3],dot=0,length=0;uint32_t visible=0,fired=0;int32_t tag=-1;int status;
    a->missile_cooldown=fmaxf(0,a->missile_cooldown-scene_step_seconds);
    if(a->engaged){status=scene_vehicle_ai_visible(s,owner,a->target,0,point,&distance,&visible);if(status)return status;}
    if(!visible||!owner->allowed){a->primary_fire.cooldown=fmaxf(-scene_step_seconds,a->primary_fire.cooldown-scene_step_seconds);a->primary_fire.held=0;a->primary_fire.warmup=0;
        scene_vehicle_loop_audio_stop(a->handle,SCENE_VEHICLE_LOOP_AI);return RF_OK;}
    status=rf_static_model_tags_find(&owner->resource->tags,a->profile==4?(rf_model_name){"primary_1",9}:(rf_model_name){"muzzle_1",8},&tag);if(status)return status;
    status=rf_static_model_tag_place(&owner->resource->tags,tag,owner->basis,owner->position,muzzle);if(status)return status;
    for(uint32_t i=0;i<3;i++){delta[i]=point[i]-muzzle[9+i];length+=delta[i]*delta[i];dot+=delta[i]*muzzle[6+i];}
    length=sqrtf(length);if(!isfinite(length)||length<.001f){scene_vehicle_loop_audio_stop(a->handle,SCENE_VEHICLE_LOOP_AI);return RF_OK;}
    uint32_t primary=distance<=scene_vehicle_ai_range[scene_vehicle_ai_kind(a->profile)]&&dot/length>=.9f;
    if(a->profile!=4){
        status=scene_vehicle_primary_tick(&a->primary_fire,scene_vehicle_ai_gun(a->profile),&owner->resource->tags,tag,
            owner->position,owner->basis,a->profile==2?delta:NULL,a->handle,primary,!!(owner->mask&1u),scene_step_seconds,
            &a->primary_reserve,scene_vehicle_ai_launch_bullet,a,&fired);if(status)return status;
        if(fired)++rf_scene_vehicle_ai[2];
        scene_vehicle_loop_audio_tick(a->handle,SCENE_VEHICLE_LOOP_AI,owner->profile,
            a->primary_fire.held&&(a->primary_reserve>0||fired),fired,muzzle+9);
    }
    if(a->profile==2)return RF_OK; /* Actual AI APC26/9627 secondary is none. */
    uint32_t missile=a->profile==4?primary&&!!(owner->mask&1u):0;
    if(a->profile!=4&&(owner->mask&2u)){
        rf_weapon_ai_secondary_input in={0};uint32_t admitted;
        in.now_ms=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);in.deadline=in.now_ms;
        in.weapon=owner->weapons[1];in.global_enabled=1;in.ai_flags_530=1;in.target_present=1;in.visible_29c=visible;
        memcpy(in.source,owner->position,12);memcpy(in.target,point,12);memcpy(in.eye,muzzle+9,12);memcpy(in.forward,owner->basis+6,12);
        in.maximum_range=scene_vehicle_ai_range[1];in.minimum_range=definition->blast;
        /* Actual compound hull extent, rather than a fabricated actor view. */
        scene_secondary_vehicle_runtime *motion=scene_secondary_vehicle_find(owner->handle);
        in.source_radius=owner->selected?s->driller_runtime->physics.radius:motion?motion->physics.radius:0;
        if(a->target==campaign_player_object.handle)in.target_radius=scene_actor_body.state.bounds.radius;
        else {scene_secondary_vehicle_runtime *other=scene_secondary_vehicle_find(a->target);
            if(other)in.target_radius=other->physics.radius;
            else for(uint32_t i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.handle==a->target)
                in.target_radius=campaign_npc_bodies[i].body.state.bounds.radius;}
        if(scene_driller_active(s)&&a->target==campaign_player_object.handle)in.target_radius=s->driller_runtime->physics.radius;
        status=rf_weapon_ai_secondary_admit(&in,scene_vehicle_ai_secondary_sight,a,&admitted);if(status)return status;missile=admitted;
    }
    if(!missile||a->missile_cooldown>1.e-6f||a->missile_reserve<=0)return RF_OK;
    if(a->profile!=4){
        rf_weapon_secondary_request_state request_state={0};rf_weapon_secondary_request request={0};rf_weapon_secondary_admission admission;
        request.now_ms=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);request_state.deadline_4bc=request.now_ms;
        request.weapon=owner->weapons[1];request.available_ammo=a->missile_reserve;request.owned=!!(owner->mask&2u);
        /* Installed Fighter Rocket has no scheduled-fire flag. The common
         * original admission still owns death/weapon/ownership/ammo refusal. */
        request.dying_al=!owner->alive;
        status=rf_weapon_secondary_request_admit(&request_state,&request,&admission);if(status)return status;
        if(admission.action!=RF_WEAPON_SECONDARY_DIRECT)return RF_OK;
    }
    uint32_t slot;for(slot=0;slot<4&&a->missiles[slot].flight.active;slot++){}if(slot==4)return RF_OK;
    float launch[12];memcpy(launch,muzzle,sizeof(launch));
    if(a->profile!=4){
        int32_t rocket_tag=-1;status=rf_static_model_tags_find(&owner->resource->tags,(rf_model_name){"secondary_1",11},&rocket_tag);if(status)return status;
        status=rf_static_model_tag_place(&owner->resource->tags,rocket_tag,owner->basis,owner->position,launch);if(status)return status;
    }else{uint32_t wet;status=scene_submarine_weapon_muzzle_wet(s,launch+9,&wet);if(status)return status;if(!wet)return RF_OK;}
    scene_submarine_weapon_round round={0};
    status=rf_weapon_flight_launch(&round.flight,launch+9,muzzle+6,definition->speed,definition->lifetime,definition->radius);if(status)return status;
    round.source=a->handle;round.driver=UINT32_MAX;round.liquid.query_flags=0x1004;memcpy(round.previous,launch+9,12);
    a->missiles[slot]=round;--a->missile_reserve;a->missile_cooldown=definition->fire_seconds;++rf_scene_vehicle_ai[3];
    combat_sound(a->profile==4?"TorpedoLaunch":"Fighter Rocket Launch",launch+9);return RF_OK;
}
static int scene_vehicle_ai_tick(scene_stream *s,uint32_t frame)
{
    if(!frame||!scene_vehicle_ai_ready)return RF_OK;
    /* Lazy owner bank. Authored AI and default mode are separate from relation.
     * A script order suppresses default combat until it completes/hands back. */
    for(uint32_t i=0;i<campaign_seeds.records.count;i++){
        uint32_t uid=(uint32_t)campaign_seeds.records.items[i].record.uid;scene_vehicle_ai_owner owner;int status;
        if(!scene_vehicle_ai_source_admits(uid))continue;
        status=scene_vehicle_ai_owner_read(s,uid,&owner);if(status==RF_NOT_FOUND)continue;if(status)return status;
        scene_vehicle_ai_state *a=scene_vehicle_ai_find(owner.handle);
        if(!a){
            if(!owner.allowed||!owner.mask||scene_vehicle_switch_parked_uid(uid))continue;
            a=calloc(1,sizeof(*a));if(!a)return RF_IO;a->uid=uid;a->handle=owner.handle;a->profile=owner.profile;a->target=UINT32_MAX;
            a->primary_reserve=owner.profile==4?0:(int32_t)scene_vehicle_ai_gun(owner.profile)->capacity;
            a->missile_reserve=(int32_t)scene_vehicle_ai_missile[scene_vehicle_ai_kind(owner.profile)].capacity;
            (void)scene_vehicle_switch_supply_seed(uid,owner.profile,&a->primary_reserve,&a->missile_reserve,&a->primary_fire,&a->missile_cooldown);
            if(owner.profile==2){a->missile_reserve=0;a->missile_cooldown=0;} /* Dormant APC mortar supply stays in its existing owner. */
            if(owner.selected){
                if(owner.profile==2){a->primary_reserve=s->apc_primary.reserve;a->primary_fire=s->apc_primary.scheduler;}
                else{a->primary_reserve=owner.profile==4?0:s->fighter_weapon.primary_reserve;
                    a->missile_reserve=owner.profile==4?s->submarine_weapon.reserve:s->fighter_weapon.rocket_reserve;
                    if(owner.profile==5)a->primary_fire=s->fighter_weapon.primary_fire;}
            }
            a->next=campaign_vehicle_ai;campaign_vehicle_ai=a;
        }
        if(a->target!=UINT32_MAX){uint32_t kind,uid;
            if(scene_vehicle_attack_target_key(a->target,&kind,&uid)||kind!=a->target_kind||uid!=a->target_uid)scene_vehicle_ai_cancel(a->handle);
        }
        status=scene_vehicle_ai_flights(s,a,frame);if(status)return status;
        scene_secondary_vehicle_runtime *motion=owner.selected?NULL:scene_secondary_vehicle_find(owner.handle);
        uint32_t route=owner.selected?campaign_vehicle_route.active:motion&&motion->route.active;
        uint32_t chase=route&&(owner.selected?campaign_vehicle_route.pursuit==2:motion->route.kind==SCENE_SECONDARY_ORDER_PLAYER);
        if(!owner.allowed||scene_vehicle_attack_active(owner.handle)||(route&&!chase)){
            scene_vehicle_loop_audio_stop(owner.handle,SCENE_VEHICLE_LOOP_AI);
            a->engaged=0;a->primary_fire.held=0;a->primary_fire.warmup=0;
            scene_vehicle_attack_state *attack=scene_vehicle_attack_find(owner.handle);if(attack)a->primary_reserve=attack->reserve;
            continue;
        }
        if(a->scan_ticks)--a->scan_ticks;
        if(chase){
            a->engaged=0;
            if(!a->scan_ticks){float point[3],distance=0;uint32_t clear;
                a->scan_ticks=30;status=scene_vehicle_ai_visible(s,&owner,campaign_player_object.handle,1,point,&distance,&clear);if(status)return status;
                if(clear){if(owner.selected)campaign_vehicle_route.active=0;else motion->route.active=0;
                    chase=0;a->scan_ticks=0;++rf_scene_vehicle_ai[1];}
            }
            if(chase){scene_vehicle_loop_audio_stop(owner.handle,SCENE_VEHICLE_LOOP_AI);continue;}
        }
        if(a->engaged){float point[3],distance=0;uint32_t hull,affiliation,visible=0,expired=0;
            if(!scene_vehicle_attack_target(a->target,point,&hull)||!scene_vehicle_ai_target_affiliation(a->target,&affiliation)||
               !scene_vehicle_ai_opposed(owner.affiliation,affiliation))scene_vehicle_ai_cancel(a->handle);
            else{
                status=scene_vehicle_ai_visible(s,&owner,a->target,0,point,&distance,&visible);if(status)return status;
                if(visible)a->unseen_seconds=0;else a->unseen_seconds+=scene_step_seconds;
                status=rf_entity_ai_blind_pursuit_expired(a->unseen_seconds,0,scene_vehicle_ai_blind_seconds[scene_vehicle_ai_blind_index(a->profile)],0,0,&expired);if(status)return status;
                if(expired)scene_vehicle_ai_cancel(a->handle);
            }
        }
        scene_vehicle_attack_state *attack=scene_vehicle_attack_find(owner.handle);
        if(attack)a->primary_reserve=attack->reserve;
        if(!a->engaged&&!a->scan_ticks){a->scan_ticks=27; /* Original normal difficulty450ms. */
            status=scene_vehicle_ai_acquire(s,a,&owner);if(status)return status;}
        status=scene_vehicle_ai_fire(s,a,&owner,frame);if(status)return status;
        if(attack)attack->reserve=a->primary_reserve;
        if(owner.selected){if(owner.profile==2)s->apc_primary.reserve=a->primary_reserve;else if(owner.profile==4)s->submarine_weapon.reserve=a->missile_reserve;
            else{s->fighter_weapon.primary_reserve=a->primary_reserve;s->fighter_weapon.rocket_reserve=a->missile_reserve;}}
        rf_scene_vehicle_ai[5]=uid;rf_scene_vehicle_ai[6]=a->target;
    }
    return RF_OK;
}
static int scene_vehicle_ai_steer(uint32_t handle,const rf_vehicle_rigid_state *body,rf_vehicle_submersible_command *out)
{
    scene_vehicle_ai_state *a=scene_vehicle_ai_find(handle);scene_vehicle_ai_owner owner;float point[3],delta[3],local[3]={0};uint32_t hull;
    if(!a||!a->engaged)return RF_OK;
    if(scene_vehicle_ai_owner_read(scene_vehicle_ai_stream,a->uid,&owner)||!owner.allowed||scene_vehicle_attack_active(handle))return RF_OK;
    if(!scene_vehicle_attack_target(a->target,point,&hull))return RF_OK;
    if(hull!=UINT32_MAX){float health;scene_campaign_vehicle_target(scene_vehicle_ai_stream,hull,point,&health);}
    float distance=0;for(uint32_t i=0;i<3;i++){delta[i]=point[i]-body->position[i];distance+=delta[i]*delta[i];}
    distance=sqrtf(distance);if(!isfinite(distance)||distance<.01f)return RF_OK;
    for(uint32_t axis=0;axis<3;axis++)for(uint32_t k=0;k<3;k++)local[axis]+=body->orientation[axis*3+k]*delta[k];
    float yaw=atan2f(local[0],local[2]),pitch=-atan2f(local[1],sqrtf(local[0]*local[0]+local[2]*local[2]));
    rf_vehicle_submersible_command command={0};command.controlled=1;command.yaw=fmaxf(-1,fminf(1,yaw/.7f));
    command.pitch=fmaxf(-1,fminf(1,pitch/.7f));
    float stand=scene_vehicle_ai_range[scene_vehicle_ai_kind(a->profile)]*.75f;
    if(distance>stand&&fabsf(yaw)<.8f&&fabsf(pitch)<.8f)command.throttle=fminf(1,(distance-stand)/8);
    *out=command;return RF_OK;
}
static int scene_vehicle_ai_ground_steer(uint32_t handle,const rf_vehicle_rigid_state *body,rf_vehicle_rigid_command *out)
{
    scene_vehicle_ai_state *a=scene_vehicle_ai_find(handle);scene_vehicle_ai_owner owner;float target[3],health;uint32_t hull;
    if(!a||a->profile!=2||!a->engaged)return RF_OK;
    if(scene_vehicle_ai_owner_read(scene_vehicle_ai_stream,a->uid,&owner)||!owner.allowed||
       !scene_vehicle_attack_target(a->target,target,&hull))return RF_OK;
    if(hull!=UINT32_MAX)(void)scene_campaign_vehicle_target(scene_vehicle_ai_stream,hull,target,&health);
    float x=target[0]-body->position[0],z=target[2]-body->position[2];
    float distance=sqrtf(x*x+z*z),angle=atan2f(body->orientation[8]*x-body->orientation[6]*z,body->orientation[6]*x+body->orientation[8]*z);
    if(!isfinite(distance)||!isfinite(angle))return RF_FORMAT;
    out->controlled=1;out->turn=fmaxf(-1,fminf(1,angle/.7f));out->throttle=0;
    if(distance>scene_vehicle_ai_range[2]*.75f&&fabsf(angle)<.8f)out->throttle=fminf(.65f,(distance-scene_vehicle_ai_range[2]*.75f)/6);
    return RF_OK;
}
static void scene_vehicle_ai_player_sync(scene_stream *s,uint32_t after)
{
    if(!s||!s->driller_runtime||!s->driller_runtime->entry.session.active)return;
    scene_vehicle_ai_state *a=scene_vehicle_ai_find(s->driller_runtime->entry.host.handle);if(!a)return;
    scene_vehicle_loop_audio_stop(a->handle,SCENE_VEHICLE_LOOP_AI);
    a->engaged=0;
    if(a->profile==2){
        if(after){a->primary_reserve=s->apc_primary.reserve;a->primary_fire=s->apc_primary.scheduler;}
        else{s->apc_primary.reserve=a->primary_reserve;s->apc_primary.scheduler=a->primary_fire;}
        return;
    }
    if(after){if(a->profile!=4)a->primary_fire=s->fighter_weapon.primary_fire;
        a->primary_reserve=a->profile==4?0:s->fighter_weapon.primary_reserve;
        a->missile_reserve=a->profile==4?s->submarine_weapon.reserve:s->fighter_weapon.rocket_reserve;
        a->missile_cooldown=fmaxf(0,a->profile==4?s->submarine_weapon.cooldown:s->fighter_weapon.rocket_fire.cooldown);
    }else{if(a->profile==4){s->submarine_weapon.reserve=a->missile_reserve;s->submarine_weapon.cooldown=a->missile_cooldown;}
        else{s->fighter_weapon.primary_fire=a->primary_fire;s->fighter_weapon.primary_reserve=a->primary_reserve;s->fighter_weapon.rocket_reserve=a->missile_reserve;
            s->fighter_weapon.rocket_fire.cooldown=a->missile_cooldown;}}
}
static int scene_vehicle_ai_draw(scene_stream *s)
{
    for(scene_vehicle_ai_state *a=campaign_vehicle_ai;a;a=a->next){
        int status=scene_fighter_rounds_draw(s,a->bullets,48);if(status)return status;
        if(a->profile==4){
            const scene_vehicle_profile_pack *pack=scene_vehicle_profile_pack_get(s,4);
            const scene_driller_resources *model=scene_vehicle_ai_torpedo?scene_vehicle_ai_torpedo:s->submarine_torpedo?s->submarine_torpedo:pack?pack->torpedo:NULL;
            uint32_t base=scene_vehicle_ai_torpedo?scene_vehicle_ai_torpedo_base:s->submarine_torpedo?s->submarine_torpedo_base:pack?pack->torpedo_base:0;
            uint32_t textures=scene_vehicle_ai_torpedo?scene_vehicle_ai_torpedo_textures:s->submarine_torpedo?s->submarine_torpedo_textures:pack?pack->torpedo_textures:0;
            if(model)for(uint32_t i=0;i<4;i++)if(a->missiles[i].flight.active){uint32_t submissions,vertices;
                status=scene_apc_secondary_visual_draw(model,s,a->missiles[i].flight.position,a->missiles[i].flight.velocity,
                    base,textures,&submissions,&vertices);if(status)return status;}
        }
    }
    return RF_OK;
}
static uint32_t scene_vehicle_ai_rocket_count(void)
{uint32_t count=0;for(scene_vehicle_ai_state *a=campaign_vehicle_ai;a;a=a->next)if(a->profile>=5)count+=4;return count;}
static const rf_weapon_flight *scene_vehicle_ai_rocket(uint32_t index,float *lifetime)
{
    for(scene_vehicle_ai_state *a=campaign_vehicle_ai;a;a=a->next)if(a->profile>=5){
        if(index<4){*lifetime=scene_vehicle_ai_missile[1].lifetime;return &a->missiles[index].flight;}index-=4;}
    return NULL;
}
#endif
