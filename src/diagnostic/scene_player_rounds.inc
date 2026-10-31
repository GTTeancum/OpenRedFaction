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
 * Port option (openrf.ini tracers, default on): every player bullet weapon
 * shows Tracer01.VFX on every shot (one per shotgun shell). Cosmetic only:
 * the round keeps its own flight and contact. The streak leaves the drawn
 * barrel (scene_player_muzzle.inc) toward the point the shot lands on the
 * world and flies at the round's own $Velocity, so it keeps pace with the
 * fire rate instead of the NPC tracer speed (0.275 x). */
#define SCENE_PLAYER_ROUND_CAPACITY 48u
typedef struct scene_player_round {
    scene_player_body_shot body_shot;scene_handheld_shot handheld;scene_sniper_penetration penetration;
    float origin[3],path[3],speed,flown_fraction;uint32_t fire,pellet,active;
} scene_player_round;
#define SCENE_PLAYER_TRACER_CAPACITY 32u
/* Streak length (Tracer01.VFX trails about 5 units); the tail starts at the barrel. */
#define SCENE_PLAYER_TRACER_TAIL 5.0f
typedef struct scene_player_tracer {float origin[3],basis[9],speed,travelled,limit,age;uint32_t active;} scene_player_tracer;
static scene_player_tracer scene_player_tracers[SCENE_PLAYER_TRACER_CAPACITY];
uint32_t rf_scene_player_tracers=1; /* openrf.ini tracers (main.c sets it) */
uint32_t rf_scene_tracer_hold; /* developer capture aid (tracer-hold.flag): streaks stay where they start */
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
/* A fired shell (pellet 0): streak from the barrel to where the eye ray lands. */
static void scene_player_tracer_start(scene_stream *s,int32_t weapon,const float eye[3],const float direction[3])
{
    scene_player_tracer *t=NULL;float path[3],end[3],dir[3],matrix[3][3],length=0,range;uint32_t i,k;
    if(!rf_scene_player_tracers || !scene_tracer_visual || weapon<0 || weapon>=64 || !(scene_tracer_weapons[weapon].velocity>0))return;
    for(k=0;k<3;k++)length+=direction[k]*direction[k];length=sqrtf(length);if(!(length>0) || !isfinite(length))return;
    range=scene_tracer_weapons[weapon].velocity*scene_tracer_weapons[weapon].lifetime;if(!(range>0))return;
    for(k=0;k<3;k++)path[k]=direction[k]/length*range;
    for(i=0;i<SCENE_PLAYER_TRACER_CAPACITY;i++)if(!scene_player_tracers[i].active){t=scene_player_tracers+i;break;}
    if(!t){++rf_scene_player_tracer_stats[3];return;}
    scene_player_muzzle_world(eye,t->origin);scene_player_aim_point(s,eye,path,end);
    length=0;for(k=0;k<3;k++){dir[k]=end[k]-t->origin[k];length+=dir[k]*dir[k];}
    length=sqrtf(length);if(!(length>1) || rf_entity_navigation_basis(dir,matrix))return;
    memcpy(t->basis,matrix,36);t->speed=scene_tracer_weapons[weapon].velocity;t->travelled=t->age=0;
    t->limit=length;t->active=1; /* tail travel up to the landing point */++rf_scene_player_tracer_stats[0];
}
static void scene_player_tracers_tick(void)
{
    uint32_t i,active=0;
    for(i=0;i<SCENE_PLAYER_TRACER_CAPACITY;i++) {
        scene_player_tracer *t=scene_player_tracers+i;
        if(!t->active)continue;
        t->travelled+=t->speed*scene_step_seconds*(rf_scene_tracer_hold?0.f:1.f);t->age+=scene_step_seconds;
        if(t->travelled>=t->limit){t->active=0;continue;}
        ++active;
    }
    rf_scene_player_tracer_stats[1]=active;
}
/* Camera-facing tracer01.tga streak (bright core of Tracer01.VFX's planes)
 * (x +/-0.54): tail on the travelled point, head up to
 * SCENE_PLAYER_TRACER_TAIL ahead, never past where the shot lands. Additive. */
static int scene_player_tracers_draw(scene_stream *stream,rf_scene_particle_sink sink,void *context)
{
    uint32_t i,k;int status;
    if(!scene_tracer_beam_ready || !sink)return RF_OK;
    for(i=0;i<SCENE_PLAYER_TRACER_CAPACITY;i++) {
        const scene_player_tracer *t=scene_player_tracers+i;float tail[3],head[3],ahead;
        if(!t->active)continue;
        ahead=t->limit-t->travelled;if(ahead>SCENE_PLAYER_TRACER_TAIL)ahead=SCENE_PLAYER_TRACER_TAIL;
        for(k=0;k<3;k++){tail[k]=t->origin[k]+t->basis[6+k]*t->travelled;head[k]=tail[k]+t->basis[6+k]*ahead;}
        status=scene_beam_draw(stream,sink,context,&scene_tracer_beam_image,tail,head,.35f,0xffffffffu,RF_PARTICLE_GLOW_MODE);
        if(status)return status;
        ++rf_scene_player_tracer_stats[2];
    }
    return RF_OK;
}
static void scene_player_rounds_reset(void){memset(scene_player_rounds,0,sizeof(scene_player_rounds));
    memset(scene_player_tracers,0,sizeof(scene_player_tracers));}
#endif
