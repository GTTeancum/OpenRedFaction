#include "rf/grenade_flight.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static int floor_sweep(void *ctx,const float start[3],const float delta[3],float radius,
    rf_weapon_flight_contact *hit,uint32_t *matched)
{
    uint32_t i;int mode=*(int*)ctx;if(mode==1)return RF_IO;
    *matched=delta[1]<0 && start[1]+delta[1]<=radius;
    if(*matched){hit->hit.fraction=(radius-start[1])/delta[1];
        if(hit->hit.fraction<0)hit->hit.fraction=0;
        hit->hit.normal[1]=1;hit->hit.normal[0]=hit->hit.normal[2]=0;
        for(i=0;i<3;++i)hit->hit.point[i]=start[i]+delta[i]*hit->hit.fraction;
        hit->hit.point[1]=0;if(mode==2)hit->hit.fraction=NAN;}
    return RF_OK;
}
int main(void)
{
    rf_grenade_flight g={0},before;rf_grenade_flight_event e,saved;
    float p[3]={0,1,0},d[3]={1,0,0},gravity[3]={0,-9.8f,0};int mode=0;uint32_t i;
    CHECK(!rf_grenade_flight_launch(&g,p,d,2,.1f,2,0,0));
    CHECK(!rf_grenade_flight_step(&g,.1f,gravity,.3f,floor_sweep,&mode,&e));
    CHECK(g.position[0]>.19f && g.position[1]<1 && g.velocity[1]<0 && !e.detonate);
    /* Low-energy ordinary impact rests; the fuse continues while motion stops. */
    g.position[1]=.1f;g.velocity[0]=0;g.velocity[1]=-.01f;
    CHECK(!rf_grenade_flight_step(&g,1.0f/60,gravity,.3f,floor_sweep,&mode,&e));
    CHECK(g.resting && e.contacts && !e.detonate);
    for(i=0;i<130 && !e.detonate;++i)CHECK(!rf_grenade_flight_step(&g,1.0f/60,gravity,.3f,floor_sweep,&mode,&e));
    CHECK(e.detonate && !g.lifecycle.active && g.resting);
    CHECK(!rf_grenade_flight_step(&g,0,gravity,.3f,floor_sweep,&mode,&e) && !e.detonate);
    /* Ordinary energetic impacts reflect and keep flying. */
    memset(&g,0,sizeof(g));p[1]=.11f;d[0]=0;d[1]=-1;
    CHECK(!rf_grenade_flight_launch(&g,p,d,4,.1f,2,0,0));
    CHECK(!rf_grenade_flight_step(&g,.1f,gravity,.5f,floor_sweep,&mode,&e));
    CHECK(e.contacts==1 && g.velocity[1]>0 && !g.resting && !e.detonate);
    /* Alternate contact arms next-tick explosion rather than bouncing. */
    memset(&g,0,sizeof(g));p[1]=.11f;d[0]=0;d[1]=-1;
    CHECK(!rf_grenade_flight_launch(&g,p,d,2,.1f,2,0,0x10));
    CHECK(!rf_grenade_flight_step(&g,.1f,gravity,.3f,floor_sweep,&mode,&e));
    CHECK(g.resting && g.lifecycle.life==-1 && !e.detonate);
    CHECK(!rf_grenade_flight_step(&g,0,gravity,.3f,floor_sweep,&mode,&e) && e.detonate);
    /* Failed query/invalid contact preserve the complete projectile and event. */
    memset(&g,0,sizeof(g));CHECK(!rf_grenade_flight_launch(&g,p,d,2,.1f,2,0,0));
    for(mode=1;mode<=2;++mode){before=g;memset(&e,0xa5,sizeof(e));saved=e;
        CHECK(rf_grenade_flight_step(&g,.1f,gravity,.3f,floor_sweep,&mode,&e)!=RF_OK);
        CHECK(!memcmp(&before,&g,sizeof(g)) && !memcmp(&saved,&e,sizeof(e)));}
    /* Expiry advances only to the remaining fuse time, then emits center. */
    memset(&g,0,sizeof(g));p[1]=1;d[0]=1;d[1]=0;mode=0;
    CHECK(!rf_grenade_flight_launch(&g,p,d,2,.1f,.05f,0,0));
    CHECK(!rf_grenade_flight_step(&g,.1f,gravity,.3f,floor_sweep,&mode,&e));
    CHECK(e.detonate && fabsf(e.position[0]-.1f)<1e-6f);
    puts("grenade gravity/rest/fuse/alternate contact/rollback passed");return 0;
}
