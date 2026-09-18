#include "rf/entity_assets.h"
#include "rf/geometry.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"explosive line %d\n",__LINE__);return 1;}} while(0)
static int grenade_lifecycle(void)
{
    rf_grenade_lifecycle g={.125f,10,1,0,0},before;uint32_t event=99;
    CHECK(!rf_grenade_lifecycle_contact(&g,&event) && event==2 && g.fuse==.125f && g.life==10);
    CHECK(!rf_grenade_lifecycle_tick(&g,.25f,&event) && event==1 && !g.active && g.fuse==-.125f);
    CHECK(!rf_grenade_lifecycle_tick(&g,.25f,&event) && !event && g.fuse==-.125f);
    g=(rf_grenade_lifecycle){5,10,1,0,0x10};
    CHECK(!rf_grenade_lifecycle_contact(&g,&event) && event==1 && g.life==-1 && g.active);
    CHECK(!rf_grenade_lifecycle_tick(&g,0,&event) && event==1 && !g.active);
    g=(rf_grenade_lifecycle){5,10,1,0x40,0};
    CHECK(!rf_grenade_lifecycle_tick(&g,10,&event) && !event && g.fuse==5);
    g.fuse=0;CHECK(!rf_grenade_lifecycle_tick(&g,0,&event) && event==1);
    before=g;event=99;
    CHECK(rf_grenade_lifecycle_tick(&g,NAN,&event)==RF_RANGE && event==99 && !memcmp(&before,&g,sizeof(g)));
    return 0;
}
typedef struct sweep_context {const rf_geometry_collision_world *world;uint32_t calls;int fail,bad;} sweep_context;
static int sweep(void *context,const float start[3],const float delta[3],float radius,
    rf_weapon_flight_contact *out,uint32_t *matched)
{
    sweep_context *c=context;rf_geometry_world_sweep_hit hit;int status;++c->calls;
    if(c->fail)return RF_IO;
    status=rf_geometry_collision_world_sweep(c->world,0x460,start,delta,radius,1,&hit,matched);if(status)return status;
    if(*matched){out->hit=hit.hit;out->room=hit.room;out->face=hit.face;out->object=UINT32_MAX;if(c->bad)out->hit.fraction=2;}
    return RF_OK;
}
typedef struct liquid_sweep_context {
    unsigned calls,mode;float starts[2],deltas[2];uint32_t flags[2];
} liquid_sweep_context;
static int liquid_sweep(void *context,const float start[3],const float delta[3],float radius,
    uint32_t flags,rf_weapon_flight_contact *out,uint32_t *is_liquid,uint32_t *matched)
{
    liquid_sweep_context *c=context;unsigned n=c->calls++;(void)radius;
    if(n>=2)return RF_FORMAT;
    c->starts[n]=start[0];c->deltas[n]=delta[0];c->flags[n]=flags;
    if(n && c->mode==5)return RF_IO;
    *is_liquid=(n==0 || c->mode==6);*matched=1;
    if(c->mode==7)*is_liquid=0;
    if(n && c->mode==1){*matched=0;*is_liquid=0;return RF_OK;}
    out->hit.fraction=n?.5f:(c->mode==2?0:c->mode==3?1:.25f);
    out->hit.normal[0]=-1;out->hit.normal[1]=out->hit.normal[2]=0;
    out->hit.point[0]=start[0]+delta[0]*out->hit.fraction;
    out->hit.point[1]=out->hit.point[2]=0;out->room=3;out->face=n+8;out->object=UINT32_MAX;
    return RF_OK;
}
static int dry_sweep(void *context,const float start[3],const float delta[3],float radius,
    rf_weapon_flight_contact *out,uint32_t *matched)
{uint32_t liquid;return liquid_sweep(context,start,delta,radius,0,out,&liquid,matched);}
static int liquid_flight_tests(void)
{
    unsigned mode;
    for(mode=0;mode<=7;mode++) {
        const float pos[3]={0,0,0},dir[3]={1,0,0};
        rf_weapon_flight f={0},before;rf_weapon_flight_liquid_state q={0x1004},q_before=q;
        rf_weapon_flight_liquid_policy p={0,1,23,42};rf_weapon_flight_liquid_event e,saved_e;
        liquid_sweep_context c={0};int status;
        c.mode=mode;if(mode==4)p.weapon_flags=0x10000;
        CHECK(!rf_weapon_flight_launch(&f,pos,dir,100,10,.1f));before=f;
        memset(&e,0x5a,sizeof(e));saved_e=e;
        status=rf_weapon_flight_step_liquid(&f,.1f,&q,&p,liquid_sweep,&c,&e);
        if(mode==5 || mode==6) {
            CHECK(status==(mode==5?RF_IO:RF_FORMAT));
            CHECK(!memcmp(&f,&before,sizeof(f)) && !memcmp(&q,&q_before,sizeof(q)) && !memcmp(&e,&saved_e,sizeof(e)));
            continue;
        }
        CHECK(!status);
        if(mode==7) {
            rf_weapon_flight dry=before;rf_weapon_flight_event event;liquid_sweep_context d={0};d.mode=7;
            CHECK(!rf_weapon_flight_step(&dry,.1f,dry_sweep,&d,&event));
            CHECK(!memcmp(&f,&dry,sizeof(f)) && !memcmp(&e.terminal,&event,sizeof(event)) && !e.has_liquid && q.query_flags==0x1004);
        } else if(mode==3) {
            CHECK(c.calls==1 && !e.has_liquid && !e.terminal.kind && q.query_flags==0x1004 && f.active && f.position[0]==10);
        } else {
            CHECK(e.has_liquid && e.liquid_contact.face==8 && e.liquid_effect.handle==42 && e.liquid_effect.size==.5f && q.query_flags==4);
            if(mode==4)CHECK(c.calls==1 && !f.active && !f.remaining && e.terminal.kind==2);
            else {
                CHECK(c.calls==2 && c.flags[0]==0x1004 && c.flags[1]==4);
                CHECK(fabsf(c.starts[1]-(mode==2?0:2.45f))<1e-6f);
                CHECK(fabsf(c.deltas[1]-(mode==2?10:7.5f))<1e-6f);
                if(mode==1)CHECK(f.active && !e.terminal.kind && fabsf(f.position[0]-9.95f)<1e-6f && fabs(f.remaining-9.9)<1e-6);
                else CHECK(!f.active && e.terminal.kind==1 && e.terminal.contact.face==9 && fabsf(f.position[0]-(mode==2?5:6.2f))<1e-6f);
            }
        }
    }
    {
        const float pos[3]={0,0,0},dir[3]={1,0,0};
        rf_weapon_flight f={0},saved;rf_weapon_flight_liquid_state q={0x1004};
        rf_weapon_flight_liquid_policy p={0,0,23,42};rf_weapon_flight_liquid_event e;
        liquid_sweep_context c={0};c.mode=3;
        CHECK(!rf_weapon_flight_launch(&f,pos,dir,100,.05f,.1f));saved=f;
        memset(&e,0x5a,sizeof(e));
        CHECK(!rf_weapon_flight_step_liquid(&f,0,&q,&p,liquid_sweep,&c,&e));
        CHECK(!c.calls && !e.has_liquid && !e.terminal.kind && !memcmp(&f,&saved,sizeof(f)) && q.query_flags==0x1004);
        CHECK(!rf_weapon_flight_step_liquid(&f,.1f,&q,&p,liquid_sweep,&c,&e));
        CHECK(c.calls==1 && !f.active && !f.remaining && e.terminal.kind==2 && !e.has_liquid && f.position[0]==5 && q.query_flags==0x1004);
        CHECK(!rf_weapon_flight_step_liquid(&f,.1f,&q,&p,liquid_sweep,&c,&e));
        CHECK(c.calls==1 && !e.has_liquid && !e.terminal.kind);
    }
    return 0;
}
int main(int argc,char **argv)
{
    CHECK(!grenade_lifecycle());
    CHECK(!liquid_flight_tests());
    /* Original verify_projectile_liquid_contact.py: six complete contact-dispatch cases.
     * Selector/clamp boundaries additionally exercise the 4c4e30 contract. */
    {
        const uint32_t flags[]={0,0x10000,0x1000000};unsigned k,liquid;
        for(k=0;k<3;k++)for(liquid=1;liquid<=2;liquid++) {
            rf_weapon_liquid_state s={10,liquid,0x1000};rf_weapon_liquid_effect e;
            CHECK(!rf_weapon_liquid_contact(&s,.1f,flags[k],1,23,42,&e));
            CHECK(s.life==(k==1?0:10) && !s.is_liquid && !s.query_flags && e.handle==42 && e.size==.5f);
        }
        {
            const float radii[]={-1,0,.249f,.25f,.251f,2};
            const int selectors[]={-1,0,1,2,3};unsigned r,t;
            for(r=0;r<6;r++)for(t=0;t<5;t++) {
                rf_weapon_liquid_state s={-1,1,0xdeadffff};rf_weapon_liquid_effect e;
                CHECK(!rf_weapon_liquid_contact(&s,radii[r],0,selectors[t],-1,77,&e));
                CHECK(s.life==-1 && !s.is_liquid && s.query_flags==0xdeadefff);
                CHECK(e.handle==((selectors[t]==1 || selectors[t]==2)?77:-1));
                CHECK(e.size==(radii[r]<.25f?.5f:radii[r]+radii[r]));
            }
        }
        {
            rf_weapon_liquid_state s={10,0,0x1004},saved_s=s;
            rf_weapon_liquid_effect e={44,9},saved_e=e;
            CHECK(rf_weapon_liquid_contact(&s,.1f,0,1,23,42,&e)==RF_NOT_FOUND);
            CHECK(!memcmp(&s,&saved_s,sizeof(s)) && !memcmp(&e,&saved_e,sizeof(e)));
            s.is_liquid=1;saved_s=s;
            CHECK(rf_weapon_liquid_contact(&s,INFINITY,0,1,23,42,&e)==RF_RANGE);
            CHECK(!memcmp(&s,&saved_s,sizeof(s)) && !memcmp(&e,&saved_e,sizeof(e)));
            CHECK(rf_weapon_liquid_contact(NULL,.1f,0,1,23,42,&e)==RF_FORMAT);
        }
    }
    const char *prefix="$Name: \"test\" $Weapon Type: \"explosive\" ";
    const char *valid="$Velocity: 20 $Lifetime: 15 $Collision Radius: .051 $Damage Radius: 5 +Crater Radius: 5 ";
    const char *bad[]={
        "$Velocity: 0 $Lifetime: 15 $Collision Radius: .051 $Damage Radius: 5 +Crater Radius: 5",
        "$Velocity: 20 $Lifetime: -1 $Collision Radius: .051 $Damage Radius: 5 +Crater Radius: 5",
        "$Velocity: 20 $Lifetime: 15 $Collision Radius: -.1 $Damage Radius: 5 +Crater Radius: 5",
        "$Velocity: 20 $Lifetime: 15 $Collision Radius: .051 $Damage Radius: -5 +Crater Radius: 5",
        "$Velocity: 20 $Lifetime: 15 $Collision Radius: .051 $Damage Radius: 5 +Crater Radius: nan",
        "$Velocity: 20 $Lifetime: 15 $Collision Radius: .051 $Damage Radius: 5",
        "$Velocity: 20 $Lifetime: 15 $Collision Radius: .051 $Damage Radius: 5 +Crater Radius: 5 $Lifetime: 3",
        "$Velocity: 20 $Lifetime: 15 $Collision Radius: .051 $Damage Radius: 5 +Crater Radius: 5 $Weapon Type: \"explosive\""
    };
    rf_weapon_explosive_definition value,saved;char text[1024],path[1024];unsigned i;
    snprintf(text,sizeof(text),"%s%s $Velocity Multi: 100 $Lifetime Multi: 3 $Damage Radius Multi: 12 #End",prefix,valid);
    CHECK(!rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value));
    CHECK(value.speed==20 && value.lifetime==15 && fabsf(value.collision_radius-.051f)<1e-6f && value.damage_radius==5 && value.crater_radius==5);
    saved=value;
    for(i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        snprintf(text,sizeof(text),"%s%s #End",prefix,bad[i]);
        CHECK(rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value)!=RF_OK && !memcmp(&value,&saved,sizeof(value)));
    }
    snprintf(text,sizeof(text),"$Name: \"test\" $Weapon Type: \"bullet\" %s #End",valid);
    CHECK(rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value)==RF_FORMAT && !memcmp(&value,&saved,sizeof(value)));
    CHECK(rf_weapon_explosive_read(text,(uint32_t)strlen(text),"missing",&value)==RF_NOT_FOUND && !memcmp(&value,&saved,sizeof(value)));
    {
        const char *bad_glow[]={
            "$Glow: true +Inner Radius: 4 +Outer Radius: 3 +Color: {100,50,100}",
            "$Glow: true +Inner Radius: 1 +Outer Radius: 0 +Color: {100,50,100}",
            "$Glow: true +Inner Radius: 1 +Outer Radius: 3 +Color: {256,50,100}",
            "$Glow: true +Inner Radius: 1 +Outer Radius: 3 +Color: {100,50}",
            "$Glow: true +Inner Radius: nan +Outer Radius: 3 +Color: {100,50,100}",
            "$Glow: false $Glow: true +Inner Radius: 1 +Outer Radius: 3 +Color: {100,50,100}",
            "$Glow: true +Inner Radius: 1 $Muzzle Flash Light: true +Outer Radius: 7 +Color: {255,255,255}"
        };
        snprintf(text,sizeof(text),"%s%s $Glow: true +Inner Radius: 1 +Outer Radius: 3 +Color: { 100, 50, 100 } $Muzzle Flash Light: true +Inner Radius: 4 +Outer Radius: 7 +Color: {255,255,255} #End",prefix,valid);
        CHECK(!rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value));
        CHECK(value.glow && value.glow_inner==1 && value.glow_outer==3 && fabsf(value.glow_color[0]-100.f/255)<1e-7f && fabsf(value.glow_color[1]-50.f/255)<1e-7f);
        saved=value;
        for(i=0;i<sizeof(bad_glow)/sizeof(*bad_glow);i++) {
            snprintf(text,sizeof(text),"%s%s %s #End",prefix,valid,bad_glow[i]);
            CHECK(rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value)!=RF_OK && !memcmp(&value,&saved,sizeof(value)));
        }
        snprintf(text,sizeof(text),"%s%s $Glow: false #End",prefix,valid);
        CHECK(!rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value));
        CHECK(!value.glow && !value.glow_inner && !value.glow_outer && !value.glow_color[0] && !value.glow_color[1] && !value.glow_color[2]);
    }
    {
        const char *bad_impact[]={
            "$Impact Vclips: (\"a\")",
            "$Impact Vclips Radius: (1)",
            "$Impact Vclips: (\"a\" \"b\") $Impact Vclips Radius: (1)",
            "$Impact Vclips: (\"a\") $Impact Vclips Radius: (1 2)",
            "$Impact Vclips: (\"a\" \"b\" \"c\" \"d\") $Impact Vclips Radius: (1 2 3 4)",
            "$Impact Vclips: (\"a\") $Impact Vclips Radius: (-1)",
            "$Impact Vclips: (\"a\") $Impact Vclips Radius: (nan)",
            "$Impact Vclips: (a) $Impact Vclips Radius: (1)",
            "$Impact Vclips: (\"\") $Impact Vclips Radius: (1)",
            "$Impact Vclips: (\"a\") $Impact Vclips: (\"b\") $Impact Vclips Radius: (1)",
            "$Impact Vclips: (\"a\") $Impact Vclips Radius: (1) $Impact Vclips Radius: (2)",
            "$Impact Vclips: (\"a\" $Impact Vclips Radius: (1)",
            "$Impact Vclips: (\"a\") $Impact Vclips Radius: (1 #End"
        };
        snprintf(text,sizeof(text),"%s%s $Impact Vclips Radius: (0 1.5 2) $Impact Sound: \"default\" \"unused\" $Impact Vclips: (\"small\" \"medium\" \"large\") #End",prefix,valid);
        CHECK(!rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value));
        CHECK(value.impact_count==3 && !strcmp(value.impact_vclips[0],"small") && !strcmp(value.impact_vclips[2],"large"));
        CHECK(value.impact_radius[0]==0 && value.impact_radius[1]==1.5f && value.impact_radius[2]==2);
        saved=value;
        for(i=0;i<sizeof(bad_impact)/sizeof(*bad_impact);i++) {
            snprintf(text,sizeof(text),"%s%s %s #End",prefix,valid,bad_impact[i]);
            CHECK(rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value)!=RF_OK && !memcmp(&saved,&value,sizeof(value)));
        }
        snprintf(text,sizeof(text),"%s%s $Impact Vclips: () $Impact Vclips Radius: () #End",prefix,valid);
        CHECK(!rf_weapon_explosive_read(text,(uint32_t)strlen(text),"test",&value) && !value.impact_count);
    }
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);
    {
        rf_vpp tables={0};rf_weapon_primary_definition primary;
        CHECK(!rf_vpp_open(&tables,path));
        CHECK(!rf_weapon_explosive_load(&tables,"Rocket Launcher",128*1024,&value));
        CHECK(value.speed==20 && value.lifetime==15 && fabsf(value.collision_radius-.051f)<1e-6f && value.damage_radius==5 && value.crater_radius==5);
        CHECK(value.glow==1 && value.glow_inner==1 && value.glow_outer==3);
        CHECK(value.impact_count==1 && !strcmp(value.impact_vclips[0],"rocket_impact") && value.impact_radius[0]==1.5f);
        CHECK(fabsf(value.glow_color[0]-100.f/255)<1e-7f && fabsf(value.glow_color[1]-50.f/255)<1e-7f && fabsf(value.glow_color[2]-100.f/255)<1e-7f);
        {
            rf_weapon_flight flight={0};rf_vfx_light_source light,kept;
            CHECK(!rf_weapon_flight_launch(&flight,(float[3]){1,2,3},(float[3]){1,0,0},20,15,.051f));
            CHECK(!rf_weapon_projectile_light(&value,&flight,&light));
            CHECK(light.type==2 && light.profile==0 && light.radius==3);
            CHECK(!memcmp(light.position,flight.position,12) && !memcmp(light.color,value.glow_color,12));
            flight.position[0]=4;CHECK(!rf_weapon_projectile_light(&value,&flight,&light));CHECK(light.position[0]==4);
            kept=light;flight.active=0;
            CHECK(rf_weapon_projectile_light(&value,&flight,&light)==RF_NOT_FOUND && !memcmp(&kept,&light,sizeof(light)));
            flight.active=1;value.glow=0;
            CHECK(rf_weapon_projectile_light(&value,&flight,&light)==RF_NOT_FOUND && !memcmp(&kept,&light,sizeof(light)));
            value.glow=1;flight.position[0]=NAN;
            CHECK(rf_weapon_projectile_light(&value,&flight,&light)==RF_RANGE && !memcmp(&kept,&light,sizeof(light)));
        }
        CHECK(!rf_weapon_primary_load(&tables,"Rocket Launcher",128*1024,&primary));
        CHECK(primary.magazine==6 && primary.damage==400 && primary.damage_kind==3 && primary.fire_seconds==1.25f && primary.reload_seconds==1.7f);
        CHECK(!rf_weapon_explosive_load(&tables,"Grenade",128*1024,&value));
        CHECK(value.speed==10 && value.lifetime==5 && value.collision_radius==.15f && value.damage_radius==8 && value.crater_radius==5);
        CHECK(value.impact_count==1 && !strcmp(value.impact_vclips[0],"rocket_impact") && value.impact_radius[0]==1.5f);
        saved=value;CHECK(rf_weapon_explosive_load(&tables,"Rocket Launcher",1,&value)==RF_RANGE && !memcmp(&value,&saved,sizeof(value)));
        rf_vpp_close(&tables);
    }
    {
        rf_vpp archive={0};rf_level level={0};rf_geometry geometry={0};rf_geometry_collision_world world={0};
        rf_weapon_flight flight={0},before,coarse;rf_weapon_flight_event event,sentinel;
        const float start[3]={0,-10,10},direction[3]={3,0,0};sweep_context context={0};unsigned calls,steps;
        snprintf(path,sizeof(path),"%s/levelsm.vpp",argv[1]);CHECK(!rf_vpp_open(&archive,path));
        CHECK(!rf_level_open(&level,&archive,"glass_house.rfl"));CHECK(!rf_geometry_open(&geometry,&level,1024*1024));
        CHECK(!rf_geometry_collision_world_open(&geometry,8*1024*1024,&world));context.world=&world;
        CHECK(!rf_weapon_flight_launch(&flight,start,direction,20,15,.051f));CHECK(flight.velocity[0]==20);
        before=flight;CHECK(rf_weapon_flight_launch(&flight,start,direction,20,15,.051f)==RF_RANGE && !memcmp(&flight,&before,sizeof(flight)));
        memset(&event,0xa5,sizeof(event));sentinel=event;context.fail=1;
        CHECK(rf_weapon_flight_step(&flight,1,sweep,&context,&event)==RF_IO && !memcmp(&flight,&before,sizeof(flight)) && !memcmp(&event,&sentinel,sizeof(event)));
        context.fail=0;context.bad=1;
        CHECK(rf_weapon_flight_step(&flight,1,sweep,&context,&event)==RF_FORMAT && !memcmp(&flight,&before,sizeof(flight)) && !memcmp(&event,&sentinel,sizeof(event)));
        context.bad=0;CHECK(!rf_weapon_flight_step(&flight,1,sweep,&context,&event));
        CHECK(event.kind==1 && !flight.active && event.contact.room==0);
        CHECK(fabsf(flight.position[0]-15.949f)<1e-4f && fabsf(event.contact.hit.point[0]-16)<1e-4f && event.contact.hit.normal[0]<-.99f);
        coarse=flight;calls=context.calls;CHECK(!rf_weapon_flight_step(&flight,1,sweep,&context,&event) && !event.kind && calls==context.calls);
        CHECK(!rf_weapon_flight_launch(&flight,start,direction,20,15,.051f));
        for(steps=0;flight.active && steps<60;steps++)CHECK(!rf_weapon_flight_step(&flight,1.f/60,sweep,&context,&event));
        CHECK(steps==48 && event.kind==1 && fabsf(flight.position[0]-coarse.position[0])<2e-4f);
        CHECK(fabs(flight.remaining-coarse.remaining)<2e-5);
        CHECK(!rf_weapon_flight_launch(&flight,start,direction,20,.1f,.051f));
        CHECK(!rf_weapon_flight_step(&flight,1,sweep,&context,&event));
        CHECK(event.kind==2 && !flight.active && flight.remaining==0 && fabsf(flight.position[0]-2)<1e-5f);
        calls=context.calls;CHECK(!rf_weapon_flight_step(&flight,1,sweep,&context,&event) && !event.kind && context.calls==calls);
        rf_geometry_collision_world_close(&world);rf_geometry_close(&geometry);rf_vpp_close(&archive);
    }
    puts("PASS: authored-room swept flight, no tunneling,60Hz agreement, expiry and callback rollback");
    puts("PASS: installed rocket/grenade SP flight and blast fields, primary rocket rules, malformed/duplicate rollback");return 0;
}
