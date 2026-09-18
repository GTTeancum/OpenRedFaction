#ifndef RF_SCENE_REMOTE_GAMEPLAY_INC
#define RF_SCENE_REMOTE_GAMEPLAY_INC
#include "rf/remote_charge.h"
/* Include after scene_weapon_submit; forward-declare input/tick before combat.
 * Parent binds campaign_remote_id, campaign_remote and campaign_remote_primary.
 * Mode1 charge: primary throw, alternate place. Mode2 detonator: either button.
 * Release21/15 ticks and cooldown45 are authored .35/.25/.75 at fixed60Hz.
 * No loaded-magazine or separate detonator ammo bank is introduced. */
static rf_remote_charge scene_remote_charges[RF_REMOTE_CHARGE_CAPACITY];
static uint32_t scene_remote_host_tags[RF_REMOTE_CHARGE_CAPACITY];
static uint32_t scene_remote_held,scene_remote_pending,scene_remote_delay,scene_remote_cooldown;
/* starts, launches, attaches, detonations, live, poolfull, missing hosts, status */
uint32_t rf_scene_remote[8];
static void scene_remote_reset(void)
{
    memset(scene_remote_charges,0,sizeof(scene_remote_charges));memset(scene_remote_host_tags,0xff,sizeof(scene_remote_host_tags));
    memset(rf_scene_remote,0,sizeof(rf_scene_remote));scene_remote_held=scene_remote_pending=scene_remote_delay=scene_remote_cooldown=0;
}
/* Tags are dispatch tokens, never pointers. Actor bindings retain registry
 * identity. Mover view object_id is the available identity for that scene.
 * Detached fragment/unknown tags are deliberately not dereferenced: they remain
 * fixed at impact until detonation (following fragment lifetime is deferred). */
static int scene_remote_host(scene_stream *s,uint32_t tag,uint32_t expected,rf_remote_charge_host *host)
{
    uint32_t index=tag&0xffffu;memset(host,0,sizeof(*host));host->handle=expected;
    if((tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
        campaign_npc_body *owner;if(index>=campaign_npc_body_count)return RF_OK;owner=campaign_npc_bodies+index;
        if(!owner->registration.view || !owner->body.allocated_bytes ||
           rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration)return RF_OK;
        if(expected!=UINT32_MAX && expected!=owner->registration.handle)return RF_OK;
        host->handle=owner->registration.handle;host->found=host->entity=1;
        host->entity_flags=owner->view.flags_810 | (owner->damage.effects.health<=0?1u:0u);
        if(campaign_seeds.items && index<campaign_seeds.records.count && campaign_seeds.items[index].class_index<campaign_seeds.class_count)
            host->class_flags=campaign_seeds.classes[campaign_seeds.items[index].class_index].physics.flags;
        memcpy(host->position,owner->body.state.position,12);memcpy(host->orientation,owner->body.state.orientation,36);
    }else if((tag&0xffff0000u)==SCENE_MOVER_ROCKET_OWNER){
        const rf_collision_solid_view *view;if(index>=campaign_movers.count)return RF_OK;view=campaign_movers.views+index;
        /* A tag fallback is level-local, never a generic registry handle. */
        host->handle=view->object_id==UINT32_MAX?tag:view->object_id;
        if(expected!=UINT32_MAX && expected!=host->handle){host->handle=expected;return RF_OK;}
        host->found=1;memcpy(host->position,view->output_origin,12);memcpy(host->orientation,view->output_matrix,36);
    }
    (void)s;return RF_OK;
}
static int scene_remote_launch(scene_stream *s,uint32_t frame,const float position[3],const float forward[3],uint32_t place,uint32_t *spawned)
{
    rf_remote_charge pending={0};uint32_t i,k;int32_t ammo;int status;
    if(!s || !spawned || campaign_remote_id<0 || campaign_remote_id>=64 || place>1)return RF_RANGE;*spawned=0;
    ammo=campaign_weapon_supply.definitions[campaign_remote_id].ammo_type;if(ammo<0 || ammo>=32)return RF_RANGE;
    if(campaign_player_inventory.reserve[ammo]<=0)return RF_OK;
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY && scene_remote_charges[i].flight.lifecycle.active;i++){}
    if(i==RF_REMOTE_CHARGE_CAPACITY){++rf_scene_remote[5];return RF_OK;}
    /* Place uses a slower short toss until full authored hand placement exists. */
    status=rf_remote_charge_launch(&pending,campaign_player_object.handle,(uint32_t)campaign_remote_id,position,forward,
        place?2.0f:campaign_remote.speed,campaign_remote.collision_radius,campaign_remote.lifetime);if(status)return status;
    for(k=0;k<3;k++)pending.flight.velocity[k]+=scene_actor_body.state.velocity[k];
    scene_remote_charges[i]=pending;scene_remote_host_tags[i]=UINT32_MAX;
    --campaign_player_inventory.reserve[ammo];++rf_scene_remote[1];++rf_scene_remote[4];*spawned=1;campaign_ammo_publish();
    if(rf_scene_combat_trace)printf("REMOTE_RELEASE %u %u %u %d\n",frame,i,place,campaign_player_inventory.reserve[ammo]);return RF_OK;
}
static int scene_remote_request(uint32_t frame,uint32_t owner,uint32_t *any)
{
    uint32_t i,selected;int status;if(!any)return RF_RANGE;*any=0;
    /* Core request owns the class/owner predicate. Scene skips retired slots;
     * process one registered record at a time without pool-sized stack copies. */
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++)if(scene_remote_charges[i].flight.lifecycle.active){
        status=rf_remote_charge_request(scene_remote_charges+i,1,owner,&selected);if(status)return status;*any|=selected;}
    if(rf_scene_combat_trace)printf("REMOTE_REQUEST %u %u %u\n",frame,owner,*any);return RF_OK;
}
static int scene_remote_tick(scene_stream *s,uint32_t frame)
{
    uint32_t i;int status;rf_scene_remote[4]=0;
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++)if(scene_remote_charges[i].flight.lifecycle.active){
        rf_remote_charge *charge=scene_remote_charges+i;rf_remote_charge_event event;
        if(charge->host_bound){rf_remote_charge_host host;uint32_t action;
            status=scene_remote_host(s,scene_remote_host_tags[i],charge->host,&host);if(status)return status;
            status=rf_remote_charge_update_host(charge,&host,&action);if(status)return status;
            if(action==3){++rf_scene_remote[6];continue;}}
        status=rf_remote_charge_step(charge,1.f/60,scene_gravity.vector,scene_grenade_sweep,s,&event);if(status)return status;
        if(event.attached){rf_remote_charge_host host;++rf_scene_remote[2];
            scene_remote_host_tags[i]=event.contact.object;
            if(event.contact.object!=UINT32_MAX){
                status=scene_remote_host(s,event.contact.object,UINT32_MAX,&host);if(status)return status;
                if(host.found){status=rf_remote_charge_bind_host(charge,&host);if(status)return status;}}
            combat_sound("Remote Charge Hit Default",event.position);
            if(rf_scene_combat_trace)printf("REMOTE_ATTACH %u %u %u %u\n",frame,i,event.contact.object,charge->host_bound);
        }
        if(event.detonate){rf_weapon_flight_contact contact=event.contact;rf_collision_room_location location;
            memcpy(contact.hit.point,event.position,12);contact.object=contact.face=UINT32_MAX;contact.hit.fraction=0;
            if(!charge->attached)contact.hit.normal[1]=1;
            status=rf_geometry_collision_world_locate(s->collision,event.position,&location);if(status)return status;contact.room=location.room;
            /* charge_explode uses the same rocket hit recipe and Medium Explosion
             * foley; only the authored visual radius differs (2 versus1.5). */
            status=scene_impact_start_sized(s,&contact,frame,campaign_remote.impact_radius[0]);if(status)return status;
            scene_impact_sound(event.position,frame);
            status=scene_explosion_blast(s,frame,event.position,campaign_remote_primary.damage,campaign_remote.damage_radius);if(status)return status;
            status=scene_explosion_terrain(s,frame,&contact,campaign_remote.crater_radius);if(status)return status;
            ++rf_scene_remote[3];if(rf_scene_combat_trace)printf("REMOTE_DETONATE %u %u %.9g %.9g %.9g\n",frame,i,event.position[0],event.position[1],event.position[2]);
        }
        if(charge->flight.lifecycle.active)++rf_scene_remote[4];
    }
    return RF_OK;
}
static int scene_remote_input(scene_stream *s,uint32_t frame,const float position[3],const float forward[3],uint32_t mode,uint32_t triggers)
{
    uint32_t edges=triggers&~scene_remote_held,spawned,any;int32_t ammo;int status;
    if(mode>2 || triggers>3)return RF_RANGE;scene_remote_held=triggers;
    if(scene_remote_cooldown)--scene_remote_cooldown;
    if(mode!=1)scene_remote_pending=scene_remote_delay=0;
    if(mode==2 && edges){status=scene_remote_request(frame,campaign_player_object.handle,&any);if(status)return status;
        if(any){++rf_scene_combat[0];campaign_last_alt=0;}return RF_OK;}
    if(mode!=1 || campaign_remote_id<0)return RF_OK;
    ammo=campaign_weapon_supply.definitions[campaign_remote_id].ammo_type;if(ammo<0 || ammo>=32)return RF_RANGE;
    if(scene_remote_pending){
        if(scene_remote_delay)--scene_remote_delay;
        if(!scene_remote_delay){status=scene_remote_launch(s,frame,position,forward,scene_remote_pending==2,&spawned);if(status)return status;
            scene_remote_pending=0;if(spawned)scene_remote_cooldown=45;}
    }else if(edges && !scene_remote_cooldown && campaign_player_inventory.reserve[ammo]>0){
        scene_remote_pending=edges&2?2:1;scene_remote_delay=scene_remote_pending==2?15:21;
        ++rf_scene_remote[0];++rf_scene_combat[0];campaign_last_alt=scene_remote_pending==2;
        combat_sound(campaign_last_alt?"Remote Charge Alt Launch":"Remote Charge Launch",position);
    }
    return RF_OK;
}
static int scene_remote_draw(scene_stream *stream)
{
    scene_weapon_context c={0};uint32_t i,state[20]={0},model;int status;
    if(campaign_remote_id<0 || !rf_scene_remote[4])return RF_OK;
    model=campaign_weapon_models.weapons[campaign_remote_id].model;if(!model)return RF_NOT_FOUND;
    c.stream=stream;c.pickup=1;state[0]=0x80;
    c.buffers.cache=stream->npc_memory;c.buffers.clip=(float(*)[3])((uint8_t*)stream->npc_memory+4096*32);
    c.buffers.vertices=(uint8_t(*)[40])((uint8_t*)stream->npc_memory+4096*56);c.buffers.capacity=4096;
    c.attributes=(rf_model_render_output){1,{255,255,255},255,1,1};c.lights.ambient[0]=40;c.lights.ambient[1]=50;c.lights.ambient[2]=60;
    c.planes.near_depth=.1f;c.planes.far_depth=1000;c.projection.scale[0]=320;c.projection.scale[1]=240;c.projection.clamp=1;
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++)if(scene_remote_charges[i].flight.lifecycle.active){
        rf_weapon_hand_placement pose={0};memcpy(pose.position,scene_remote_charges[i].flight.position,12);memcpy(pose.basis,scene_remote_charges[i].orientation,36);
        status=scene_weapon_submit(&c,model,&pose,state);if(status)return status;}
    return RF_OK;
}
#endif
