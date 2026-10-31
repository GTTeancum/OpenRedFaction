#ifndef RF_SCENE_PLAYER_ROUNDS_INC
#define RF_SCENE_PLAYER_ROUNDS_INC
/* Flying player bullets. In RF.exe every firearm round is a weapon object
 * (4c77a0) that moves at weapons.tbl $Velocity and lives $Lifetime seconds,
 * sweeping its $Collision Radius each frame; a contact applies the shot then.
 * Tracer rounds fly at 0.275 x $Velocity, but the player's own from_eye
 * weapons never make tracers (4c785f..4c787b, 48aa30), so player rounds here
 * always fly at full speed and draw nothing. Penetration power and visited
 * handles live on the round (projectile+2f0, 4c7c2e..4c7c40).
 * Port policy: each step re-runs the existing instant-ray selection over the
 * round's remaining path from its current position and commits only a contact
 * this step reaches (scene_player_round_step), so damage, shields, props,
 * vehicles, fragments, world impacts and penetration keep their established
 * behaviour, now at the moment of arrival. Riot reach and the precision rifles
 * (slots 6/7) stay instant.
 * Port option (openrf.ini tracers, default on): weapons with $Tracers also
 * show Tracer01.VFX on the player's rounds, one per +Tracer Frequency like
 * NPC rounds. It is cosmetic only: the round keeps full speed and its own
 * contact; the streak leaves the flame muzzle point at the tracer speed
 * (0.275 x $Velocity) and stops at the distance where the round ended. */
#define SCENE_PLAYER_ROUND_CAPACITY 48u
typedef struct scene_player_round {
    scene_player_body_shot body_shot;scene_handheld_shot handheld;scene_sniper_penetration penetration;
    float origin[3],path[3],speed,flown_fraction;uint32_t fire,pellet,active,tracer;
} scene_player_round;
#define SCENE_PLAYER_TRACER_CAPACITY 32u
typedef struct scene_player_tracer {float origin[3],basis[9],speed,travelled,limit,age,lifetime;uint32_t active;} scene_player_tracer;
static scene_player_tracer scene_player_tracers[SCENE_PLAYER_TRACER_CAPACITY];
static int32_t scene_player_tracer_counter;
uint32_t rf_scene_player_tracers=1; /* openrf.ini tracers (main.c sets it) */
/* tracers started, active, drawn emissions, pool full */
uint32_t rf_scene_player_tracer_stats[4];
static scene_player_round scene_player_rounds[SCENE_PLAYER_ROUND_CAPACITY];
/* launched, active, completed by contact or range, instant fallbacks (no table entry or pool full) */
uint32_t rf_scene_player_rounds[4];
static int scene_player_round_step(scene_stream *stream,uint32_t frame,scene_player_round *r,
    const float position[3],const float delta[3],float limit,uint32_t *done);
/* Queue a fired pellet. RF_NOT_FOUND / RF_RANGE: caller resolves it at once. */
static int scene_player_round_launch(const scene_player_round *source,int32_t weapon,const float direction[3])
{
    scene_player_round *r=NULL;float length=0,range;uint32_t i,k;int status;
    if(weapon<0 || weapon>=64 || !(scene_tracer_weapons[weapon].velocity>0) || !(scene_tracer_weapons[weapon].lifetime>0)){++rf_scene_player_rounds[3];return RF_NOT_FOUND;}
    for(i=0;i<SCENE_PLAYER_ROUND_CAPACITY;i++)if(!scene_player_rounds[i].active){r=scene_player_rounds+i;break;}
    if(!r){++rf_scene_player_rounds[3];return RF_RANGE;}
    for(k=0;k<3;k++)length+=direction[k]*direction[k];
    length=sqrtf(length);if(!(length>0) || !isfinite(length))return RF_RANGE;
    *r=*source;
    r->speed=scene_tracer_weapons[weapon].velocity;range=r->speed*scene_tracer_weapons[weapon].lifetime;
    for(k=0;k<3;k++)r->path[k]=direction[k]/length*range;
    r->flown_fraction=0;r->tracer=0;
    /* Same counter rule as NPC rounds (4c7871 + 429f60), one player owner. */
    if(scene_tracer_weapons[weapon].enabled) {
        int tracer=scene_player_tracer_counter<=0;
        if(--scene_player_tracer_counter<0)scene_player_tracer_counter=scene_tracer_weapons[weapon].frequency;
        if(tracer && rf_scene_player_tracers && scene_tracer_visual) {
            scene_player_tracer *t=NULL;float dir[3],matrix[3][3];
            for(i=0;i<SCENE_PLAYER_TRACER_CAPACITY;i++)if(!scene_player_tracers[i].active){t=scene_player_tracers+i;break;}
            for(k=0;k<3;k++)dir[k]=direction[k]/length;
            if(!t)++rf_scene_player_tracer_stats[3];
            else if(!rf_entity_navigation_basis(dir,matrix)) {
                /* Flame muzzle offset from the eye (right .18, down .2, ahead .4) in the shot's own basis. */
                for(k=0;k<3;k++)t->origin[k]=r->origin[k]+matrix[2][k]*.4f+matrix[0][k]*.18f-matrix[1][k]*.2f;
                memcpy(t->basis,matrix,36);t->speed=scene_tracer_weapons[weapon].speed;t->travelled=t->age=0;
                t->limit=range;t->lifetime=scene_tracer_weapons[weapon].lifetime;t->active=1;
                r->tracer=1+(uint32_t)(t-scene_player_tracers);++rf_scene_player_tracer_stats[0];
            }
        }
    }
    if(r->handheld.enabled) {
        status=scene_sniper_penetration_begin(r->handheld.enabled,r->handheld.power,r->origin,r->path,&r->penetration);
        if(status)return status;
    }
    r->active=1;++rf_scene_player_rounds[0];return RF_OK;
}
/* One game update for every flying player round. */
static int scene_player_rounds_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,k,active=0,done;int status;
    for(i=0;i<SCENE_PLAYER_ROUND_CAPACITY;i++) {
        scene_player_round *r=scene_player_rounds+i;float position[3],delta[3],remaining,step,limit;
        if(!r->active)continue;
        remaining=1-r->flown_fraction;
        for(k=0;k<3;k++){position[k]=r->origin[k]+r->path[k]*r->flown_fraction;delta[k]=r->path[k]*remaining;}
        step=r->speed*scene_step_seconds;
        {float left=0;for(k=0;k<3;k++)left+=delta[k]*delta[k];left=sqrtf(left);limit=left>step?step/left:1;}
        status=scene_player_round_step(stream,frame,r,position,delta,limit,&done);if(status)return status;
        if(done || limit>=1) {
            /* The streak stops where the round did (eye-to-contact distance). */
            if(r->tracer && done) {
                float d=0;for(k=0;k<3;k++)d+=r->path[k]*r->path[k];
                scene_player_tracers[r->tracer-1].limit=sqrtf(d)*(r->flown_fraction+remaining*limit);
            }
            r->active=0;++rf_scene_player_rounds[2];continue;
        }
        r->flown_fraction+=remaining*limit;++active;
    }
    rf_scene_player_rounds[1]=active;return RF_OK;
}
static void scene_player_tracers_tick(void)
{
    uint32_t i,active=0;
    for(i=0;i<SCENE_PLAYER_TRACER_CAPACITY;i++) {
        scene_player_tracer *t=scene_player_tracers+i;
        if(!t->active)continue;
        t->travelled+=t->speed*scene_step_seconds;t->age+=scene_step_seconds;
        if(t->travelled>=t->limit || t->age>=t->lifetime){t->active=0;continue;}
        ++active;
    }
    rf_scene_player_tracer_stats[1]=active;
}
static int scene_player_tracers_draw(scene_stream *stream)
{
    uint32_t i,k;int status;
    if(!scene_tracer_visual)return RF_OK;
    for(i=0;i<SCENE_PLAYER_TRACER_CAPACITY;i++) {
        const scene_player_tracer *t=scene_player_tracers+i;float position[3];
        if(!t->active)continue;
        for(k=0;k<3;k++)position[k]=t->origin[k]+t->basis[6+k]*t->travelled;
        status=scene_vfx_emit(stream,scene_tracer_visual,position,t->basis,fmodf(t->age*15,scene_tracer_loop_frames));
        if(status==RF_RANGE)return RF_OK; /* stream full this frame: skip, never fail */
        if(status)return status;
        ++rf_scene_player_tracer_stats[2];
    }
    return RF_OK;
}
static void scene_player_rounds_reset(void){memset(scene_player_rounds,0,sizeof(scene_player_rounds));
    memset(scene_player_tracers,0,sizeof(scene_player_tracers));scene_player_tracer_counter=0;}
#endif
