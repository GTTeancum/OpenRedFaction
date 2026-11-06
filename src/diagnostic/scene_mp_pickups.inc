#ifndef RF_SCENE_MP_PICKUPS_INCLUDED
#define RF_SCENE_MP_PICKUPS_INCLUDED
/* Multiplayer bot item pickups, reconstructed from the PS2 game (SLUS-20073;
 * docs/MP-BOTS-PS2.md section 8). Include after campaign_pickups_tick.
 *  - 16c2f0/16cba0: in multiplayer any character picks items up, not only
 *    the human; the item's handler runs for the picker.
 *  - Health 16dfb0 / armour 16de80: refused when the picker is already at its
 *    class maximum (class +0x44 / +0x48); otherwise the item amount is added
 *    (unscaled in multiplayer) and capped at that maximum.
 *  - Weapons and ammo 16d970: added to the inventory (1327e0); a bot then
 *    takes its preferred usable weapon (154a20 -> 12a700 forced): the first
 *    weapon of its preference row (28e760) that it owns with clip or reserve
 *    ammo (12a680).
 *  - 16cb00: a bot in AI mode 4 or 7 whose life and armour are both above 95%
 *    of the class maximum goes back to fighting (133d80).
 *  - Respawn (178220 loads the authored seconds: 0 becomes -1, else
 *    seconds * 1000 ms; 16cba0 on a pickup: negative removes the item,
 *    positive hides it (189ff0) and starts timer +0x280; 16ce10 shows it
 *    again (16cda0, sound 1) once the timer expires, checked every 100 ms).
 *  - Spin (16c5e0): in multiplayer an item whose class has items.tbl flag
 *    `spins_in_multi` (bit 2) is drawn turned about the vertical by its own
 *    angle +0x290, advanced 0.75 * pi rad/s while it is drawn.
 *  - Touch (189d50 -> 16c2f0): an item is a physics object with a 0.25
 *    radius sphere (16c760 create info +0x84; mass +0x14 1.0); a character
 *    whose collision spheres touch it picks it up. The default handler
 *    16e0e0 gives the class weapon (16d970) or ammo (16d740) to anyone;
 *    envirosuit 16dde0 sets armour to the class maximum, never refused.
 *  - Sound (16cba0): the class sound +0x20, else 0 (powerups) or 12
 *    (weapon/ammo), played for any picker; reappearing plays sound 1.
 * Port: respawn and spin apply in multiplayer only (single player keeps
 * taken items retired for saves); the doctor uniform's handler is not traced,
 * so bots leave it. */
uint32_t rf_scene_mp_pickups[8]; /* health, armour, weapon or ammo, weapon switches, full refusals, back to fighting, items respawned, items spinning */
#define SCENE_MP_ITEMS 256u
static uint32_t scene_mp_item_due[SCENE_MP_ITEMS]; /* game ms at which it shows again; 0 none */
static uint32_t scene_mp_item_clock,scene_mp_item_checked; /* game ms, last 100 ms check (16ce10) */
/* Global sound slot at a point, as campaign_pickup_sound starts it. */
static void scene_mp_item_sound(uint32_t index,const float position[3])
{
    if(index>=sizeof(campaign_audio_evictable))return;
    if(!rf_audio_bank_sample(&campaign_audio_bank,index)){if(campaign_ambient_reload(index))return;campaign_audio_evictable[index]=1;}
    (void)campaign_sound_start((int32_t)index,position,1,0,1,0);
}
static float scene_mp_item_angle[SCENE_MP_ITEMS];
static uint8_t scene_mp_item_was_taken[SCENE_MP_ITEMS];
static const rf_item_definition *scene_mp_item_definition(const scene_stream *stream,uint32_t i)
{
    int kind=pickup_class(stream->pickups.items[i].class_name);
    if(kind<0)return NULL;
    return kind?(stream->pickup_resources?&stream->pickup_resources[kind-1].definition:NULL):&stream->handgun_pickup;
}
/* 16c5e0: drawing basis of a spinning multiplayer item; 0 = use authored. */
static uint32_t scene_mp_item_basis(const scene_stream *stream,uint32_t i,float basis[9])
{
    const rf_item_definition *definition;float c,s;
    if(!rf_scene_mp[0] || i>=SCENE_MP_ITEMS)return 0;
    definition=scene_mp_item_definition(stream,i);if(!definition || !(definition->flags&2u))return 0;
    c=cosf(scene_mp_item_angle[i]);s=sinf(scene_mp_item_angle[i]);
    basis[0]=c;basis[1]=0;basis[2]=-s;basis[3]=0;basis[4]=1;basis[5]=0;basis[6]=s;basis[7]=0;basis[8]=c;
    return 1;
}
/* 12a700 forced: switch to the first weapon of the bot's row it can fire. */
static int scene_mp_bot_choose_weapon(uint32_t slot)
{
    campaign_npc_body *npc=campaign_npc_bodies+slot;uint32_t i;int32_t id,type;
    for(i=0;i<8;i++){
        id=scene_mp_weapon_ids[scene_mp_preference_rows[slot][i]];
        if(id<0 || id>=64 || (uint32_t)id>=campaign_weapon_supply.names.count || !npc->inventory.owned[id])continue;
        type=campaign_weapon_supply.definitions[id].ammo_type;
        if(type<0 || type>=32)continue;
        if(npc->inventory.loaded[id]<=0 && npc->inventory.reserve[type]<=0)continue;
        if(id==npc->view.weapons[0])return RF_OK;
        /* A weapon the bot's class has no motions for is passed over. */
        if(campaign_enemy_weapon_switch(slot,id))continue;
        ++rf_scene_mp_pickups[3];return RF_OK;
    }
    return RF_OK;
}
/* Respawn timers and spin for every item, whoever took it (16ce10, 16c5e0). */
static void scene_mp_items_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,count=stream->pickups.count<SCENE_MP_ITEMS?stream->pickups.count:SCENE_MP_ITEMS,check;
    if(!frame){memset(scene_mp_item_due,0,sizeof(scene_mp_item_due));memset(scene_mp_item_angle,0,sizeof(scene_mp_item_angle));
        scene_mp_item_clock=scene_mp_item_checked=0;
        for(i=0;i<count;i++)scene_mp_item_was_taken[i]=stream->pickup_taken[i];}
    else scene_mp_item_clock+=(uint32_t)(scene_step_seconds*1000.0f+.5f);
    /* 16ce10: timers are looked at when more than 99 ms have passed. */
    check=scene_mp_item_clock-scene_mp_item_checked>99;if(check)scene_mp_item_checked=scene_mp_item_clock;
    rf_scene_mp_pickups[7]=0;
    for(i=0;i<count;i++){
        const rf_level_item *item=stream->pickups.items+i;const rf_item_definition *definition;
        if(stream->pickup_taken[i] && !scene_mp_item_was_taken[i] && item->respawn_seconds>0)
            scene_mp_item_due[i]=scene_mp_item_clock+(uint32_t)item->respawn_seconds*1000u+1u;
        if(check && stream->pickup_taken[i] && scene_mp_item_due[i] && scene_mp_item_clock>=scene_mp_item_due[i]-1u){
            scene_mp_item_due[i]=0;stream->pickup_taken[i]=0;
            rf_scene_campaign_pickups.items[stream->pickup_slots[i]].retired=0;++rf_scene_mp_pickups[6];
            scene_mp_item_sound(1,item->position); /* 16cda0 */
        }
        scene_mp_item_was_taken[i]=stream->pickup_taken[i];
        definition=scene_mp_item_definition(stream,i);
        if(!stream->pickup_taken[i] && definition && (definition->flags&2u)){
            scene_mp_item_angle[i]+=scene_step_seconds*.75f*3.1415927f;
            if(scene_mp_item_angle[i]>6.2831855f)scene_mp_item_angle[i]-=6.2831855f;
            ++rf_scene_mp_pickups[7];
        }
    }
}
/* 16c2f0 touch: the item's 0.25 sphere against the character's collision
 * spheres (local centres turned by the body orientation). */
static uint32_t scene_mp_item_touch(const campaign_npc_body *npc,const float item[3])
{
    uint32_t n,j,k;
    for(n=0;n<npc->body.spheres.count;n++){
        const rf_physics_sphere *sp=npc->body.spheres.items+n;float c[3],d=0,r=sp->radius+.25f;
        for(k=0;k<3;k++){c[k]=npc->body.state.position[k];for(j=0;j<3;j++)c[k]+=sp->center[j]*npc->body.state.orientation[j*3+k];}
        for(k=0;k<3;k++)d+=(item[k]-c[k])*(item[k]-c[k]);
        if(d<r*r)return 1;
    }
    return 0;
}

/* PS2 129410 target choice for multiplayer bots, run by 12b580 on the skill
 * timer (scene_mp_scan_due). Candidates: every other live, shown character
 * not catatonic and not in a vehicle that the bot is hostile to (1291f0:
 * table 28e750[self * 4 + other] on affiliation, or one that has hit it,
 * list +0x3d4), inside its field of view (128eb0: dot of the unit direction
 * with the eye facing at least +0x764 = cos(instance FOV / 2), from 177010)
 * and seen from its body origin or eye, alternately (194fd0 to the
 * candidate's eye). Distance to a human is weighted by 1 - skill/5; the
 * nearest under 25 wins. With no human chosen, a previous target is kept
 * when rand % 3 != 0. A found target replaces the current one (134570); none
 * found leaves it. The stealth/hidden-player and mode 0xb clauses do not
 * apply to the port's multiplayer and are left out. */
static const uint8_t scene_mp_hostile[16]={0,0,1,1, 0,0,0,0, 1,0,0,1, 1,1,1,1};
static uint32_t scene_mp_attackers[SCENE_MP_BOTS][8],scene_mp_attacker_count[SCENE_MP_BOTS];
static float scene_mp_fov_cos[SCENE_MP_BOTS];static uint8_t scene_mp_eye_origin[SCENE_MP_BOTS];
uint32_t rf_scene_mp_targets[4]; /* scans, targets set, previous kept, last status */
static void scene_mp_attacked(uint32_t slot,uint32_t source)
{
    uint32_t i;
    if(slot>=SCENE_MP_BOTS)return;
    for(i=0;i<scene_mp_attacker_count[slot];i++)if(scene_mp_attackers[slot][i]==source)return;
    if(scene_mp_attacker_count[slot]<8)scene_mp_attackers[slot][scene_mp_attacker_count[slot]++]=source;
}
/* PS2 132b10 -> 12a520: a bot's attack range is its authored combat range
 * override (AI +0x47c/+0x480) when enabled, else its weapon's AI attack
 * range (weapon record +0x4f4; 0.5 for no weapon). The secondary weapon
 * (0.5 for none) does not raise it. */
static float scene_mp_attack_range(uint32_t slot)
{
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,campaign_shotgun_id,
        campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    campaign_npc_body *npc=campaign_npc_bodies+slot;campaign_enemy_weapon_selection weapon={0};
    rf_level_entity_combat_range authored={0};int status;float range=.5f;
    if(campaign_seeds.records.items && slot<campaign_seeds.records.count &&
       !rf_level_entity_combat_range_read(campaign_seeds.records.items+slot,&authored) && authored.enabled)return authored.range;
    if(npc->view.weapons[0]<=0)return .5f;
    status=campaign_enemy_weapon_select(npc->view.weapons[0],ids,campaign_primary,
        campaign_weapon_supply.definitions,campaign_weapon_supply.names.count,&weapon);
    if(status==RF_NOT_FOUND)status=campaign_enemy_extra_weapon_select(npc->view.weapons[0],&campaign_weapon_supply,&weapon);
    if(!status && weapon.primary)range=weapon.primary->ai_attack_range;
    return range>.5f?range:.5f;
}
/* A new body starts with no attackers. */
static void scene_mp_respawned_slot(uint32_t slot){if(slot<SCENE_MP_BOTS)scene_mp_attacker_count[slot]=0;}
static int scene_mp_choose_target(scene_stream *stream,uint32_t slot,uint32_t frame,const float player_eye[3])
{
    static const float scale[4]={1.0f,.8f,.6f,.4f};
    campaign_npc_body *self=campaign_npc_bodies+slot;uint32_t i,k,best_handle=0,human_chosen=0,seen=0;float best=25.0f;int status;
    uint32_t self_aff=self->damage.effects.affiliation&3;
    ++rf_scene_mp_targets[0];
    /* Two-player: both humans are candidates (the second after the first). */
    for(i=0;i<=campaign_npc_body_count+1;i++){
        const float *pos,*eye;uint32_t handle,aff,human=i>=campaign_npc_body_count,hostile,blocked=0,a;float weight=1,d=0,dir[3],dot=0,len=0,from[3],delta[3];
        if(i==campaign_npc_body_count+1){
            scene_human_ref other;
            if(!scene_player2_live || !scene_human_get(rf_scene_player_context[0]&1u?0:1,&other) || other.health<=0)continue;
            pos=other.position;eye=other.eye;handle=other.handle;
            aff=other.affiliation;weight=scale[rf_scene_session_get_difficulty()&3];
        }else if(human){
            if(!campaign_player_object.view || campaign_player_damage.state.effects.health<=0)continue;
            pos=scene_actor_body.state.position;eye=player_eye;handle=campaign_player_object.handle;
            aff=campaign_player_damage.state.effects.affiliation&3;weight=scale[rf_scene_session_get_difficulty()&3];
        }else{
            const campaign_npc_body *o=campaign_npc_bodies+i;
            if(i==slot || !o->registration.view || o->damage.effects.health<=0 || (o->object_flags&2) || o->ai_mode.action_280==1)continue;
            pos=o->body.state.position;eye=o->eye_position;handle=o->registration.handle;aff=o->damage.effects.affiliation&3;
        }
        hostile=scene_mp_hostile[self_aff*4+aff];
        for(a=0;!hostile && a<scene_mp_attacker_count[slot];a++)hostile=scene_mp_attackers[slot][a]==handle;
        if(!hostile)continue;
        for(k=0;k<3;k++){dir[k]=pos[k]-self->body.state.position[k];d+=dir[k]*dir[k];}
        d=sqrtf(d)*weight;if(d>=best)continue;
        for(k=0;k<3;k++){dot+=dir[k]*self->look.orientation[6+k];len+=dir[k]*dir[k];}
        if(len<=0 || dot<scene_mp_fov_cos[slot]*sqrtf(len))continue;
        scene_mp_eye_origin[slot]^=1;
        for(k=0;k<3;k++){from[k]=scene_mp_eye_origin[slot]?self->body.state.position[k]:self->eye_position[k];delta[k]=eye[k]-from[k];}
        status=combat_obstructed(stream,from,delta,&blocked);if(status)return status;
        if(blocked)continue;
        if(self->combat_alert && handle==self->combat_target)seen=1;
        best=d;best_handle=handle;human_chosen=human;
    }
    /* 12b580: a scan that sees the target (+0x1ec) sets the facing bit 8. */
    if(seen || best_handle)scene_mp_target_seen(self);
    if(!human_chosen && self->combat_alert && self->combat_target && scene_mp_next()%3){++rf_scene_mp_targets[2];return RF_OK;}
    if(!best_handle || (self->combat_alert && self->combat_target==best_handle))return RF_OK;
    scene_pursuit_token_invalidate_owner(self);
    if(!campaign_script_movement_owns(self)){scene_patrol_combat_begin(self,0);campaign_pursuit_stop(self);}
    self->combat_scripted=human_chosen?0:2;self->combat_target=best_handle;self->combat_alert=1;
    self->combat_navigation_due=self->combat_burst_remaining=0;self->combat_due=scene_mp_first_shot(frame);scene_blind_sync(self,frame);
    ++rf_scene_mp_targets[1];
    return RF_OK;
}
static int scene_mp_targets_tick(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    uint32_t slot,count=campaign_npc_body_count<SCENE_MP_BOTS?campaign_npc_body_count:SCENE_MP_BOTS;int status;
    if(!frame){
        memset(scene_mp_attacker_count,0,sizeof(scene_mp_attacker_count));memset(rf_scene_mp_targets,0,sizeof(rf_scene_mp_targets));
        for(slot=0;slot<count;slot++){
            rf_level_entity_ai ai={0};
            scene_mp_fov_cos[slot]=campaign_seeds.records.items && slot<campaign_seeds.records.count &&
                !rf_level_entity_ai_read(campaign_seeds.records.items+slot,&ai)?cosf((float)ai.fov_degrees*3.1415927f/180.0f/2.0f):-1.0f;
        }
        return RF_OK;
    }
    for(slot=0;slot<count;slot++){
        campaign_npc_body *npc=campaign_npc_bodies+slot;
        if(!rf_scene_mp_roster[slot].used || rf_scene_mp_roster[slot].dead || !npc->registration.view || npc->damage.effects.health<=0)continue;
        if(!scene_mp_scan_due(frame,slot,0))continue;
        status=scene_mp_choose_target(stream,slot,frame,player_eye);rf_scene_mp_targets[3]=(uint32_t)status;if(status)return status;
    }
    return RF_OK;
}

/* PS2 16d000/16d080 dropped-item cap: every 1.5 s, when more than 2 dropped
 * items exist in multiplayer (6 in single player), the oldest starts a 1 s
 * fade (+0x28c bit 4, alpha +0x284 from 1) and is then removed. Port: the
 * drops are the bots' death drops (one record per bot); the fade is drawn
 * through the faded vertex-lit path (scene_mp_drop_alpha), alpha 1 -> 0
 * over the second, and the drop goes when it ends. */
static uint32_t scene_mp_drop_birth[SCENE_MP_BOTS],scene_mp_drop_fade[SCENE_MP_BOTS],scene_mp_drop_was[SCENE_MP_BOTS],scene_mp_drop_check;
uint32_t rf_scene_mp_drops[3]; /* live drops, fades started, removed */
/* Harness: frame (rf_scene_profile_stage[0]) and alpha of the last drawn
 * fading drop. Observation only. */
uint32_t rf_scene_mp_fade_draw[2];
/* 0 when not fading, else 1-255. */
static uint32_t scene_mp_drop_alpha(uint32_t slot)
{
    uint32_t age;float a;
    if(!rf_scene_mp[0] || slot>=SCENE_MP_BOTS || !scene_mp_drop_fade[slot])return 0;
    age=scene_mp_item_clock-scene_mp_drop_fade[slot];if(age>=1000u)return 1;
    a=255.0f*(1.0f-(float)age/1000.0f);return a<1?1:a>254?254:(uint32_t)a;
}
static void scene_mp_drops_tick(uint32_t frame)
{
    uint32_t slot,count=campaign_npc_body_count<SCENE_MP_BOTS?campaign_npc_body_count:SCENE_MP_BOTS,live=0,oldest=UINT32_MAX;
    if(!frame){memset(scene_mp_drop_birth,0,sizeof(scene_mp_drop_birth));memset(scene_mp_drop_fade,0,sizeof(scene_mp_drop_fade));
        memset(scene_mp_drop_was,0,sizeof(scene_mp_drop_was));memset(rf_scene_mp_drops,0,sizeof(rf_scene_mp_drops));scene_mp_drop_check=0;return;}
    for(slot=0;slot<count;slot++){
        campaign_npc_body *npc=campaign_npc_bodies+slot;rf_campaign_weapon_drop *drop;uint32_t on;
        if(!npc->persistence_registered || npc->persistence_slot>=rf_scene_defeated_actors.count)continue;
        drop=rf_scene_defeated_actors.drops+npc->persistence_slot;on=drop->state==1;
        if(on && !scene_mp_drop_was[slot]){scene_mp_drop_birth[slot]=scene_mp_item_clock;scene_mp_drop_fade[slot]=0;}
        scene_mp_drop_was[slot]=on;
        if(!on)continue;
        if(scene_mp_drop_fade[slot] && scene_mp_item_clock>=scene_mp_drop_fade[slot]+1000u){
            drop->state=0;scene_mp_drop_was[slot]=0;scene_mp_drop_fade[slot]=0;++rf_scene_mp_drops[2];continue;
        }
        ++live;
        if(!scene_mp_drop_fade[slot] && (oldest==UINT32_MAX || scene_mp_drop_birth[slot]<scene_mp_drop_birth[oldest]))oldest=slot;
    }
    rf_scene_mp_drops[0]=live;
    if(scene_mp_item_clock-scene_mp_drop_check<1500u)return;
    scene_mp_drop_check=scene_mp_item_clock;
    if(live>2 && oldest!=UINT32_MAX){scene_mp_drop_fade[oldest]=scene_mp_item_clock?scene_mp_item_clock:1u;++rf_scene_mp_drops[1];}
}
static int scene_mp_bot_pickups(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    uint32_t slot,i,k,count;int status;
    if(!rf_scene_mp[0])return RF_OK;
    scene_mp_items_tick(stream,frame);
    scene_mp_drops_tick(frame);
    if(rf_scene_mp[6])return RF_OK;
    status=scene_mp_targets_tick(stream,frame,player_eye);if(status)return status;
    count=campaign_npc_body_count<SCENE_MP_BOTS?campaign_npc_body_count:SCENE_MP_BOTS;
    for(slot=0;slot<count;slot++){
        campaign_npc_body *npc=campaign_npc_bodies+slot;rf_damage_effect_state *v=&npc->damage.effects;
        uint32_t switched=0;
        if(!rf_scene_mp_roster[slot].used || !npc->registration.view || v->health<=0)continue;
        for(i=0;i<stream->pickups.count;i++){
            const rf_level_item *item=stream->pickups.items+i;const rf_item_definition *definition;
            int kind=pickup_class(item->class_name);float delta[3];uint32_t blocked=0;
            if(stream->pickup_taken[i] || kind<0 || item->quantity<0 || kind==SCENE_PICKUP_DOCTOR_UNIFORM)continue;
            if(!scene_mp_item_touch(npc,item->position))continue;
            definition=kind?&stream->pickup_resources[kind-1].definition:&stream->handgun_pickup;
            if(definition->flags&1u)continue; /* 16cba0: +0x28c bit 0 (no_pickup) */
            /* 16cba0 touch: the item must see the picker's eye (194fd0). */
            for(k=0;k<3;k++)delta[k]=item->position[k]-npc->eye_position[k];
            status=combat_shot_obstructed(stream,npc->eye_position,delta,1,&blocked);if(status)return status;
            if(blocked)continue;
            if(kind==SCENE_PICKUP_MINER_SUIT){v->armor=v->class_armor;++rf_scene_mp_pickups[1];} /* 16dde0 */
            else if(kind==1 || kind==2 || kind==SCENE_PICKUP_FIRST_AID){
                float *value=kind==2?&v->armor:&v->health,limit=kind==2?v->class_armor:v->class_health;
                if(*value>=limit){++rf_scene_mp_pickups[4];continue;}
                *value+=(float)item->quantity;if(*value>limit)*value=limit;
                ++rf_scene_mp_pickups[kind==2?1:0];
            }else{
                rf_weapon_pickup_grant grant={0};int32_t id=-1,weapon_slot=scene_pickup_weapon_slot(kind);
                if(weapon_slot>=0)id=campaign_slot_weapon((uint32_t)weapon_slot);
                else if(definition->weapon[0])id=rf_weapon_name_find(&campaign_weapon_supply.names,definition->weapon);
                if(id<0 || id>=64 || (uint32_t)id>=campaign_weapon_supply.names.count)continue;
                /* A refused grant (full, or a reserve the shared rule rejects) leaves the item. */
                if(scene_pickup_shared_ammo_grant(&npc->inventory,&campaign_weapon_supply,id,
                    campaign_item_charge(id,item->quantity,definition->gives_weapon),definition->gives_weapon,&grant) ||
                   (!grant.rounds && !grant.acquired)){++rf_scene_mp_pickups[4];continue;}
                ++rf_scene_mp_pickups[2];switched=1;
            }
            stream->pickup_taken[i]=1;rf_scene_campaign_pickups.items[stream->pickup_slots[i]].retired=1;
            campaign_pickup_sound(definition,item->position,item->uid); /* 16cba0, any picker */
            /* 16cb00: a bot running a route with life and armour above 95% fights again. */
            if(scene_mp_routing[slot]==1 && v->health>v->class_health*.95f && v->armor>v->class_armor*.95f){
                npc->script_move.active=0;npc->navigation.retained.count=0;scene_mp_routing[slot]=0;++rf_scene_mp_pickups[5];
            }
        }
        if(switched){status=scene_mp_bot_choose_weapon(slot);if(status)return status;}
    }
    return RF_OK;
}
#endif
