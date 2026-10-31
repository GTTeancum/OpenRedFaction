#ifndef RF_SCENE_BULLET_TRACERS_INC
#define RF_SCENE_BULLET_TRACERS_INC
/* NPC firearm rounds and tracers. Reconstructed from RF.exe:
 *   4c77a0 every firearm round is a weapon object that flies at weapons.tbl
 *          $Velocity for $Lifetime and applies its shot on contact.
 *   4c456c..4c45da "$Tracers:" sets info+264 bit 0x40000000,
 *          "+Tracer Frequency:" info+274, "+Tracer Effect:" info+540/548.
 *   4c7871 with the flag set, a round is a tracer when the owner's counter
 *          (entity+1424, 429f40) is <= 0; a tracer flies at 0.275 x $Velocity
 *          (5a2564) and so covers 0.275 x the range in the same $Lifetime.
 *          from_eye weapons owned by a local player never make tracers
 *          (4c785f..4c787b, 48aa30); player rounds are scene_player_rounds.inc.
 *   4c7c46..4c7c5e every created round then runs 429f60 with the weapon's
 *          frequency: decrement the counter, reload it below zero (frequency
 *          4: one tracer in five rounds; 0: every round).
 *   4c754e..4c75ee a tracer round with a tracer effect advances that VFX by the
 *          frame time, looping (503360), and draws it at the round's position
 *          and orientation (503100); orientation from 4fcea0.
 *   41b5a0 NPC rounds leave the held weapon's muzzle (rf_scene_npc_muzzle).
 * Tracer01.VFX (meshes.vpp, version 40006): two static planes, a Tracer01.tga
 * streak trailing about 5 units behind the round and a LightCorona01.tga glow,
 * additive, visible for effect frames 0..10 (looped); drawn by the shared
 * projectile emitter scene_vfx_emit.
 * Port policy: each update re-runs the established enemy ray resolution over
 * the round's remaining path from its current position (scene_npc_round_step)
 * and commits only a contact that update reaches. Vauss/Tankbot penetration,
 * NPC shotgun and melee stay instant; a round whose shooter no longer exists
 * ends (the selection needs its registration); deferred vehicle hits do not
 * raise the vehicle hit event. */
#define SCENE_NPC_ROUND_CAPACITY 64u
#define SCENE_TRACER_NPC_COUNTERS 512u
/* shotgun: one pellet of an NPC shotgun shell (player damage at difficulty 1,
 * as campaign_enemy_shotgun_fire applied); piercing: Vauss/Tankbot round with
 * its accepted shot and 4c9b20 power state (scene_ai_actor_penetration_fire). */
typedef struct scene_npc_round {
    scene_ai_pistol_shot pistol;const rf_weapon_primary_definition *definition;
    scene_ai_actor_penetration_shot actor;scene_sniper_penetration penetration;
    float origin[3],path[3],basis[9],speed,flown_fraction,damage,age;
    uint32_t slot,source,intended,penetrates_world,tracer,shotgun,piercing,active;int32_t weapon;
} scene_npc_round;
typedef struct scene_npc_round_result {float amount,player_amount;uint32_t vehicle_applied,victim_slot,victim_handle,vehicle_handle;float vehicle_amount;} scene_npc_round_result;
static int scene_vfx_emit(scene_stream *s,scene_rocket_visual *v,const float position[3],const float *basis,float time);
static int scene_npc_round_step(scene_stream *stream,uint32_t frame,scene_npc_round *r,const float start[3],
    const float ray[3],float limit,const float player_eye[3],scene_npc_round_result *out,uint32_t *done);
static scene_npc_round scene_npc_rounds[SCENE_NPC_ROUND_CAPACITY];
static float scene_npc_round_player_eye[3];static uint32_t scene_npc_round_player_eye_valid;
/* Tracer01.VFX with its textures merged into the stream materials. */
static scene_rocket_visual *scene_tracer_visual;static float scene_tracer_loop_frames;
/* speed: tracer flight (0.275 x $Velocity); velocity/lifetime: the round itself. */
static struct {uint8_t enabled;int32_t frequency;float speed,velocity,lifetime;} scene_tracer_weapons[64];
static int32_t scene_tracer_npc_counter[SCENE_TRACER_NPC_COUNTERS];
/* NPC rounds launched, tracer rounds, VFX emissions drawn (total), weapons with tracers (+0x10000 = Tracer01.VFX loaded),
 * rounds completed, instant fallbacks (no table entry or pool full), active rounds, rounds that hit */
uint32_t rf_scene_tracers[8];
uint32_t rf_scene_npc_round_kinds[4]; /* launched NPC rounds: pistol, shotgun pellet, piercing, other */
/* Plain decimal ("275.0"); NXDK's strtof/atof are unimplemented asserts. */
static float scene_tracer_number(const char *s)
{
    float value=0,scale=1;int negative=*s=='-';
    if(*s=='-' || *s=='+')++s;
    for(;*s>='0' && *s<='9';++s)value=value*10+(float)(*s-'0');
    if(*s=='.')for(++s;*s>='0' && *s<='9';++s){scale*=.1f;value+=(float)(*s-'0')*scale;}
    return negative?-value:value;
}
/* Level load: weapons.tbl round fields for every supplied weapon name. */
static int scene_tracers_load(rf_vpp *tables)
{
    rf_vpp_entry entry;char *text,*line,*next;int32_t weapon=-1;int status;
    memset(scene_tracer_weapons,0,sizeof(scene_tracer_weapons));memset(scene_npc_rounds,0,sizeof(scene_npc_rounds));
    memset(scene_tracer_npc_counter,0,sizeof(scene_tracer_npc_counter));rf_scene_tracers[3]=0;memset(rf_scene_npc_round_kinds,0,sizeof(rf_scene_npc_round_kinds));scene_npc_round_player_eye_valid=0;
    if(!tables || !campaign_weapon_supply.names.count)return RF_OK;
    status=rf_vpp_find(tables,"weapons.tbl",&entry);if(status)return status==RF_NOT_FOUND?RF_OK:status;
    if(!entry.size || entry.size>256u*1024u)return RF_RANGE;
    text=malloc(entry.size+1u);if(!text)return RF_IO;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);if(status){free(text);return status;}
    text[entry.size]=0;
    for(line=text;line && *line;line=next) {
        char *p=line,*value;
        next=strpbrk(line,"\r\n");if(next){*next++=0;while(*next=='\r' || *next=='\n')++next;}
        while(*p==' ' || *p=='\t')++p;
        if(p[0]=='/' && p[1]=='/')continue;
        value=strchr(p,':');if(!value)continue;++value;while(*value==' ' || *value=='\t')++value;
        if(!strncmp(p,"$Name:",6)) {
            char name[64];size_t n=0;
            if(*value=='"')++value;
            while(value[n] && value[n]!='"' && n+1<sizeof(name)){name[n]=value[n];++n;}
            name[n]=0;weapon=rf_weapon_name_find(&campaign_weapon_supply.names,name);
            if(weapon>=64)weapon=-1;continue;
        }
        if(weapon<0)continue;
        if(!strncmp(p,"$Tracers:",9)){if(!strncmp(value,"true",4)){scene_tracer_weapons[weapon].enabled=1;++rf_scene_tracers[3];}}
        else if(!strncmp(p,"+Tracer Frequency:",18))scene_tracer_weapons[weapon].frequency=(int32_t)scene_tracer_number(value);
        else if(!strncmp(p,"$Velocity:",10)){scene_tracer_weapons[weapon].velocity=scene_tracer_number(value);scene_tracer_weapons[weapon].speed=scene_tracer_weapons[weapon].velocity*.275f;}
        else if(!strncmp(p,"$Lifetime:",10))scene_tracer_weapons[weapon].lifetime=scene_tracer_number(value);
    }
    free(text);return RF_OK;
}
static void scene_tracer_visual_close(void)
{
    if(!scene_tracer_visual)return;
    rf_vfx_asset_materials_close(&scene_tracer_visual->materials);rf_vfx_geometry_asset_close(&scene_tracer_visual->geometry);
    free(scene_tracer_visual);scene_tracer_visual=NULL;
}
/* Level load, after the weapon table: Tracer01.VFX and its textures, merged
 * into the stream materials as the projectile visuals are. Missing or
 * unsupported data draws no tracer (never fatal). */
static int scene_tracer_visual_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,rf_materials *materials)
{
    scene_rocket_visual *v;uint32_t i,n;rf_material *combined;
    scene_tracer_visual_close();rf_scene_tracers[3]&=0xffffu;scene_tracer_loop_frames=0;
    {extern uint32_t rf_scene_player_tracers;if(!rf_scene_tracers[3] && !rf_scene_player_tracers)return RF_OK;} /* player option traces every bullet weapon */
    v=calloc(1,sizeof(*v));if(!v)return RF_IO;scene_tracer_visual=v;
    if(rf_vfx_geometry_asset_open(meshes,"Tracer01.VFX",64*1024,&v->geometry) ||
       rf_vfx_asset_materials_open(v->geometry,maps,map_count,64*1024,&v->materials)){scene_tracer_visual_close();return RF_OK;}
    n=v->materials->textures.texture_count;
    for(i=0;i<v->geometry->count;i++){
        const rf_vfx_mesh_timing *timing=&v->geometry->meshes[i]->prefix.timing;
        if(v->geometry->meshes[i]->prefix.faces>128){scene_tracer_visual_close();return RF_OK;}
        if((timing->end-timing->start)*15>scene_tracer_loop_frames)scene_tracer_loop_frames=(timing->end-timing->start)*15;
    }
    for(i=0;i<n;i++)if(v->materials->textures.textures[i].animation.count!=1){scene_tracer_visual_close();return RF_OK;}
    if(!(scene_tracer_loop_frames>0) || materials->count+n>RF_CAMPAIGN_TEXTURE_SLOTS){scene_tracer_visual_close();return RF_OK;}
    combined=realloc(materials->items,(materials->count+n)*sizeof(*combined));if(!combined)return RF_IO;
    materials->items=combined;v->base=materials->count;
    for(i=0;i<n;i++){
        rf_image *image=v->materials->textures.textures[i].animation.images;
        combined[materials->count]=(rf_material){*image,RF_OK,v->materials->textures.textures[i].animation.archive_index};
        materials->allocated_bytes+=sizeof(*combined)+image->bytes;memset(image,0,sizeof(*image));++materials->count;++materials->loaded;
    }
    rf_scene_tracers[3]|=0x10000u;return RF_OK;
}
/* Fire: run the owner's tracer counter (4c7871 + 4c7c5e) and queue the round.
 * RF_NOT_FOUND (no $Velocity/$Lifetime) or RF_RANGE (pool full): the caller
 * resolves the round at once. */
static int scene_npc_round_launch(const scene_npc_round *source,const float direction[3])
{
    scene_npc_round *r=NULL;float length=0,dir[3],range,matrix[3][3];uint32_t i,k;int32_t *counter;int tracer;
    if(source->weapon<0 || source->weapon>=64){++rf_scene_tracers[5];return RF_NOT_FOUND;}
    counter=scene_tracer_npc_counter+source->slot%SCENE_TRACER_NPC_COUNTERS;
    tracer=scene_tracer_weapons[source->weapon].enabled && *counter<=0;
    if(--*counter<0)*counter=scene_tracer_weapons[source->weapon].frequency;
    if(!(scene_tracer_weapons[source->weapon].velocity>0) || !(scene_tracer_weapons[source->weapon].lifetime>0)){++rf_scene_tracers[5];return RF_NOT_FOUND;}
    for(k=0;k<3;k++)length+=direction[k]*direction[k];
    length=sqrtf(length);if(!(length>0) || !isfinite(length))return RF_RANGE;
    for(k=0;k<3;k++)dir[k]=direction[k]/length;
    if(rf_entity_navigation_basis(dir,matrix))return RF_RANGE;
    for(i=0;i<SCENE_NPC_ROUND_CAPACITY;i++)if(!scene_npc_rounds[i].active){r=scene_npc_rounds+i;break;}
    if(!r){++rf_scene_tracers[5];return RF_RANGE;}
    *r=*source;r->tracer=(uint32_t)tracer;
    r->speed=tracer?scene_tracer_weapons[source->weapon].speed:scene_tracer_weapons[source->weapon].velocity;
    range=r->speed*scene_tracer_weapons[source->weapon].lifetime;
    for(k=0;k<3;k++)r->path[k]=dir[k]*range;
    if(r->piercing && scene_sniper_penetration_begin(r->actor.enabled,r->actor.power,r->origin,r->path,&r->penetration)){r->active=0;return RF_RANGE;}
    memcpy(r->basis,matrix,36);r->flown_fraction=0;r->age=0;r->active=1;
    ++rf_scene_tracers[0];if(tracer)++rf_scene_tracers[1];
    ++rf_scene_npc_round_kinds[r->pistol.active?0:r->shotgun?1:r->piercing?2:3];return RF_OK;
}
/* The enemy tick publishes the player eye it aimed with; rounds use the latest. */
static void scene_npc_rounds_player_eye(const float eye[3]){memcpy(scene_npc_round_player_eye,eye,12);scene_npc_round_player_eye_valid=1;}
/* One game update for every flying NPC round, with the instant shot's hit
 * bookkeeping when a round lands. */
static int scene_npc_rounds_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,k,active=0,done;int status;
    if(!scene_npc_round_player_eye_valid)return RF_OK;
    for(i=0;i<SCENE_NPC_ROUND_CAPACITY;i++) {
        scene_npc_round *r=scene_npc_rounds+i;scene_npc_round_result result;float position[3],delta[3],remaining,step,limit,left=0;
        if(!r->active)continue;
        remaining=1-r->flown_fraction;
        for(k=0;k<3;k++){position[k]=r->origin[k]+r->path[k]*r->flown_fraction;delta[k]=r->path[k]*remaining;left+=delta[k]*delta[k];}
        step=r->speed*scene_step_seconds;left=sqrtf(left);limit=left>step?step/left:1;
        status=scene_npc_round_step(stream,frame,r,position,delta,limit,scene_npc_round_player_eye,&result,&done);if(status)return status;
        if(done || limit>=1) {
            r->active=0;++rf_scene_tracers[4];
            if(result.amount>0) {
                float player_amount=result.player_amount;
                ++rf_scene_enemy_combat[3];++rf_scene_tracers[7];
                if(!r->pistol.active && !r->piercing && !r->shotgun && result.victim_slot==UINT32_MAX && !result.vehicle_applied)player_amount=result.amount;
                if(player_amount>0)campaign_combat_event(frame,1,r->source,player_amount,campaign_player_damage.state.effects.health);
                if(result.vehicle_applied) {
                    /* The instant shot's vehicle hit event, raised when the round lands. */
                    float hit_eye[3],hit_health=0;
                    (void)scene_campaign_vehicle_target(stream,result.vehicle_handle,hit_eye,&hit_health);
                    campaign_combat_event(frame,2,r->source,result.vehicle_amount,hit_health);
                }
            }
            continue;
        }
        r->flown_fraction+=remaining*limit;r->age+=scene_step_seconds;++active;
    }
    rf_scene_tracers[6]=active;return RF_OK;
}
/* Draw: Tracer01.VFX at each flying tracer round, effect time looping from launch. */
static int scene_tracers_draw(scene_stream *stream)
{
    uint32_t i,k;int status;
    if(!scene_tracer_visual)return RF_OK;
    for(i=0;i<SCENE_NPC_ROUND_CAPACITY;i++){
        const scene_npc_round *r=scene_npc_rounds+i;float position[3];
        if(!r->active || !r->tracer)continue;
        for(k=0;k<3;k++)position[k]=r->origin[k]+r->path[k]*r->flown_fraction;
        status=scene_vfx_emit(stream,scene_tracer_visual,position,r->basis,fmodf(r->age*15,scene_tracer_loop_frames));
        if(status==RF_RANGE)return RF_OK; /* stream full this frame: skip, never fail */
        if(status)return status;
        ++rf_scene_tracers[2];
    }
    return RF_OK;
}
#endif
