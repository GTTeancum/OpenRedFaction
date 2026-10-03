/* First-pass fixed60Hz static-turret combat. Include after turret owner and
 * scene_ai_gameplay.inc (campaign_enemy_cadence). The parent supplies live
 * candidates and the existing world-cover/firearm dispatch, not UI input.
 * Authored class60/64 rates, class764 FOV and PHB limits are retained; this
 * acquisition/aim policy is not a claim of the original turret AI scheduler. */
typedef struct scene_turret_combat_target {
    uint32_t handle,alive,hostile,moving;float point[3];
} scene_turret_combat_target;
typedef struct scene_turret_combat_backend {
    uint32_t count;
    /* Hostility is relative to source; NOT_FOUND skips an absent candidate. */
    int (*target)(void *,uint32_t source,uint32_t index,scene_turret_combat_target *);
    /* World, movers and solid clutter. The source itself must be excluded. */
    int (*blocked)(void *,uint32_t source,const float start[3],const float delta[3],uint32_t *);
    /* Vauss is from_eye. Muzzle is for presentation; ray starts at eye and
     * has authored attack-range length. Reuse ordinary spread, nearest body,
     * shield, cover, damage and feedback paths. RF_OK consumes one shot even
     * on a miss; RF_NOT_FOUND means no shot emitted and retries next tick.
     * Callbacks may damage actors but must not retire/reallocate this owner. */
    int (*shot)(void *,uint32_t source,uint32_t target,int32_t weapon,
        const float eye[3],const float muzzle[3],const float ray[3],const rf_weapon_primary_definition *);
    void *context;
} scene_turret_combat_backend;
typedef struct scene_turret_combat_state {
    uint32_t handle,last_frame,burst;int32_t action;
    float mount[9],angles[3];rf_angular_velocity_state angular;
} scene_turret_combat_state;
static scene_turret_combat_state scene_turret_combat_states[SCENE_TURRET_OWNER_LIMIT];
static rf_weapon_primary_definition scene_turret_primary;
static int32_t scene_turret_vauss_id=-1;
static uint32_t scene_turret_combat_ready;
uint32_t rf_scene_turret_combat[10]; /* ticks,acquired,turned,shots,cover,aim holds,inert,last UID,last target,errors */
static void scene_turret_combat_close(void)
{
    memset(scene_turret_combat_states,0,sizeof(scene_turret_combat_states));
    memset(&scene_turret_primary,0,sizeof(scene_turret_primary));
    scene_turret_vauss_id=-1;scene_turret_combat_ready=0;
}
static int scene_turret_combat_open(rf_vpp *tables,int32_t vauss_id)
{
    rf_weapon_primary_definition primary;uint32_t i,j;int status;
    if(scene_turret_combat_ready || scene_turret_count>SCENE_TURRET_OWNER_LIMIT)return RF_RANGE;
    memset(rf_scene_turret_combat,0,sizeof(rf_scene_turret_combat));
    if(!scene_turret_count)return RF_OK;
    if(!tables || vauss_id<0 || vauss_id>=64)return RF_RANGE;
    status=rf_weapon_primary_load(tables,"Vauss",128*1024,&primary);if(status)return status;
    if(primary.projectiles!=1 || primary.damage_kind<0 || primary.damage_kind>8 ||
       primary.ai_attack_range<=0 || primary.magazine || primary.reload_seconds)return RF_FORMAT;
    for(i=0;i<scene_turret_count;i++) {
        const scene_turret_owner *o=scene_turrets+i;const rf_entity_seed_class *c=campaign_seeds.classes+o->class_index;
        float cone;memcpy(&cone,&c->vitals.field_764,4);
        if(scene_turret_lookup(o->registration.handle)!=o || !isfinite(cone) || cone< -1 || cone>1 ||
           !isfinite(c->rotation.maximum_velocity) || c->rotation.maximum_velocity<=0 ||
           !isfinite(c->rotation.acceleration) || c->rotation.acceleration<=0)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(c->eye_limits.minimum[j]) || !isfinite(c->eye_limits.maximum[j]) ||
            c->eye_limits.minimum[j]>0 || c->eye_limits.maximum[j]<0)return RF_FORMAT;
    }
    memset(scene_turret_combat_states,0,sizeof(scene_turret_combat_states));
    for(i=0;i<scene_turret_count;i++) {
        scene_turret_owner *o=scene_turrets+i;scene_turret_combat_state *s=scene_turret_combat_states+i;
        s->handle=o->registration.handle;s->last_frame=UINT32_MAX;s->action=o->view.action_520;
        memcpy(s->mount,o->basis,36);o->target=UINT32_MAX;o->fire_due=0;
    }
    scene_turret_primary=primary;scene_turret_vauss_id=vauss_id;scene_turret_combat_ready=1;return RF_OK;
}
static void scene_turret_combat_release(scene_turret_owner *o,scene_turret_combat_state *s)
{
    o->target=UINT32_MAX;o->fire_due=0;s->burst=0;memset(&s->angular,0,sizeof(s->angular));
}
static float scene_turret_angle_error(float desired,float current)
{
    float error=desired-current;
    if(error>3.1415927410125732f)error-=6.2831854820251465f;
    if(error< -3.1415927410125732f)error+=6.2831854820251465f;
    return error;
}
static int scene_turret_combat_turn(scene_turret_owner *o,scene_turret_combat_state *s,
    const float eye[3],const float target[3],uint32_t *reachable)
{
    const rf_entity_seed_class *c=campaign_seeds.classes+o->class_index;
    float delta[3],local[3],desired[3]={0},command[3]={0},error[3],matrix[9],next[9];
    float dt=1.f/60.f,rate=c->rotation.maximum_velocity;uint32_t i;int status;
    *reachable=1;
    for(i=0;i<3;i++)delta[i]=target[i]-eye[i];
    for(i=0;i<3;i++)local[i]=s->mount[i*3]*delta[0]+s->mount[i*3+1]*delta[1]+s->mount[i*3+2]*delta[2];
    /* rf_look_orientation uses normalized forward ((1-|sin p|)*sin h,
     * sin p,(1-|sin p|)*cos h), so invert that convention exactly. */
    {float horizontal=sqrtf(local[0]*local[0]+local[2]*local[2]);
     float span=horizontal+fabsf(local[1]);
     desired[0]=span>0?asinf(fmaxf(-1,fminf(1,local[1]/span))):0;
     desired[1]=atan2f(local[0],local[2]);}
    for(i=0;i<2;i++) {
        if(desired[i]<c->eye_limits.minimum[i] || desired[i]>c->eye_limits.maximum[i])*reachable=0;
        desired[i]=fmaxf(c->eye_limits.minimum[i],fminf(c->eye_limits.maximum[i],desired[i]));
        error[i]=i?scene_turret_angle_error(desired[i],s->angles[i]):desired[i]-s->angles[i];
        command[i]=fmaxf(-1,fminf(1,error[i]/(rate*dt)));
    }
    error[2]=0;
    /* Shared reconstructed49f485 angular drive response; zero force makes
     * mass immaterial here. Translation/body collision remains static. */
    status=rf_angular_velocity_step(&s->angular,command,rate,c->rotation.acceleration,1,dt,0,1);if(status)return status;
    for(i=0;i<2;i++) {
        float step=s->angular.velocity[i]*dt;
        if(step*error[i]>=0 && fabsf(step)>=fabsf(error[i])){step=error[i];s->angular.velocity[i]=0;}
        s->angles[i]+=step;
        if(i)s->angles[i]=scene_turret_angle_error(s->angles[i],0);
        s->angles[i]=fmaxf(c->eye_limits.minimum[i],fminf(c->eye_limits.maximum[i],s->angles[i]));
    }
    status=rf_look_orientation(s->angles,matrix);if(status)return status;
    for(i=0;i<3;i++)scene_turret_world_vector(s->mount,matrix+i*3,next+i*3);
    if(memcmp(o->basis,next,36))++rf_scene_turret_combat[2];memcpy(o->basis,next,36);return RF_OK;
}
static int scene_turret_combat_tick(uint32_t frame,const scene_turret_combat_backend *be)
{
    uint32_t i,j,k;int status;
    if(!scene_turret_count)return RF_OK;
    if(!scene_turret_combat_ready || !be || !be->target || !be->blocked || !be->shot || be->count>1024)return RF_RANGE;
    ++rf_scene_turret_combat[0];
    for(i=0;i<scene_turret_count;i++) {
        scene_turret_owner *o=scene_turrets+i;scene_turret_combat_state *s=scene_turret_combat_states+i;
        const rf_entity_seed_class *c=campaign_seeds.classes+o->class_index;
        scene_turret_combat_target chosen={0};float eye[3],delta[3],best=scene_turret_primary.ai_attack_range;
        uint32_t generated_ready=0,generated=0,alive,affiliation,found=0,blocked,reachable,operator_bound,operator_eligible,operator_actor;float cone;
        if(s->handle!=o->registration.handle || scene_turret_lookup(s->handle)!=o)return RF_FORMAT;
        /* The player owns aim/cadence until exit. Do not release or rewrite
         * that state through the autonomous action/target branches below. */
        if(scene_turret_player_controls(s->handle))continue;
        if(s->last_frame==frame)continue;
        if(s->last_frame!=UINT32_MAX && frame<s->last_frame)scene_turret_combat_release(o,s);
        s->last_frame=frame;
        if(s->action!=o->view.action_520){scene_turret_combat_release(o,s);s->action=o->view.action_520;}
        status=scene_turret_target(s->handle,eye,&affiliation,&alive);if(status)goto fail;
        /* Waiting2 can acquire, matching ordinary NPC semantics. Catatonic1,
         * scripted-look/fire modes and player/occupant control suspend auto-AI. */
        status=scene_turret_operator_controls(s->handle,&operator_bound,&operator_eligible,&affiliation,&operator_actor);if(status)goto fail;
        status=scene_turret_generated_ready(s->handle,&generated_ready);
        if(status!=RF_OK&&status!=RF_NOT_FOUND)goto fail;generated=status==RF_OK;
        if((generated&&!generated_ready) || (operator_bound&&!operator_eligible) || !alive || (!generated&&o->view.linked_handle!=-1) || o->view.weapons[0]!=scene_turret_vauss_id ||
           (s->action!=0 && s->action!=-1 && s->action!=2 && s->action!=3 && s->action!=11)) {
            scene_turret_combat_release(o,s);++rf_scene_turret_combat[6];continue;
        }
        memcpy(&cone,&c->vitals.field_764,4);best*=best;
        for(j=0;j<be->count;j++) {
            scene_turret_combat_target candidate={0};float distance=0,dot=0;
            status=be->target(be->context,s->handle,j,&candidate);if(status==RF_NOT_FOUND)continue;if(status)goto fail;
            if(!candidate.alive || !candidate.hostile || candidate.handle==s->handle || candidate.handle==UINT32_MAX)continue;
            for(k=0;k<3;k++) {
                if(!isfinite(candidate.point[k])){status=RF_FORMAT;goto fail;}
                delta[k]=candidate.point[k]-eye[k];distance+=delta[k]*delta[k];dot+=o->basis[6+k]*delta[k];
            }
            if(distance<.0001f || distance>scene_turret_primary.ai_attack_range*scene_turret_primary.ai_attack_range)continue;
            if(candidate.handle!=o->target) {
                if((s->action==11||scene_turret_operator_motion_only(s->handle)) && !candidate.moving)continue;
                if(dot<cone*sqrtf(distance))continue;
            }
            if(found && chosen.handle==o->target)continue;
            if(found && candidate.handle!=o->target && distance>=best)continue;
            status=be->blocked(be->context,s->handle,eye,delta,&blocked);if(status)goto fail;
            if(blocked){++rf_scene_turret_combat[4];continue;}
            chosen=candidate;best=distance;found=1;
        }
        if(!found){scene_turret_combat_release(o,s);continue;}
        if(o->target!=chosen.handle){o->target=chosen.handle;o->fire_due=0;s->burst=0;++rf_scene_turret_combat[1];}
        rf_scene_turret_combat[7]=o->uid;rf_scene_turret_combat[8]=chosen.handle;
        status=scene_turret_combat_turn(o,s,eye,chosen.point,&reachable);if(status)goto fail;
        if(!reachable){++rf_scene_turret_combat[5];continue;}
        if(frame<o->fire_due)continue;
        status=scene_turret_target(s->handle,eye,&affiliation,&alive);if(status)goto fail;
        {float distance=0,dot=0,pose[12],muzzle[3],ray[3];uint32_t remaining=s->burst,due;
         for(k=0;k<3;k++){delta[k]=chosen.point[k]-eye[k];distance+=delta[k]*delta[k];dot+=o->basis[6+k]*delta[k];}
         if(distance<.0001f || distance>scene_turret_primary.ai_attack_range*scene_turret_primary.ai_attack_range)continue;
         /* Five-degree convergence is a first-pass policy; never shoot through
          * the authored pitch limits toward an unreachable target. */
         if(dot<.996194698f*sqrtf(distance)){++rf_scene_turret_combat[5];continue;}
         status=scene_turret_model_tag(o->live_model,"muzzle_1",o->position,o->basis,pose);if(status)goto fail;
         memcpy(muzzle,pose+9,12);
         /* Sight and muzzle line must both be clear, including a barrel whose
          * presentation position has turned behind nearby world geometry. */
         status=be->blocked(be->context,s->handle,eye,delta,&blocked);if(status)goto fail;
         if(blocked){++rf_scene_turret_combat[4];continue;}
         for(k=0;k<3;k++)ray[k]=chosen.point[k]-muzzle[k];
         status=be->blocked(be->context,s->handle,muzzle,ray,&blocked);if(status)goto fail;
         if(blocked){++rf_scene_turret_combat[4];continue;}
         for(k=0;k<3;k++)ray[k]=delta[k]*(scene_turret_primary.ai_attack_range/sqrtf(distance));
         status=campaign_enemy_cadence(&scene_turret_primary,frame,&remaining,&due);if(status)goto fail;
         status=be->shot(be->context,s->handle,chosen.handle,scene_turret_vauss_id,eye,muzzle,ray,&scene_turret_primary);
         if(status==RF_NOT_FOUND)continue;if(status)goto fail;
         s->burst=remaining;o->fire_due=due;++rf_scene_turret_combat[3];}
    }
    return RF_OK;
fail:
    ++rf_scene_turret_combat[9];return status;
}
