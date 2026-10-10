#include "rf/npc_checkpoint.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>
_Static_assert(sizeof(float)==4,"RFNC requires binary32");
static uint32_t word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t signed_word(const unsigned char *p)
{uint32_t u=word(p);int32_t v;memcpy(&v,&u,4);return v;}
static float real(const unsigned char *p)
{uint32_t u=word(p);float v;memcpy(&v,&u,4);return v;}
static void put_real(unsigned char *p,float v)
{uint32_t u;memcpy(&u,&v,4);put(p,u);}
static uint32_t hash(const unsigned char *p,uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=(i>=12&&i<16)?0:p[i];h*=16777619u;}return h;}
static int catalog_valid(const rf_npc_checkpoint_catalog *c)
{
    uint32_t i;if(!c||!c->count||c->count>64)return RF_RANGE;
    for(i=0;i<64;i++){
        const rf_weapon_acquire_definition *d=c->weapons+i;
        if(c->supported[i]>1||(i>=c->count&&c->supported[i]))return RF_RANGE;
        if(c->supported[i]&&(d->ammo_type< -1||d->ammo_type>=32||d->capacity<0||d->magazine<0||
           (d->ammo_type<0&&d->magazine)))return RF_RANGE;
    }
    return RF_OK;
}
static int selected(int32_t id,const rf_npc_checkpoint_record *r,const rf_npc_checkpoint_catalog *c)
{return id==-1||(id>=0&&(uint32_t)id<c->count&&c->supported[id]&&r->inventory.owned[id]);}
static int animation_valid(const rf_npc_checkpoint_record *r)
{
    const rf_motion_playback_state *p=&r->playback;const rf_motion_slot_state *a=&p->completion.active;
    uint32_t i,j;rf_motion_playback_state zero={0};rf_motion_controller zero_controller={0};
    const rf_motion_controller *controller=&r->controller;
    if(r->animation_present>1)return RF_FORMAT;
    if(!r->animation_present)return r->script_animation.active||r->script_animation.loop||r->script_animation.freeze||
        r->script_animation.motion||memcmp(p,&zero,sizeof(zero))||memcmp(controller,&zero_controller,sizeof(zero_controller))?RF_FORMAT:RF_OK;
    if(r->retired||r->script_animation.active>1||r->script_animation.loop>1||r->script_animation.freeze>1||
       (r->script_animation.active?r->script_animation.motion<0:(r->script_animation.loop||r->script_animation.freeze||r->script_animation.motion))||a->count>16||!a->count||p->completion.frozen>1||p->completion.primary_flag>255||
       p->generation>65535||p->event_mask>3||!isfinite(p->phase)||p->phase<0||p->phase>1||
       a->freeze_slot< -1||a->primary_slot< -1||a->dominant_slot< -1||
       a->freeze_slot>=(int32_t)a->count||a->primary_slot>=(int32_t)a->count||a->dominant_slot>=(int32_t)a->count)return RF_FORMAT;
    if(controller->current<0||controller->current>=23||controller->next< -1||controller->next>=23||
       controller->override_enabled>1||controller->override_state< -1||controller->override_state>=23||
       (controller->override_enabled&&controller->override_state<0)||!isfinite(controller->duration)||controller->duration<0||
       !isfinite(controller->elapsed)||controller->elapsed<0||
       (controller->duration>0&&(controller->next<0||controller->elapsed>controller->duration)))return RF_FORMAT;
    for(i=0;i<2;i++)for(j=0;j<3;j++)if(!isfinite(p->completion.primary_vectors[i][j]))return RF_FORMAT;
    for(i=0;i<a->count;i++){
        if(a->slots[i].motion<0||!isfinite(a->slots[i].weight)||a->slots[i].weight<0)return RF_FORMAT;
        for(j=0;j<i;j++)if(a->slots[j].motion==a->slots[i].motion)return RF_FORMAT;
    }
    /* Inactive slots are implementation scratch, intentionally not serialized. */
    return RF_OK;
}
static uint32_t animation_bytes(const rf_npc_checkpoint_record *r)
{return r->animation_present?RF_NPC_CHECKPOINT_ANIMATION_BASE+12*r->playback.completion.active.count:0;}
static uint32_t extension_bytes(const rf_npc_checkpoint_record *r)
{
    rf_npc_checkpoint_move zero_move={0};rf_npc_checkpoint_look zero_look={0};float zero[3]={0};
    return memcmp(&r->move,&zero_move,sizeof(zero_move))||memcmp(&r->look,&zero_look,sizeof(zero_look))||
        memcmp(r->look_command,zero,12)||memcmp(r->look_delta,zero,12)||
        memcmp(r->look_offset,zero,12)||memcmp(r->look_vector,zero,12)?RF_NPC_CHECKPOINT_EXTENSION_BYTES:0;
}
static uint32_t combat_bytes(const rf_npc_checkpoint_record *r)
{rf_npc_checkpoint_combat zero={0};return memcmp(&r->combat,&zero,sizeof(zero))?RF_NPC_CHECKPOINT_COMBAT_BYTES:0;}
static uint32_t shot_bytes(const rf_npc_checkpoint_record *r)
{return r->shot_count*24u;}
static int pain_valid(const rf_npc_checkpoint_record *r)
{
    const rf_npc_checkpoint_pain *p=&r->pain;rf_npc_checkpoint_pain zero={0};uint32_t i;
    if(p->present>1)return RF_FORMAT;
    if(!p->present)return memcmp(p,&zero,sizeof(zero))?RF_FORMAT:RF_OK;
    if(r->retired||r->dead_pose||r->health<=0||
       (p->action!=-1&&p->action!=22&&p->action!=23))return RF_FORMAT;
    for(i=0;i<3;i++)if(p->remaining[i]< -1||p->remaining[i]>RF_TIMER_PERIOD/2)return RF_FORMAT;
    return RF_OK;
}
static void read_pain(const unsigned char *p,rf_npc_checkpoint_pain *r)
{
    uint32_t i;r->present=word(p);
    for(i=0;i<3;i++)r->remaining[i]=signed_word(p+4+4*i);
    r->action=signed_word(p+16);r->random=word(p+20);
}
static void write_pain(unsigned char *p,const rf_npc_checkpoint_pain *r)
{
    uint32_t i;put(p,r->present);
    for(i=0;i<3;i++)put(p+4+4*i,(uint32_t)r->remaining[i]);
    put(p+16,(uint32_t)r->action);put(p+20,r->random);
}
static int physics_valid(const rf_npc_checkpoint_record *r)
{
    const rf_npc_checkpoint_physics *p=&r->physics;rf_npc_checkpoint_physics zero={0};
    const float *vectors[5]={p->velocity,p->angular,p->momentum,p->force,p->torque};uint32_t i,j;
    if(p->present>1||p->scripted>1)return RF_FORMAT;
    if(!p->present)return memcmp(p,&zero,sizeof(zero))?RF_FORMAT:RF_OK;
    /* RFNC17 admits the existing terminal death profile's source body. The
     * enclosing validator still proves fatal health, death flags and clip. */
    if(r->retired||(!r->dead_pose&&r->health<=0)||r->ai_mode==13||
       (p->body_bits&~RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK)||
       (p->object_bits&~RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK)||
       (p->scripted&&!(p->body_bits&0x80000000u)&&(p->body_bits&0x18000000u)!=0x18000000u)||
       (!!r->support_uid!=!!(p->body_bits&0x400000u))||
       (r->support_uid&&(p->body_bits&1u)))return RF_FORMAT;
    for(i=0;i<5;i++)for(j=0;j<3;j++)if(!isfinite(vectors[i][j])||
        (!i&&fabsf(vectors[i][j])>.001f))return RF_FORMAT;
    return RF_OK;
}
static void read_physics(const unsigned char *p,rf_npc_checkpoint_physics *r)
{
    float *vectors[5]={r->velocity,r->angular,r->momentum,r->force,r->torque};uint32_t i,j;
    r->present=word(p);r->scripted=word(p+4);r->body_bits=word(p+8);r->object_bits=word(p+12);
    for(i=0;i<5;i++)for(j=0;j<3;j++)vectors[i][j]=real(p+16+12*i+4*j);
}
static void write_physics(unsigned char *p,const rf_npc_checkpoint_physics *r)
{
    const float *vectors[5]={r->velocity,r->angular,r->momentum,r->force,r->torque};uint32_t i,j;
    put(p,r->present);put(p+4,r->scripted);put(p+8,r->body_bits);put(p+12,r->object_bits);
    for(i=0;i<5;i++)for(j=0;j<3;j++)put_real(p+16+12*i+4*j,vectors[i][j]);
}
static int drone_valid(const rf_npc_checkpoint_record *r)
{
    const rf_npc_checkpoint_drone *p=&r->drone;rf_npc_checkpoint_drone zero={0};
    const float *vectors[5]={p->velocity,p->angular,p->momentum,p->force,p->torque};uint32_t i,j;
    if(p->kind>2)return RF_FORMAT;
    if(!p->kind)return memcmp(p,&zero,sizeof(zero))?RF_FORMAT:RF_OK;
    if(!isfinite(p->pitch)||r->physics.present||r->dead_pose)return RF_FORMAT;
    if(p->kind==1){
        if(r->retired||r->health<=0||r->ai_mode==13||
           (p->body_bits&~RF_NPC_CHECKPOINT_PHYSICS_BODY_MASK)||
           (p->object_bits&~RF_NPC_CHECKPOINT_PHYSICS_OBJECT_MASK)||
           (!!r->support_uid!=!!(p->body_bits&0x400000u))||
           (r->support_uid&&(p->body_bits&1u)))return RF_FORMAT;
    }else if(!r->retired||p->body_bits||p->object_bits)return RF_FORMAT;
    for(i=0;i<5;i++)for(j=0;j<3;j++)if(!isfinite(vectors[i][j])||
        (!i&&fabsf(vectors[i][j])>.001f)||(p->kind==2&&vectors[i][j]!=0))return RF_FORMAT;
    return RF_OK;
}
static void read_drone(const unsigned char *p,rf_npc_checkpoint_drone *r)
{
    float *vectors[5]={r->velocity,r->angular,r->momentum,r->force,r->torque};uint32_t i,j;
    r->kind=word(p);r->pitch=real(p+4);r->body_bits=word(p+8);r->object_bits=word(p+12);
    for(i=0;i<5;i++)for(j=0;j<3;j++)vectors[i][j]=real(p+16+12*i+4*j);
}
static void write_drone(unsigned char *p,const rf_npc_checkpoint_drone *r)
{
    const float *vectors[5]={r->velocity,r->angular,r->momentum,r->force,r->torque};uint32_t i,j;
    put(p,r->kind);put_real(p+4,r->pitch);put(p+8,r->body_bits);put(p+12,r->object_bits);
    for(i=0;i<5;i++)for(j=0;j<3;j++)put_real(p+16+12*i+4*j,vectors[i][j]);
}
static void read_item_drop(const unsigned char *p,rf_campaign_item_drop *r)
{
    uint32_t i;r->state=word(p);r->stable_definition_id=signed_word(p+4);r->quantity=signed_word(p+8);
    for(i=0;i<3;i++)r->position[i]=real(p+12+4*i);
}
static void write_item_drop(unsigned char *p,const rf_campaign_item_drop *r)
{
    uint32_t i;put(p,r->state);put(p+4,(uint32_t)r->stable_definition_id);put(p+8,(uint32_t)r->quantity);
    for(i=0;i<3;i++)put_real(p+12+4*i,r->position[i]);
}
static void read_medic(const unsigned char *p,rf_campaign_medic_state *r)
{r->reserve_seen=word(p);r->reserve=real(p+4);r->child_state=word(p+8);r->child_health=real(p+12);}
static void write_medic(unsigned char *p,const rf_campaign_medic_state *r)
{put(p,r->reserve_seen);put_real(p+4,r->reserve);put(p+8,r->child_state);put_real(p+12,r->child_health);}
static void read_route(const unsigned char *p,rf_campaign_actor_route *r)
{r->flags=word(p);r->action=signed_word(p+4);r->offset=word(p+8);r->count=word(p+12);
 r->cursor=word(p+16);r->section_hash=word(p+20);r->origin=word(p+24);r->event=word(p+28);r->patrol=word(p+32);}
static void write_route(unsigned char *p,const rf_campaign_actor_route *r)
{put(p,r->flags);put(p+4,(uint32_t)r->action);put(p+8,r->offset);put(p+12,r->count);
 put(p+16,r->cursor);put(p+20,r->section_hash);put(p+24,r->origin);put(p+28,r->event);put(p+32,r->patrol);}
static int route_valid(const rf_npc_checkpoint_record *r)
{
    const rf_campaign_actor_route *a=&r->route;const rf_npc_checkpoint_move *m=&r->move;
    if(rf_campaign_actor_route_validate(a))return RF_FORMAT;
    if(!a->flags)return (r->ai_mode==3||r->ai_mode==4||r->ai_mode==10)?RF_FORMAT:RF_OK;
    if(a->action!=(r->ai_mode?r->ai_mode:-1))return RF_FORMAT;
    if((r->ai_mode==3||r->ai_mode==4||r->ai_mode==10)&&!(a->patrol&1))return RF_FORMAT;
    if(r->retired||r->dead_pose||r->health<=0)return (a->origin||(a->flags&62))?RF_FORMAT:RF_OK;
    if((a->patrol&1)&&(r->look.active||r->combat.active==1))return RF_FORMAT;
    if(r->ai_mode==3&&(!r->combat_alert||(r->combat.active==1)||a->origin))return RF_FORMAT;
    if(r->ai_mode==4&&(!a->origin||!(a->flags&2)||!(a->flags&32)))return RF_FORMAT;
    if((a->flags&32)!=((m->active&&m->follow!=2)?32u:0u))return RF_FORMAT;
    if(m->active&&m->follow!=2){
        if(a->event!=m->event||a->count!=m->path_count||a->cursor!=m->path_index||
           ((a->flags>>2)&3)!=m->path_mode||((a->flags>>4)&1)!=m->path_reverse||
           (a->origin==2&&(m->event||m->follow)))return RF_FORMAT;
    }
    if(r->ai_mode==10&&(a->origin!=1||(a->flags&2)||(m->active&&m->follow!=1)))return RF_FORMAT;
    return RF_OK;
}
static void read_animation(const unsigned char *p,rf_npc_checkpoint_record *r)
{
    rf_motion_playback_state *b=&r->playback;rf_motion_slot_state *a=&b->completion.active;uint32_t i,j;
    r->animation_present=1;r->script_animation.active=word(p);r->script_animation.loop=word(p+4);
    r->script_animation.freeze=word(p+8);r->script_animation.motion=signed_word(p+12);
    a->count=word(p+16);a->freeze_slot=signed_word(p+20);a->primary_slot=signed_word(p+24);a->dominant_slot=signed_word(p+28);
    b->completion.frozen=word(p+32);b->completion.primary_flag=word(p+36);
    b->completion.primary_words[0]=word(p+40);b->completion.primary_words[1]=word(p+44);
    for(i=0;i<2;i++)for(j=0;j<3;j++)b->completion.primary_vectors[i][j]=real(p+48+12*i+4*j);
    b->phase=real(p+72);b->generation=word(p+76);b->event_mask=word(p+80);
    r->controller.current=signed_word(p+84);r->controller.next=signed_word(p+88);
    r->controller.duration=real(p+92);r->controller.elapsed=real(p+96);
    r->controller.override_state=signed_word(p+100);r->controller.override_enabled=word(p+104);
    for(i=0;i<a->count;i++){a->slots[i].motion=signed_word(p+108+12*i);a->slots[i].tick=signed_word(p+112+12*i);a->slots[i].weight=real(p+116+12*i);}
}
static void write_animation(unsigned char *p,const rf_npc_checkpoint_record *r)
{
    const rf_motion_playback_state *b=&r->playback;const rf_motion_slot_state *a=&b->completion.active;uint32_t i,j;
    put(p,r->script_animation.active);put(p+4,r->script_animation.loop);put(p+8,r->script_animation.freeze);put(p+12,(uint32_t)r->script_animation.motion);
    put(p+16,a->count);put(p+20,(uint32_t)a->freeze_slot);put(p+24,(uint32_t)a->primary_slot);put(p+28,(uint32_t)a->dominant_slot);
    put(p+32,b->completion.frozen);put(p+36,b->completion.primary_flag);put(p+40,b->completion.primary_words[0]);put(p+44,b->completion.primary_words[1]);
    for(i=0;i<2;i++)for(j=0;j<3;j++)put_real(p+48+12*i+4*j,b->completion.primary_vectors[i][j]);
    put_real(p+72,b->phase);put(p+76,b->generation);put(p+80,b->event_mask);
    put(p+84,(uint32_t)r->controller.current);put(p+88,(uint32_t)r->controller.next);
    put_real(p+92,r->controller.duration);put_real(p+96,r->controller.elapsed);
    put(p+100,(uint32_t)r->controller.override_state);put(p+104,r->controller.override_enabled);
    for(i=0;i<a->count;i++){put(p+108+12*i,(uint32_t)a->slots[i].motion);put(p+112+12*i,(uint32_t)a->slots[i].tick);put_real(p+116+12*i,a->slots[i].weight);}
}
static int valid(const rf_npc_checkpoint_record *r,const rf_npc_checkpoint_catalog *c)
{
    uint32_t i,j;uint8_t used[32]={0};rf_npc_checkpoint_move zero_move={0};rf_npc_checkpoint_look zero_look={0};rf_npc_checkpoint_combat zero_combat={0};
    if(route_valid(r)||physics_valid(r)||pain_valid(r)||drone_valid(r)||rf_campaign_medic_state_validate(&r->medic)||r->ai_suppressed>1)return RF_FORMAT;
    if(rf_campaign_item_drop_validate(&r->item_drop)||
       (r->item_drop.state&&((!r->retired&&!r->dead_pose)||r->health>0)))return RF_FORMAT;
    if(r->capek_shield_broken>1 || (r->capek_shield_broken &&
       (!r->retired&&!r->dead_pose&&r->health>0&&(r->armor!=0||!r->movement_present))))return RF_FORMAT;
    if(r->holster>3 || (r->holster&&(r->retired||r->dead_pose||r->health<=0)))return RF_FORMAT;
    if(r->movement_present>1||r->movement_slot>=16||r->speed_mode>2||
       (!r->movement_present&&(r->movement_slot||r->speed_mode))||
       (r->movement_present&&(r->retired||r->dead_pose||r->health<=0)))return RF_FORMAT;
    if(r->shield_disabled>2||r->shot_count>16||(!r->shot_count&&r->shot_rng)||
       (r->shot_count&&(r->retired||r->dead_pose||r->health<=0)))return RF_FORMAT;
    for(i=0;i<r->shot_count;i++){
        const rf_npc_checkpoint_shot *shot=r->shots+i;
        if(!shot->event||shot->event==UINT32_MAX||shot->mode>2||shot->remaining>120||
           (i&&shot->remaining<r->shots[i-1].remaining))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(shot->point[j]))return RF_FORMAT;
    }
    if(r->support_uid==UINT32_MAX||(r->support_uid&&(r->retired||r->dead_pose||r->health<=0)))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(r->support_velocity[i])||fabsf(r->support_velocity[i])>10000.f||
        (!r->support_uid&&r->support_velocity[i]!=0))return RF_FORMAT;
    if(r->uid==UINT32_MAX||r->class_id==UINT32_MAX||r->controller_uid==UINT32_MAX||r->retired>1||r->combat_alert>1||r->dead_pose>1||
       (r->retired&&r->combat_alert)||(r->flags&~0x4004u)||
       !isfinite(r->health)||(!r->retired&&!r->dead_pose&&r->health<=0)||!isfinite(r->armor)||r->armor<0||
       !isfinite(r->yaw)||
       (r->ai_mode!=-1&&r->ai_mode!=0&&r->ai_mode!=1&&r->ai_mode!=2&&r->ai_mode!=3&&r->ai_mode!=4&&r->ai_mode!=10&&r->ai_mode!=11&&r->ai_mode!=13))return RF_FORMAT;
    if(r->dead_pose){
        if(r->retired||r->health>0||r->combat_alert||!(r->death_flags_810&1u)||
           r->death_action<0||r->death_action>=45||!r->animation_present||
           r->playback.completion.active.count!=1||r->script_animation.active)return RF_FORMAT;
    }else if(r->death_flags_810||r->death_action)return RF_FORMAT;
    if(r->move.active>1)return RF_FORMAT;
    if(!r->move.active){if(memcmp(&r->move,&zero_move,sizeof(zero_move)))return RF_FORMAT;}
    else {
        if(r->retired||r->dead_pose||r->health<=0||r->move.follow>2||
           (r->move.follow==2?r->move.event!=0:(r->route.origin==2?r->move.event!=0:r->move.event==0))||
           r->move.path_mode>2||r->move.path_reverse>1||
           (r->move.path_count?r->move.path_index>=r->move.path_count:
            r->move.path_index||r->move.path_mode||r->move.path_reverse)||
           (r->move.path_reverse&&r->move.path_mode!=2&&!(r->move.path_mode==1&&(r->route.patrol&1)))||
           (r->move.follow&&r->move.path_count)||r->move.retained_count>4||
           (r->move.retained_count&&r->move.route_index>r->move.retained_count)||
           r->move.retry>60||!isfinite(r->move.fall_speed))return RF_FORMAT;
        for(i=0;i<3;i++)if(!isfinite(r->move.target[i])||!isfinite(r->move.route_start[i])||
            !isfinite(r->move.route_goal[i]))return RF_FORMAT;
        for(i=0;i<r->move.retained_count;i++)if(!r->move.retained_nodes[i])return RF_FORMAT;
        for(i=r->move.retained_count;i<4;i++)if(r->move.retained_nodes[i])return RF_FORMAT;
    }
    if(r->look.active>1)return RF_FORMAT;
    if(!r->look.active){if(memcmp(&r->look,&zero_look,sizeof(zero_look)))return RF_FORMAT;}
    else {
        if(r->retired||r->dead_pose||r->health<=0||!r->look.event)return RF_FORMAT;
        for(i=0;i<3;i++)if(!isfinite(r->look.position[i]))return RF_FORMAT;
    }
    if(r->combat.active>4)return RF_FORMAT;
    if(!r->combat.active){if(memcmp(&r->combat,&zero_combat,sizeof(zero_combat)))return RF_FORMAT;}
    else {
        if(r->retired||r->dead_pose||r->health<=0||!r->combat_alert||
           (r->combat.active==1?(r->move.active||!r->combat.event):
            (r->combat.event==UINT32_MAX||(r->combat.active>=3&&!r->combat.event)||
             (r->combat.active==4&&r->combat.event==r->uid)||
             (r->move.active&&r->move.follow!=2)))||
           r->combat.burst>256||r->combat.due_remaining>3600||
           r->combat.reload_remaining>3600||r->combat.reload_weapon< -1||
           r->combat.reload_weapon>=64||
           (r->combat.reload_remaining&&r->combat.reload_weapon<0))return RF_FORMAT;
        for(i=0;i<3;i++)if(!isfinite(r->combat.point[i]))return RF_FORMAT;
        if(r->combat.active>=2&&(r->combat.point[0]||r->combat.point[1]||r->combat.point[2]))return RF_FORMAT;
    }
    for(i=0;i<3;i++){
        const float *vectors[4]={r->look_command,r->look_delta,r->look_offset,r->look_vector};
        for(j=0;j<4;j++)if(!isfinite(vectors[j][i]))return RF_FORMAT;
    }
    for(i=0;i<3;i++)if(!isfinite(r->eye_angles[i])||!isfinite(r->position[i])||!isfinite(r->drop.position[i]))return RF_FORMAT;
    for(i=0;i<64;i++){
        const rf_weapon_acquire_definition *d=c->weapons+i;
        if(r->inventory.owned[i]>1||r->inventory.loaded[i]<0||
           (!c->supported[i]&&(r->inventory.owned[i]||r->inventory.loaded[i]))||
           (c->supported[i]&&r->inventory.loaded[i]>d->magazine))return RF_FORMAT;
        if(c->supported[i]&&d->ammo_type>=0)used[d->ammo_type]=1;
    }
    for(i=0;i<32;i++)if(r->inventory.reserve[i]<0||(!used[i]&&r->inventory.reserve[i]))return RF_FORMAT;
    if(!selected(r->primary,r,c)||!selected(r->secondary,r,c))return RF_FORMAT;
    if(r->drop.state>2||r->drop.quantity<0)return RF_FORMAT;
    if(r->drop.state){
        if((!r->retired&&!r->dead_pose)||r->drop.weapon<0||(uint32_t)r->drop.weapon>=c->count||!c->supported[r->drop.weapon])return RF_FORMAT;
    }else if(r->drop.weapon||r->drop.quantity||r->drop.position[0]||r->drop.position[1]||r->drop.position[2])return RF_FORMAT;
    return animation_valid(r);
}
static void read_row(const unsigned char *p,uint32_t version,rf_npc_checkpoint_record *r)
{
    uint32_t i,move_at=version>=10?600:version>=9?588:version>=8?572:568,combat_at=0;memset(r,0,sizeof(*r));r->uid=word(p);r->class_id=word(p+4);r->retired=word(p+8);
    r->flags=word(p+12);r->affiliation=word(p+16);r->health=real(p+20);r->armor=real(p+24);
    for(i=0;i<3;i++)r->position[i]=real(p+28+i*4);r->yaw=real(p+40);
    r->primary=signed_word(p+44);r->secondary=signed_word(p+48);
    r->drop.state=word(p+52);r->drop.weapon=signed_word(p+56);r->drop.quantity=signed_word(p+60);
    for(i=0;i<3;i++)r->drop.position[i]=real(p+64+i*4);
    memcpy(r->inventory.owned,p+76,64);
    for(i=0;i<32;i++)r->inventory.reserve[i]=signed_word(p+140+i*4);
    for(i=0;i<64;i++)r->inventory.loaded[i]=signed_word(p+268+i*4);
    r->ai_mode=signed_word(p+524);
    if(version>=2)for(i=0;i<3;i++)r->eye_angles[i]=real(p+528+i*4);
    if(version>=4)r->controller_uid=word(p+544);
    if(version>=5)r->combat_alert=word(p+548);
    if(version>=6){r->dead_pose=word(p+552);r->death_flags_810=word(p+556);r->death_action=signed_word(p+560);}
    if(version>=9){r->support_uid=word(p+572);for(i=0;i<3;i++)r->support_velocity[i]=real(p+576+4*i);}
    r->shield_disabled=2; /* Legacy rows retain the constructed shield state. */
    if(version>=10){
        uint32_t at=move_at+word(p+564)+word(p+568),j;
        r->shot_count=word(p+588);r->shot_rng=word(p+592);r->shield_disabled=word(p+596);
        for(i=0;i<r->shot_count;i++,at+=24){
            rf_npc_checkpoint_shot *shot=r->shots+i;
            shot->event=word(p+at);shot->remaining=word(p+at+4);shot->mode=word(p+at+8);
            for(j=0;j<3;j++)shot->point[j]=real(p+at+12+4*j);
        }
    }
    if(version>=7&&word(p+564)){
        uint32_t *fields[14]={&r->move.active,&r->move.event,&r->move.follow,&r->move.path_index,
            &r->move.path_mode,&r->move.path_reverse,&r->move.path_count,&r->move.route_index,
            &r->move.retry,&r->move.retained_count,&r->move.retained_nodes[0],&r->move.retained_nodes[1],
            &r->move.retained_nodes[2],&r->move.retained_nodes[3]};
        for(i=0;i<14;i++)*fields[i]=word(p+move_at+4*i);
        for(i=0;i<3;i++)r->move.target[i]=real(p+move_at+56+4*i);
        r->move.fall_speed=real(p+move_at+68);
        for(i=0;i<3;i++){r->move.route_start[i]=real(p+move_at+72+4*i);r->move.route_goal[i]=real(p+move_at+84+4*i);
            r->look_command[i]=real(p+move_at+96+4*i);r->look_delta[i]=real(p+move_at+108+4*i);
            r->look_offset[i]=real(p+move_at+120+4*i);r->look_vector[i]=real(p+move_at+132+4*i);
            r->look.position[i]=real(p+move_at+156+4*i);}
        r->look.active=word(p+move_at+144);r->look.event=word(p+move_at+148);r->look.target_uid=word(p+move_at+152);
    }
    if(version>=8&&word(p+568)){
        combat_at=move_at+word(p+564);
        r->combat.active=word(p+combat_at);r->combat.event=word(p+combat_at+4);
        r->combat.burst=word(p+combat_at+8);r->combat.due_remaining=word(p+combat_at+12);
        r->combat.reload_remaining=word(p+combat_at+16);r->combat.reload_weapon=signed_word(p+combat_at+20);
        for(i=0;i<3;i++)r->combat.point[i]=real(p+combat_at+24+4*i);
        r->combat.spread_rng=word(p+combat_at+36);
    }
    if(version>=3&&word(p+540))read_animation(p+(version>=8?move_at+word(p+564)+word(p+568)+shot_bytes(r):version>=7?568+word(p+564):version>=6?564:version>=5?552:version>=4?548:544),r);
    if(version>=13){
        uint32_t tail=RF_NPC_CHECKPOINT_ROW+word(p+564)+word(p+568)+shot_bytes(r)+word(p+540);
        r->movement_present=word(p+tail);r->movement_slot=word(p+tail+4);r->speed_mode=word(p+tail+8);
        if(version>=14)read_physics(p+tail+12,&r->physics);
        if(version>=15)read_pain(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES,&r->pain);
        if(version>=16)r->holster=word(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES);
        if(version>=18)r->capek_shield_broken=word(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES);
        if(version>=19)read_drone(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES,&r->drone);
        if(version>=20)read_item_drop(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES,&r->item_drop);
        if(version>=21)r->ai_suppressed=word(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES+RF_NPC_CHECKPOINT_ITEM_DROP_BYTES);
        if(version>=22)read_medic(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES+RF_NPC_CHECKPOINT_ITEM_DROP_BYTES+RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES,&r->medic);
        if(version>=23)read_route(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES+RF_NPC_CHECKPOINT_ITEM_DROP_BYTES+RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES+RF_NPC_CHECKPOINT_MEDIC_BYTES,&r->route);
    }
}
static void write_row(unsigned char *p,const rf_npc_checkpoint_record *r,uint32_t version)
{
    uint32_t i,combat_at=RF_NPC_CHECKPOINT_ROW+extension_bytes(r);put(p,r->uid);put(p+4,r->class_id);put(p+8,r->retired);put(p+12,r->flags);put(p+16,r->affiliation);
    put_real(p+20,r->health);put_real(p+24,r->armor);for(i=0;i<3;i++)put_real(p+28+i*4,r->position[i]);
    put_real(p+40,r->yaw);put(p+44,(uint32_t)r->primary);put(p+48,(uint32_t)r->secondary);
    put(p+52,r->drop.state);put(p+56,(uint32_t)r->drop.weapon);put(p+60,(uint32_t)r->drop.quantity);
    for(i=0;i<3;i++)put_real(p+64+i*4,r->drop.position[i]);memcpy(p+76,r->inventory.owned,64);
    for(i=0;i<32;i++)put(p+140+i*4,(uint32_t)r->inventory.reserve[i]);
    for(i=0;i<64;i++)put(p+268+i*4,(uint32_t)r->inventory.loaded[i]);put(p+524,(uint32_t)r->ai_mode);
    for(i=0;i<3;i++)put_real(p+528+i*4,r->eye_angles[i]);
    put(p+540,animation_bytes(r));put(p+544,r->controller_uid);put(p+548,r->combat_alert);
    put(p+552,r->dead_pose);put(p+556,r->death_flags_810);put(p+560,(uint32_t)r->death_action);
    put(p+564,extension_bytes(r));put(p+568,combat_bytes(r));
    put(p+572,r->support_uid);for(i=0;i<3;i++)put_real(p+576+4*i,r->support_velocity[i]);
    put(p+588,r->shot_count);put(p+592,r->shot_rng);put(p+596,r->shield_disabled);
    if(extension_bytes(r)){
     const uint32_t *fields[14]={&r->move.active,&r->move.event,&r->move.follow,&r->move.path_index,
        &r->move.path_mode,&r->move.path_reverse,&r->move.path_count,&r->move.route_index,
        &r->move.retry,&r->move.retained_count,&r->move.retained_nodes[0],&r->move.retained_nodes[1],
        &r->move.retained_nodes[2],&r->move.retained_nodes[3]};
     for(i=0;i<14;i++)put(p+RF_NPC_CHECKPOINT_ROW+4*i,*fields[i]);
     for(i=0;i<3;i++)put_real(p+RF_NPC_CHECKPOINT_ROW+56+4*i,r->move.target[i]);put_real(p+RF_NPC_CHECKPOINT_ROW+68,r->move.fall_speed);
     for(i=0;i<3;i++){put_real(p+RF_NPC_CHECKPOINT_ROW+72+4*i,r->move.route_start[i]);put_real(p+RF_NPC_CHECKPOINT_ROW+84+4*i,r->move.route_goal[i]);
         put_real(p+RF_NPC_CHECKPOINT_ROW+96+4*i,r->look_command[i]);put_real(p+RF_NPC_CHECKPOINT_ROW+108+4*i,r->look_delta[i]);
         put_real(p+RF_NPC_CHECKPOINT_ROW+120+4*i,r->look_offset[i]);put_real(p+RF_NPC_CHECKPOINT_ROW+132+4*i,r->look_vector[i]);
         put_real(p+RF_NPC_CHECKPOINT_ROW+156+4*i,r->look.position[i]);}
     put(p+RF_NPC_CHECKPOINT_ROW+144,r->look.active);put(p+RF_NPC_CHECKPOINT_ROW+148,r->look.event);put(p+RF_NPC_CHECKPOINT_ROW+152,r->look.target_uid);
    }
    if(combat_bytes(r)){
        put(p+combat_at,r->combat.active);put(p+combat_at+4,r->combat.event);
        put(p+combat_at+8,r->combat.burst);put(p+combat_at+12,r->combat.due_remaining);
        put(p+combat_at+16,r->combat.reload_remaining);put(p+combat_at+20,(uint32_t)r->combat.reload_weapon);
        for(i=0;i<3;i++)put_real(p+combat_at+24+4*i,r->combat.point[i]);
        put(p+combat_at+36,r->combat.spread_rng);
    }
    for(i=0;i<r->shot_count;i++){
        uint32_t j,at=combat_at+combat_bytes(r)+24*i;const rf_npc_checkpoint_shot *shot=r->shots+i;
        put(p+at,shot->event);put(p+at+4,shot->remaining);put(p+at+8,shot->mode);
        for(j=0;j<3;j++)put_real(p+at+12+4*j,shot->point[j]);
    }
    if(r->animation_present)write_animation(p+combat_at+combat_bytes(r)+shot_bytes(r),r);
    if(version>=13){
        uint32_t tail=combat_at+combat_bytes(r)+shot_bytes(r)+animation_bytes(r);
        put(p+tail,r->movement_present);put(p+tail+4,r->movement_slot);put(p+tail+8,r->speed_mode);
        if(version>=14)write_physics(p+tail+12,&r->physics);
        if(version>=15)write_pain(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES,&r->pain);
        if(version>=16)put(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES,r->holster);
        if(version>=18)put(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES,r->capek_shield_broken);
        if(version>=19)write_drone(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES,&r->drone);
        if(version>=20)write_item_drop(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES,&r->item_drop);
        if(version>=21)put(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES+RF_NPC_CHECKPOINT_ITEM_DROP_BYTES,r->ai_suppressed);
        if(version>=22)write_medic(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES+RF_NPC_CHECKPOINT_ITEM_DROP_BYTES+RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES,&r->medic);
        if(version>=23)write_route(p+tail+12+RF_NPC_CHECKPOINT_PHYSICS_BYTES+RF_NPC_CHECKPOINT_PAIN_BYTES+RF_NPC_CHECKPOINT_HOLSTER_BYTES+RF_NPC_CHECKPOINT_CAPEK_BYTES+RF_NPC_CHECKPOINT_DRONE_BYTES+RF_NPC_CHECKPOINT_ITEM_DROP_BYTES+RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES+RF_NPC_CHECKPOINT_MEDIC_BYTES,&r->route);
    }
}
int rf_npc_checkpoint_encode(const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    const rf_npc_checkpoint_record *rows,uint32_t count,void *output,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=output;uint32_t i,bytes,at,version=10,pain_seen=0,pain_random=0;int status;
    if(!identity||!output||!written||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    status=catalog_valid(c);if(status)return status;
    bytes=64+count*RF_NPC_CHECKPOINT_ROW;if(bytes>capacity)return RF_RANGE;
    for(i=0;i<count;i++){
        status=valid(rows+i,c);if(status)return status;if(i&&rows[i-1].uid>=rows[i].uid)return RF_FORMAT;
        if(rows[i].pain.present){
            if(pain_seen&&pain_random!=rows[i].pain.random)return RF_FORMAT;
            pain_seen=1;pain_random=rows[i].pain.random;if(version<15)version=15;
        }
        else if(rows[i].physics.present&&version<14)version=14;
        else if(rows[i].movement_present&&version<13)version=13;
        else if(rows[i].combat.active==4&&version<12)version=12;
        else if(rows[i].combat.active==3&&version<11)version=11;
        if(rows[i].holster&&version<16)version=16;
        if(rows[i].physics.present&&rows[i].dead_pose&&version<17)version=17;
        if(rows[i].capek_shield_broken&&version<18)version=18;
        if(rows[i].drone.kind&&version<19)version=19;
        if(rows[i].item_drop.state&&version<20)version=20;
        if(rows[i].ai_suppressed&&version<21)version=21;
        if((rows[i].medic.reserve_seen||rows[i].medic.child_state)&&version<22)version=22;
        if(rows[i].route.flags&&version<23)version=23;
        bytes+=extension_bytes(rows+i)+combat_bytes(rows+i)+shot_bytes(rows+i)+animation_bytes(rows+i);
    }
    if(version>=13)bytes+=count*12;
    if(version>=14)bytes+=count*RF_NPC_CHECKPOINT_PHYSICS_BYTES;
    if(version>=15)bytes+=count*RF_NPC_CHECKPOINT_PAIN_BYTES;
    if(version>=16)bytes+=count*RF_NPC_CHECKPOINT_HOLSTER_BYTES;
    if(version>=18)bytes+=count*RF_NPC_CHECKPOINT_CAPEK_BYTES;
    if(version>=19)bytes+=count*RF_NPC_CHECKPOINT_DRONE_BYTES;
    if(version>=20)bytes+=count*RF_NPC_CHECKPOINT_ITEM_DROP_BYTES;
    if(version>=21)bytes+=count*RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES;
    if(version>=22)bytes+=count*RF_NPC_CHECKPOINT_MEDIC_BYTES;
    if(version>=23)bytes+=count*RF_NPC_CHECKPOINT_ROUTE_BYTES;
    if(bytes>capacity)return RF_RANGE;
    memset(p,0,bytes);memcpy(p,"RFNC",4);put(p+4,version);put(p+8,bytes);put(p+16,count);memcpy(p+24,identity,32);put(p+56,c->hash);
    for(i=0,at=64;i<count;i++){write_row(p+at,rows+i,version);at+=RF_NPC_CHECKPOINT_ROW+extension_bytes(rows+i)+combat_bytes(rows+i)+shot_bytes(rows+i)+animation_bytes(rows+i)+(version>=13?12:0)+(version>=14?RF_NPC_CHECKPOINT_PHYSICS_BYTES:0)+(version>=15?RF_NPC_CHECKPOINT_PAIN_BYTES:0)+(version>=16?RF_NPC_CHECKPOINT_HOLSTER_BYTES:0)+(version>=18?RF_NPC_CHECKPOINT_CAPEK_BYTES:0)+(version>=19?RF_NPC_CHECKPOINT_DRONE_BYTES:0)+(version>=20?RF_NPC_CHECKPOINT_ITEM_DROP_BYTES:0)+(version>=21?RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES:0)+(version>=22?RF_NPC_CHECKPOINT_MEDIC_BYTES:0)+(version>=23?RF_NPC_CHECKPOINT_ROUTE_BYTES:0);}
    put(p+12,hash(p,bytes));*written=bytes;return RF_OK;
}
static int row_span(const unsigned char *p,uint32_t available,uint32_t version,uint32_t *span)
{
    uint32_t base=version==1?RF_NPC_CHECKPOINT_ROW_V1:version==2?RF_NPC_CHECKPOINT_ROW_V2:version==3?RF_NPC_CHECKPOINT_ROW_V3:version==4?RF_NPC_CHECKPOINT_ROW_V4:version==5?RF_NPC_CHECKPOINT_ROW_V5:version==6?RF_NPC_CHECKPOINT_ROW_V6:version==7?RF_NPC_CHECKPOINT_ROW_V7:version==8?RF_NPC_CHECKPOINT_ROW_V8:version==9?RF_NPC_CHECKPOINT_ROW_V9:RF_NPC_CHECKPOINT_ROW,n=0,extension=0,combat=0,shots=0;
    if(available<base)return RF_FORMAT;
    if(version>=7){extension=word(p+564);if((extension!=0&&extension!=RF_NPC_CHECKPOINT_EXTENSION_BYTES)||extension>available-base)return RF_FORMAT;}
    if(version>=8){combat=word(p+568);if((combat!=0&&combat!=RF_NPC_CHECKPOINT_COMBAT_BYTES)||combat>available-base-extension)return RF_FORMAT;}
    if(version>=10){uint32_t count=word(p+588);if(count>16)return RF_FORMAT;
        shots=count*24;if(shots>available-base-extension-combat)return RF_FORMAT;}
    if(version>=3){n=word(p+540);if(n){
        uint32_t count;if(n<RF_NPC_CHECKPOINT_ANIMATION_BASE||n>RF_NPC_CHECKPOINT_ANIMATION_BASE+192||n>available-base-extension-combat-shots)return RF_FORMAT;
        count=word(p+base+extension+combat+shots+16);if(count>16||n!=RF_NPC_CHECKPOINT_ANIMATION_BASE+12*count)return RF_FORMAT;
    }}
    *span=base+extension+combat+shots+n;
    if(version>=13){if(available-*span<12)return RF_FORMAT;*span+=12;}
    if(version>=14){if(available-*span<RF_NPC_CHECKPOINT_PHYSICS_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_PHYSICS_BYTES;}
    if(version>=15){if(available-*span<RF_NPC_CHECKPOINT_PAIN_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_PAIN_BYTES;}
    if(version>=16){if(available-*span<RF_NPC_CHECKPOINT_HOLSTER_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_HOLSTER_BYTES;}
    if(version>=18){if(available-*span<RF_NPC_CHECKPOINT_CAPEK_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_CAPEK_BYTES;}
    if(version>=19){if(available-*span<RF_NPC_CHECKPOINT_DRONE_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_DRONE_BYTES;}
    if(version>=20){if(available-*span<RF_NPC_CHECKPOINT_ITEM_DROP_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_ITEM_DROP_BYTES;}
    if(version>=21){if(available-*span<RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_AI_SUPPRESSION_BYTES;}
    if(version>=22){if(available-*span<RF_NPC_CHECKPOINT_MEDIC_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_MEDIC_BYTES;}
    if(version>=23){if(available-*span<RF_NPC_CHECKPOINT_ROUTE_BYTES)return RF_FORMAT;*span+=RF_NPC_CHECKPOINT_ROUTE_BYTES;}
    return RF_OK;
}
static int decode(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *out_count,uint32_t publish)
{
    const unsigned char *p=data;rf_npc_checkpoint_record r;uint32_t i,count,version,span,at,previous=0,pain_seen=0,pain_random=0;int status;
    if(!data||!identity||!out_count)return RF_RANGE;
    status=catalog_valid(c);if(status)return status;
    if(bytes<64||bytes>64+RF_NPC_CHECKPOINT_MAX_COUNT*RF_NPC_CHECKPOINT_ROW_MAX||memcmp(p,"RFNC",4)||word(p+4)<1||word(p+4)>23||
       word(p+8)!=bytes||word(p+20)||word(p+60)||word(p+56)!=c->hash||memcmp(p+24,identity,32)||word(p+12)!=hash(p,bytes))return RF_FORMAT;
    version=word(p+4);count=word(p+16);if(count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_FORMAT;
    if(publish&&(count>capacity||(count&&!rows)))return RF_RANGE;
    for(i=0,at=64;i<count;i++){
        status=row_span(p+at,bytes-at,version,&span);if(status)return status;
        read_row(p+at,version,&r);
        if((version<11&&r.combat.active==3)||(version<12&&r.combat.active==4)||
           (version<17&&r.physics.present&&r.dead_pose))return RF_FORMAT;
        status=valid(&r,c);if(status)return status;
        if(r.pain.present){
            if(pain_seen&&pain_random!=r.pain.random)return RF_FORMAT;
            pain_seen=1;pain_random=r.pain.random;
        }
        if(i&&previous>=r.uid)return RF_FORMAT;previous=r.uid;at+=span;
    }
    if(at!=bytes)return RF_FORMAT;
    if(publish)for(i=0,at=64;i<count;i++){(void)row_span(p+at,bytes-at,version,&span);read_row(p+at,version,rows+i);at+=span;}
    *out_count=count;return RF_OK;
}
int rf_npc_checkpoint_decode(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{return decode(data,bytes,identity,c,rows,capacity,count,1);}
int rf_npc_checkpoint_preflight(const void *data,uint32_t bytes,const unsigned char identity[32],const rf_npc_checkpoint_catalog *c,uint32_t *count)
{return decode(data,bytes,identity,c,NULL,0,count,0);}
