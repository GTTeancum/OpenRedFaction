#ifndef RF_SCENE_WORLD_ENVIRONMENT_CHECKPOINT_INCLUDED
#define RF_SCENE_WORLD_ENVIRONMENT_CHECKPOINT_INCLUDED
#include "rf/prepare_memory.h"
#include "rf/prepare_storage.h"
/* RFEN9: identity-bound gravity, force-volume state, changed navigation radii,
 * blackout timer, player support mover UID and portable cutscene clock/camera. Navigation rows use authored UIDs and
 * raw radius bits, never indices. Monitor rows bind authored UIDs; the RFEN6 tail preserves the
 * 30-frame enemy sight schedule and gunfire random state. RFEN7 appends the rotating player's
 * support-point velocity only when needed. RFEN8 adds applied monitor camera lists, selection,
 * remaining cycle ticks and sticky continuous state, with an explicit support-point presence word.
 * RFEN9 appends a32-byte section-bound sky owner after the monitor extension.
 * Current output always uses explicit support/monitor records; RFEN1-8 remain readable.
 * No handles, absolute clocks or images are serialized.
 * Queued level handoffs are not admitted; other presentation state is separate. */
enum {SCENE_ENV_HEADER=80,SCENE_ENV_ROW=16,SCENE_ENV_NAV_ROW=8,
    SCENE_ENV_FORM_BYTES=24,SCENE_ENV_MONITOR_HEADER=24,SCENE_ENV_MONITOR_ROW=20,
    SCENE_ENV_REPLAY_BYTES=8,SCENE_ENV_SUPPORT_POINT_BYTES=12,
    SCENE_ENV_SUPPORT_STATE_BYTES=16,SCENE_ENV_MONITOR_CYCLE_HEADER=8,SCENE_ENV_MONITOR_CYCLE_ROW=24,
    SCENE_ENV_SKYBOX_BYTES=32,
    SCENE_ENV_FORCE_MAX=65536/sizeof(rf_physics_force_region),SCENE_ENV_NAV_MAX=65536};
typedef struct scene_environment_entry {
    uint32_t slot,active;float strength;rf_physics_force_region before;
} scene_environment_entry;
typedef struct scene_environment_nav_entry {
    uint32_t slot,uid,before,after,baseline;
} scene_environment_nav_entry;
typedef struct scene_world_environment_stage {
    rf_physics_gravity before,next;rf_physics_force_region *owner;uint32_t count,bytes;
    rf_level_navigation_node *navigation;uint32_t navigation_count,nav_count;
    rf_cutscene_runtime cutscene_before,cutscene_next;
    campaign_player_form_state form_before,form_next;
    int32_t blackout_before,blackout_next;uint32_t support_uid,support_point_present;
    float support_point_velocity[3];
    uint32_t sight_before,sight_next,spread_before,spread_next;
    rf_scene_skybox_state skybox_before,skybox_next;
    uint32_t monitor_count,monitor_before_count,monitor_before_words[5],monitor_next_words[5];
    uint32_t monitor_portable,monitor_epoch_before;
    scene_monitor_binding monitor_before[SCENE_MONITOR_BINDING_LIMIT],monitor_next[SCENE_MONITOR_BINDING_LIMIT];
    scene_environment_entry rows[];
} scene_world_environment_stage;
/* The public owner contract is32 bytes; the two snapshots stay within the
 * existing environment allocation and whole-restore budget. */
_Static_assert(sizeof(rf_scene_skybox_state)==SCENE_ENV_SKYBOX_BYTES,"RFEN9 skybox state budget");
static int scene_environment_skybox_legacy(const rf_scene_skybox_state *target,rf_scene_skybox_state *out)
{
    /* RFEN1-8 never encoded applied sky state. Preserve only the target's
     * admitted geometry identity, then use the documented fresh-section
     * baseline. Do not inherit live rotation or replay a historical event. */
    *out=*target;out->axis='x';out->rate=0;out->phase=0;
    return rf_scene_skybox_validate(out);
}
static int scene_environment_skybox_same(const rf_scene_skybox_state *a,const rf_scene_skybox_state *b)
{
    /* Compare logical fields, never ABI padding or derived renderer resources. */
    return a->version==b->version&&a->present==b->present&&
        a->level_identity==b->level_identity&&a->room_uid==b->room_uid&&
        a->source_identity==b->source_identity&&a->axis==b->axis&&
        !memcmp(&a->rate,&b->rate,4)&&!memcmp(&a->phase,&b->phase,4);
}
static int scene_environment_skybox_owner(const rf_scene_skybox_state *a,const rf_scene_skybox_state *b)
{
    return a->version==b->version&&a->present==b->present&&
        a->level_identity==b->level_identity&&a->room_uid==b->room_uid&&
        a->source_identity==b->source_identity;
}
static void scene_environment_skybox_encode(unsigned char *p,const rf_scene_skybox_state *state)
{
    scene_history_put(p,state->version);scene_history_put(p+4,state->present);
    scene_history_put(p+8,state->level_identity);scene_history_put(p+12,state->room_uid);
    scene_history_put(p+16,state->source_identity);scene_history_put(p+20,state->axis);
    scene_history_put_float(p+24,state->rate);scene_history_put_float(p+28,state->phase);
}
static int scene_environment_skybox_decode(const unsigned char *p,rf_scene_skybox_state *state)
{
    memset(state,0,sizeof(*state));state->version=scene_history_word(p);
    state->present=scene_history_word(p+4);state->level_identity=scene_history_word(p+8);
    state->room_uid=scene_history_word(p+12);state->source_identity=scene_history_word(p+16);
    state->axis=scene_history_word(p+20);state->rate=scene_history_float(p+24);
    state->phase=scene_history_float(p+28);
    /* The renderer owner is the sole authority for units, bounds and canonical
     * no-sky state. Its pure validator also binds the admitted target scene. */
    return RF_OK; /* Common candidate validation follows legacy/current decode. */
}
static scene_environment_nav_entry *scene_environment_nav_rows(scene_world_environment_stage *s)
{return (scene_environment_nav_entry *)(s->rows+s->count);}
/* The single budgeted stage allocation owns both snapshots after the nav rows. */
static scene_monitor_cameras *scene_environment_monitor_before(scene_world_environment_stage *s)
{return (scene_monitor_cameras *)(scene_environment_nav_rows(s)+s->nav_count);}
static scene_monitor_cameras *scene_environment_monitor_next(scene_world_environment_stage *s)
{return scene_environment_monitor_before(s)+s->monitor_before_count;}
static uint32_t scene_environment_geometry(const rf_physics_force_region *r)
{
    unsigned char bytes[100];uint32_t i,at=12;
    scene_history_put(bytes,r->shape);scene_history_put(bytes+4,r->uid);scene_history_put(bytes+8,r->flags);
#define ENV_VECTOR(field,n) for(i=0;i<(n);i++){scene_history_put_float(bytes+at,r->field[i]);at+=4;}
    ENV_VECTOR(center,3);ENV_VECTOR(matrix,9);scene_history_put_float(bytes+at,r->radius_squared);at+=4;
    ENV_VECTOR(minimum,3);ENV_VECTOR(maximum,3);ENV_VECTOR(size,3);
#undef ENV_VECTOR
    /* All geometry bytes matter; unlike envelope hashes there is no checksum field. */
    {uint32_t hash=2166136261u;for(i=0;i<sizeof(bytes);i++)hash=(hash^bytes[i])*16777619u;return hash;}
}
static int scene_environment_gravity_valid(const rf_physics_gravity *g)
{return isfinite(g->acceleration)&&g->vector[0]==0&&g->vector[2]==0&&g->vector[1]==-g->acceleration?RF_OK:RF_FORMAT;}
static int scene_environment_monitor_valid(const scene_monitor_binding *bindings,uint32_t count,const uint32_t words[5])
{
    uint32_t i,j,cameras=0,hash=2166136261u;
    if(count>SCENE_MONITOR_BINDING_LIMIT||words[0]!=count)return RF_FORMAT;
    for(i=0;i<count;i++){
        if(bindings[i].authored_header_byte>255||!campaign_monitor_binding_valid(bindings+i))return RF_FORMAT;
        for(j=0;j<i;j++)if(bindings[j].screen_uid==bindings[i].screen_uid)return RF_FORMAT;
        cameras+=bindings[i].camera_uid!=0;
        hash=npc_hash_bytes(hash,bindings+i,sizeof(*bindings));
    }
    return words[1]==cameras&&words[3]==hash?RF_OK:RF_FORMAT;
}
static int scene_environment_monitor_cameras_valid(const scene_monitor_binding *binding,const scene_monitor_cameras *list)
{
    uint32_t i,j;
    if(list->count>SCENE_MONITOR_CAMERAS||list->clock_valid>1||list->continuous>1||list->rebase>1||
       (list->count?list->current>=list->count:list->current!=0)||
       binding->camera_uid!=(list->count?list->uids[0]:0)||
       (binding->authored_header_byte==1&&!list->continuous)||
       (!list->clock_valid&&(list->clock||list->rebase||list->current)))return RF_FORMAT;
    for(i=0;i<list->count;i++){
        if(!list->uids[i]||!campaign_monitor_camera(list->uids[i]))return RF_FORMAT;
        for(j=0;j<i;j++)if(list->uids[j]==list->uids[i])return RF_FORMAT;
    }
    return RF_OK;
}
static void scene_environment_monitor_cycles_encode(unsigned char *p,uint32_t bytes,uint32_t frame)
{
    uint32_t i,j,at=SCENE_ENV_MONITOR_CYCLE_HEADER;
    scene_history_put(p,bytes);scene_history_put(p+4,campaign_monitor_count);
    for(i=0;i<campaign_monitor_count;i++){
        const scene_monitor_binding *binding=campaign_monitor_bindings+i;
        const scene_monitor_cameras *list=campaign_monitor_cameras+i;
        uint32_t ticks=campaign_monitor_cycle_ticks(binding),current=list->current,remaining=0;
        if(list->clock_valid){
            uint32_t elapsed=list->rebase?list->clock:frame-list->clock,steps=elapsed/ticks;
            if(list->count)current=(current+steps%list->count)%list->count;
            remaining=ticks-elapsed%ticks;
        }
        scene_history_put(p+at,binding->screen_uid);scene_history_put(p+at+4,list->count);
        scene_history_put(p+at+8,list->count?list->uids[current]:0);scene_history_put(p+at+12,remaining);
        scene_history_put(p+at+16,list->clock_valid);scene_history_put(p+at+20,list->continuous);
        at+=SCENE_ENV_MONITOR_CYCLE_ROW;
        for(j=0;j<list->count;j++,at+=4)scene_history_put(p+at,list->uids[j]);
    }
}
static int scene_environment_monitor_cycles_decode(const unsigned char *p,uint32_t bytes,scene_world_environment_stage *s)
{
    uint32_t i,j,at=SCENE_ENV_MONITOR_CYCLE_HEADER;
    if(bytes<at||scene_history_word(p)!=bytes||scene_history_word(p+4)!=s->monitor_count)return RF_FORMAT;
    for(i=0;i<s->monitor_count;i++){
        const scene_monitor_binding *binding=s->monitor_next+i;
        scene_monitor_cameras *list=scene_environment_monitor_next(s)+i;
        uint32_t selected,remaining,ticks=campaign_monitor_cycle_ticks(binding);
        if(bytes-at<SCENE_ENV_MONITOR_CYCLE_ROW||scene_history_word(p+at)!=binding->screen_uid)return RF_FORMAT;
        list->count=scene_history_word(p+at+4);selected=scene_history_word(p+at+8);
        remaining=scene_history_word(p+at+12);list->clock_valid=scene_history_word(p+at+16);
        list->continuous=scene_history_word(p+at+20);at+=SCENE_ENV_MONITOR_CYCLE_ROW;
        if(list->count>SCENE_MONITOR_CAMERAS||list->count>(bytes-at)/4||list->clock_valid>1||
           (list->clock_valid?(!remaining||remaining>ticks):remaining!=0))return RF_FORMAT;
        list->current=list->count?UINT32_MAX:0;
        for(j=0;j<list->count;j++,at+=4){
            list->uids[j]=scene_history_word(p+at);if(list->uids[j]==selected)list->current=j;
        }
        if(!list->count&&selected)return RF_FORMAT;
        /* Preserve phase through fresh-session and same-session loads without
         * serializing a process frame counter or advancing simulation here. */
        list->rebase=list->clock_valid;list->clock=list->clock_valid?ticks-remaining:0;
        if(scene_environment_monitor_cameras_valid(binding,list))return RF_FORMAT;
    }
    return at==bytes?RF_OK:RF_FORMAT;
}
static inline int scene_world_environment_encode(const unsigned char identity[32],int32_t now,void *out,uint32_t capacity,uint32_t *written)
{
    unsigned char *p=out;uint32_t i,j,bytes,nav_count=0,support_uid=0,support_point_present=0,tail_bytes,tail_offset;
    uint32_t monitor_bytes=SCENE_ENV_MONITOR_CYCLE_HEADER,version=9;
    rf_scene_skybox_state skybox;
    float support_point_velocity[3]={0};uint64_t required;int32_t blackout_remaining=0;int status;
    if(!identity||!out||!written||campaign_forces.count>SCENE_ENV_FORCE_MAX||
       (campaign_forces.count&&!campaign_forces.items)||campaign_navigation.count>SCENE_ENV_NAV_MAX||
       (campaign_navigation.count&&!campaign_navigation.nodes))return RF_RANGE;
    if(rf_scene_level_transition.pending)return RF_NOT_FOUND;
    status=rf_scene_skybox_capture(now,&skybox);if(status)return status;
    status=rf_scene_skybox_validate(&skybox);if(status)return status;
    status=scene_environment_gravity_valid(&scene_gravity);if(status)return status;
    for(i=0;i<campaign_forces.count;i++){
        if(!isfinite(campaign_forces.items[i].strength))return RF_FORMAT;
        for(j=0;j<i;j++)if(campaign_forces.items[j].uid==campaign_forces.items[i].uid)return RF_FORMAT;
    }
    for(i=0;i<campaign_navigation.count;i++){
        const rf_level_navigation_node *node=campaign_navigation.nodes+i;uint32_t bits;
        memcpy(&bits,&node->candidate.radius,4);
        if(bits==node->candidate.retained_018)continue;
        if(node->uid==UINT32_MAX||bits!=0)return RF_FORMAT;
        for(j=0;j<i;j++)if(campaign_navigation.nodes[j].uid==node->uid)return RF_FORMAT;
        ++nav_count;
    }
    status=scene_environment_monitor_valid(campaign_monitor_bindings,campaign_monitor_count,rf_scene_monitor_bindings);
    if(status)return status;
    if(campaign_monitor_count){
        monitor_bytes=SCENE_ENV_MONITOR_CYCLE_HEADER;
        for(i=0;i<campaign_monitor_count;i++){
            status=scene_environment_monitor_cameras_valid(campaign_monitor_bindings+i,campaign_monitor_cameras+i);
            if(status)return status;
            monitor_bytes+=SCENE_ENV_MONITOR_CYCLE_ROW+campaign_monitor_cameras[i].count*4;
        }
    }
    status=scene_checkpoint_player_support_state(scene_actor_collision_owner,&support_uid,
        support_point_velocity,&support_point_present);if(status)return status;
    if(support_point_present>1||(support_point_present&&!support_uid))return RF_FORMAT;
    if(support_point_present)for(i=0;i<3;i++)
        if(!isfinite(support_point_velocity[i])||fabsf(support_point_velocity[i])>10000.f)return RF_FORMAT;
    tail_bytes=SCENE_ENV_REPLAY_BYTES+SCENE_ENV_SUPPORT_STATE_BYTES+monitor_bytes+SCENE_ENV_SKYBOX_BYTES;
    required=SCENE_ENV_HEADER+(uint64_t)campaign_forces.count*SCENE_ENV_ROW+
        (uint64_t)nav_count*SCENE_ENV_NAV_ROW+RF_CUTSCENE_CHECKPOINT_BYTES+SCENE_ENV_FORM_BYTES+
        SCENE_ENV_MONITOR_HEADER+(uint64_t)campaign_monitor_count*SCENE_ENV_MONITOR_ROW+tail_bytes;
    if(required>capacity||required>UINT32_MAX)return RF_RANGE;bytes=(uint32_t)required;
    tail_offset=bytes-tail_bytes;
    memset(p,0,bytes);memcpy(p,"RFEN",4);scene_history_put(p+4,version);scene_history_put(p+8,bytes);
    scene_history_put(p+16,campaign_forces.count);scene_history_put(p+20,nav_count);memcpy(p+24,identity,32);
    if(campaign_blackout_deadline>=0){
        status=rf_timer_remaining(campaign_blackout_deadline,now,&blackout_remaining);if(status)return status;
        if(blackout_remaining>30000)return RF_FORMAT;
        if(blackout_remaining>0)scene_history_put(p+72,(uint32_t)blackout_remaining+1);
    }
    scene_history_put(p+76,support_uid);
    scene_history_put_float(p+56,scene_gravity.acceleration);
    for(i=0;i<3;i++)scene_history_put_float(p+60+i*4,scene_gravity.vector[i]);
    for(i=0;i<campaign_forces.count;i++){
        const rf_physics_force_region *r=campaign_forces.items+i;unsigned char *row=p+80+i*16;
        scene_history_put(row,r->uid);scene_history_put(row+4,r->active);scene_history_put_float(row+8,r->strength);scene_history_put(row+12,scene_environment_geometry(r));
    }
    for(i=0,j=0;i<campaign_navigation.count;i++){
        const rf_level_navigation_node *node=campaign_navigation.nodes+i;unsigned char *row;uint32_t bits;
        memcpy(&bits,&node->candidate.radius,4);if(bits==node->candidate.retained_018)continue;
        row=p+80+campaign_forces.count*16+j++*SCENE_ENV_NAV_ROW;
        scene_history_put(row,node->uid);scene_history_put(row+4,bits);
    }
    status=rf_cutscene_checkpoint_encode(&campaign_cutscene_runtime,now,
        p+80+campaign_forces.count*SCENE_ENV_ROW+nav_count*SCENE_ENV_NAV_ROW,RF_CUTSCENE_CHECKPOINT_BYTES);if(status)return status;
    if(campaign_player_form.active){
        unsigned char *form=p+80+campaign_forces.count*SCENE_ENV_ROW+nav_count*SCENE_ENV_NAV_ROW+RF_CUTSCENE_CHECKPOINT_BYTES;uint32_t armor_bits;
        if(campaign_player_form.compromised>1||campaign_player_form.return_slot>=scene_weapon_slots()||
           campaign_player_form.return_unarmed>1||!isfinite(campaign_player_form.normal_class_armor)||
           campaign_player_form.normal_class_armor<0)return RF_FORMAT;
        scene_history_put(form,1);scene_history_put(form+4,campaign_player_form.variant);
        scene_history_put(form+8,campaign_player_form.compromised);
        scene_history_put(form+12,campaign_player_form.return_slot);
        scene_history_put(form+16,campaign_player_form.return_unarmed);
        memcpy(&armor_bits,&campaign_player_form.normal_class_armor,4);scene_history_put(form+20,armor_bits);
    }
    {
        unsigned char *monitor=p+tail_offset-SCENE_ENV_MONITOR_HEADER-campaign_monitor_count*SCENE_ENV_MONITOR_ROW;
        scene_history_put(monitor,campaign_monitor_count);
        for(i=0;i<5;i++)scene_history_put(monitor+4+i*4,rf_scene_monitor_bindings[i]);
        for(i=0;i<campaign_monitor_count;i++){
            const scene_monitor_binding *b=campaign_monitor_bindings+i;unsigned char *row=monitor+SCENE_ENV_MONITOR_HEADER+i*SCENE_ENV_MONITOR_ROW;
            scene_history_put(row,b->screen_uid);scene_history_put(row+4,b->camera_uid);
            scene_history_put(row+8,b->raw_value1);scene_history_put(row+12,b->authored_header_byte);
            scene_history_put_float(row+16,b->refresh_seconds);
        }
    }
    scene_history_put(p+tail_offset,(combat_frame%30+campaign_enemy_sight_phase_offset)%30);
    scene_history_put(p+tail_offset+4,campaign_enemy_spread_random.value);
    {
        unsigned char *support=p+tail_offset+SCENE_ENV_REPLAY_BYTES;
        scene_history_put(support,support_point_present);
        if(support_point_present)for(i=0;i<3;i++)scene_history_put_float(support+4+i*4,support_point_velocity[i]);
        scene_environment_monitor_cycles_encode(support+SCENE_ENV_SUPPORT_STATE_BYTES,monitor_bytes,
            combat_frame==UINT32_MAX?0:combat_frame);
    }
    scene_environment_skybox_encode(p+bytes-SCENE_ENV_SKYBOX_BYTES,&skybox);
    scene_history_put(p+12,scene_history_hash(p,bytes));*written=bytes;return RF_OK;
}
static inline void scene_world_environment_close_storage(scene_world_environment_stage **s,const rf_prepare_storage *provider)
{if(s){(void)rf_prepare_storage_release(provider,NULL,NULL,*s,RF_OK);*s=NULL;}}
static inline void scene_world_environment_close(scene_world_environment_stage **s)
{scene_world_environment_close_storage(s,NULL);}
static inline int scene_world_environment_prepare_storage_accounted(const unsigned char identity[32],int32_t now,const void *data,uint32_t bytes,
    uint32_t budget,scene_world_environment_stage **out,const rf_prepare_memory *memory,const rf_prepare_storage *provider)
{
    const unsigned char *p=data;scene_world_environment_stage *s;uint32_t count,nav_count,version,i,j,k,tail_bytes,tail_offset;
    uint32_t saved_monitor_count=0;
    uint64_t cost;int status=RF_OK;
    if(!identity||!data||!out||*out)return RF_RANGE;
    if(rf_scene_level_transition.pending)return RF_NOT_FOUND;
    if(bytes<80||memcmp(p,"RFEN",4)||scene_history_word(p+8)!=bytes||
       scene_history_word(p+12)!=scene_history_hash(p,bytes)||memcmp(p+24,identity,32))return RF_FORMAT;
    version=scene_history_word(p+4);if(version<1||version>9)return RF_FORMAT;
    if((version<3&&(scene_history_word(p+72)||scene_history_word(p+76)))||scene_history_word(p+72)>30001||
       scene_history_word(p+76)==UINT32_MAX)return RF_FORMAT;
    count=scene_history_word(p+16);nav_count=scene_history_word(p+20);
    tail_bytes=(version>=6?SCENE_ENV_REPLAY_BYTES:0)+(version>=8?
        SCENE_ENV_SUPPORT_STATE_BYTES+SCENE_ENV_MONITOR_CYCLE_HEADER:version==7?SCENE_ENV_SUPPORT_POINT_BYTES:0)+
        (version>=9?SCENE_ENV_SKYBOX_BYTES:0);
    tail_offset=bytes-tail_bytes;
    {uint64_t base=80u+(uint64_t)count*SCENE_ENV_ROW+(uint64_t)nav_count*SCENE_ENV_NAV_ROW+
        (version>=3?RF_CUTSCENE_CHECKPOINT_BYTES:0)+(version>=4?SCENE_ENV_FORM_BYTES:0);
     if(version>=5){if(base+SCENE_ENV_MONITOR_HEADER+tail_bytes>bytes)return RF_FORMAT;
         saved_monitor_count=scene_history_word(p+(size_t)base);
         if(saved_monitor_count>SCENE_MONITOR_BINDING_LIMIT)return RF_FORMAT;
         base+=SCENE_ENV_MONITOR_HEADER+(uint64_t)saved_monitor_count*SCENE_ENV_MONITOR_ROW;
         if(version>=8){
             if(base+tail_bytes>bytes||bytes-base-tail_bytes>
                (uint64_t)saved_monitor_count*(SCENE_ENV_MONITOR_CYCLE_ROW+4*SCENE_MONITOR_CAMERAS))return RF_FORMAT;
             tail_offset=(uint32_t)base;
         }else if(base+tail_bytes!=bytes)return RF_FORMAT;}
     else if(base!=bytes)return RF_FORMAT;}
    if((version==1&&nav_count)||count>SCENE_ENV_FORCE_MAX||count!=campaign_forces.count||
       nav_count>campaign_navigation.count||nav_count>SCENE_ENV_NAV_MAX||
       (count&&!campaign_forces.items)||(nav_count&&!campaign_navigation.nodes)||
       campaign_monitor_count>SCENE_MONITOR_BINDING_LIMIT)return RF_FORMAT;
    cost=sizeof(*s)+(uint64_t)count*sizeof(*s->rows)+(uint64_t)nav_count*sizeof(scene_environment_nav_entry)+
        (uint64_t)(campaign_monitor_count+(version>=8?saved_monitor_count:0))*sizeof(scene_monitor_cameras);
    if(cost>budget||cost>UINT32_MAX)return RF_RANGE;
    {void *allocation=NULL;status=rf_prepare_storage_allocate(provider,memory,NULL,(size_t)cost,&allocation);
     if(status)return status;s=allocation;}s->count=count;s->bytes=(uint32_t)cost;s->owner=campaign_forces.items;
    s->navigation=campaign_navigation.nodes;s->navigation_count=campaign_navigation.count;s->nav_count=nav_count;s->before=scene_gravity;
    s->cutscene_before=campaign_cutscene_runtime;
    s->form_before=campaign_player_form;
    /* The load caller's `now` argument rebases blackout/cutscene to16. Sky
     * snapshots and rebasing use the actual current simulation clock. Capture
     * validates that clock without advancing the owner's phase/clock. */
    status=rf_scene_skybox_capture(campaign_blackout_now,&s->skybox_before);if(status)goto fail;
    status=rf_scene_skybox_validate(&s->skybox_before);if(status)goto fail;
    if(version>=9)status=scene_environment_skybox_decode(p+bytes-SCENE_ENV_SKYBOX_BYTES,&s->skybox_next);
    else status=scene_environment_skybox_legacy(&s->skybox_before,&s->skybox_next);
    if(status)goto fail;
    status=rf_scene_skybox_validate(&s->skybox_next);if(status)goto fail;
    if(!scene_environment_skybox_owner(&s->skybox_before,&s->skybox_next)){status=RF_FORMAT;goto fail;}
    s->monitor_before_count=campaign_monitor_count;
    s->monitor_epoch_before=campaign_monitor_epoch;s->monitor_portable=version>=8;
    memcpy(scene_environment_monitor_before(s),campaign_monitor_cameras,
        campaign_monitor_count*sizeof(scene_monitor_cameras));
    memcpy(s->monitor_before,campaign_monitor_bindings,sizeof(s->monitor_before));
    memcpy(s->monitor_before_words,rf_scene_monitor_bindings,sizeof(s->monitor_before_words));
    s->monitor_count=campaign_monitor_count;
    memcpy(s->monitor_next,campaign_monitor_bindings,sizeof(s->monitor_next));
    memcpy(s->monitor_next_words,rf_scene_monitor_bindings,sizeof(s->monitor_next_words));
    s->blackout_before=campaign_blackout_deadline;s->blackout_next=-1;
    s->sight_before=campaign_enemy_sight_phase_offset;
    s->spread_before=campaign_enemy_spread_random.value;
    s->sight_next=version>=6?scene_history_word(p+tail_offset):0;
    s->spread_next=version>=6?scene_history_word(p+tail_offset+4):campaign_enemy_spread_random.value;
    if(s->sight_next>=30){status=RF_FORMAT;goto fail;}
    s->support_uid=scene_history_word(p+76);
    if(version>=7){
        const unsigned char *support=p+tail_offset+SCENE_ENV_REPLAY_BYTES;
        s->support_point_present=version>=8?scene_history_word(support):1;
        if(s->support_point_present>1||(s->support_point_present&&!s->support_uid)){status=RF_FORMAT;goto fail;}
        if(version>=8)support+=4;
        for(i=0;i<3;i++){
            s->support_point_velocity[i]=scene_history_float(support+i*4);
            if(!isfinite(s->support_point_velocity[i])||fabsf(s->support_point_velocity[i])>10000.f||
               (!s->support_point_present&&scene_history_word(support+i*4))){status=RF_FORMAT;goto fail;}
        }
    }
    if(scene_history_word(p+72)){
        status=rf_timer_set(&s->blackout_next,now,(int32_t)scene_history_word(p+72)-1);if(status)goto fail;
    }
    if(version>=3){status=rf_cutscene_checkpoint_decode(&campaign_cutscene_resources,now,
        p+80+count*SCENE_ENV_ROW+nav_count*SCENE_ENV_NAV_ROW,
        RF_CUTSCENE_CHECKPOINT_BYTES,&s->cutscene_next);if(status)goto fail;}
    if(version>=4){
        const unsigned char *form=p+80+count*SCENE_ENV_ROW+nav_count*SCENE_ENV_NAV_ROW+RF_CUTSCENE_CHECKPOINT_BYTES;
        s->form_next.active=scene_history_word(form);
        s->form_next.variant=scene_history_word(form+4);
        s->form_next.compromised=scene_history_word(form+8);
        s->form_next.return_slot=scene_history_word(form+12);
        s->form_next.return_unarmed=scene_history_word(form+16);
        s->form_next.normal_class_armor=scene_history_float(form+20);
        if(s->form_next.active>1||
           (s->form_next.active?(s->form_next.compromised>1||
                s->form_next.return_slot>=scene_weapon_slots()||s->form_next.return_unarmed>1||
                !isfinite(s->form_next.normal_class_armor)||s->form_next.normal_class_armor<0):
                (s->form_next.variant||s->form_next.compromised||s->form_next.return_slot||
                 s->form_next.return_unarmed||scene_history_word(form+20)))){status=RF_FORMAT;goto fail;}
    }
    if(version>=5){
        const unsigned char *monitor=p+80+count*SCENE_ENV_ROW+nav_count*SCENE_ENV_NAV_ROW+
            RF_CUTSCENE_CHECKPOINT_BYTES+SCENE_ENV_FORM_BYTES;
        s->monitor_count=scene_history_word(monitor);
        for(i=0;i<5;i++)s->monitor_next_words[i]=scene_history_word(monitor+4+i*4);
        memset(s->monitor_next,0,sizeof(s->monitor_next));
        for(i=0;i<s->monitor_count;i++){
            const unsigned char *row=monitor+SCENE_ENV_MONITOR_HEADER+i*SCENE_ENV_MONITOR_ROW;
            scene_monitor_binding *b=s->monitor_next+i;
            b->screen_uid=scene_history_word(row);b->camera_uid=scene_history_word(row+4);
            b->raw_value1=scene_history_word(row+8);b->authored_header_byte=scene_history_word(row+12);
            b->refresh_seconds=scene_history_float(row+16);
            if(b->authored_header_byte>255){status=RF_FORMAT;goto fail;}
        }
        status=scene_environment_monitor_valid(s->monitor_next,s->monitor_count,s->monitor_next_words);
        if(status)goto fail;
    }
    if(version>=8){
        uint32_t offset=tail_offset+SCENE_ENV_REPLAY_BYTES+SCENE_ENV_SUPPORT_STATE_BYTES;
        uint32_t skybox_bytes=version>=9?SCENE_ENV_SKYBOX_BYTES:0;
        status=scene_environment_monitor_cycles_decode(p+offset,bytes-offset-skybox_bytes,s);if(status)goto fail;
    }
    s->next.acceleration=scene_history_float(p+56);for(i=0;i<3;i++)s->next.vector[i]=scene_history_float(p+60+i*4);
    status=scene_environment_gravity_valid(&s->next);if(status)goto fail;
    for(i=0;i<count;i++){
        const unsigned char *r=p+80+i*16;scene_environment_entry *e=s->rows+i;uint32_t uid=scene_history_word(r),found=0;
        for(j=0;j<count;j++)if(campaign_forces.items[j].uid==uid){e->slot=j;found++;}
        if(found!=1){status=RF_FORMAT;goto fail;}
        for(k=0;k<i;k++)if(s->rows[k].slot==e->slot){status=RF_FORMAT;goto fail;}
        e->before=campaign_forces.items[e->slot];e->active=scene_history_word(r+4);e->strength=scene_history_float(r+8);
        if(!isfinite(e->strength)||scene_history_word(r+12)!=scene_environment_geometry(&e->before)){status=RF_FORMAT;goto fail;}
    }
    for(i=0;i<nav_count;i++){
        const unsigned char *r=p+80+count*SCENE_ENV_ROW+i*SCENE_ENV_NAV_ROW;
        scene_environment_nav_entry *e=scene_environment_nav_rows(s)+i;uint32_t found=0;
        e->uid=scene_history_word(r);e->after=scene_history_word(r+4);
        for(j=0;j<campaign_navigation.count;j++)if(campaign_navigation.nodes[j].uid==e->uid){e->slot=j;found=1;break;}
        if(!found){status=RF_FORMAT;goto fail;}
        for(k=0;k<i;k++)if(scene_environment_nav_rows(s)[k].slot==e->slot){status=RF_FORMAT;goto fail;}
        e->baseline=campaign_navigation.nodes[e->slot].candidate.retained_018;
        memcpy(&e->before,&campaign_navigation.nodes[e->slot].candidate.radius,4);
        if(e->after!=0&&e->after!=e->baseline){status=RF_FORMAT;goto fail;}
    }
    *out=s;return RF_OK;
fail:(void)rf_prepare_storage_release(provider,NULL,NULL,s,RF_OK);return status;
}
static inline int scene_world_environment_prepare_accounted(const unsigned char identity[32],int32_t now,const void *data,uint32_t bytes,
    uint32_t budget,scene_world_environment_stage **out,const rf_prepare_memory *memory)
{return scene_world_environment_prepare_storage_accounted(identity,now,data,bytes,budget,out,memory,NULL);}
static inline int scene_world_environment_prepare(const unsigned char identity[32],int32_t now,const void *data,uint32_t bytes,
    uint32_t budget,scene_world_environment_stage **out)
{return scene_world_environment_prepare_accounted(identity,now,data,bytes,budget,out,NULL);}
static inline int scene_world_environment_validate(const scene_world_environment_stage *s)
{
    rf_scene_skybox_state skybox;int status;
    uint32_t i;if(!s||rf_scene_level_transition.pending||s->count!=campaign_forces.count||s->owner!=campaign_forces.items||
        s->navigation!=campaign_navigation.nodes||s->navigation_count!=campaign_navigation.count||
        memcmp(&s->before,&scene_gravity,sizeof(scene_gravity))||
        memcmp(&s->cutscene_before,&campaign_cutscene_runtime,sizeof(campaign_cutscene_runtime))||
        memcmp(&s->form_before,&campaign_player_form,sizeof(campaign_player_form))||
        s->monitor_before_count!=campaign_monitor_count||s->monitor_epoch_before!=campaign_monitor_epoch||
        memcmp(s->monitor_before,campaign_monitor_bindings,sizeof(s->monitor_before))||
        memcmp(s->monitor_before_words,rf_scene_monitor_bindings,sizeof(s->monitor_before_words))||
        memcmp(scene_environment_monitor_before((scene_world_environment_stage *)s),campaign_monitor_cameras,
            s->monitor_before_count*sizeof(scene_monitor_cameras))||
        s->blackout_before!=campaign_blackout_deadline||
        s->sight_before!=campaign_enemy_sight_phase_offset||
        s->spread_before!=campaign_enemy_spread_random.value)return RF_FORMAT;
    /* This seals the current game clock as well as the logical source owner.
     * The remaining load transaction never ticks or replaces the sky owner. */
    status=rf_scene_skybox_capture(campaign_blackout_now,&skybox);if(status)return status;
    if(!scene_environment_skybox_same(&s->skybox_before,&skybox))return RF_FORMAT;
    status=rf_scene_skybox_validate(&s->skybox_next);if(status)return status;
    for(i=0;i<s->count;i++)if(memcmp(&s->rows[i].before,s->owner+s->rows[i].slot,sizeof(rf_physics_force_region)))return RF_FORMAT;
    for(i=0;i<s->nav_count;i++){
        const scene_environment_nav_entry *e=scene_environment_nav_rows((scene_world_environment_stage *)s)+i;
        uint32_t bits;
        if(e->slot>=s->navigation_count||s->navigation[e->slot].uid!=e->uid||
           s->navigation[e->slot].candidate.retained_018!=e->baseline)return RF_FORMAT;
        memcpy(&bits,&s->navigation[e->slot].candidate.radius,4);if(bits!=e->before)return RF_FORMAT;
    }
    return RF_OK;
}
/* Unlike the legacy environment assignment, this is called only after ALL
 * fallible publication/storage-close work has succeeded and before stage free.
 * No simulation, event, render or admission callback runs during this commit. */
static inline void scene_world_environment_finish(const scene_world_environment_stage *s)
{rf_scene_skybox_restore(&s->skybox_next,campaign_blackout_now);}
static inline void scene_world_environment_assign(const scene_world_environment_stage *s)
{
    uint32_t i;scene_gravity=s->next;
    for(i=0;i<s->count;i++){s->owner[s->rows[i].slot].active=s->rows[i].active;s->owner[s->rows[i].slot].strength=s->rows[i].strength;}
    for(i=0;i<s->nav_count;i++){
        const scene_environment_nav_entry *e=scene_environment_nav_rows((scene_world_environment_stage *)s)+i;
        memcpy(&s->navigation[e->slot].candidate.radius,&e->after,4);
    }
    campaign_cutscene_runtime=s->cutscene_next;
    campaign_monitor_count=s->monitor_count;
    memcpy(campaign_monitor_bindings,s->monitor_next,sizeof(campaign_monitor_bindings));
    memcpy(rf_scene_monitor_bindings,s->monitor_next_words,sizeof(rf_scene_monitor_bindings));
    if(s->monitor_portable){
        memset(campaign_monitor_cameras,0,sizeof(campaign_monitor_cameras));
        memcpy(campaign_monitor_cameras,scene_environment_monitor_next((scene_world_environment_stage *)s),
            s->monitor_count*sizeof(scene_monitor_cameras));
        ++campaign_monitor_epoch; /* Discard transient images; exact applied state is already staged. */
    }else campaign_monitor_restored();
    campaign_blackout_deadline=s->blackout_next;
    /* A quick-load can occur mid-session; the replay frame does not restart. */
    campaign_enemy_sight_phase_offset=(s->sight_next+30-
        (combat_frame==UINT32_MAX?0:combat_frame%30))%30;
    campaign_enemy_spread_random.value=s->spread_next;
    rf_scene_enemy_spread[5]=s->spread_next;
    rf_scene_cutscene[3]=campaign_cutscene_runtime.active_uid;
    rf_scene_cutscene[4]=campaign_cutscene_runtime.point_index;
    rf_scene_cutscene[7]=campaign_cutscene_runtime.active;
    if(s->nav_count){
        const scene_environment_nav_entry *last=scene_environment_nav_rows((scene_world_environment_stage *)s)+s->nav_count-1;
        printf("WORLD_NAVPOINT_RESTORE count%u uid%u radius_bits%u\n",s->nav_count,last->uid,last->after);
    }
}
#endif
