#include "rf/grenade_flight.h"
#include <math.h>
#include <string.h>
int rf_grenade_flight_launch(rf_grenade_flight *g,const float p[3],const float d[3],
    float speed,float radius,float fuse,uint32_t cls,uint32_t flags)
{
    rf_grenade_flight next={0};rf_weapon_flight flight={0};int status;
    if(!g || g->lifecycle.active || !isfinite(fuse) || fuse<=0)return RF_RANGE;
    status=rf_weapon_flight_launch(&flight,p,d,speed,10,radius);if(status)return status;
    memcpy(next.position,flight.position,12);memcpy(next.velocity,flight.velocity,12);
    next.radius=radius;next.lifecycle=(rf_grenade_lifecycle){fuse,10,1,cls,flags};
    *g=next;return RF_OK;
}
int rf_grenade_flight_step(rf_grenade_flight *g,float dt,const float gravity[3],
    float restitution,rf_weapon_flight_sweep sweep,void *context,rf_grenade_flight_event *out)
{
    rf_grenade_flight next;rf_grenade_flight_event event={0};float remaining;
    uint32_t i,fire;int status;
    if(!g || !out || !gravity || !sweep || !isfinite(dt) || dt<0 || dt>.25f ||
       !isfinite(restitution) || restitution<0 || restitution>1 ||
       !isfinite(g->radius) || g->radius<0)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(gravity[i]) || !isfinite(g->position[i]) || !isfinite(g->velocity[i]))return RF_RANGE;
    next=*g;
    /* Check a prior contact expiry before applying this tick's gravity. */
    status=rf_grenade_lifecycle_tick(&next.lifecycle,0,&fire);if(status)return status;
    event.detonate=fire;
    if(next.lifecycle.active){
        remaining=dt;
        if(!(next.lifecycle.class_flags&0x40u) && next.lifecycle.fuse<remaining)remaining=next.lifecycle.fuse;
        status=rf_grenade_lifecycle_tick(&next.lifecycle,dt,&fire);if(status)return status;
        event.detonate=fire;
        if(!next.resting && remaining>0){
            for(i=0;i<3;++i){
                next.velocity[i]=(float)((double)next.velocity[i]+gravity[i]*remaining);
                if(!isfinite(next.velocity[i]))return RF_RANGE;
            }
            while(remaining>0 && event.contacts<4){
                float delta[3],normal[3],dot=0,speed2=0;double length=0;
                uint32_t matched=0,response=0;rf_weapon_flight_contact hit={0};
                for(i=0;i<3;++i){delta[i]=next.velocity[i]*remaining;if(!isfinite(delta[i]))return RF_RANGE;}
                status=sweep(context,next.position,delta,next.radius,&hit,&matched);if(status)return status;
                if(!matched){for(i=0;i<3;++i)next.position[i]+=delta[i];remaining=0;break;}
                if(!isfinite(hit.hit.fraction) || hit.hit.fraction<0 || hit.hit.fraction>1)return RF_RANGE;
                for(i=0;i<3;++i){
                    if(!isfinite(hit.hit.normal[i]) || !isfinite(hit.hit.point[i]))return RF_RANGE;
                    length+=(double)hit.hit.normal[i]*hit.hit.normal[i];
                }
                if(length<1e-20)return RF_RANGE;length=sqrt(length);
                for(i=0;i<3;++i){normal[i]=(float)(hit.hit.normal[i]/length);dot+=next.velocity[i]*normal[i];
                    next.position[i]+=delta[i]*hit.hit.fraction+normal[i]*.0001f;}
                ++event.contacts;event.contact=hit;
                remaining*=1-hit.hit.fraction;
                /* A fuse due this tick takes precedence over deferred contact. */
                if(!event.detonate){status=rf_grenade_lifecycle_contact(&next.lifecycle,&response);if(status)return status;}
                if(response==1){next.resting=1;memset(next.velocity,0,12);remaining=0;break;}
                if(dot<0)for(i=0;i<3;++i)next.velocity[i]=
                    .8f*(next.velocity[i]-dot*normal[i])-restitution*dot*normal[i];
                for(i=0;i<3;++i)speed2+=next.velocity[i]*next.velocity[i];
                if(!isfinite(speed2))return RF_RANGE;
                if(normal[1]>.5f && speed2<.25f){next.resting=1;memset(next.velocity,0,12);remaining=0;}
            }
            if(remaining>0)event.limited=1;
        }
    }
    for(i=0;i<3;++i){if(!isfinite(next.position[i]) || !isfinite(next.velocity[i]))return RF_RANGE;event.position[i]=next.position[i];}
    *g=next;*out=event;return RF_OK;
}
