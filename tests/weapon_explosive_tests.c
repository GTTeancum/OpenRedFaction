#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"explosive line %d\n",__LINE__);return 1;}} while(0)
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
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);
    {
        rf_vpp tables={0};rf_weapon_primary_definition primary;
        CHECK(!rf_vpp_open(&tables,path));
        CHECK(!rf_weapon_explosive_load(&tables,"Rocket Launcher",128*1024,&value));
        CHECK(value.speed==20 && value.lifetime==15 && fabsf(value.collision_radius-.051f)<1e-6f && value.damage_radius==5 && value.crater_radius==5);
        CHECK(!rf_weapon_primary_load(&tables,"Rocket Launcher",128*1024,&primary));
        CHECK(primary.magazine==6 && primary.damage==400 && primary.damage_kind==3 && primary.fire_seconds==1.25f && primary.reload_seconds==1.7f);
        CHECK(!rf_weapon_explosive_load(&tables,"Grenade",128*1024,&value));
        CHECK(value.speed==10 && value.lifetime==5 && value.collision_radius==.15f && value.damage_radius==8 && value.crater_radius==5);
        saved=value;CHECK(rf_weapon_explosive_load(&tables,"Rocket Launcher",1,&value)==RF_RANGE && !memcmp(&value,&saved,sizeof(value)));
        rf_vpp_close(&tables);
    }
    puts("PASS: installed rocket/grenade SP flight and blast fields, primary rocket rules, malformed/duplicate rollback");return 0;
}
