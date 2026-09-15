#include "rf/entity_assets.h"
#include "rf/geometry.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"explosive line %d\n",__LINE__);return 1;}} while(0)
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
int main(int argc,char **argv)
{
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
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);
    {
        rf_vpp tables={0};rf_weapon_primary_definition primary;
        CHECK(!rf_vpp_open(&tables,path));
        CHECK(!rf_weapon_explosive_load(&tables,"Rocket Launcher",128*1024,&value));
        CHECK(value.speed==20 && value.lifetime==15 && fabsf(value.collision_radius-.051f)<1e-6f && value.damage_radius==5 && value.crater_radius==5);
        CHECK(value.glow==1 && value.glow_inner==1 && value.glow_outer==3);
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
