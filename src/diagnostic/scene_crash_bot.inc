/* Test-only "bot pilot" (crash harness, D:\crash-bot.flag; never in tester
 * builds): the player's input is driven so test matches meet the situations
 * the bots meet. The pilot follows one live bot's actual trail (a crumb
 * every metre it moves, so every step is walkable; the bots' own PS2
 * routing picks where it goes: items, roaming, fights), fires while the
 * crash aim helper (run after it) has an aligned target, presses Fire to
 * respawn, and switches to another bot when it stalls or its bot dies.
 * Input only: no game state is changed. Its random numbers are its own. */
uint32_t rf_scene_crash_bot;
uint32_t rf_scene_crash_bot_trace[6]; /* bots followed, crumbs reached, stalls, bot deaths, fire frames, crumbs queued */
#define SCENE_CRASH_BOT_CRUMBS 64u
static float scene_crash_bot_crumb[SCENE_CRASH_BOT_CRUMBS][3];static uint32_t scene_crash_bot_head,scene_crash_bot_tail;
static uint32_t scene_crash_bot_slot=UINT32_MAX,scene_crash_bot_random=0x5eed,scene_crash_bot_check;static float scene_crash_bot_best;
static uint32_t scene_crash_bot_next(void){scene_crash_bot_random=scene_crash_bot_random*1103515245u+12345u;return scene_crash_bot_random>>8;}
static int scene_crash_bot_live(uint32_t slot)
{
    return slot<campaign_npc_body_count && slot<SCENE_MP_BOTS && rf_scene_mp_roster[slot].used && !rf_scene_mp_roster[slot].dead &&
        campaign_npc_bodies[slot].registration.view && campaign_npc_bodies[slot].damage.effects.health>0;
}
static void scene_crash_bot_pick(uint32_t frame)
{
    uint32_t live[SCENE_MP_BOTS],n=0,i;
    for(i=0;i<SCENE_MP_BOTS;i++)if(scene_crash_bot_live(i))live[n++]=i;
    scene_crash_bot_slot=n?live[scene_crash_bot_next()%n]:UINT32_MAX;
    scene_crash_bot_head=scene_crash_bot_tail=0;scene_crash_bot_best=1e30f;scene_crash_bot_check=frame;
    if(n){memcpy(scene_crash_bot_crumb[0],campaign_npc_bodies[scene_crash_bot_slot].body.state.position,12);scene_crash_bot_tail=1;++rf_scene_crash_bot_trace[0];}
}
static void scene_crash_bot_input(uint32_t frame,rf_scene_input *input)
{
    const float *p=scene_actor_body.state.position;float dx,dz,distance,yaw,s,c,forward,right,length;const float *goal;
    if(!rf_scene_crash_bot || !rf_scene_mp[0])return;
    if(!frame){memset(rf_scene_crash_bot_trace,0,sizeof(rf_scene_crash_bot_trace));scene_crash_bot_slot=UINT32_MAX;return;}
    memset(input,0,sizeof(*input));
    /* Fire respawns; a fresh bot to follow after it. */
    if(campaign_player_damage.state.effects.health<=0){input->fire=frame&1;scene_crash_bot_slot=UINT32_MAX;return;}
    if(scene_crash_bot_slot!=UINT32_MAX && !scene_crash_bot_live(scene_crash_bot_slot)){++rf_scene_crash_bot_trace[3];scene_crash_bot_slot=UINT32_MAX;}
    if(scene_crash_bot_slot==UINT32_MAX){scene_crash_bot_pick(frame);if(scene_crash_bot_slot==UINT32_MAX)return;}
    /* The followed bot drops a crumb every metre it moves. */
    {const float *b=campaign_npc_bodies[scene_crash_bot_slot].body.state.position,*last=scene_crash_bot_crumb[(scene_crash_bot_tail-1)%SCENE_CRASH_BOT_CRUMBS];
     float ex=b[0]-last[0],ey=b[1]-last[1],ez=b[2]-last[2];
     if(ex*ex+ey*ey+ez*ez>1.0f){
        if(scene_crash_bot_tail-scene_crash_bot_head>=SCENE_CRASH_BOT_CRUMBS)++scene_crash_bot_head; /* drop the oldest */
        memcpy(scene_crash_bot_crumb[scene_crash_bot_tail%SCENE_CRASH_BOT_CRUMBS],b,12);++scene_crash_bot_tail;++rf_scene_crash_bot_trace[5];
     }}
    /* Walk the oldest crumb; at the last one, stay near the bot. */
    goal=scene_crash_bot_crumb[scene_crash_bot_head%SCENE_CRASH_BOT_CRUMBS];
    dx=goal[0]-p[0];dz=goal[2]-p[2];distance=sqrtf(dx*dx+dz*dz);
    if(distance<1.0f){
        if(scene_crash_bot_tail-scene_crash_bot_head>1){++scene_crash_bot_head;++rf_scene_crash_bot_trace[1];scene_crash_bot_best=1e30f;scene_crash_bot_check=frame;}
        else{scene_crash_bot_check=frame;input->fire=1;return;}
    }
    /* Stall: no 0.5 m of progress in 3 s, jump at 2 s, then another bot. */
    if(distance<scene_crash_bot_best-.5f){scene_crash_bot_best=distance;scene_crash_bot_check=frame;}
    else if(frame-scene_crash_bot_check>180){scene_crash_bot_slot=UINT32_MAX;++rf_scene_crash_bot_trace[2];return;}
    else if(frame-scene_crash_bot_check==120)input->jump=1;
    /* Turn toward the crumb (the aim helper replaces this while it has a
     * target) and walk toward it in the body frame. */
    yaw=atan2f(dx,dz)-actor_look.state.body_angles[1];
    while(yaw>3.14159265f)yaw-=6.28318531f;
    while(yaw< -3.14159265f)yaw+=6.28318531f;
    input->look[1]=fmaxf(-.8f,fminf(.8f,yaw*3.f));
    input->look[0]=fmaxf(-.8f,fminf(.8f,-actor_look.state.eye_angles[0]*3.f));
    s=sinf(actor_look.state.body_angles[1]);c=cosf(actor_look.state.body_angles[1]);
    forward=dx*s+dz*c;right=dx*c-dz*s;length=sqrtf(forward*forward+right*right);
    if(length>0){input->move[2]=forward/length;input->move[0]=right/length;}
    input->fire=1;++rf_scene_crash_bot_trace[4]; /* the aim helper keeps it only on an aligned target */
}
