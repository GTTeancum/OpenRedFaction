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
 *    while it runs (+484 bit 0x800000) it takes no new target (12eed0).
 *    From the attack update (12ffd0, every frame outside mode 7): weapon
 *    seek 12f750 (1 in 4; then always when the bot's weapon is not in its
 *    preference row 28e760, else 1 in rank+2) and roam 12f8e0 (1 in 64,
 *    any list); losing the target roams at once (12f8e0 forced).
 *  - Announcer (1dc0e0 -> 1dbb60 8/9): when the human player's kills reach
 *    limit-5 / limit-1, mp_ann_10 / mp_ann_11 play once (PC audio.vpp holds
 *    the same lines as the PS2 .vse files).
 * Port deviations: respawned bots keep their authored affiliation instead of
 * 3 (the port AI has no multiplayer affiliation rules yet); bots do not pick
 * items up yet, so a weapon route never changes the bot's weapon; the ammo
 * seek 12f810 (below a third of the clip's ammo, 1 in 64) is left out because
 * port bots do not count ammo; the attack-update weapon seek and 1-in-64 roam
 * are held off (SCENE_MP_WEAPON_SEEK) until bots face and fire at their
 * target while walking a route (1373c0: target choice each path step,
 * 138e60 turn to the target); bots with no target and no route roam at once
 * (the PS2 idle mode is not ported); the hurt-bot health route of the
 * MP branch of 1339e0 is not wired. */
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
static campaign_npc_body *scene_mp_snapshot;static rf_entity_pose scene_mp_pose[SCENE_MP_BOTS];
/* First-frame model publication (appearance, room, basis) for the revive. */
static struct {uint32_t appearance,room;float basis[9];} scene_mp_model[SCENE_MP_BOTS];
uint32_t rf_scene_mp_match_over(void){return rf_scene_mp[0] && rf_scene_mp[6];}
static uint32_t scene_mp_next(void){scene_mp_random=scene_mp_random*1103515245u+12345u;return scene_mp_random>>16;}
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
    static const uint32_t period[4]={36,27,18,12};uint32_t p;
    if(!rf_scene_mp[0])return (frame%30+phase)%30==slot%30;
    p=period[rf_scene_session_get_difficulty()&3];
    return (frame+phase)%p==slot%p;
}
/* First shot after a new target. Single player keeps the port's half-second
 * wait; multiplayer fires on the first attack frame (12ffd0 step 8 runs
 * 12ae50 the same frame; 12a960 needs only the target within 0.9 of the
 * facing; the port keeps campaign_enemy_aim_aligned's 30-degree cone). */
static uint32_t scene_mp_first_shot(uint32_t frame){return rf_scene_mp[0]?frame:frame+30;}
/* Corpse removal (campaign_remove_object): 2748f0 respawns the bot. */
static void scene_mp_bot_removed(uint32_t slot)
{if(rf_scene_mp[0] && slot<SCENE_MP_BOTS && rf_scene_mp_roster[slot].used)rf_scene_mp_roster[slot].respawn_pending=1;}
/* 132fb0: a bot already fighting someone else turns on its attacker at once
 * one time in three (the full rule also re-checks its current target). */
static uint32_t scene_mp_keep_target(const campaign_npc_body *owner,uint32_t source)
{
    if(!rf_scene_mp[0] || !owner->combat_alert || !owner->combat_target || owner->combat_target==source)return 0;
    return scene_mp_next()%3!=0;
}
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
/* 12f460 score (12f310): squared distance from the target to the closest
 * point of the segment bot->goal; the goal whose route stays farthest from
 * the target wins. */
static float scene_mp_goal_score(const float *target,const float *self,const float *goal)
{
    float d[3],e[3],dd=0,de=0,t,s=0;uint32_t k;
    for(k=0;k<3;k++){d[k]=goal[k]-self[k];e[k]=target[k]-self[k];dd+=d[k]*d[k];de+=d[k]*e[k];}
    t=dd>0?de/dd:0;if(t<0)t=0;if(t>1)t=1;
    for(k=0;k<3;k++){float c=self[k]+d[k]*t-target[k];s+=c*c;}
    return s;
}
/* 12f5f0 + 12f460: one time in 64 (or when forced) pick a goal among the
 * path point nearest the player and its neighbours (the list 2abee4 is built
 * around the player, 1ddxxx at 28285) and walk there. The PS2 runs this from
 * the combat update; the Xbox uses it only for idle bots on an arena with no
 * waypoint lists. */
static void scene_mp_seek(campaign_npc_body *npc,uint32_t forced)
{
    const float *target=scene_actor_body.state.position,*self=npc->body.state.position;
    uint32_t i,near=UINT32_MAX,best_node=UINT32_MAX;float best=-1,d,score;
    const rf_entity_navigation_reference *r;
    if(!campaign_navigation.count || !campaign_navigation.references)return;
    if(!forced && scene_mp_next()%64)return;
    for(i=0;i<campaign_navigation.count;i++){
        const float *p=campaign_navigation.nodes[i].candidate.position;uint32_t k;
        for(d=0,k=0;k<3;k++)d+=(p[k]-target[k])*(p[k]-target[k]);
        if(near==UINT32_MAX || d<best){best=d;near=i;}
    }
    best=0;r=campaign_navigation.references+near;
    for(i=0;i<=r->neighbor_count;i++){
        uint32_t node=i?r->neighbors[i-1]:near;
        if(node>=campaign_navigation.count)continue;
        score=scene_mp_goal_score(target,self,campaign_navigation.nodes[node].candidate.position);
        if(score>best){best=score;best_node=node;}
    }
    if(best_node==UINT32_MAX)return;
    npc->script_move.active=1;npc->script_move.stop=0;npc->script_move.follow=0;
    npc->script_move.route_index=1;npc->script_move.retry=0;npc->navigation.retained.count=0;
    memcpy(npc->script_move.target,campaign_navigation.nodes[best_node].candidate.position,12);
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
#define SCENE_MP_WEAPON_SEEK 0 /* attack-update seeks; see scene_mp_route_tick */
/* Bot on a route (1 seek, +484 0x800000; 2 idle walk), bot had a target last update. */
static uint8_t scene_mp_routing[SCENE_MP_BOTS],scene_mp_had_target[SCENE_MP_BOTS];
/* Routes started: weapon, roam, target-lost roam, finished; announcements, last route status. */
uint32_t rf_scene_mp_routes[6];
static uint32_t scene_mp_announced;
/* 12a5d0: rank of the bot's weapon in its row, -2 when absent. */
static int32_t scene_mp_weapon_rank(uint32_t slot,int32_t weapon)
{
    uint32_t i;
    for(i=0;i<8;i++)if(scene_mp_weapon_ids[scene_mp_preference_rows[slot][i]]==weapon)return (int32_t)i;
    return -2;
}
/* 12e2a0: lists whose name starts with prefix, one at random, walked one
 * way in waypoint mode 4. A seek (+484 0x800000) takes no target on the way;
 * an idle walk (port stand-in for the idle mode) runs as an actor default
 * route, which the ordinary acquisition may interrupt. */
static int scene_mp_route(uint32_t slot,const char *prefix,uint32_t idle)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;rf_level_waypoint_path found[32];
    uint32_t count,lists,cursor=4,i,n=0,length,nodes,plen=(uint32_t)strlen(prefix);int status;
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
    npc->script_move.event=0;npc->script_move.origin=idle?SCENE_MOVE_ORIGIN_ACTOR_DEFAULT:SCENE_MOVE_ORIGIN_EVENT;
    npc->script_move.path=found[i];npc->script_move.path_index=0;
    npc->script_move.path_mode=0;npc->script_move.path_reverse=0;
    status=campaign_ai_mode_apply(npc,4,scene_patrol_now(rf_scene_profile_stage[0]));
    rf_scene_mp_routes[5]=(uint32_t)status;
    if(status){npc->script_move.path.indices=NULL;npc->script_move.path.count=0;return status==RF_NOT_FOUND?RF_RANGE:status;}
    scene_mp_routing[slot]=(uint8_t)(idle?2:1);return RF_OK;
}
/* 12ffd0 tail for one live bot: 12f750 weapon seek, 12f8e0 roam. */
static int scene_mp_route_tick(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;uint32_t target=npc->combat_alert!=0;int status=RF_NOT_FOUND;int32_t rank;
    if(scene_mp_routing[slot]){
        uint32_t walking=npc->script_move.active && npc->script_move.path.count &&
            npc->script_move.origin==(scene_mp_routing[slot]==2?SCENE_MOVE_ORIGIN_ACTOR_DEFAULT:SCENE_MOVE_ORIGIN_EVENT);
        if(walking && scene_mp_routing[slot]==2 && target){npc->script_move.active=0;npc->navigation.retained.count=0;walking=0;}
        if(walking)return RF_OK;
        scene_mp_routing[slot]=0;++rf_scene_mp_routes[3];
    }
    if(npc->combat_scripted==1 || campaign_player_damage.state.effects.health<=0){scene_mp_had_target[slot]=(uint8_t)target;return RF_OK;}
    if(target){
        /* Held off (with the 1-in-64 roam below): alone, the per-frame
         * 12f750/12f8e0 rates pull bots off every fight (dm01 matches: 0 and
         * 21 bot shots against 359 without them); the PS2 behaviour that
         * keeps them shooting (fire on the route, item pickup) is not traced
         * yet. */
        if(SCENE_MP_WEAPON_SEEK && !(scene_mp_next()%4)){
            rank=scene_mp_weapon_rank(slot,npc->view.weapons[0]);
            if(rank==-2 || (rank>=0 && !(scene_mp_next()%(uint32_t)(rank+2)))){
                status=scene_mp_route(slot,"weapon",0);if(!status)++rf_scene_mp_routes[0];
            }
        }
        if(SCENE_MP_WEAPON_SEEK && status==RF_NOT_FOUND && !(scene_mp_next()%64)){status=scene_mp_route(slot,"",0);if(!status)++rf_scene_mp_routes[1];}
    }else if(!npc->script_move.active){
        status=scene_mp_route(slot,"",!scene_mp_had_target[slot]);if(!status)++rf_scene_mp_routes[scene_mp_had_target[slot]?2:1];
    }
    scene_mp_had_target[slot]=(uint8_t)target;
    if(status==RF_NOT_FOUND && !target && !npc->script_move.active)scene_mp_seek(npc,0); /* arena without lists */
    return status==RF_NOT_FOUND?RF_OK:status;
}
/* 1dbb60(8/9): mp_ann_10 / mp_ann_11, played flat. */
static void scene_mp_announce(const char *name)
{
    uint32_t index=0;
    if(rf_audio_bank_declare(&campaign_audio_bank,name,1,1,1,&index))return;
    if(!rf_audio_bank_sample(&campaign_audio_bank,index)){
        if(campaign_ambient_reload(index))return;
        if(index<sizeof(campaign_audio_evictable))campaign_audio_evictable[index]=1;
    }
    if(campaign_sound_start((int32_t)index,scene_actor_body.state.position,1,0,0,0)>=0)++rf_scene_mp_routes[4];
}
/* Per update, after frame-zero setup. */
static int scene_mp_tick(uint32_t frame)
{
    uint32_t i,count;int status;
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
        scene_mp_random=0x9e3779b9u;
        for(i=0;i<8;i++)scene_mp_weapon_ids[i]=rf_weapon_name_find(&campaign_weapon_supply.names,scene_mp_weapon_names[i]);
        memset(scene_mp_routing,0,sizeof(scene_mp_routing));memset(scene_mp_had_target,0,sizeof(scene_mp_had_target));
        memset(rf_scene_mp_routes,0,sizeof(rf_scene_mp_routes));scene_mp_announced=0;
        return RF_OK;
    }
    if(rf_scene_mp[6])return RF_OK;
    for(i=0;i<count;i++){
        campaign_npc_body *npc=campaign_npc_bodies+i;scene_mp_bot *b=rf_scene_mp_roster+i;
        if(!b->dead && npc->registration.view && npc->damage.effects.health<=0){
            b->dead=1;scene_mp_score(i,npc->registration.handle,npc->damage.responsible_handle);
        }
        if(b->dead && !b->respawn_pending && (!npc->registration.view ||
           (campaign_model_owners && i<campaign_model_owner_count && campaign_model_owners[i].owned)))b->respawn_pending=1;
        if(b->respawn_pending){status=scene_mp_bot_respawn(i);if(status)return status;scene_mp_routing[i]=scene_mp_had_target[i]=0;}
        else if(!b->dead && npc->registration.view && npc->damage.effects.health>0){
            status=scene_mp_route_tick(i);if(status)return status;
        }
    }
    if(!scene_mp_player_dead && campaign_player_damage.state.effects.health<=0){
        scene_mp_player_dead=1;
        scene_mp_score(UINT32_MAX,campaign_player_object.handle,campaign_player_damage.state.responsible_handle);
    }else if(campaign_player_damage.state.effects.health>0)scene_mp_player_dead=0;
    rf_scene_mp[3]=rf_scene_player_life[0];rf_scene_mp[7]=rf_scene_player_life[1];
    /* 1dc0e0: time, then the kill limit for the player and every bot. */
    if(rf_scene_mp_time_limit){
        float left;scene_mp_clock+=scene_step_seconds;left=(float)rf_scene_mp_time_limit-scene_mp_clock;
        rf_scene_mp[5]=left>0?(uint32_t)left:0;
        if(left<0)rf_scene_mp[6]=1;
    }
    if(rf_scene_mp_kill_limit){
        if(rf_scene_mp[2]!=scene_mp_announced){
            if(rf_scene_mp_kill_limit>5 && rf_scene_mp[2]==rf_scene_mp_kill_limit-5)scene_mp_announce("mp_ann_10.wav");
            else if(rf_scene_mp_kill_limit>1 && rf_scene_mp[2]==rf_scene_mp_kill_limit-1)scene_mp_announce("mp_ann_11.wav");
            scene_mp_announced=rf_scene_mp[2];
        }
        if(rf_scene_mp[2]>=rf_scene_mp_kill_limit)rf_scene_mp[6]=1;
        for(i=0;i<count;i++)if(rf_scene_mp_roster[i].used && rf_scene_mp_roster[i].kills>=(int32_t)rf_scene_mp_kill_limit)rf_scene_mp[6]=1;
    }
    return RF_OK;
}
/* campaign_life_input: "Press Fire to respawn" (strings.tbl 3). */
static uint32_t scene_mp_fire_held;
static uint32_t scene_mp_respawn_pressed(const rf_scene_input *input)
{
    uint32_t pressed;
    if(!rf_scene_mp[0])return 0;
    pressed=input->fire && !scene_mp_fire_held;scene_mp_fire_held=!!input->fire;return pressed;
}
static void scene_mp_player_respawned(void)
{
    if(!rf_scene_mp[0])return;
    memcpy(scene_actor_body.state.position,scene_mp_pick(0),12);
    memset(scene_actor_body.state.velocity,0,12);
}
