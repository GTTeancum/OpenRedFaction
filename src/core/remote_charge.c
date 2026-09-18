#include "rf/remote_charge.h"
#include <math.h>
#include <string.h>
int rf_remote_charge_launch(rf_remote_charge *state,uint32_t owner,uint32_t type,
    const float position[3],const float direction[3],float speed,float radius,float fuse)
{
    rf_remote_charge next={0};int status;
    if(!state || state->flight.lifecycle.active)return RF_RANGE;
    status=rf_grenade_flight_launch(&next.flight,position,direction,speed,radius,fuse,0x40,0);if(status)return status;
    next.owner=owner;next.type=type;next.authored_fuse=fuse;next.host=UINT32_MAX;next.physics_flags=0x80000008u;
    next.orientation[0]=next.orientation[4]=next.orientation[8]=1;next.contact.object=UINT32_MAX;*state=next;return RF_OK;
}
int rf_remote_charge_step(rf_remote_charge *state,float dt,const float gravity[3],
    rf_weapon_flight_sweep sweep,void *context,rf_remote_charge_event *out)
{
    rf_remote_charge next;rf_remote_charge_event event={0};uint32_t i,fire;int status;
    if(!state || !out || !gravity || !sweep || !isfinite(dt) || dt<0 || dt>.25f || state->attached>1)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(gravity[i]) || !isfinite(state->flight.position[i]) || !isfinite(state->flight.velocity[i]))return RF_RANGE;
    if(!isfinite(state->flight.radius) || state->flight.radius<0)return RF_RANGE;
    next=*state;status=rf_grenade_lifecycle_tick(&next.flight.lifecycle,dt,&fire);if(status)return status;
    event.detonate=fire;
    if(next.flight.lifecycle.active && !next.attached && dt>0){
        rf_weapon_flight moving={0};rf_weapon_flight_event movement;
        memcpy(moving.position,next.flight.position,12);moving.radius=next.flight.radius;moving.remaining=1;moving.active=1;
        for(i=0;i<3;i++){moving.velocity[i]=next.flight.velocity[i]+gravity[i]*dt;if(!isfinite(moving.velocity[i]))return RF_RANGE;}
        status=rf_weapon_flight_step(&moving,dt,sweep,context,&movement);if(status)return status;
        memcpy(next.flight.position,moving.position,12);memcpy(next.flight.velocity,moving.velocity,12);
        if(movement.kind==1){
            double length=0;next.contact=movement.contact;
            for(i=0;i<3;i++){if(!isfinite(next.contact.hit.point[i]) || !isfinite(next.contact.hit.normal[i]))return RF_RANGE;
                length+=(double)next.contact.hit.normal[i]*next.contact.hit.normal[i];}
            if(length<1e-20)return RF_RANGE;length=sqrt(length);
            for(i=0;i<3;i++){
                next.contact.hit.normal[i]=(float)(next.contact.hit.normal[i]/length);
                next.flight.position[i]=next.contact.hit.point[i]+(next.contact.object==UINT32_MAX?.05f*next.contact.hit.normal[i]:0);
            }
            memset(next.flight.velocity,0,12);next.attached=next.flight.resting=1;event.attached=1;
            if(next.contact.object==UINT32_MAX)next.physics_flags&=~8u;
        }
    }
    if(fire)next.object_flags|=2;
    memcpy(event.position,next.flight.position,12);event.contact=next.contact;
    *state=next;*out=event;return RF_OK;
}
int rf_remote_charge_request(rf_remote_charge *items,uint32_t count,uint32_t owner,uint32_t *any)
{
    uint32_t i,found=0;if(!any || count>RF_REMOTE_CHARGE_CAPACITY || (count && !items))return RF_RANGE;
    for(i=0;i<count;i++)if(items[i].owner==owner && (items[i].flight.lifecycle.class_flags&0x40)){
        items[i].flight.lifecycle.fuse=-1;found=1;}
    *any=found;return RF_OK;
}
int rf_remote_charge_available(const rf_remote_charge *items,uint32_t count,uint32_t owner,uint32_t type,uint32_t *any)
{
    uint32_t i,found=0;if(!any || count>RF_REMOTE_CHARGE_CAPACITY || (count && !items))return RF_RANGE;
    for(i=0;i<count;i++)if(items[i].owner==owner && items[i].type==type && !(items[i].object_flags&2) &&
        items[i].flight.lifecycle.fuse>0 && items[i].flight.lifecycle.life>0){found=1;break;}
    *any=found;return RF_OK;
}

static int remote_host_pose_valid(const rf_remote_charge_host *h)
{
    uint32_t i,j,k;
    if(!h || h->found>1 || h->entity>1)return 0;
    for(i=0;i<3;i++)if(!isfinite(h->position[i]))return 0;
    for(i=0;i<9;i++)if(!isfinite(h->orientation[i]))return 0;
    for(i=0;i<3;i++)for(j=i;j<3;j++){
        double d=0;for(k=0;k<3;k++)d+=(double)h->orientation[i*3+k]*h->orientation[j*3+k];
        if(fabs(d-(i==j?1:0))>.001)return 0;
    }
    return 1;
}
int rf_remote_charge_bind_host(rf_remote_charge *state,const rf_remote_charge_host *host)
{
    rf_remote_charge next;float forward[3],right[3],up[3]={0,1,0};double norm=0;uint32_t i,j,k;
    if(!state || !host)return RF_RANGE;
    if(state->attachment_initialized)return RF_OK;
    if(!state->flight.lifecycle.active || !state->attached || state->contact.object==UINT32_MAX ||
       !host->found || host->handle==UINT32_MAX || !remote_host_pose_valid(host) ||
       !isfinite(state->authored_fuse) || state->authored_fuse<=0)return RF_RANGE;
    for(i=0;i<3;i++){if(!isfinite(state->contact.hit.normal[i]) || !isfinite(state->contact.hit.point[i]))return RF_RANGE;
        norm+=(double)state->contact.hit.normal[i]*state->contact.hit.normal[i];}
    if(norm<1e-20)return RF_RANGE;norm=sqrt(norm);
    for(i=0;i<3;i++)forward[i]=(float)(-state->contact.hit.normal[i]/norm);
    if(fabsf(forward[1])>.99f){up[0]=1;up[1]=0;}
    right[0]=up[1]*forward[2]-up[2]*forward[1];right[1]=up[2]*forward[0]-up[0]*forward[2];right[2]=up[0]*forward[1]-up[1]*forward[0];
    norm=sqrt((double)right[0]*right[0]+(double)right[1]*right[1]+(double)right[2]*right[2]);
    for(i=0;i<3;i++)right[i]=(float)(right[i]/norm);
    up[0]=forward[1]*right[2]-forward[2]*right[1];up[1]=forward[2]*right[0]-forward[0]*right[2];up[2]=forward[0]*right[1]-forward[1]*right[0];
    next=*state;memcpy(next.orientation,right,12);memcpy(next.orientation+3,up,12);memcpy(next.orientation+6,forward,12);
    for(i=0;i<3;i++){
        double d=0;for(k=0;k<3;k++)d+=((double)next.contact.hit.point[k]-host->position[k])*host->orientation[i*3+k];
        next.local_offset[i]=(float)d;if(!isfinite(next.local_offset[i]))return RF_RANGE;
        for(j=0;j<3;j++){d=0;for(k=0;k<3;k++)d+=(double)next.orientation[i*3+k]*host->orientation[j*3+k];next.local_orientation[i*3+j]=(float)d;}
    }
    next.host=host->handle;next.host_bound=next.attachment_initialized=1;next.physics_flags&=~0x80000000u;
    next.flight.lifecycle.instance_flags|=0x40;next.flight.lifecycle.fuse=next.authored_fuse;
    *state=next;return RF_OK;
}
int rf_remote_charge_update_host(rf_remote_charge *state,const rf_remote_charge_host *host,uint32_t *action)
{
    rf_remote_charge next;uint32_t i,j,k,value=0;
    if(!state || !action)return RF_RANGE;
    if(!state->host_bound){*action=0;return RF_OK;}
    if(!host || host->handle!=state->host || host->found>1 || host->entity>1)return RF_RANGE;
    next=*state;
    if(host->entity && ((host->entity_flags&1) || (host->class_flags&0x4000000u))){
        next.host_bound=0;next.host=UINT32_MAX;next.attached=next.flight.resting=0;
        memset(next.flight.velocity,0,12);next.physics_flags|=0x80000000u;value=2;
    }else if(!host->found){
        next.object_flags|=2;next.flight.lifecycle.active=0;value=3;
    }else{
        if(!remote_host_pose_valid(host))return RF_RANGE;
        for(k=0;k<3;k++){
            double d=host->position[k];for(i=0;i<3;i++)d+=(double)next.local_offset[i]*host->orientation[i*3+k];
            next.flight.position[k]=(float)d;if(!isfinite(next.flight.position[k]))return RF_RANGE;
            for(j=0;j<3;j++){d=0;for(i=0;i<3;i++)d+=(double)next.local_orientation[j*3+i]*host->orientation[i*3+k];
                next.orientation[j*3+k]=(float)d;if(!isfinite(next.orientation[j*3+k]))return RF_RANGE;}
        }
        memcpy(next.contact.hit.point,next.flight.position,12);
        for(k=0;k<3;k++)next.contact.hit.normal[k]=-next.orientation[6+k];value=1;
    }
    *state=next;*action=value;return RF_OK;
}
