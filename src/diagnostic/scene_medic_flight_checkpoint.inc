#ifndef RF_SCENE_MEDIC_FLIGHT_CHECKPOINT_INCLUDED
#define RF_SCENE_MEDIC_FLIGHT_CHECKPOINT_INCLUDED
/* Pointer-free RFNC25 translation; admission never routes, draws or publishes. */
static int scene_medic_flight_checkpoint_capture(const campaign_npc_body *o,int32_t now,
    rf_npc_checkpoint_flight *out)
{
    scene_medic_flight_snapshot p;rf_npc_checkpoint_flight f={0};int status;uint32_t i;
    status=scene_medic_flight_capture(o,now,&p);if(status)return status;
    if(!p.state.qualified){*out=f;return RF_OK;}
    f.present=p.state.qualified;f.entered=p.state.entered;f.target_kind=p.state.target_kind;
    {const int32_t deadlines[5]={p.state.deadline_250,p.state.sight_deadline_294,
        p.state.no_route_deadline_4d4,p.state.reset_deadline_1370,p.state.stall_deadline_4d0};
     for(i=0;i<5;i++){
        if(deadlines[i]<0)f.remaining[i]=-1;
        else {status=rf_timer_remaining(deadlines[i],now,f.remaining+i);if(status)return status;
            if(f.remaining[i]<0)f.remaining[i]=0;}
     }}
    if(p.state.nav.wait_134<0)f.wait_remaining=-1;
    else {status=rf_timer_remaining(p.state.nav.wait_134,now,&f.wait_remaining);if(status)return status;
        if(f.wait_remaining<0)f.wait_remaining=0;}
    f.visible[0]=p.state.visible_published_29c;
    f.visible[1]=p.state.visible_eye_29d;
    f.count=p.state.nav.count;
    f.previous=p.state.nav.previous;
    f.current=p.state.nav.current;
    f.bias=p.state.nav.bias_158;
    f.approach=p.state.nav.approach_15c;
    f.activity=p.state.nav.activity_170;
    f.start_valid=p.state.nav.start_valid;
    f.eye_valid=p.state.nav.eye_valid;
    f.start_radius=p.start.radius;
    f.start_height=p.start.height;
    f.start_word=p.start.word_040;
    memcpy(f.turn,p.state.nav.turn_138,12);
    memcpy(f.eye,p.state.nav.request_eye_14c,12);
    memcpy(f.steering,p.state.nav.steering_174,12);
    memcpy(f.start_position,p.start.position,12);
    memcpy(f.start_query,p.start.query_point,12);
    memcpy(f.command,p.command_714,12);
    memcpy(f.angular,p.angular_150,12);memcpy(f.torque,p.torque_174,12);
    for(i=0;i<2;i++){f.pair[i].kind=p.state.nav.pair[i].kind;f.pair[i].ordinal=p.state.nav.pair[i].ordinal;}
    for(i=0;i<4;i++){f.prefix[i].kind=p.state.nav.prefix[i].kind;f.prefix[i].ordinal=p.state.nav.prefix[i].ordinal;}
    *out=f;return RF_OK;
}
static int scene_medic_flight_checkpoint_prepare(const rf_npc_checkpoint_flight *f,int32_t now,
    scene_medic_flight_snapshot *out)
{
    scene_medic_flight_snapshot p={0};uint32_t i;int status;
    if(!f||!out)return RF_RANGE;if(!f->present){*out=p;return RF_OK;}
    p.state.qualified=f->present;p.state.entered=f->entered;p.state.target_kind=f->target_kind;
    {int32_t *deadlines[5]={&p.state.deadline_250,&p.state.sight_deadline_294,
        &p.state.no_route_deadline_4d4,&p.state.reset_deadline_1370,&p.state.stall_deadline_4d0};
     for(i=0;i<5;i++){
        if(f->remaining[i]<0)*deadlines[i]=-1;
        else {status=rf_timer_set(deadlines[i],now,f->remaining[i]);if(status)return status;}
     }}
    if(f->wait_remaining<0)p.state.nav.wait_134=-1;
    else {status=rf_timer_set(&p.state.nav.wait_134,now,f->wait_remaining);if(status)return status;}
    p.state.visible_published_29c=f->visible[0];
    p.state.visible_eye_29d=f->visible[1];
    p.state.nav.count=f->count;
    p.state.nav.previous=f->previous;
    p.state.nav.current=f->current;
    p.state.nav.bias_158=f->bias;
    p.state.nav.approach_15c=f->approach;
    p.state.nav.activity_170=f->activity;
    p.state.nav.start_valid=f->start_valid;
    p.state.nav.eye_valid=f->eye_valid;
    p.start.radius=f->start_radius;
    p.start.height=f->start_height;
    p.start.word_040=f->start_word;
    memcpy(p.state.nav.turn_138,f->turn,12);
    memcpy(p.state.nav.request_eye_14c,f->eye,12);
    memcpy(p.state.nav.steering_174,f->steering,12);
    memcpy(p.start.position,f->start_position,12);
    memcpy(p.start.query_point,f->start_query,12);
    memcpy(p.command_714,f->command,12);
    memcpy(p.angular_150,f->angular,12);memcpy(p.torque_174,f->torque,12);
    for(i=0;i<2;i++){p.state.nav.pair[i].kind=f->pair[i].kind;p.state.nav.pair[i].ordinal=f->pair[i].ordinal;}
    for(i=0;i<4;i++){p.state.nav.prefix[i].kind=f->prefix[i].kind;p.state.nav.prefix[i].ordinal=f->prefix[i].ordinal;}
    *out=p;return RF_OK;
}
#endif
