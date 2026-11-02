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
 *    identity is released, the corpse deleted, the slot restored from its
 *    first-frame snapshot and its model and registration made again. The
 *    corpse goes with it (the PS2 corpse stays).
 *  - Scoring per death (274c50) from the lethal credit (responsible handle):
 *    a bot victim gains a death and loses its streak; a suicide loses a kill;
 *    a bot killer gains a kill and streak (and a human kill when the victim
 *    is the player); a player killer gains a kill.
 *  - Match (1dbf20/1dc0e0): time limit counts down (0 = none); the kill
 *    limit ends the match when the player or any bot reaches it (0 = none).
 *  - The player respawns on Fire ("Press Fire to respawn", strings.tbl 3) at
 *    a player point.
 * Port deviations: respawned bots keep their authored affiliation instead of
 * 3 (the port AI has no multiplayer affiliation rules yet); the limit-5 and
 * limit-1 announcements (1dbb60 8/9) are not shown. */
uint32_t rf_scene_mp_bots,rf_scene_mp_kill_limit,rf_scene_mp_time_limit;
/* active, bots, player kills, player deaths, bot respawns, seconds left, match over, player respawns */
uint32_t rf_scene_mp[8];
/* Bot creation trace: step reached, status. */
uint32_t rf_scene_mp_seed_trace[2];
/* Bot deaths, kills scored by bots, bot suicides. */
uint32_t rf_scene_mp_bot_deaths[3];
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
/* 2740e0: the arena's characters, first N kept. Classes are compacted to
 * those the kept records use (each class needs its first record). */
static int scene_mp_bot_seeds(const rf_level *level,rf_vpp *tables)
{
    uint32_t count,k,c,remap[256],classes=0;int status;
    memset(rf_scene_mp,0,sizeof(rf_scene_mp));memset(rf_scene_mp_roster,0,sizeof(rf_scene_mp_roster));
    memset(rf_scene_mp_bot_deaths,0,sizeof(rf_scene_mp_bot_deaths));scene_mp_clock=0;scene_mp_player_dead=0;
    rf_scene_mp_seed_trace[0]=1;
    status=scene_mp_spawns_read(level);rf_scene_mp_seed_trace[1]=(uint32_t)status;if(status)return status;
    rf_scene_mp_seed_trace[0]=2;
    status=rf_entity_seeds_open(level,tables,1024*1024,&campaign_seeds);rf_scene_mp_seed_trace[1]=(uint32_t)status;
    if(status)return status;
    count=rf_scene_mp_bots<campaign_seeds.records.count?rf_scene_mp_bots:campaign_seeds.records.count;
    if(count>SCENE_MP_BOTS)count=SCENE_MP_BOTS;
    if(campaign_seeds.class_count>256)return RF_RANGE;
    for(c=0;c<campaign_seeds.class_count;c++)remap[c]=UINT32_MAX;
    for(k=0;k<count;k++){
        c=campaign_seeds.items[k].class_index;if(c>=campaign_seeds.class_count)return RF_RANGE;
        if(remap[c]==UINT32_MAX){
            remap[c]=classes;
            if(classes!=c){rf_entity_seed_class keep=campaign_seeds.classes[c];campaign_seeds.classes[classes]=keep;}
            campaign_seeds.classes[classes].record_index=k;++classes;
        }
        campaign_seeds.items[k].class_index=remap[c];
        rf_scene_mp_roster[k].used=1;
    }
    campaign_seeds.records.count=count;campaign_seeds.class_count=classes;
    scene_seed_uid_table.items=NULL;rf_scene_mp_seed_trace[0]=3;
    rf_scene_mp[0]=1;rf_scene_mp[1]=count;rf_scene_mp[5]=rf_scene_mp_time_limit;
    return RF_OK;
}
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
        /* Delete the corpse that holds this actor's model (its update.model is
         * slot+1), the way campaign_live_corpses_close retires corpses. */
        uint32_t c;
        for(c=0;c<RF_CORPSE_CAPACITY;++c)
            if((campaign_live_corpses.pool.active_mask&(1u<<c)) && campaign_live_corpses.slots[c].corpse.update.model==slot+1){
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
 * the combat update; until the idle path-following (12e520) is ported, idle
 * Xbox bots run it as well. */
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
        if(b->respawn_pending){status=scene_mp_bot_respawn(i);if(status)return status;}
        else if(!b->dead && npc->registration.view && npc->damage.effects.health>0 &&
                !npc->combat_alert && !npc->combat_scripted && !npc->script_move.active &&
                campaign_player_damage.state.effects.health>0)scene_mp_seek(npc,0);
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
