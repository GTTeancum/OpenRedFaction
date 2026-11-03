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
static float scene_mp_aim_applied[SCENE_MP_BOTS][2]; /* scene_mp_aim_bone */
char rf_scene_mp_player_class[32],rf_scene_mp_player_skin[32];
/* The human's class flags (+0x264 -> +0x714) from entity.tbl, read at match
 * start; the model and skin are not drawn (no third-person view of the
 * human in the port). */
static uint32_t scene_mp_player_class_flags;
extern float rf_scene_gameplay_eye[3];static int combat_obstructed(scene_stream *,const float *,const float *,uint32_t *);
/* Bot sight of its target (12b310, every 200-400 ms, timer +0x1e4): +0x1ec
 * the target's body, +0x1ed its eye; timer +0x188 runs 2-5 s once both
 * are lost (12ffd0). Times in ms of the patrol clock; 0 is unset. */
static uint8_t scene_mp_sight[SCENE_MP_BOTS][2];static uint32_t scene_mp_sight_due[SCENE_MP_BOTS],scene_mp_lost_due[SCENE_MP_BOTS];
uint32_t rf_scene_mp_aim_trace[8]; /* see scene_mp_aim_tick */
static uint32_t scene_mp_aim_seen;
static float scene_mp_aim_clock[SCENE_MP_BOTS];static uint8_t scene_mp_aim_on[SCENE_MP_BOTS],scene_mp_aim_active[SCENE_MP_BOTS]; /* upper-body aim (scene_mp_aim_tick) */
uint32_t rf_scene_mp_player_stats[3];float rf_scene_mp_elapsed;static uint32_t scene_mp_player_streak; /* MP STATS (see scene_mp_round_count) */
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
    memset(rf_scene_mp_player_stats,0,sizeof(rf_scene_mp_player_stats));scene_mp_player_streak=0;rf_scene_mp_elapsed=0;
    memset(scene_mp_aim_applied,0,sizeof(scene_mp_aim_applied));memset(scene_mp_sight,0,sizeof(scene_mp_sight));memset(scene_mp_sight_due,0,sizeof(scene_mp_sight_due));memset(scene_mp_lost_due,0,sizeof(scene_mp_lost_due));
    {rf_entity_seed_class def;memset(&def,0,sizeof(def));scene_mp_player_class_flags=0;
     if(!rf_entity_seed_class_read(tables,rf_scene_mp_player_class[0]?rf_scene_mp_player_class:"miner2",&def))scene_mp_player_class_flags=def.physics.flags;}
    memset(scene_mp_aim_clock,0,sizeof(scene_mp_aim_clock));memset(rf_scene_mp_aim_trace,0,sizeof(rf_scene_mp_aim_trace));memset(scene_mp_aim_on,0,sizeof(scene_mp_aim_on));memset(scene_mp_aim_active,0,sizeof(scene_mp_aim_active));
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
            /* miner1 with its default skin (section 5 note 1): an authored skin
             * of the original class (dm07) is not a miner1 skin. */
            strcpy(record->class_name,"miner1");record->skin[0]=0;campaign_seeds.items[k].class_index=miner;
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
/* MP STATS counters (2399a0). Human (+0x2c record): kills +0 and streak
 * +0x10 / longest +0xc (1a6af0), deaths +8 clearing the streak (1a6b60),
 * hits +0x14, shots +0x18; bots the same in the roster ([6] hits, [7]
 * shots). A shot is one round created (1ccef0); a hit is a round striking
 * an entity that is not its shooter (1ca3c0). */
static void scene_mp_round_count(uint32_t source,uint32_t hit)
{
    uint32_t i;
    if(!rf_scene_mp[0] || rf_scene_mp[6])return;
    if(source==campaign_player_object.handle){++rf_scene_mp_player_stats[hit?1:2];return;}
    for(i=0;i<campaign_npc_body_count && i<SCENE_MP_BOTS;i++)
        if(rf_scene_mp_roster[i].used && campaign_npc_bodies[i].registration.handle==source){
            if(hit)++rf_scene_mp_roster[i].hits;else ++rf_scene_mp_roster[i].shots;return;
        }
}
static float scene_mp_accuracy(uint32_t hits,uint32_t shots)
{float a;if(!shots)return 0;a=(float)hits/(float)shots;return a>1?1:a;}
uint32_t rf_scene_mp_stats(rf_scene_mp_stats_row *rows,uint32_t capacity)
{
    uint32_t n=0,i,j;
    if(!rows || !capacity || !rf_scene_mp[0])return 0;
    memset(rows,0,capacity*sizeof(*rows));
    strcpy(rows[0].name,"PLAYER"); /* the port has no multiplayer name */
    rows[0].kills=(int32_t)rf_scene_mp[2];rows[0].deaths=(int32_t)rf_scene_mp[3];rows[0].longest=rf_scene_mp_player_stats[0];
    rows[0].accuracy=scene_mp_accuracy(rf_scene_mp_player_stats[1],rf_scene_mp_player_stats[2]);n=1;
    for(i=0;i<SCENE_MP_BOTS && n<capacity;i++)if(rf_scene_mp_roster[i].used){
        const scene_mp_bot *b=rf_scene_mp_roster+i;
        strncpy(rows[n].name,rf_scene_mp_bot_name(i),31);
        rows[n].kills=b->kills;rows[n].deaths=(int32_t)b->deaths;rows[n].longest=b->longest;
        rows[n].accuracy=scene_mp_accuracy(b->hits,b->shots);++n;
    }
    /* 239220: row i swaps with any later row that has more kills. */
    for(i=0;i<n;i++)for(j=i;j<n;j++)if(rows[j].kills>rows[i].kills){rf_scene_mp_stats_row t=rows[i];rows[i]=rows[j];rows[j]=t;}
    return n;
}
/* 274c50 for one death: victim slot (UINT32_MAX = player), its credit. */
static void scene_mp_score(uint32_t victim,uint32_t victim_handle,uint32_t killer)
{
    uint32_t i;
    if(victim<SCENE_MP_BOTS){++rf_scene_mp_roster[victim].deaths;rf_scene_mp_roster[victim].streak=0;++rf_scene_mp_bot_deaths[0];}
    else scene_mp_player_streak=0;
    if(killer==victim_handle){
        if(victim<SCENE_MP_BOTS){--rf_scene_mp_roster[victim].kills;++rf_scene_mp_bot_deaths[2];}
        else if(rf_scene_mp[2])--rf_scene_mp[2];
        return;
    }
    if(killer==campaign_player_object.handle){
        ++rf_scene_mp[2];++scene_mp_player_streak;
        if(scene_mp_player_streak>rf_scene_mp_player_stats[0])rf_scene_mp_player_stats[0]=scene_mp_player_streak;
        return;
    }
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
static float scene_mp_last_position[SCENE_MP_BOTS][3]; /* body position at the previous update */
static uint32_t scene_mp_range_due[SCENE_MP_BOTS]; /* timer +0x180 (ms): set 0 at start, 500 ms ahead while in range */
static float scene_mp_attack_range(uint32_t slot);
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
/* Upper-body aim (151110 -> 14f210 / 150c30) for a bot. 14f210 (not the
 * human): a target, a weapon, the target at least 1.0 higher or lower in Y,
 * not the riot shield, alive, a skeletal humanoid. Port note: its movement
 * state 0xd/0xf, AI mode, carried-object, hidden, scripted-animation and
 * vehicle-seat tests are not mapped (none of those occur for these bots).
 * 150c30: timer +0x12e4 (dt) ramps the weight up over 0.75 s while on, down
 * after it turns off (timer reset on the change, and at 0); for each spine
 * bone (+0x1028, +0x102c = rf_model_death_bones 0/1) the eye facing (+0x71c)
 * is taken into the entity frame (+0x48) then into that bone's last frame;
 * the pitch atan2(y, z) (halved for the first bone; the second's quadrant
 * fix-up is 3.1415927 - atan for y<0, z<0, as the code does) is clamped to
 * -0.785398..0.45, and the override basis is the bone's basis turned about
 * its own right axis by that angle (1f86f0: up turned, forward = right x
 * up), forward toward the target. */
static int scene_mp_aim_target(const campaign_npc_body *npc,float position[3])
{
    uint32_t j;
    if(!npc->combat_target)return 0;
    if(npc->combat_target==campaign_player_object.handle){
        if(campaign_player_damage.state.effects.health<=0)return 0;
        memcpy(position,scene_actor_body.state.position,12);return 1;
    }
    for(j=0;j<campaign_npc_body_count;j++)if(campaign_npc_bodies[j].registration.view && campaign_npc_bodies[j].registration.handle==npc->combat_target){
        if(campaign_npc_bodies[j].damage.effects.health<=0)return 0;
        memcpy(position,campaign_npc_bodies[j].body.state.position,12);return 1;
    }
    return 0;
}
/* Harness: bots fully leaning (weight 1, second-bone pitch at least 0.1)
 * this update, the most-leaning one's UID, its pitch in milliradians
 * (signed), and 1 when it is the crash-input aim target or aims at the
 * player. Observation only. */
uint32_t rf_scene_mp_aim[4];
/* Cumulative: updates, no pose/overrides, no spine bones, target present,
 * height test passed, aim on, weight-1 updates, max |pitch| milliradians. */
extern uint32_t rf_scene_crash_aim_trace[4];
/* The turn last applied to each spine bone (weight * angle; the pose blend
 * of a turn about the bone's own right axis is that axis at weight*angle). */

static float scene_mp_aim_bone(rf_entity_pose *pose,int32_t bone,const float facing[3],uint32_t first,float weight,float *applied)
{
    const float *p;float m[9],d[3],angle,c,s,up[3],forward[3];uint32_t k;rf_model_bone_override *o;
    if(bone<0 || (uint32_t)bone>=pose->bone_count)return 0;
    p=pose->matrices[bone];o=pose->overrides+bone;
    /* The pose holds last update's turned frame; measure in the animated
     * frame (150c30 reads the bone frame without this aim), so undo the
     * applied turn about the right axis first. */
    c=cosf(*applied);s=sinf(*applied);memcpy(m,p,12);
    for(k=0;k<3;k++){m[3+k]=p[3+k]*c+p[6+k]*s;m[6+k]=p[6+k]*c-p[3+k]*s;}
    for(k=0;k<3;k++)d[k]=facing[0]*m[k*3]+facing[1]*m[k*3+1]+facing[2]*m[k*3+2];
    angle=d[2]!=0?atanf(d[1]/d[2]):0;
    if(d[1]>0 && d[2]<0)angle+=3.1415927f;
    else if(d[1]<0 && d[2]<0)angle=first?angle-3.1415927f:3.1415927f-angle;
    if(first)angle*=.5f;
    if(angle>.45f)angle=.45f;else if(angle<-.785398f)angle=-.785398f;
    c=cosf(angle);s=sinf(angle);
    for(k=0;k<3;k++){up[k]=m[3+k]*c-m[6+k]*s;}
    forward[0]=m[1]*up[2]-m[2]*up[1];forward[1]=m[2]*up[0]-m[0]*up[2];forward[2]=m[0]*up[1]-m[1]*up[0];
    memcpy(o->basis,m,12);memcpy(o->basis+3,up,12);memcpy(o->basis+6,forward,12);
    o->weight=weight;o->enabled=1;*applied=weight*angle;return angle;
}
static void scene_mp_aim_tick(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;rf_entity_pose *pose=NULL;int32_t bones[3];
    float target[3],facing[3],weight;const float *eye,*basis;uint32_t k,on;
    ++rf_scene_mp_aim_trace[0];
    if(campaign_model_pose(slot,&pose) || !pose || !pose->overrides || !pose->matrices || !campaign_model_owners){++rf_scene_mp_aim_trace[1];return;}
    if(rf_entity_class_death_bones(&campaign_seeds,&campaign_skeletons,campaign_seeds.items[slot].class_index,bones) ||
       bones[0]<0 || bones[1]<0){++rf_scene_mp_aim_trace[2];return;}
    if(scene_mp_aim_target(npc,target)){++rf_scene_mp_aim_trace[3];if(fabsf(npc->body.state.position[1]-target[1])>=1.0f)++rf_scene_mp_aim_trace[4];}
    on=npc->damage.effects.health>0 && npc->view.weapons[0]>=0 && npc->view.weapons[0]!=campaign_shield_id &&
       scene_mp_aim_target(npc,target) && fabsf(npc->body.state.position[1]-target[1])>=1.0f;
    if(!on){
        if(!scene_mp_aim_active[slot])return; /* 151110: only while an override is live */
        if(scene_mp_aim_on[slot]){scene_mp_aim_clock[slot]=0;scene_mp_aim_on[slot]=0;}
    }else{scene_mp_aim_on[slot]=1;++rf_scene_mp_aim_trace[5];}
    scene_mp_aim_clock[slot]+=scene_step_seconds;
    if(on)weight=scene_mp_aim_clock[slot]<.75f?scene_mp_aim_clock[slot]/.75f:1.0f;
    else weight=scene_mp_aim_clock[slot]>=.75f?0.0f:1.0f-scene_mp_aim_clock[slot]/.75f;
    /* Eye facing into the entity frame (1fb210 rows dot). */
    eye=npc->look.orientation+6;basis=campaign_model_owners[slot].basis;
    for(k=0;k<3;k++)facing[k]=eye[0]*basis[k*3]+eye[1]*basis[k*3+1]+eye[2]*basis[k*3+2];
    {float pitch;int32_t milli;
     scene_mp_aim_bone(pose,bones[0],facing,1,weight,&scene_mp_aim_applied[slot][0]);
     pitch=scene_mp_aim_bone(pose,bones[1],facing,0,weight,&scene_mp_aim_applied[slot][1]);milli=(int32_t)(pitch*1000.0f);
     if(weight>=1.0f){++rf_scene_mp_aim_trace[6];if((uint32_t)(milli<0?-milli:milli)>rf_scene_mp_aim_trace[7])rf_scene_mp_aim_trace[7]=(uint32_t)(milli<0?-milli:milli);}
     if(weight>=1.0f && fabsf(pitch)>=.1f){
        uint32_t uid=(uint32_t)campaign_seeds.records.items[slot].record.uid;
        const float *f=actor_look.eye_orientation+6;float v[3],len=0,dot=0;uint32_t seen,blocked=1;
        for(k=0;k<3;k++){v[k]=npc->body.state.position[k]-scene_actor_body.state.position[k];len+=v[k]*v[k];dot+=v[k]*f[k];}
        /* On screen: inside the camera's view cone (cosine 0.85) with a clear
         * sight ray from the camera to the bot's eye. */
        for(k=0;k<3;k++){v[k]=npc->eye_position[k]-rf_scene_gameplay_eye[k];}
        seen=len>0 && dot>.85f*sqrtf(len) && scene_actor_collision_owner &&
             !combat_obstructed(scene_actor_collision_owner,rf_scene_gameplay_eye,v,&blocked) && !blocked;
        if(seen && (!rf_scene_mp_aim[3] || (milli<0?-milli:milli)>((int32_t)rf_scene_mp_aim[2]<0?-(int32_t)rf_scene_mp_aim[2]:(int32_t)rf_scene_mp_aim[2]))){
            rf_scene_mp_aim[1]=uid;rf_scene_mp_aim[2]=(uint32_t)milli;rf_scene_mp_aim[3]=1;scene_mp_aim_seen=scene_mp_boot_updates;
            if(rf_scene_crash_hold_enabled==2 && !rf_scene_crash_hold && len<6.0f*6.0f && (milli<0?-milli:milli)>=150)rf_scene_crash_hold=1; /* close enough to see */
        }
        ++rf_scene_mp_aim[0];
     }}
    scene_mp_aim_active[slot]=1;
    if(!on && weight==0){pose->overrides[bones[0]].enabled=pose->overrides[bones[1]].enabled=0;scene_mp_aim_active[slot]=0;scene_mp_aim_clock[slot]=0;
        scene_mp_aim_applied[slot][0]=scene_mp_aim_applied[slot][1]=0;}
}
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
 * The bot's node comes from 138500 (scene_mp_bot_node). Port: the single best
 * node is walked as a one-node route (no fallback to the other candidates
 * when it cannot be planned); the second node +0x5c0 is not kept. */
static uint8_t scene_mp_step_node[SCENE_MP_BOTS][4],scene_mp_step_pending[SCENE_MP_BOTS];
static float scene_mp_segment_gap(const float target[3],const float self[3],const float point[3])
{
    float d[3],e[3],dd=0,de=0,t,g=0;uint32_t k;
    for(k=0;k<3;k++){d[k]=point[k]-self[k];e[k]=target[k]-self[k];dd+=d[k]*d[k];de+=d[k]*e[k];}
    t=dd>0?de/dd:0;if(t<0)t=0;if(t>1)t=1;
    for(k=0;k<3;k++){float c=self[k]+d[k]*t-target[k];g+=c*c;}
    return g;
}
static int scene_mp_bot_node(campaign_npc_body *npc,uint32_t *node);
static int scene_mp_step_away(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;rf_level_waypoint_path path;
    const float *target,*self=npc->body.state.position;float best=0,gap;
    uint32_t i,k,near_node=UINT32_MAX,chosen=UINT32_MAX;
    scene_mp_routing[slot]=1;scene_mp_step_query=1;target=scene_mp_route_aim(npc);scene_mp_step_query=0;scene_mp_routing[slot]=0;
    if(!target || !campaign_navigation.nodes || !campaign_navigation.references)return RF_NOT_FOUND;
    if(scene_mp_bot_node(npc,&near_node))return RF_NOT_FOUND; /* 138b30 -> 138500 */
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
 * skipped. */
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
    /* 132fb0 then: a humanoid (class flag 0x20000, 15a3b0) whose weapon is
     * not melee-only (134100: weapon record +0x244 bit 0x20 `melee` with no
     * secondary; the riot stick in multiplayer) runs a `health` route when
     * rand & 3 == 0 (the mode 2 branch 12e8e0 cannot occur in multiplayer). */
    {uint32_t cls=campaign_seeds.items[slot].class_index;
     if(cls<campaign_seeds.class_count && (campaign_seeds.classes[cls].physics.flags&0x20000u) &&
        npc->view.weapons[0]!=campaign_riot_id && !(scene_mp_next()&3)){
         if(scene_mp_route(slot,"health")==RF_OK)++rf_scene_mp_hits[0];
         return 1;
     }}
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
        /* Step 3 (12ffd0). 12b310 first: on its 200-400 ms tick, two sight
         * rays from the bot's eye (+0x6f8), to the target's body (+0x1ec)
         * and its eye (+0x1ed); either clear sets the last-seen time and,
         * with the target within 132b10's range, timer +0x180 500 ms ahead.
         * Losing both starts timer +0x188 (2-5 s). Then 12fa00: out of range
         * (body to body, minus 3 when the target's class flag 0x400 is set),
         * or in range with both unseen and +0x188 expired; with +0x180
         * expired and rand < 0x1fffffff, a health route (1339e0). Port
         * notes: the rays are the port's sight ray (not 1953e0's swept
         * sphere); the halved interval for weapon flag +0x248 8, the 1.5
         * range factor and the flat distance for movement sub-state +0x204
         * (2, 3; not tracked) are left out. */
        if(SCENE_MP_WEAPON_SEEK){
            const float *tp;float d=0,range=scene_mp_attack_range(slot);uint32_t k,now=(uint32_t)scene_patrol_now(rf_scene_profile_stage[0]),out;
            scene_mp_routing[slot]=1;scene_mp_step_query=1;tp=scene_mp_route_aim(npc);scene_mp_step_query=0;scene_mp_routing[slot]=0;
            if(tp){
                const float *teye=NULL;uint32_t flags=0,j,was=scene_mp_sight[slot][0] || scene_mp_sight[slot][1];
                if(npc->combat_target==campaign_player_object.handle){teye=rf_scene_gameplay_eye;flags=scene_mp_player_class_flags;}
                else for(j=0;j<campaign_npc_body_count;j++)if(campaign_npc_bodies[j].registration.handle==npc->combat_target){
                    uint32_t c=campaign_seeds.items[j].class_index;teye=campaign_npc_bodies[j].eye_position;
                    if(c<campaign_seeds.class_count)flags=campaign_seeds.classes[c].physics.flags;break;}
                for(k=0;k<3;k++)d+=(tp[k]-npc->body.state.position[k])*(tp[k]-npc->body.state.position[k]);
                d=sqrtf(d);if(flags&0x400u)d-=3.0f; /* 15a1e0 */
                if(scene_actor_collision_owner && (int32_t)(now-scene_mp_sight_due[slot])>=0){
                    float ray[3];uint32_t blocked=1,m;
                    scene_mp_sight_due[slot]=now+200u+scene_mp_next()%200u;
                    for(m=0;m<2;m++){
                        const float *to=m?teye:tp;blocked=1;
                        if(to){for(k=0;k<3;k++)ray[k]=to[k]-npc->eye_position[k];
                            if(combat_obstructed(scene_actor_collision_owner,npc->eye_position,ray,&blocked))blocked=1;}
                        scene_mp_sight[slot][m]=!blocked;
                    }
                    if(scene_mp_sight[slot][0] || scene_mp_sight[slot][1]){if(d<range)scene_mp_range_due[slot]=now+500u;}
                    else if(was)scene_mp_lost_due[slot]=(now+2000u+scene_mp_next()%3001u)|1u;
                }
                out=d>range || (!scene_mp_sight[slot][0] && !scene_mp_sight[slot][1] && scene_mp_lost_due[slot] && (int32_t)(now-scene_mp_lost_due[slot])>=0);
                if(out && (int32_t)(now-scene_mp_range_due[slot])>=0 && scene_mp_next()<0x1fffffffu){
                    status=scene_mp_route(slot,"health");
                    if(!status){++rf_scene_mp_hits[0];scene_mp_had_target[slot]=1;return RF_OK;}
                    if(status!=RF_NOT_FOUND && status!=RF_RANGE)return status;
                    status=RF_NOT_FOUND;
                }
            }
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
/* Harness: announcer lines started, failed starts, bit mask of lines that
 * started, bit mask of lines seen still playing on the device. */
uint32_t rf_scene_mp_line_trace[4];
static uint32_t scene_mp_line_playing(uint32_t n)
{
    uint32_t source;
    if(scene_mp_line_voice[n]<0 || rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,scene_mp_line_voice[n],&source))return 0;
    /* The device clock decides on the Xbox (its mixer copy is not drained). */
    if(campaign_audio_events.playing){
        uint32_t on=campaign_audio_events.playing(campaign_audio_events_context,source)!=0;
        if(on)rf_scene_mp_line_trace[3]|=1u<<n;return on;
    }
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
    if(scene_mp_line_voice[n]>=0){++rf_scene_mp_routes[4];++rf_scene_mp_line_trace[0];rf_scene_mp_line_trace[2]|=1u<<n;}
    else ++rf_scene_mp_line_trace[1];
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
/* End screen (1dc1a0): play the queued winner line once line 4 ends, and
 * report whether a line still plays; the continue prompt (2ac560) is held
 * at zero meanwhile. */
uint32_t rf_scene_mp_results_audio(void)
{
    uint32_t k;
    if(!rf_scene_mp[0])return 0;
    if(scene_mp_winner_line>=0 && scene_mp_line_played[4] && !scene_mp_line_playing(4)){
        scene_mp_line_start((uint32_t)scene_mp_winner_line);scene_mp_winner_line=-1;
    }
    if(scene_mp_winner_line>=0)return 1;
    for(k=0;k<11;k++)if(scene_mp_line_playing(k))return 1;
    return 0;
}
uint32_t rf_scene_mp_ps2_hud;
uint32_t rf_scene_mp_hud_values(int32_t *health,int32_t *armor,int32_t *kills,uint32_t *rank,uint32_t *total,uint32_t *seconds)
{
    uint32_t i;
    if(!rf_scene_mp[0] || rf_scene_mp[6] || campaign_player_damage.state.effects.health<=0)return 0;
    *health=(int32_t)campaign_player_damage.state.effects.health;*armor=(int32_t)campaign_player_damage.state.effects.armor;
    *kills=(int32_t)rf_scene_mp[2];*rank=1;*total=1;
    for(i=0;i<SCENE_MP_BOTS;i++)if(rf_scene_mp_roster[i].used){++*total;if(rf_scene_mp_roster[i].kills>(int32_t)rf_scene_mp[2])++*rank;}
    *seconds=rf_scene_mp_time_limit?rf_scene_mp[5]:UINT32_MAX;
    return 1;
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
uint32_t rf_scene_mp_rank(void){return rf_scene_mp[0]?scene_mp_player_rank():0;}
/* Per update, after frame-zero setup. */
static int scene_mp_tick(uint32_t frame)
{
    uint32_t i,count;int status;
    ++scene_mp_boot_updates;
    if(!rf_scene_mp[0])return RF_OK;
    /* Held 30 updates so the 1 Hz harness sample can see it. */
    if(scene_mp_boot_updates-scene_mp_aim_seen>30)memset(rf_scene_mp_aim,0,sizeof(rf_scene_mp_aim));
    rf_scene_mp_elapsed+=scene_step_seconds; /* 160f40: 80d80c += frame time */
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
        memset(scene_mp_line_played,0,sizeof(scene_mp_line_played));scene_mp_winner_line=-1;memset(rf_scene_mp_line_trace,0,sizeof(rf_scene_mp_line_trace));scene_mp_fade_phase=0;
        for(i=0;i<11;i++)scene_mp_line_voice[i]=-1;
        memset(scene_mp_step_pending,0,sizeof(scene_mp_step_pending));memset(scene_mp_range_due,0,sizeof(scene_mp_range_due));memset(scene_mp_face_target,0,sizeof(scene_mp_face_target));memset(rf_scene_mp_hits,0,sizeof(rf_scene_mp_hits));
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
            /* 149cd0 picks the movement state every frame: stand while the
             * velocity (+0x634) is zero, else walk or run by the speed mode;
             * the attack set while fighting (scene_mp_locomotion_state). The
             * port's velocity test is whether the body moved last update. */
            {uint32_t k,moved=0;
             for(k=0;k<3;k++)if(npc->body.state.position[k]!=scene_mp_last_position[i][k])moved=1;
             memcpy(scene_mp_last_position[i],npc->body.state.position,12);
             status=campaign_script_locomotion(i,moved);if(status)return status;}
            status=scene_mp_route_tick(i);if(status)return status;
            scene_mp_aim_tick(i);
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
/* The PS2 menu pack (platform path; NULL without it), opened per match for
 * the PS2-only sound effects it carries. */
const char *rf_scene_ps2_pack_path;
static rf_vpp scene_mp_ps2_pack;static uint32_t scene_mp_ps2_pack_open;
/* A sound from the PS2 menu pack: declared and loaded from that archive,
 * the bank's archive restored after. */
static int scene_mp_pack_sound(const char *name,float near_distance,float volume,float rolloff,uint32_t *index)
{
    rf_vpp *archive=campaign_audio_bank.archive;int status;uint32_t released[2];
    if(!archive)return RF_NOT_FOUND;
    if(!scene_mp_ps2_pack_open){
        if(!rf_scene_ps2_pack_path || rf_vpp_open(&scene_mp_ps2_pack,rf_scene_ps2_pack_path))return RF_NOT_FOUND;
        scene_mp_ps2_pack_open=1;
    }
    campaign_audio_bank.archive=&scene_mp_ps2_pack;
    status=rf_audio_bank_declare(&campaign_audio_bank,name,near_distance,volume,rolloff,index);
    if(!status && !rf_audio_bank_sample(&campaign_audio_bank,*index)){
        if((campaign_audio_events.play || campaign_audio_events.play_mode) && !campaign_audio_events.release_idle_sample)
            status=rf_audio_bank_reload(&campaign_audio_bank,&scene_mp_ps2_pack,*index);
        else{
            status=rf_audio_bank_reload_idle(&campaign_audio_bank,&campaign_audio_mixer,&scene_mp_ps2_pack,*index,
                campaign_audio_evictable,sizeof(campaign_audio_evictable),campaign_audio_events.release_idle_sample,
                campaign_audio_events_context,released);
            if(!status){rf_scene_sound_bank[1]-=released[0];rf_scene_sound_bank[2]-=released[1];rf_scene_live_audio[1]=campaign_audio_bank.bytes;}
        }
    }
    campaign_audio_bank.archive=archive;return status;
}
/* campaign_life_input: "Press Fire to respawn" (strings.tbl 3). */
static uint32_t scene_mp_fire_held;
uint32_t rf_scene_mp_respawn_sound[3]; /* harness: respawn sounds started, failed, last load status */
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
        /* Sound 0x40 is the PS2 sounds.tbl's "respawn.wav" (10.0, 0.8, 1.0;
         * SND_RESPAWN, multiplayer; respawn.vse on the disc, decoded into
         * the PS2 menu pack). The PC table has Flag_Pickup.wav at 0x40. */
        uint32_t index=0;int32_t voice=-1;
        scene_mp_fade_phase=1;scene_mp_fade_clock=0;
        int loaded=scene_mp_pack_sound("respawn.wav",10.0f,.8f,1.0f,&index);rf_scene_mp_respawn_sound[2]=(uint32_t)loaded;
        if(!loaded){
            if(index<sizeof(campaign_audio_evictable))campaign_audio_evictable[index]=1;
            voice=campaign_sound_start((int32_t)index,scene_actor_body.state.position,1,0,0,0);
        }
        ++rf_scene_mp_respawn_sound[voice>=0?0:1];
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
