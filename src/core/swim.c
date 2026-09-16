#include "rf/swim.h"
#include <math.h>
#include <string.h>
int rf_swim_transition_prepare(const rf_swim_state *state,
    const rf_movement_descriptor descriptors[16],uint32_t class_flags,
    int has_room,int body_wet,int eye_wet,int outer_fall_admission,
    rf_swim_transition *output)
{
    rf_swim_transition value;uint32_t slot;int swimming;
    if(!state || !descriptors || !output || state->mode>=16)return RF_RANGE;
    memset(&value,0,sizeof(value));value.state=*state;value.posture_request=-1;
    swimming=state->mode==4 || state->mode==7;
    if(!has_room || !body_wet) {
        value.state.object_flags&=~0x80000u;value.state.entity_flags&=~0x3000u;
        if(has_room) {
            value.state.query_flags|=0x1000u;
            if(swimming) {
                slot=rf_movement_fall(descriptors,class_flags,&value.state.body_flags);
                value.state.mode=descriptors[slot].index;value.exited=1;value.reset_parent_basis=1;
            }
        }
    } else {
        value.state.object_flags|=0x80000u;value.state.entity_flags|=0x1000u;
        if(eye_wet)value.state.entity_flags|=0x2000u;
        else value.state.entity_flags&=~0x2000u;
        if(!swimming && (class_flags&0x500u) && (eye_wet || outer_fall_admission)) {
            slot=(class_flags&0x400u)?7u:4u;
            value.posture_request=(class_flags&0x400u)?1:0;
            if(!(descriptors[slot].enabled&255u))slot=0;
            value.state.mode=descriptors[slot].index;value.entered=1;value.reset_parent_basis=1;
        }
    }
    *output=value;return RF_OK;
}
int rf_swim_motion_prepare(const rf_swim_controls *c,const float eye[9],
    float class_acceleration,float drag,float mass,float dt,int is_player,
    rf_swim_motion *output)
{
    static const uint32_t refs[3]={1,1,1};rf_swim_motion value;float strengths[8];uint32_t i;int status;
    if(!c || !eye || !output || !isfinite(drag) || drag<0 || !isfinite(mass) || mass<=0 ||
       !isfinite(dt) || dt<=0)return RF_RANGE;
    strengths[0]=c->right;strengths[1]=c->left;strengths[2]=c->up;strengths[3]=c->down;
    strengths[4]=c->forward;strengths[5]=c->backward;strengths[6]=c->jump;strengths[7]=c->crouch;
    for(i=0;i<8;i++)if(!isfinite(strengths[i]))return RF_RANGE;
    value.local[0]=(float)((double)c->right-c->left);
    value.local[1]=(float)((double)c->up-c->down);
    value.local[2]=(float)((double)c->forward-c->backward);
    if(c->jump_press_mode==1)value.local[1]=(float)((double)value.local[1]+c->jump);
    if(c->crouch_press_mode==1)value.local[1]=(float)((double)value.local[1]-c->crouch);
    status=rf_movement_acceleration(refs,value.local,class_acceleration,eye,eye,eye,value.acceleration);
    if(status)return status;
    value.resistance=(float)((2.0*drag)/mass);
    if(is_player) {float ceiling=(float)(1.0/dt);if(ceiling<value.resistance)value.resistance=ceiling;}
    if(!isfinite(value.resistance))return RF_RANGE;
    *output=value;return RF_OK;
}
int rf_swim_surface_jump(const rf_swim_state *state,
    const rf_movement_descriptor descriptors[16],uint32_t class_flags,
    int other_gates_allow,float velocity_y,float jump_speed,float frame_time,
    rf_swim_jump *output)
{
    rf_swim_jump value;double speed;uint32_t slot;
    if(!state || !descriptors || !output || state->mode>=16 || !isfinite(velocity_y) ||
       !isfinite(jump_speed) || jump_speed<0 || !isfinite(frame_time) || frame_time<0)return RF_RANGE;
    memset(&value,0,sizeof(value));value.state=*state;value.velocity_y=velocity_y;
    if(state->mode==4 && !(state->entity_flags&0x2000u) && other_gates_allow) {
        speed=(1.25-((double).1f-frame_time)*(double)-4.200000286102295f)*jump_speed;
        if(velocity_y<0)speed+=velocity_y;
        value.velocity_y=(float)speed;if(!isfinite(value.velocity_y))return RF_RANGE;
        value.state.entity_flags|=2u;
        slot=rf_movement_fall(descriptors,class_flags,&value.state.body_flags);
        value.state.mode=descriptors[slot].index;value.applied=1;value.reset_parent_basis=1;
    }
    *output=value;return RF_OK;
}
