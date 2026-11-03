/* One-player multiplayer against bots, reconstructed from the PS2 game
 * (SLUS-20073); addresses and evidence in docs/MP-BOTS-PS2.md.
 *  - Bots are the arena's own characters (PS2 section 0x30000, carried into
 *    the PC map by main.c): the first N become bots, the rest are left out
 *    (2740e0). Roster of 16 slots named from 293ba0 (273f70).
 *  - Respawn points (174040/274280): flags player 1, player 2, bot; the
 *    choice (274620) takes matching points whose nearest live character is
 *    beyond 2.1, then 1.1, then 0.1 units (2744f0), at random, else point 0.
 *  - A bot respawns when its actor becomes a corpse (2748f0 runs from the
 *    actor cleanup path), as a new body at a bot point. Port mechanism: when
 *    the actor's model passes to a corpse owner (rf_scene_model_detach) its
 *    identity is released, the corpse's model moved to a spare owner (so the
 *    corpse stays and fades by the ordinary corpse rules), the slot restored
 *    from its first-frame snapshot and its model and registration made again.
 *  - Scoring per death (274c50) from the lethal credit (responsible handle):
 *    a bot victim gains a death and loses its streak; a suicide loses a kill;
 *    a bot killer gains a kill and streak (and a human kill when the victim
 *    is the player); a player killer gains a kill.
 *  - Match (1dbf20/1dc0e0): time limit counts down (0 = none); the kill
 *    limit ends the match when the player or any bot reaches it (0 = none).
 *  - The player respawns on Fire ("Press Fire to respawn", strings.tbl 3) at
 *    a player point.
 *  - Routes (12e2a0): a bot walks one of the arena's named waypoint lists
 *    (PS2 section 0x10000, carried in by main.c) whose name starts with a
 *    prefix, picked at random among up to 32, one way, in waypoint mode 4;
 *    while it runs (+484 bit 0x800000) mode 4 takes no new target (12eed0),
 *    but in multiplayer each path step (1373c0) re-runs the target choice and
 *    fire (12b580 -> 12ae50) and faces a sighted target, so bots fight on
 *    the way.
 *    From the attack update (12ffd0, every frame outside mode 7): weapon
 *    seek 12f750 (1 in 4; then always when the bot's weapon is not in its
 *    preference row 28e760, else 1 in rank+2) and roam 12f8e0 (1 in 64,
 *    any list); losing the target roams at once (12f8e0 forced).
 *  - Announcer (1dc0e0 -> 1dbb60 8/9): when the human player's kills reach
 *    limit-5 / limit-1, mp_ann_10 / mp_ann_11 play once (PC audio.vpp holds
 *    the same lines as the PS2 .vse files).
 * Port deviations still open: see docs/MP-BOTS-PS2.md section 12 (audit). */
uint32_t rf_scene_mp_bots,rf_scene_mp_kill_limit,rf_scene_mp_time_limit;
/* active, bots, player kills, player deaths, bot respawns, seconds left, match over, player respawns */
uint32_t rf_scene_mp[8];
/* Bot creation trace: step reached, status. */
uint32_t rf_scene_mp_seed_trace[2];
/* Bot deaths, kills scored by bots, bot suicides. */
uint32_t rf_scene_mp_bot_deaths[3];
/* Corpses kept on a spare model owner when their bot was made again. */
uint32_t rf_scene_mp_corpse_trace[4]; /* kept, last refusal, respawns, corpses found */
#define SCENE_MP_SPAWNS 32u
#define SCENE_MP_BOTS 16u
scene_mp_bot rf_scene_mp_roster[SCENE_MP_BOTS];
static const char *const scene_mp_names[SCENE_MP_BOTS]={"Myles","Willis","Tyler","Abraham","Lance","Jared",
    "Benedict","Butch","Logan","Bryce","Tristan","Connor","Mason","Lucas","Sergio","Adrian"};
const char *rf_scene_mp_bot_name(uint32_t slot){return slot<SCENE_MP_BOTS?scene_mp_names[slot]:"";}
static struct {float position[3];uint8_t flags[3];} scene_mp_spawn[SCENE_MP_SPAWNS];
static uint32_t scene_mp_spawn_count,scene_mp_random=1,scene_mp_player_dead;
static float scene_mp_clock;
/* Respawn fade (1ddd90): 0 none, 1 to white, 2 back; seconds in phase. */
static uint32_t scene_mp_fade_phase;static float scene_mp_fade_clock;
static campaign_npc_body *scene_mp_snapshot;static rf_entity_pose scene_mp_pose[SCENE_MP_BOTS];
/* First-frame model publication (appearance, room, basis) for the revive. */
static struct {uint32_t appearance,room;float basis[9];} scene_mp_model[SCENE_MP_BOTS];
uint32_t rf_scene_mp_match_over(void){return rf_scene_mp[0] && rf_scene_mp[6];}
/* PS2 rand 25b740: seed = seed * 0x41c64e6d + 0x3039, result its low 31
 * bits; srand 25b730 seeds it at level start (282380) with the frame count
 * since boot (2ac8b0). Port: the multiplayer draws use their own sequence
 * (the PS2 shares one with every other system), seeded from the port's
 * update count since boot. */
static uint32_t scene_mp_boot_updates;
static uint32_t scene_mp_next(void){scene_mp_random=scene_mp_random*0x41c64e6du+0x3039u;return scene_mp_random&0x7fffffffu;}
/* 174040: uid, position, orientation, name, one byte, team, three flags. */
static int scene_mp_spawns_read(const rf_level *level)
{
    const rf_level_section *s=rf_level_find(level,0x700);unsigned char b[64];uint32_t count,cursor=4,i,len;int status;
    scene_mp_spawn_count=0;
    if(!s)return RF_NOT_FOUND;
    status=rf_level_read(level,s,0,b,4);if(status)return status;
    count=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
    for(i=0;i<count && scene_mp_spawn_count<SCENE_MP_SPAWNS;i++){
        status=rf_level_read(level,s,cursor+4,scene_mp_spawn[scene_mp_spawn_count].position,12);if(status)return status;
        status=rf_level_read(level,s,cursor+52,b,2);if(status)return status;
        len=(uint32_t)b[0]|(uint32_t)b[1]<<8;
        status=rf_level_read(level,s,cursor+54+len,b,8);if(status)return status;
        memcpy(scene_mp_spawn[scene_mp_spawn_count].flags,b+5,3);
        ++scene_mp_spawn_count;cursor+=54+len+8;
    }
    return scene_mp_spawn_count?RF_OK:RF_NOT_FOUND;
}
/* 2744f0: distance from a point to the nearest live character. */
static float scene_mp_nearest(const float *p)
{
    float best=100000.0f,d;uint32_t i,k;
    if(campaign_player_damage.state.effects.health>0){
        for(d=0,k=0;k<3;k++)d+=(scene_actor_body.state.position[k]-p[k])*(scene_actor_body.state.position[k]-p[k]);
        if(sqrtf(d)<best)best=sqrtf(d);
    }
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        if(!o->registration.view || o->damage.effects.health<=0)continue;
        for(d=0,k=0;k<3;k++)d+=(o->body.state.position[k]-p[k])*(o->body.state.position[k]-p[k]);
        if(sqrtf(d)<best)best=sqrtf(d);
    }
    return best;
}
/* 274620: flag 0 player 1, 2 bot. */
static const float *scene_mp_pick(uint32_t flag)
{
    uint32_t list[SCENE_MP_SPAWNS],n,i;float threshold;
    for(threshold=2.1f;threshold>0;threshold-=1.0f){
        for(n=0,i=0;i<scene_mp_spawn_count;i++)
            if(scene_mp_spawn[i].flags[flag] && scene_mp_nearest(scene_mp_spawn[i].position)>threshold)list[n++]=i;
        if(n)return scene_mp_spawn[list[scene_mp_next()%n]].position;
    }
    return scene_mp_spawn[0].position;
}
/* 2740e0: the arena's characters, first N kept. 177010: in multiplayer a
 * character whose class model is skinned (.vcm, kind 2) is created as
 * `miner1`, keeping its authored fields and weapon. Classes are rebuilt for
 * the kept records by name (each class needs its first record). */
static int scene_mp_bot_seeds(const rf_level *level,rf_vpp *tables)
{
    uint32_t count,k,j,classes=0,miner=UINT32_MAX;int status;rf_entity_seed_class *rebuilt;
    memset(rf_scene_mp,0,sizeof(rf_scene_mp));memset(rf_scene_mp_roster,0,sizeof(rf_scene_mp_roster));
    memset(rf_scene_mp_bot_deaths,0,sizeof(rf_scene_mp_bot_deaths));memset(rf_scene_mp_corpse_trace,0,sizeof(rf_scene_mp_corpse_trace));scene_mp_clock=0;scene_mp_player_dead=0;
    rf_scene_mp_seed_trace[0]=1;
    status=scene_mp_spawns_read(level);rf_scene_mp_seed_trace[1]=(uint32_t)status;if(status)return status;
    rf_scene_mp_seed_trace[0]=2;
    status=rf_entity_seeds_open(level,tables,1024*1024,&campaign_seeds);rf_scene_mp_seed_trace[1]=(uint32_t)status;
    if(status)return status;
    count=rf_scene_mp_bots<campaign_seeds.records.count?rf_scene_mp_bots:campaign_seeds.records.count;
    if(count>SCENE_MP_BOTS)count=SCENE_MP_BOTS;
    rebuilt=calloc(count?count:1,sizeof(*rebuilt));if(!rebuilt)return RF_IO;
    for(k=0;k<count;k++){
        uint32_t c=campaign_seeds.items[k].class_index;rf_level_entity *record=&campaign_seeds.records.items[k].record;
        if(c>=campaign_seeds.class_count){free(rebuilt);return RF_RANGE;}
        if(campaign_seeds.classes[c].model_kind==2){
            if(miner==UINT32_MAX){
                status=rf_entity_seed_class_read(tables,"miner1",rebuilt+classes);
                if(status){free(rebuilt);return status;}
                rebuilt[classes].record_index=k;miner=classes++;
            }
            strcpy(record->class_name,"miner1");campaign_seeds.items[k].class_index=miner;
        }else{
            for(j=0;j<k;j++)if(!strcmp(campaign_seeds.records.items[j].record.class_name,record->class_name))break;
            if(j<k)campaign_seeds.items[k].class_index=campaign_seeds.items[j].class_index;
            else{rebuilt[classes]=campaign_seeds.classes[c];rebuilt[classes].record_index=k;campaign_seeds.items[k].class_index=classes++;}
        }
        rf_scene_mp_roster[k].used=1;
    }
    free(campaign_seeds.classes);campaign_seeds.classes=rebuilt;
    campaign_seeds.records.count=count;campaign_seeds.class_count=classes;
    scene_seed_uid_table.items=NULL;rf_scene_mp_seed_trace[0]=3;
    rf_scene_mp[0]=1;rf_scene_mp[1]=count;rf_scene_mp[5]=rf_scene_mp_time_limit;
    return RF_OK;
}
/* Target scan cadence. Single player: one look every 30 frames, phased per
 * actor. Multiplayer: 12b580 re-runs the target choice every 28e9b0[skill] =
 * 600, 450, 300, 200 ms (36, 27, 18, 12 frames; skill = session difficulty). */
static uint32_t scene_mp_scan_due(uint32_t frame,uint32_t slot,uint32_t phase)
{
    static const uint32_t period[4]={36,27,18,12};uint32_t p,k,n=rf_tick_frames?rf_tick_frames:1;
    p=rf_scene_mp[0]?period[rf_scene_session_get_difficulty()&3]:30;
    /* An update may cover several 60ths: any frame number it covers counts. */
    for(k=0;k<n && k<p && k<=frame;k++)if((frame-k+phase)%p==slot%p)return 1;
    return 0;
}
/* First shot after a new target. Single player keeps the port's half-second
 * wait; multiplayer fires on the first attack frame (12ffd0 step 8 runs
 * 12ae50 the same frame; 12a960 needs only the target within 0.9 of the
 * facing; the port keeps campaign_enemy_aim_aligned's 30-degree cone). */
static uint32_t scene_mp_first_shot(uint32_t frame){return rf_scene_mp[0]?frame:frame+30;}
/* Corpse removal (campaign_remove_object): 2748f0 respawns the bot. */
static void scene_mp_bot_removed(uint32_t slot)
{if(rf_scene_mp[0] && slot<SCENE_MP_BOTS && rf_scene_mp_roster[slot].used)rf_scene_mp_roster[slot].respawn_pending=1;}
/* Death hands the model to a corpse owner (rf_scene_model_detach). Release
 * it, restore the saved pose (its matrices live in the shared pose block) and
 * register the model again as campaign_models_open does at load. */
static int scene_mp_model_revive(uint32_t slot,const float position[3])
{
    campaign_model_owner *owner;rf_entity_pose *pose;int status;
    if(!campaign_model_owners || slot>=campaign_model_owner_count || !campaign_collision_caches)return RF_RANGE;
    rf_scene_mp_seed_trace[1]=0x410+slot;
    status=rf_scene_model_retire(slot);if(status)return status;
    rf_scene_mp_seed_trace[1]=0x420+slot;
    campaign_poses.items[slot]=scene_mp_pose[slot];
    owner=campaign_model_owners+slot;pose=campaign_poses.items+slot;
    /* The draw needs a published appearance before the next model update. */
    owner->room=scene_mp_model[slot].room;owner->appearance=scene_mp_model[slot].appearance;
    memcpy(owner->basis,scene_mp_model[slot].basis,36);memcpy(owner->position,position,12);
    owner->pose=pose;owner->registration.loaded=1;owner->registration.active=&pose->playback.completion.active;
    status=rf_model_skeletal_register(&owner->registration,&campaign_model_head,campaign_model_owner_count);if(status)return status;
    rf_scene_mp_seed_trace[1]=0x430+slot;
    return rf_entity_collision_cache_open(pose,(uint32_t)sizeof(*campaign_collision_caches)+pose->bone_count*50,campaign_collision_caches+slot);
}
/* Move a corpse's model from its actor's owner to a free spare owner (added
 * after the level's actors by campaign_models_open in multiplayer), relinking
 * the registration ring, so the actor's slot is free for the new body. */
static int scene_mp_corpse_keep(uint32_t slot,rf_corpse *corpse)
{
    campaign_model_owner *from,*to;uint32_t s;
    if(!campaign_model_owners || slot>=campaign_poses.count || !campaign_collision_caches){rf_scene_mp_corpse_trace[1]=1;return RF_RANGE;}
    from=campaign_model_owners+slot;
    if(!from->owned || !from->registration.loaded || !from->registration.next){rf_scene_mp_corpse_trace[1]=2+!from->owned*4+!from->registration.loaded*8+!from->registration.next*16;return RF_RANGE;}
    for(s=campaign_poses.count;s<campaign_model_owner_count;++s){
        to=campaign_model_owners+s;
        if(!to->registration.loaded && !to->pose && !to->owned)break;
    }
    if(s>=campaign_model_owner_count){rf_scene_mp_corpse_trace[1]=0x100+campaign_model_owner_count-campaign_poses.count;return RF_NOT_FOUND;}
    *to=*from;
    if(from->registration.next==&from->registration)to->registration.next=to->registration.previous=&to->registration;
    else{to->registration.previous->next=&to->registration;to->registration.next->previous=&to->registration;}
    if(campaign_model_head==&from->registration)campaign_model_head=&to->registration;
    campaign_collision_caches[s]=campaign_collision_caches[slot];
    memset(campaign_collision_caches+slot,0,sizeof(*campaign_collision_caches));
    memset(from,0,sizeof(*from));
    corpse->update.model=s+1;++rf_scene_mp_corpse_trace[0];
    return RF_OK;
}
static void scene_mp_respawned_slot(uint32_t slot);
static int scene_mp_bot_respawn(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;const float *p=scene_mp_pick(2);int status;
    rf_scene_mp_seed_trace[1]=0x100+slot;
    campaign_pursuit_stop(npc);scene_pursuit_token_invalidate_owner(npc);
    rf_scene_mp_seed_trace[1]=0x200+slot;
    /* Release the old identity only. The ordinary removal (campaign_remove_object)
     * also files the actor as defeated for saves; a second filing of the same
     * bot hung the game. */
    if(npc->registration.view){status=rf_entity_view_unregister(&campaign_registry,&campaign_entities,&npc->registration);if(status)return status;}
    rf_scene_mp_seed_trace[1]=0x300+slot;
    {
        /* The corpse holding this actor's model (its update.model is slot+1)
         * stays, as on the PS2: its model moves to a spare owner. With no spare
         * free it is deleted the way campaign_live_corpses_close retires corpses. */
        uint32_t c;++rf_scene_mp_corpse_trace[2];
        for(c=0;c<RF_CORPSE_CAPACITY;++c)
            if((campaign_live_corpses.pool.active_mask&(1u<<c)) && campaign_live_corpses.slots[c].corpse.update.model==slot+1){
                ++rf_scene_mp_corpse_trace[3];
                if(scene_mp_corpse_keep(slot,&campaign_live_corpses.slots[c].corpse)==RF_OK)continue;
                status=rf_corpse_owned_delete(&campaign_live_corpses,c,&campaign_registry,&campaign_live_corpse_count,
                    &campaign_live_corpse_object_count,0,&campaign_live_corpse_delete_backend);
                if(status)return status;
            }
    }
    rf_scene_mp_seed_trace[1]=0x400+slot;
    *npc=scene_mp_snapshot[slot];
    status=scene_mp_model_revive(slot,p);if(status)return status;
    rf_scene_mp_seed_trace[1]=0x500+slot;
    memset(&npc->registration,0,sizeof(npc->registration));
    status=rf_entity_view_register(&campaign_registry,&campaign_entities,&npc->view,&npc->registration);if(status)return status;
    memcpy(npc->body.state.position,p,12);memset(npc->body.state.velocity,0,12);
    memcpy(npc->published,p,12);memcpy(npc->previous,p,12);
    rf_scene_mp_roster[slot].dead=rf_scene_mp_roster[slot].respawn_pending=0;++rf_scene_mp[4];
    npc->damage.effects.affiliation=3; /* 2748f0: +0x204 = 3 */
    scene_mp_respawned_slot(slot);
    rf_scene_mp_seed_trace[1]=0x600+slot;
    return RF_OK;
}
/* 274c50 for one death: victim slot (UINT32_MAX = player), its credit. */
static void scene_mp_score(uint32_t victim,uint32_t victim_handle,uint32_t killer)
{
    uint32_t i;
    if(victim<SCENE_MP_BOTS){++rf_scene_mp_roster[victim].deaths;rf_scene_mp_roster[victim].streak=0;++rf_scene_mp_bot_deaths[0];}
    if(killer==victim_handle){
        if(victim<SCENE_MP_BOTS){--rf_scene_mp_roster[victim].kills;++rf_scene_mp_bot_deaths[2];}
        else if(rf_scene_mp[2])--rf_scene_mp[2];
        return;
    }
    if(killer==campaign_player_object.handle){++rf_scene_mp[2];return;}
    for(i=0;i<campaign_npc_body_count && i<SCENE_MP_BOTS;i++)
        if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==killer){
            scene_mp_bot *b=rf_scene_mp_roster+i;
            ++b->kills;++b->streak;if(b->streak>b->longest)b->longest=b->streak;
            if(victim==UINT32_MAX)++b->human_kills;
            ++rf_scene_mp_bot_deaths[1];return;
        }
}
/* 28e760: eight preferred weapons per roster slot (273f70 gives slot i row
 * i), PS2 weapons.tbl indices resolved by name. */
static const char *const scene_mp_weapon_names[]={"Shotgun","Machine Pistol","Sniper Rifle","Assault Rifle",
    "Flamethrower","Rocket Launcher","rail_gun","12mm handgun"};
static const uint8_t scene_mp_preference_rows[SCENE_MP_BOTS][8]={
    {0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{2,6,3,1,0,5,4,7},{3,1,4,0,5,2,6,7},
    {1,3,0,6,5,4,2,7},{0,3,4,5,1,2,6,7},{3,2,6,5,0,4,1,7},{3,4,5,0,6,2,1,7},
    {4,6,5,3,2,0,1,7},{6,5,4,3,2,0,1,7},{6,5,4,3,2,0,1,7},{6,0,5,4,3,2,1,7},
    {5,6,4,3,2,0,1,7},{5,6,0,4,3,2,1,7},{5,6,0,4,3,2,1,7},{5,6,0,4,3,2,1,7}};
static int32_t scene_mp_weapon_ids[8];
#define SCENE_MP_WEAPON_SEEK 1 /* attack-update seeks (12f750, 12f8e0); 0 holds them off */
/* Bot on a route (+484 0x800000), bot had a target last update. */
static uint8_t scene_mp_routing[SCENE_MP_BOTS],scene_mp_had_target[SCENE_MP_BOTS];
/* Facing bit +0x484 8 (scene_mp_target_seen), node index last seen on the route. */
static uint8_t scene_mp_face_target[SCENE_MP_BOTS];
static uint32_t scene_mp_last_node[SCENE_MP_BOTS];
/* Routes started: weapon, roam, target-lost roam, finished; announcements, last route status. */
uint32_t rf_scene_mp_routes[6];
/* Hit health routes, step-aways marked, attacker targeted, forced step-aways, chance step-aways, ammo routes. */
uint32_t rf_scene_mp_hits[6];
static uint32_t scene_mp_announced;
/* 12a5d0: rank of the bot's weapon in its row, -2 when absent. */
static int32_t scene_mp_weapon_rank(uint32_t slot,int32_t weapon)
{
    uint32_t i;
    for(i=0;i<8;i++)if(scene_mp_weapon_ids[scene_mp_preference_rows[slot][i]]==weapon)return (int32_t)i;
    return -2;
}
/* 12e2a0: lists whose name starts with prefix (up to 32), one at random
 * (rand % count), walked one way in waypoint mode 4 (+484 0x800000). */
static int scene_mp_route_start(uint32_t slot,const rf_level_waypoint_path *path);
static int scene_mp_route(uint32_t slot,const char *prefix)
{
    rf_level_waypoint_path found[32];
    uint32_t count,lists,cursor=4,i,n=0,length,nodes,plen=(uint32_t)strlen(prefix);
    const unsigned char *w=campaign_waypoints;
    if(!w || campaign_waypoint_bytes<4)return RF_NOT_FOUND;
    lists=(uint32_t)w[0]|(uint32_t)w[1]<<8|(uint32_t)w[2]<<16|(uint32_t)w[3]<<24;
    for(i=0;i<lists && n<32;i++){
        if(cursor+2>campaign_waypoint_bytes)return RF_FORMAT;
        length=(uint32_t)w[cursor]|(uint32_t)w[cursor+1]<<8;
        if(cursor+2+length+4>campaign_waypoint_bytes)return RF_FORMAT;
        memcpy(&count,w+cursor+2+length,4);nodes=cursor+6+length;
        if(count>(campaign_waypoint_bytes-nodes)/4)return RF_FORMAT;
        if(count && length>=plen && !memcmp(w+cursor+2,prefix,plen)){found[n].indices=w+nodes;found[n].count=count;++n;}
        cursor=nodes+count*4;
    }
    if(!n)return RF_NOT_FOUND;
    i=scene_mp_next()%n;
    return scene_mp_route_start(slot,&found[i]);
}
/* 12e210/136900: walk one list one way in waypoint mode 4. The mode change
 * drops combat; a seek keeps the bot's target (+0x210 is untouched by
 * 12e210/136900) so it fights on the way (1373c0). */
static int scene_mp_route_start(uint32_t slot,const rf_level_waypoint_path *path)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;uint32_t alert,target,scripted;int status;
    alert=npc->combat_alert;target=npc->combat_target;scripted=npc->combat_scripted;
    npc->script_move.event=0;npc->script_move.origin=SCENE_MOVE_ORIGIN_EVENT;
    npc->script_move.path=*path;npc->script_move.path_index=0;scene_mp_last_node[slot]=0;
    npc->script_move.path_mode=0;npc->script_move.path_reverse=0;
    status=campaign_ai_mode_apply(npc,4,scene_patrol_now(rf_scene_profile_stage[0]));
    rf_scene_mp_routes[5]=(uint32_t)status;
    if(status){npc->script_move.path.indices=NULL;npc->script_move.path.count=0;return status==RF_NOT_FOUND?RF_RANGE:status;}
    if(alert && target){
        npc->combat_alert=alert;npc->combat_target=target;npc->combat_scripted=scripted;
        scene_blind_sync(npc,rf_scene_profile_stage[0]);
    }
    scene_mp_routing[slot]=1;return RF_OK;
}
/* 1373c0 in multiplayer: a bot on a seek route that has a live target turns
 * to face it as it walks (138e60 with the target's position; displacement
 * stays on the route), so the ordinary fire check (12b580 -> 12ae50) passes
 * on the way. Returns the target position, or NULL to steer along the route. */
static uint32_t scene_mp_step_query; /* scene_mp_step_away wants the target whatever the facing bit. */
static const float *scene_mp_route_aim(const campaign_npc_body *npc)
{
    uint32_t slot=(uint32_t)(npc-campaign_npc_bodies),j;
    if(!rf_scene_mp[0] || slot>=SCENE_MP_BOTS || scene_mp_routing[slot]!=1 || !npc->combat_alert || !npc->combat_target)return NULL;
    if(!scene_mp_face_target[slot] && !scene_mp_step_query)return NULL;
    if(npc->combat_target==campaign_player_object.handle)
        return campaign_player_damage.state.effects.health>0?scene_actor_body.state.position:NULL;
    for(j=0;j<campaign_npc_body_count;j++)
        if(campaign_npc_bodies[j].registration.view && campaign_npc_bodies[j].registration.handle==npc->combat_target &&
           campaign_npc_bodies[j].damage.effects.health>0)return campaign_npc_bodies[j].body.state.position;
    return NULL;
}
/* PS2 149cd0/149830 movement state for a bot (0 stand, 1 attack_stand,
 * 2 walk, 3 attack_walk, 4 run, 5 attack_run). An armed bot fighting (149160:
 * a weapon and the attack bit +0x484 & 1) uses the attack set, and in
 * multiplayer 149830 always picks attack_walk/attack_run (3, 5); otherwise
 * stand/walk/run (0, 2, 4). Moving picks walk or run by the speed mode +0x7e8
 * (159f10/159f60; port movement.mode 0 slow, 1 normal). Bots run: 12e210
 * starts a route running for the bot flag +0x734 0x100000 (1579c0) and 133d80
 * resumes fighting running. */
static int32_t scene_mp_locomotion_state(const campaign_npc_body *npc,uint32_t moving,int32_t fallback)
{
    uint32_t slot=(uint32_t)(npc-campaign_npc_bodies),armed;
    if(!rf_scene_mp[0] || slot>=SCENE_MP_BOTS || !rf_scene_mp_roster[slot].used)return fallback;
    armed=npc->view.weapons[0]>=0 && npc->combat_alert;
    if(!moving)return armed?1:0;
    return npc->movement.mode==0?(armed?3:2):(armed?5:4);
}
/* 156980 in multiplayer: the run speed mode (+0x7e8 1) sets the maximum
 * speed +0x7e4 to 8.0 (2ab7bc) instead of the class speed. */
static void scene_mp_bot_speed(campaign_npc_body *npc)
{if(npc->movement.mode==1)npc->movement.speed=8.0f;}
/* 1373c0 faces the target only while +0x484 bit 8 is set: 12b580 sets it on
 * a target scan that sees the target (+0x1ec), and in multiplayer 137aa0
 * flips it one time in two at each node reached. Port: the scan sight comes
 * from the fire check's line test (scene_mp_target_seen). */
static void scene_mp_target_seen(const campaign_npc_body *npc)
{
    uint32_t slot=(uint32_t)(npc-campaign_npc_bodies);
    if(rf_scene_mp[0] && slot<SCENE_MP_BOTS)scene_mp_face_target[slot]=1;
}
/* 12f5f0 step-away: 138b30 gathers the bot's nearest path node and its
 * linked nodes (up to 100); 12f460 scores each by 12f310, the squared
 * distance from the target to the straight segment bot -> node, and takes
 * the largest (none when every score is 0); 136bb0 plans a path there and
 * +0x484 |= 0x2000008, stepped by 1373c0 (fighting on the way) until done.
 * Port: the single best node is walked as a one-node route (no fallback to
 * the other candidates when it cannot be planned); the nearest node stands
 * in for the bot's current pair +0x5bc/+0x5c0. */
static uint8_t scene_mp_step_node[SCENE_MP_BOTS][4],scene_mp_step_pending[SCENE_MP_BOTS];
static float scene_mp_segment_gap(const float target[3],const float self[3],const float point[3])
{
    float d[3],e[3],dd=0,de=0,t,g=0;uint32_t k;
    for(k=0;k<3;k++){d[k]=point[k]-self[k];e[k]=target[k]-self[k];dd+=d[k]*d[k];de+=d[k]*e[k];}
    t=dd>0?de/dd:0;if(t<0)t=0;if(t>1)t=1;
    for(k=0;k<3;k++){float c=self[k]+d[k]*t-target[k];g+=c*c;}
    return g;
}
static int scene_mp_step_away(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;rf_level_waypoint_path path;
    const float *target,*self=npc->body.state.position;float best=0,gap,distance,nearest=FLT_MAX;
    uint32_t i,k,near_node=UINT32_MAX,chosen=UINT32_MAX;
    scene_mp_routing[slot]=1;scene_mp_step_query=1;target=scene_mp_route_aim(npc);scene_mp_step_query=0;scene_mp_routing[slot]=0;
    if(!target || !campaign_navigation.nodes || !campaign_navigation.references)return RF_NOT_FOUND;
    for(i=0;i<campaign_navigation.count;i++){
        const float *p=campaign_navigation.nodes[i].candidate.position;
        for(distance=0,k=0;k<3;k++)distance+=(p[k]-self[k])*(p[k]-self[k]);
        if(distance<nearest){nearest=distance;near_node=i;}
    }
    if(near_node==UINT32_MAX)return RF_NOT_FOUND;
    for(i=0;i<=campaign_navigation.references[near_node].neighbor_count && i<100;i++){
        uint32_t node=i?campaign_navigation.references[near_node].neighbors[i-1]:near_node;
        if(node>=campaign_navigation.count)return RF_FORMAT;
        gap=scene_mp_segment_gap(target,self,campaign_navigation.nodes[node].candidate.position);
        if(gap>best){best=gap;chosen=node;}
    }
    if(chosen==UINT32_MAX)return RF_NOT_FOUND;
    for(k=0;k<4;k++)scene_mp_step_node[slot][k]=(uint8_t)(chosen>>(8*k));
    path.indices=scene_mp_step_node[slot];path.count=1;
    return scene_mp_route_start(slot,&path);
}
/* 132fb0 in multiplayer, for a bot hit by another character: one time in
 * three it reacts (1339e0, which in multiplayer only runs a `health` route,
 * 12e3f0); otherwise, above 25 life, one time in five it marks a step-away
 * for its next update (+0x484 bit 0x1000000, 131ac0 -> 12f5f0 forced). A bot
 * that did not react turns on the attacker and fights (134570, 133d80; mode
 * 3 ends any route). Returns 1 when the port's ordinary retaliation must be
 * skipped. The 1-in-4 `health` branch for class flag 0x20000 is not ported. */
static void scene_mp_attacked(uint32_t slot,uint32_t source);
static uint32_t scene_mp_hit(campaign_npc_body *npc,uint32_t source)
{
    uint32_t slot=(uint32_t)(npc-campaign_npc_bodies);
    if(!rf_scene_mp[0] || slot>=SCENE_MP_BOTS)return 0;
    if(source==npc->registration.handle)return 1;
    scene_mp_attacked(slot,source); /* attacker list +0x3d4, read by 1291f0 */
    if(scene_mp_next()<0x2aaaaaaau){ /* 132fb0: rand < 0x2aaaaaaa */
        if(scene_mp_route(slot,"health")==RF_OK)++rf_scene_mp_hits[0];
        return 1;
    }
    if(npc->damage.effects.health>25 && scene_mp_next()%5==0){scene_mp_step_pending[slot]=1;++rf_scene_mp_hits[1];}
    if(scene_mp_routing[slot]==1){npc->script_move.active=0;npc->navigation.retained.count=0;scene_mp_routing[slot]=0;}
    ++rf_scene_mp_hits[2];return 0;
}
/* 12ffd0 tail for one live bot: 12f750 weapon seek, 12f8e0 roam. */
static int scene_mp_route_tick(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;uint32_t target=npc->combat_alert!=0;int status=RF_NOT_FOUND;int32_t rank;
    if(scene_mp_routing[slot]){
        uint32_t walking;
        /* 137aa0 in multiplayer: each node reached flips the facing bit 1 in 2. */
        if(npc->script_move.path_index!=scene_mp_last_node[slot]){
            scene_mp_last_node[slot]=npc->script_move.path_index;
            if(scene_mp_next()&1)scene_mp_face_target[slot]^=1;
        }
        walking=npc->script_move.active && npc->script_move.path.count && npc->script_move.origin==SCENE_MOVE_ORIGIN_EVENT;
        if(walking)return RF_OK;
        scene_mp_routing[slot]=0;++rf_scene_mp_routes[3];
    }
    if(npc->combat_scripted==1 || campaign_player_damage.state.effects.health<=0){scene_mp_had_target[slot]=(uint8_t)target;return RF_OK;}
    if(target){
        /* 131ac0 (a marked step-away) and attack-update step 2 (12f5f0,
         * rand & 0x3f == 0) come first; a started step-away ends the update. */
        if(scene_mp_step_pending[slot] || !(scene_mp_next()&0x3f)){
            uint32_t forced=scene_mp_step_pending[slot];scene_mp_step_pending[slot]=0;
            status=scene_mp_step_away(slot);
            if(status==RF_OK){++rf_scene_mp_hits[forced?3:4];scene_mp_had_target[slot]=1;return RF_OK;}
            if(status!=RF_NOT_FOUND && status!=RF_RANGE)return status;
            status=RF_NOT_FOUND;
        }
        /* Steps 5-7 (12f810 ammo, 12f750 weapon, 12f8e0 roam), each skipped
         * once a route runs (+0x484 0x800000). They rely on fighting on the
         * route (scene_mp_route keeps the target, scene_mp_route_aim). */
        if(SCENE_MP_WEAPON_SEEK){
            int32_t weapon=npc->view.weapons[0];
            /* 12f810: reserve of the weapon's ammo type (AI +0x4c) below a
             * third of its maximum (weapon record +0x80): rand < 0x1ffffff. */
            if(weapon>=0 && (uint32_t)weapon<campaign_weapon_supply.names.count){
                int32_t type=campaign_weapon_supply.definitions[weapon].ammo_type;
                if(type>=0 && type<32 && npc->inventory.reserve[type]<campaign_weapon_supply.definitions[weapon].capacity/3 &&
                   scene_mp_next()<0x1ffffffu){status=scene_mp_route(slot,"ammo");if(!status)++rf_scene_mp_hits[5];}
            }
            /* 12f750: rand < 0x1fffffff, then rank -2 or rand % (rank+2) == 0. */
            if(status==RF_NOT_FOUND && scene_mp_next()<0x1fffffffu){
                rank=scene_mp_weapon_rank(slot,weapon);
                if(rank==-2 || (rank>=0 && !(scene_mp_next()%(uint32_t)(rank+2)))){
                    status=scene_mp_route(slot,"weapon");if(!status)++rf_scene_mp_routes[0];
                }
            }
            /* 12f8e0 unforced: rand < 0x1ffffff. */
            if(status==RF_NOT_FOUND && scene_mp_next()<0x1ffffffu){status=scene_mp_route(slot,"");if(!status)++rf_scene_mp_routes[1];}
        }
    }else if(!npc->script_move.active){
        /* 12ffd0 step 1: no target, so 12f8e0 roams at once (forced). Mode 2
         * turns straight into mode 3 in multiplayer (12bbb0), so this is also
         * what a bot does before its first target. An arena without lists
         * leaves the bot where it is. */
        status=scene_mp_route(slot,"");if(!status)++rf_scene_mp_routes[scene_mp_had_target[slot]?2:1];
    }
    scene_mp_had_target[slot]=(uint8_t)target;
    return status==RF_NOT_FOUND?RF_OK:status;
}
/* Announcer lines 28f510 (0..10): sound, playing handle, played flag. */
static const char *const scene_mp_lines[11]={"mp_ann_01.wav","mp_ann_02.wav","mp_ann_03.wav","mp_ann_04.wav",
    "mp_ann_05.wav","mp_ann_07.wav","mp_ann_08.wav","mp_ann_09.wav","mp_ann_10.wav","mp_ann_11.wav","mp_ann_allyourbase.wav"};
static int32_t scene_mp_line_voice[11];static uint8_t scene_mp_line_played[11];
static int32_t scene_mp_winner_line; /* 2ac564: line queued after line 4; -1 none */
static uint32_t scene_mp_line_playing(uint32_t n)
{
    uint32_t source;
    if(scene_mp_line_voice[n]<0 || rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,scene_mp_line_voice[n],&source))return 0;
    return campaign_audio_mixer.voices[source&0xffff].active;
}
static void scene_mp_line_stop(uint32_t n)
{
    uint32_t source;
    if(scene_mp_line_voice[n]>=0 && !rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,scene_mp_line_voice[n],&source))
        (void)rf_audio_voice_stop(&campaign_audio_mixer,source);
    scene_mp_line_voice[n]=-1;
}
static void scene_mp_line_start(uint32_t n)
{
    uint32_t index=0;
    scene_mp_line_voice[n]=-1;
    if(rf_audio_bank_declare(&campaign_audio_bank,scene_mp_lines[n],1,1,1,&index))return;
    if(!rf_audio_bank_sample(&campaign_audio_bank,index)){
        if(campaign_ambient_reload(index))return;
        if(index<sizeof(campaign_audio_evictable))campaign_audio_evictable[index]=1;
    }
    scene_mp_line_voice[n]=campaign_sound_start((int32_t)index,scene_actor_body.state.position,1,0,0,0);
    if(scene_mp_line_voice[n]>=0)++rf_scene_mp_routes[4];
}
/* 1dbb60(line, stop others, wait for others): stop every line first, or with
 * wait leave the line unplayed (so it is retried) while another still plays. */
static void scene_mp_announce(uint32_t n,uint32_t stop,uint32_t wait)
{
    uint32_t k;
    if(stop)for(k=0;k<11;k++)scene_mp_line_stop(k);
    else if(wait)for(k=0;k<11;k++)if(scene_mp_line_playing(k))return;
    scene_mp_line_start(n);scene_mp_line_played[n]=1;
}
/* 1ddbb0: the leader's score, the human player's kills or any bot's. */
static int32_t scene_mp_leader_score(void)
{
    int32_t best=(int32_t)rf_scene_mp[2];uint32_t i;
    for(i=0;i<SCENE_MP_BOTS;i++)if(rf_scene_mp_roster[i].used && rf_scene_mp_roster[i].kills>best)best=rf_scene_mp_roster[i].kills;
    return best;
}
/* 1ddb60: the player's rank, 1 + everyone with more kills. */
static uint32_t scene_mp_player_rank(void)
{
    uint32_t i,rank=1;
    for(i=0;i<SCENE_MP_BOTS;i++)if(rf_scene_mp_roster[i].used && rf_scene_mp_roster[i].kills>(int32_t)rf_scene_mp[2])++rank;
    return rank;
}
/* Per update, after frame-zero setup. */
static int scene_mp_tick(uint32_t frame)
{
    uint32_t i,count;int status;
    ++scene_mp_boot_updates;
    if(!rf_scene_mp[0])return RF_OK;
    count=campaign_npc_body_count<SCENE_MP_BOTS?campaign_npc_body_count:SCENE_MP_BOTS;
    if(!frame){
        free(scene_mp_snapshot);scene_mp_snapshot=calloc(count?count:1,sizeof(*scene_mp_snapshot));
        if(!scene_mp_snapshot)return RF_RANGE;
        for(i=0;i<count;i++){
            scene_mp_snapshot[i]=campaign_npc_bodies[i];scene_mp_pose[i]=campaign_poses.items[i];
            if(campaign_model_owners && i<campaign_model_owner_count){
                scene_mp_model[i].appearance=campaign_model_owners[i].appearance;scene_mp_model[i].room=campaign_model_owners[i].room;
                memcpy(scene_mp_model[i].basis,campaign_model_owners[i].basis,36);
            }
        }
        scene_mp_random=scene_mp_boot_updates;
        for(i=0;i<8;i++)scene_mp_weapon_ids[i]=rf_weapon_name_find(&campaign_weapon_supply.names,scene_mp_weapon_names[i]);
        memset(scene_mp_routing,0,sizeof(scene_mp_routing));memset(scene_mp_had_target,0,sizeof(scene_mp_had_target));
        memset(rf_scene_mp_routes,0,sizeof(rf_scene_mp_routes));scene_mp_announced=0;
        memset(scene_mp_line_played,0,sizeof(scene_mp_line_played));scene_mp_winner_line=-1;scene_mp_fade_phase=0;
        for(i=0;i<11;i++)scene_mp_line_voice[i]=-1;
        memset(scene_mp_step_pending,0,sizeof(scene_mp_step_pending));memset(scene_mp_face_target,0,sizeof(scene_mp_face_target));memset(rf_scene_mp_hits,0,sizeof(rf_scene_mp_hits));
        return RF_OK;
    }
    if(rf_scene_mp[6]){
        /* 1dc1a0: once line 4 has finished, the queued winner line plays. */
        if(scene_mp_winner_line>=0 && scene_mp_line_played[4] && !scene_mp_line_playing(4)){
            scene_mp_line_start((uint32_t)scene_mp_winner_line);scene_mp_winner_line=-1;
        }
        return RF_OK;
    }
    for(i=0;i<count;i++){
        campaign_npc_body *npc=campaign_npc_bodies+i;scene_mp_bot *b=rf_scene_mp_roster+i;
        if(!b->dead && npc->registration.view && npc->damage.effects.health<=0){
            b->dead=1;scene_mp_score(i,npc->registration.handle,npc->damage.responsible_handle);
        }
        if(b->dead && !b->respawn_pending && (!npc->registration.view ||
           (campaign_model_owners && i<campaign_model_owner_count && campaign_model_owners[i].owned)))b->respawn_pending=1;
        if(b->respawn_pending){status=scene_mp_bot_respawn(i);if(status)return status;scene_mp_routing[i]=scene_mp_had_target[i]=scene_mp_step_pending[i]=scene_mp_face_target[i]=0;}
        else if(!b->dead && npc->registration.view && npc->damage.effects.health>0){
            scene_mp_bot_speed(npc);
            status=scene_mp_route_tick(i);if(status)return status;
        }
    }
    if(!scene_mp_player_dead && campaign_player_damage.state.effects.health<=0){
        scene_mp_player_dead=1;
        scene_mp_score(UINT32_MAX,campaign_player_object.handle,campaign_player_damage.state.responsible_handle);
    }else if(campaign_player_damage.state.effects.health>0)scene_mp_player_dead=0;
    rf_scene_mp[3]=rf_scene_player_life[0];rf_scene_mp[7]=rf_scene_player_life[1];
    if(scene_mp_fade_phase==2){scene_mp_fade_clock+=scene_step_seconds;if(scene_mp_fade_clock>=.5f)scene_mp_fade_phase=0;}
    /* 156980: in multiplayer the human's run mode maximum is 8.0 too. */
    if(rf_scene_actor_movement_settings.mode==1)rf_scene_actor_movement_settings.speed=8.0f;
    /* 1dc0e0: time, then the kill limit for the player and every bot. */
    if(rf_scene_mp_time_limit){
        float left;uint32_t minutes,seconds;scene_mp_clock+=scene_step_seconds;left=(float)rf_scene_mp_time_limit-scene_mp_clock;
        rf_scene_mp[5]=left>0?(uint32_t)left:0;
        if(left<0)rf_scene_mp[6]=1;
        /* 1dc1a0 time callouts in the last minute: line 0, under 31 s line 1
         * (both wait for other lines), under 11 s line 2. */
        minutes=rf_scene_mp[5]/60;seconds=rf_scene_mp[5]%60;
        if(!rf_scene_mp[6] && !minutes){
            if(!scene_mp_line_played[0])scene_mp_announce(0,0,1);
            if(seconds<31 && !scene_mp_line_played[1])scene_mp_announce(1,0,1);
            if(seconds<11 && !scene_mp_line_played[2])scene_mp_announce(2,0,0);
        }
    }
    if(rf_scene_mp_kill_limit){
        /* 1dc1a0: the leader's score (1ddbb0) reaching limit-5 / limit-1 plays
         * line 8 / 9 once per score change. */
        int32_t leader=scene_mp_leader_score();
        if((uint32_t)leader!=scene_mp_announced){
            if(leader==(int32_t)rf_scene_mp_kill_limit-5)scene_mp_announce(8,0,0);
            else if(leader==(int32_t)rf_scene_mp_kill_limit-1)scene_mp_announce(9,0,0);
            scene_mp_announced=(uint32_t)leader;
        }
        if(rf_scene_mp[2]>=rf_scene_mp_kill_limit)rf_scene_mp[6]=1;
        for(i=0;i<count;i++)if(rf_scene_mp_roster[i].used && rf_scene_mp_roster[i].kills>=(int32_t)rf_scene_mp_kill_limit)rf_scene_mp[6]=1;
    }
    if(rf_scene_mp[6]){
        /* Match over: line 4, stopping the others; one human ranked first
         * queues line 5 (2ac564 = 28f54c). */
        scene_mp_announce(4,1,0);
        scene_mp_winner_line=scene_mp_player_rank()==1?5:-1;
    }
    return RF_OK;
}
/* campaign_life_input: "Press Fire to respawn" (strings.tbl 3). */
static uint32_t scene_mp_fire_held;
/* 1a51b0: with no entity, primary or secondary fire more than 1.0 s after
 * death starts the respawn: 1dd8f0 plays sound 0x40 and 1ddd90 fades to
 * white (alpha up to 165) over 0.15 s, respawns (1dd930), then fades back
 * over 0.5 s. Returns 1 when the respawn itself is due. */
static uint32_t scene_mp_respawn_pressed(const rf_scene_input *input,uint32_t ready)
{
    uint32_t pressed,held=input->fire || input->alt_fire;
    if(!rf_scene_mp[0])return 0;
    pressed=held && !scene_mp_fire_held;scene_mp_fire_held=held;
    if(scene_mp_fade_phase==0 && pressed && ready){
        uint32_t index=0x40;
        scene_mp_fade_phase=1;scene_mp_fade_clock=0;
        if(index<sizeof(campaign_audio_evictable) && (rf_audio_bank_sample(&campaign_audio_bank,index) || !campaign_ambient_reload(index))){
            campaign_audio_evictable[index]=1;(void)campaign_sound_start((int32_t)index,scene_actor_body.state.position,1,0,0,0);
        }
        return 0;
    }
    if(scene_mp_fade_phase==1){
        scene_mp_fade_clock+=scene_step_seconds;
        if(scene_mp_fade_clock>=.15f){scene_mp_fade_phase=2;scene_mp_fade_clock=0;return 1;}
    }
    return 0;
}
/* Current white fade alpha (0 none). */
uint32_t rf_scene_mp_fade_alpha(void)
{
    float a=0;
    if(!rf_scene_mp[0])return 0;
    if(scene_mp_fade_phase==1)a=165.0f*scene_mp_fade_clock/.15f;
    else if(scene_mp_fade_phase==2)a=165.0f*(1.0f-scene_mp_fade_clock/.5f);
    return a<=0?0:a>=165?165:(uint32_t)a;
}
static void scene_mp_player_respawned(void)
{
    if(!rf_scene_mp[0])return;
    memcpy(scene_actor_body.state.position,scene_mp_pick(0),12);
    memset(scene_actor_body.state.velocity,0,12);
}
