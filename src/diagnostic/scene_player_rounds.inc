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
 * (slots 6/7) stay instant. */
#define SCENE_PLAYER_ROUND_CAPACITY 48u
typedef struct scene_player_round {
    scene_player_body_shot body_shot;scene_handheld_shot handheld;scene_sniper_penetration penetration;
    float origin[3],path[3],speed,flown_fraction;uint32_t fire,pellet,active;
} scene_player_round;
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
    r->flown_fraction=0;
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
        if(done || limit>=1){r->active=0;++rf_scene_player_rounds[2];continue;}
        r->flown_fraction+=remaining*limit;++active;
    }
    rf_scene_player_rounds[1]=active;return RF_OK;
}
static void scene_player_rounds_reset(void){memset(scene_player_rounds,0,sizeof(scene_player_rounds));}
#endif
