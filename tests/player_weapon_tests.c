#include "rf/player_weapon.h"
#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"weapon line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    rf_vpp meshes={0},motions={0},maps[5]={{0}};rf_player_weapon *w=NULL,*other=NULL,*rifle=NULL,*riot=NULL;
    const char *map_names[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    char path[1024];uint32_t i,j;rf_motion_sample sample;
    {
        const float axes[4][3]={{0,0,100},{0,100,0},{0,-100,0},{60,0,80}};
        for(unsigned k=0;k<4;k++) {
            rf_random_state random={123},repeat={123};
            for(unsigned j=0;j<512;j++) {
                float ray[3],other[3];double length=0,dot=0;
                CHECK(rf_weapon_spread_ray(axes[k],3,&random,ray)==RF_OK);
                CHECK(rf_weapon_spread_ray(axes[k],3,&repeat,other)==RF_OK && !memcmp(ray,other,sizeof(ray)));
                for(unsigned n=0;n<3;n++){length+=(double)ray[n]*ray[n];dot+=(double)ray[n]*axes[k][n];}
                CHECK(fabs(length-10000)<.01 && dot/10000>=cos(3*0.017453292519943295)-.000001);
            }
        }
        {rf_random_state random={123};float axis[3]={0,0,100},out[3]={7,8,9},saved[3];memcpy(saved,out,12);
         CHECK(rf_weapon_spread_ray(axis,-1,&random,out)==RF_RANGE && random.value==123 && !memcmp(out,saved,12));
         CHECK(rf_weapon_spread_ray(axis,91,&random,out)==RF_RANGE && random.value==123 && !memcmp(out,saved,12));
         CHECK(rf_weapon_spread_ray(axis,0,&random,out)==RF_OK && random.value==123 && !memcmp(out,axis,12));}
    }
    {
        const char *valid="$Name: \"pistol\" $Flags: (\"semi_automatic\") $Clip Size: 16 8 $Clip Reload Time: 1.1 $Fire Wait: .5 $Damage: 40 $Damage Type: \"bullet\" $Damage Multi: 25 #End";
        const char *duplicate="$Name: \"pistol\" $Clip Size: 16 8 $Clip Size: 8 8";
        const char *bad="$Name: \"pistol\" $Clip Size: -1 8";
        rf_weapon_primary_definition d={0},saved;
        CHECK(rf_weapon_primary_read(valid,(uint32_t)strlen(valid),"pistol",&d)==RF_OK);
        CHECK(d.damage_kind==1 && d.magazine==16 && d.semi_automatic==1 && d.damage==40 && d.fire_seconds==.5f && d.reload_seconds==1.1f);saved=d;
        CHECK(rf_weapon_primary_read(duplicate,(uint32_t)strlen(duplicate),"pistol",&d)==RF_FORMAT && !memcmp(&d,&saved,sizeof(d)));
        CHECK(rf_weapon_primary_read(bad,(uint32_t)strlen(bad),"pistol",&d)==RF_RANGE && !memcmp(&d,&saved,sizeof(d)));
        CHECK(rf_weapon_primary_read(valid,(uint32_t)strlen(valid),"missing",&d)==RF_NOT_FOUND && !memcmp(&d,&saved,sizeof(d)));
        CHECK(d.ai_attack_range==0 && d.ai_spread_degrees==0);
        CHECK(d.projectiles==1 && d.spread_degrees==0 && d.alt_spread_degrees==0);
        {char table[1024];const char *end=strstr(valid,"#End");
         const char *invalid[]={"$Num Projectiles: 0","$Num Projectiles: 33","$Num Projectiles: -1",
             "$Num Projectiles: 4 $Num Projectiles: 4","$Num Projectiles:",
             "$Spread Degrees: -1","$Spread Degrees: 91","$Spread Degrees: 3 $Spread Degrees: 4",
             "$Alt Spread Degrees: -1","$Alt Spread Degrees: 91","$Alt Spread Degrees: 6 $Alt Spread Degrees: 7"};
         snprintf(table,sizeof(table),"%.*s $Num Projectiles: 4 $Spread Degrees: 3 $Spread Degrees Multi: 2.75 $Alt Spread Degrees: 6 $Alt Spread Degrees Multi: 4 #End",(int)(end-valid),valid);
         CHECK(!rf_weapon_primary_read(table,(uint32_t)strlen(table),"pistol",&d));
         CHECK(d.projectiles==4 && d.spread_degrees==3 && d.alt_spread_degrees==6);
         snprintf(table,sizeof(table),"%.*s $Spread Degrees: 3 #End",(int)(end-valid),valid);
         CHECK(!rf_weapon_primary_read(table,(uint32_t)strlen(table),"pistol",&d) && d.alt_spread_degrees==3);
         d=saved;
         for(unsigned k=0;k<sizeof(invalid)/sizeof(*invalid);k++) {
             snprintf(table,sizeof(table),"%.*s %s #End",(int)(end-valid),valid,invalid[k]);
             CHECK(rf_weapon_primary_read(table,(uint32_t)strlen(table),"pistol",&d)!=RF_OK && !memcmp(&d,&saved,sizeof(d)));
         }}

        {char spread[512];const char *end=strstr(valid,"#End");
         snprintf(spread,sizeof(spread),"%.*s $AI Spread Degrees: 3 4 #End",(int)(end-valid),valid);
         CHECK(rf_weapon_primary_read(spread,(uint32_t)strlen(spread),"pistol",&d)==RF_OK && d.ai_spread_degrees==3);d=saved;
         const char *bad_spread[]={"-1 4","3 91","3","3 4 $AI Spread Degrees: 2 2"};
         for(unsigned i=0;i<4;i++) {snprintf(spread,sizeof(spread),"%.*s $AI Spread Degrees: %s #End",(int)(end-valid),valid,bad_spread[i]);
          CHECK(rf_weapon_primary_read(spread,(uint32_t)strlen(spread),"pistol",&d)!=RF_OK && !memcmp(&d,&saved,sizeof(d)));}}

        {char ranged[512];const char *end=strstr(valid,"#End");
         snprintf(ranged,sizeof(ranged),"%.*s $AI attack range: 20 10 #End",(int)(end-valid),valid);
         CHECK(rf_weapon_primary_read(ranged,(uint32_t)strlen(ranged),"pistol",&d)==RF_OK && d.ai_attack_range==20);d=saved;}

        {const char *invalid[]={
            "$Name: \"pistol\" $AI attack range: -1 20",
            "$Name: \"pistol\" $AI attack range: 20",
            "$Name: \"pistol\" $AI attack range: 20 20 $AI attack range: 30 30"};
         for(uint32_t k=0;k<3;k++)CHECK(rf_weapon_primary_read(invalid[k],(uint32_t)strlen(invalid[k]),"pistol",&d)!=RF_OK && !memcmp(&d,&saved,sizeof(d)));}

    }
    {
        rf_weapon_inventory inv={0},saved;rf_weapon_acquire_definition d={0,125,16};uint32_t moved=999;
        inv.owned[3]=1;inv.loaded[3]=10;inv.reserve[0]=3;
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==3 && inv.loaded[3]==13 && inv.reserve[0]==0);
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==0 && inv.loaded[3]==13);
        inv.reserve[0]=10;CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==3 && inv.loaded[3]==16 && inv.reserve[0]==7);
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_OK && moved==0 && inv.reserve[0]==7);
        inv.loaded[3]=17;saved=inv;moved=999;
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_RANGE && moved==999 && !memcmp(&saved,&inv,sizeof(inv)));
        inv.loaded[3]=0;inv.reserve[0]=-1;saved=inv;
        CHECK(rf_weapon_reload_transfer(&inv,&d,3,&moved)==RF_RANGE && !memcmp(&saved,&inv,sizeof(inv)));
    }
    {
        rf_weapon_inventory inv={0},saved;rf_weapon_acquire_definition d={0,125,16};rf_weapon_pickup_grant grant;
        CHECK(rf_weapon_pickup_grant_sp(&inv,&d,3,16,1,&grant)==RF_OK && grant.acquired==1 && grant.rounds==16 && inv.loaded[3]==16);
        CHECK(rf_weapon_pickup_grant_sp(&inv,&d,3,32,0,&grant)==RF_OK && !grant.acquired && grant.rounds==32 && inv.reserve[0]==32);
        inv.reserve[0]=120;CHECK(rf_weapon_pickup_grant_sp(&inv,&d,3,16,1,&grant)==RF_OK && grant.rounds==5 && inv.reserve[0]==125);
        CHECK(rf_weapon_pickup_grant_sp(&inv,&d,3,16,1,&grant)==RF_OK && !grant.rounds && !grant.acquired);
        saved=inv;CHECK(rf_weapon_pickup_grant_sp(&inv,&d,3,-1,1,&grant)==RF_RANGE && !memcmp(&saved,&inv,sizeof(inv)));
        memset(&inv,0,sizeof(inv));inv.reserve[0]=100;
        CHECK(rf_weapon_pickup_grant_sp(&inv,&d,3,2147483647,1,&grant)==RF_OK && grant.rounds==41 && inv.reserve[0]==125 && inv.loaded[3]==16);
    }
    {
        rf_weapon_trigger_rules rules={45,3,6,0};rf_weapon_trigger_state state={0},saved;
        rf_weapon_inventory inv={0};rf_weapon_acquire_definition defs[64]={{0}};uint32_t i,event,shots=0;
        defs[3]=(rf_weapon_acquire_definition){0,200,42};inv.owned[3]=1;inv.loaded[3]=42;
        for(i=0;i<60;i++) {
            CHECK(rf_weapon_trigger_step(&state,&rules,i==0,0,inv.loaded[3],&event)==RF_OK);
            CHECK((event==1)==(i==0 || i==6 || i==12));
            if(event==1){CHECK(rf_weapon_consume_shot(&inv,defs,64,3)==RF_OK);shots++;}
        }
        CHECK(shots==3 && inv.loaded[3]==39); /* Release still completes accepted burst. */
        memset(&state,0,sizeof(state));inv.loaded[3]=42;shots=0;
        for(i=0;i<90;i++) {
            CHECK(rf_weapon_trigger_step(&state,&rules,1,0,inv.loaded[3],&event)==RF_OK);
            CHECK((event==1)==(i==0 || i==6 || i==12 || i==45 || i==51 || i==57));
            if(event==1){CHECK(rf_weapon_consume_shot(&inv,defs,64,3)==RF_OK);shots++;}
        }
        CHECK(shots==6 && inv.loaded[3]==36); /* Burst-start cooldown, no extra bullets. */

        memset(&state,0,sizeof(state));inv.loaded[3]=2;shots=0;
        for(i=0;i<20;i++) {
            CHECK(rf_weapon_trigger_step(&state,&rules,i==0,0,inv.loaded[3],&event)==RF_OK);
            if(event==1){CHECK(rf_weapon_consume_shot(&inv,defs,64,3)==RF_OK);shots++;}
            if(i==12)CHECK(event==2 && !state.remaining);
        }
        CHECK(shots==2 && !inv.loaded[3]);
        memset(&state,0,sizeof(state));
        CHECK(rf_weapon_trigger_step(&state,&rules,1,0,42,&event)==RF_OK && event==1);
        CHECK(rf_weapon_trigger_step(&state,&rules,0,1,41,&event)==RF_OK && !event && !state.remaining);
        for(i=0;i<30;i++)CHECK(rf_weapon_trigger_step(&state,&rules,0,0,41,&event)==RF_OK && !event);
        rules=(rf_weapon_trigger_rules){30,1,0,1};memset(&state,0,sizeof(state));
        for(i=0;i<90;i++){CHECK(rf_weapon_trigger_step(&state,&rules,1,0,16,&event)==RF_OK);CHECK(event==(i==0));}
        CHECK(rf_weapon_trigger_step(&state,&rules,0,0,15,&event)==RF_OK && !event);
        CHECK(rf_weapon_trigger_step(&state,&rules,1,1,15,&event)==RF_OK && !event);
        CHECK(rf_weapon_trigger_step(&state,&rules,1,0,15,&event)==RF_OK && !event); /* Blocked edge consumed. */
        saved=state;event=999;rules.burst_count=0;
        CHECK(rf_weapon_trigger_step(&state,&rules,1,0,15,&event)==RF_RANGE && event==999 && !memcmp(&state,&saved,sizeof(state)));
        printf("Trigger PASS burst cadence, partial magazine, cancellation, semi-auto and blocked edges\n");
    }
    {
        uint32_t remainder=0,used=0,total=0,tick;int32_t loaded=100;
        for(tick=0;tick<300;tick++) {
            CHECK(!rf_weapon_charge_step(&remainder,100,150,tick%2==0,&loaded,&used));total+=used;
            CHECK(loaded==100-(int32_t)(((tick+2)/2)*100/150));
        }
        CHECK(total==100 && loaded==0 && remainder==0);
        CHECK(!rf_weapon_charge_step(&remainder,100,150,1,&loaded,&used) && !used);
        used=999;CHECK(rf_weapon_charge_step(&remainder,100,0,1,&loaded,&used)==RF_RANGE && used==999 && remainder==0);
        loaded=101;CHECK(rf_weapon_charge_step(&remainder,100,150,1,&loaded,&used)==RF_RANGE && loaded==101 && used==999);
        puts("Charge PASS full drain, intermittent holds, exhausted and malformed inputs");
    }
    CHECK(argc==2);snprintf(path,sizeof(path),"%s/meshes.vpp",argv[1]);CHECK(rf_vpp_open(&meshes,path)==RF_OK);
    snprintf(path,sizeof(path),"%s/motions.vpp",argv[1]);CHECK(rf_vpp_open(&motions,path)==RF_OK);
    for(i=0;i<5;i++){snprintf(path,sizeof(path),"%s/%s",argv[1],map_names[i]);CHECK(rf_vpp_open(maps+i,path)==RF_OK);}
    CHECK(rf_player_weapon_open(&meshes,&motions,maps,5,1024*1024,&w)==RF_OK);
    {
        rf_vpp tables={0};rf_weapon_primary_definition d;
        snprintf(path,sizeof(path),"%s/tables.vpp",argv[1]);CHECK(rf_vpp_open(&tables,path)==RF_OK);
        CHECK(rf_weapon_primary_load(&tables,"12mm handgun",128*1024,&d)==RF_OK);
        {
            rf_item_definition item,saved;
            CHECK(rf_item_definition_load(&tables,"Handgun",128*1024,&item)==RF_OK);
            CHECK(!strcmp(item.mesh,"weapon_ultorgun.v3d") && !strcmp(item.weapon,"12mm handgun") && item.count==16 && item.gives_weapon==1 && item.mesh_kind==1 && !item.flags);
            CHECK(rf_item_definition_load(&tables,"12mm_ammo",128*1024,&item)==RF_OK);
            CHECK(!strcmp(item.weapon,"12mm handgun") && item.count==32 && !item.gives_weapon && item.mesh_kind==1);saved=item;
            CHECK(rf_item_definition_load(&tables,"missing",128*1024,&item)==RF_NOT_FOUND && !memcmp(&item,&saved,sizeof(item)));
            CHECK(!rf_item_definition_load(&tables,"riot_stick_battery",128*1024,&item));
            CHECK(item.count==100 && !item.gives_weapon && item.mesh_kind==1);
            {rf_model_file model;rf_static_render_resource resource={0};rf_model_materials material={0};char compiled[64];
             CHECK(!rf_model_compiled_filename(item.mesh,compiled,".v3m"));
             CHECK(!rf_model_file_open(&model,&meshes,compiled));
             CHECK(!rf_static_render_resource_open(&model,2*1024*1024-sizeof(model)-sizeof(resource),&resource));
             CHECK(!rf_model_materials_open_records(&material,resource.materials,resource.material_count,maps,5,2*1024*1024-sizeof(model)-sizeof(resource)-resource.allocated_bytes));
             CHECK(resource.allocated_bytes+material.peak_bytes+sizeof(model)+sizeof(resource)<=2*1024*1024);
             rf_model_materials_close(&material);rf_static_render_resource_close(&resource);}
            item=saved;
            {const char *fixture="$Class Name: \"test\" $V3D Filename: \"test.v3d\" $V3D Type: \"static\" $Count Single: 7 $Count: 3 $Count Multi: 99 $Flags: (\"no_pickup\")";
             CHECK(rf_item_definition_read(fixture,(uint32_t)strlen(fixture),"test",&item)==RF_OK && item.count==7 && item.flags==1);}
        }
        {
            rf_weapon_view_definition view,saved;
            const char *fixture="$Name: \"test\" $1st Person Mesh: \"test.v3d\" +State: \"idle\" \"idle.mvf\" +Action: \"fire\" \"fire.mvf\" \"\" +Action: \"reload\" \"reload.mvf\" \"\"";
            CHECK(rf_weapon_view_read(fixture,(uint32_t)strlen(fixture),"test",&view)==RF_OK);
            CHECK(!strcmp(view.mesh,"test.v3c") && !strcmp(view.clips[2],"reload.rfa"));saved=view;
            {char duplicate[1024];snprintf(duplicate,sizeof(duplicate),"%s +State: \"idle\" \"other.mvf\"",fixture);
             CHECK(rf_weapon_view_read(duplicate,(uint32_t)strlen(duplicate),"test",&view)==RF_FORMAT && !memcmp(&view,&saved,sizeof(view)));}

            CHECK(rf_weapon_view_read(fixture,(uint32_t)strlen(fixture),"missing",&view)==RF_NOT_FOUND && !memcmp(&view,&saved,sizeof(view)));
            CHECK(rf_weapon_view_read(fixture,(uint32_t)strlen(fixture)-20,"test",&view)!=RF_OK && !memcmp(&view,&saved,sizeof(view)));
            CHECK(rf_weapon_view_load(&tables,"12mm handgun",128*1024,&view)==RF_OK);
            CHECK(!strcmp(view.mesh,"fp_glock.v3c") && !strcmp(view.clips[1],"fp_glock_fire.rfa"));
            CHECK(rf_weapon_view_load(&tables,"Assault Rifle",128*1024,&view)==RF_OK);
            CHECK(!strcmp(view.mesh,"fp_aslt_rfl.v3c") && !strcmp(view.clips[1],"fp_aslt_rfl_fire_burst.rfa"));
            CHECK(view.alt_loop && !strcmp(view.clips[3],"fp_aslt_rfl_fire.rfa"));
            CHECK(rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,1024*1024+8192,&rifle)==RF_OK);
            CHECK(rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,rifle->resident_bytes-1,&other)==RF_RANGE && !other);
            memset(view.mesh,'x',64);
            CHECK(rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,2*1024*1024,&other)==RF_RANGE && !other);
            CHECK(!rf_weapon_view_load(&tables,"Shotgun",128*1024,&view));
            CHECK(!strcmp(view.mesh,"fp_shotgun.v3c") && !strcmp(view.clips[1],"fp_shotgun_fire_slow.rfa"));
            CHECK(!rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,1024*1024,&other));
            CHECK(other->clip_count==4 && !view.alt_loop && !strcmp(view.clips[3],"fp_shotgun_fire_fast.rfa") && other->peak_bytes<=1024*1024);
            printf("Shotgun resources: resident=%u peak=%u\n",other->resident_bytes,other->peak_bytes);
            for(unsigned clip=0;clip<4;clip++) {
                CHECK(!rf_player_weapon_step(other,(int32_t)clip,1.0f/60));
                for(unsigned tick=0;tick<180;tick++)CHECK(!rf_player_weapon_step(other,-1,1.0f/60));
                CHECK(other->current==0);
            }
            rf_player_weapon_close(&other);
            {rf_weapon_primary_definition shotgun;
             CHECK(!rf_weapon_primary_load(&tables,"Shotgun",128*1024,&shotgun));
             CHECK(shotgun.projectiles==4 && shotgun.spread_degrees==3 && shotgun.alt_spread_degrees==6);
             CHECK(shotgun.magazine==8 && shotgun.reload_seconds==2 && shotgun.fire_seconds==1.5f && shotgun.alt_fire_seconds==.225f);
             CHECK(shotgun.damage==40 && shotgun.alt_damage==40 && shotgun.damage_kind==1 && shotgun.ai_spread_degrees==5);}
            CHECK(rf_weapon_view_load(&tables,"Riot Stick",128*1024,&view)==RF_OK);
            CHECK(!strcmp(view.clips[3],"fp_riot_attack_taserB.rfa"));
            CHECK(!rf_player_weapon_open_view(&meshes,&motions,maps,5,&view,1024*1024,&riot));
            CHECK(riot->clip_count==4 && riot->peak_bytes<=1024*1024);
            {rf_weapon_primary_definition baton;
             CHECK(!rf_weapon_primary_load(&tables,"Riot Stick",128*1024,&baton));
             CHECK(baton.alt_fire_seconds==.5f && baton.alt_damage==120 && baton.drain_seconds==2.5f && baton.reload_drain_seconds==1.3f && baton.magazine==100 && baton.ai_attack_range==2.6f);}
        }
        {rf_weapon_primary_definition rifle,saved;
         CHECK(rf_weapon_primary_load(&tables,"Assault Rifle",128*1024,&rifle)==RF_OK);
         CHECK(rifle.burst_count==3 && rifle.burst_seconds==.1f && rifle.magazine==42 && rifle.fire_seconds==.75f && rifle.damage==60 && rifle.damage_kind==2 && rifle.ai_attack_range==30 && rifle.ai_spread_degrees==2);
         saved=rifle;
         {const char *bad="$Name: \"test\" $Clip Size: 42 42 $Clip Reload Time: 1.35 $Fire Wait: .75 $Damage: 60 $Damage Type: \"bullet\" $Burst Mode: true +Burst Count: 3";
          CHECK(rf_weapon_primary_read(bad,(uint32_t)strlen(bad),"test",&rifle)==RF_FORMAT && !memcmp(&rifle,&saved,sizeof(rifle)));}
         {const char *alt="$Name: \"test\" $Clip Size: 42 42 $Clip Reload Time: 1.35 $Fire Wait: .75 $Damage: 60 $Damage Type: \"bullet\" $Burst Mode: true +Burst Count: 3 +Burst Delay: .1 +Burst Alt Fire: true";
          char duplicate[1024];
          CHECK(rf_weapon_primary_read(alt,(uint32_t)strlen(alt),"test",&rifle)==RF_OK && rifle.burst_count==1 && rifle.burst_seconds==0);saved=rifle;
          snprintf(duplicate,sizeof(duplicate),"%s +Burst Count: 4",alt);
          CHECK(rf_weapon_primary_read(duplicate,(uint32_t)strlen(duplicate),"test",&rifle)==RF_FORMAT && !memcmp(&rifle,&saved,sizeof(rifle)));}
         CHECK(d.burst_count==1 && d.burst_seconds==0);
        }
        rf_vpp_close(&tables);
        CHECK(d.damage_kind==1 && d.magazine==16 && d.semi_automatic==1 && d.damage==40 && d.fire_seconds==.5f && d.reload_seconds==1.1f);CHECK(d.ai_attack_range==20 && d.ai_spread_degrees==3);
        printf("Primary definition PASS magazine=%u semi=%u damage=%g reload=%g fire=%g\n",d.magazine,d.semi_automatic,d.damage,d.reload_seconds,d.fire_seconds);
    }
    CHECK(w->bone_count && w->geometry.vertex_count && w->materials.count && w->peak_bytes<=1024*1024);
    CHECK(rf_player_weapon_open(&meshes,&motions,maps,5,w->resident_bytes-1,&other)==RF_RANGE && !other);
    rf_vpp_close(&meshes);rf_vpp_close(&motions);for(i=0;i<5;i++)rf_vpp_close(maps+i);
    CHECK(!rf_player_weapon_step(riot,3,1.0f/60));
    for(i=0;i<240;i++)CHECK(!rf_player_weapon_step(riot,-1,1.0f/60) && riot->current==3);
    CHECK(!rf_player_weapon_step(riot,0,1.0f/60) && riot->current==0 && riot->playback.completion.active.count==1);
    rf_player_weapon_close(&riot);CHECK(!riot);
    {
        uint32_t ticks;
        CHECK(rf_player_weapon_step(rifle,0,0)==RF_OK);
        CHECK(rf_player_weapon_step(rifle,1,.08f)==RF_OK && rifle->current==1);
        CHECK(rf_player_weapon_step(rifle,2,.2f)==RF_OK && rifle->current==2);
        for(ticks=0;ticks<600 && rifle->current!=0;ticks++)CHECK(rf_player_weapon_step(rifle,-1,1.0f/60)==RF_OK);
        CHECK(rifle->current==0 && ticks<600);
        printf("Assault resource PASS bones=%u vertices=%u resident=%u peak=%u reload-return=%u\n",rifle->bone_count,rifle->geometry.vertex_count,rifle->resident_bytes,rifle->peak_bytes,ticks);
        CHECK(rf_player_weapon_step(rifle,3,0)==RF_OK && rifle->current==3);
        for(ticks=0;ticks<180;ticks++)CHECK(rf_player_weapon_step(rifle,-1,1.0f/60)==RF_OK && rifle->current==3);
        CHECK(rf_player_weapon_step(rifle,0,0)==RF_OK && rifle->current==0);
        rf_player_weapon_close(&rifle);rf_player_weapon_close(&rifle);CHECK(!rifle);
    }
    for(i=0;i<3;i++)for(j=0;j<w->bone_count;j++) {
        rf_motion_track t;CHECK(rf_motion_file_track(w->clips+i,j,&t)==RF_OK);
        CHECK(rf_motion_file_sample(w->clips+i,j,t.envelope.start_tick,1,&sample)==RF_OK);
        CHECK(rf_motion_file_sample(w->clips+i,j,t.envelope.end_tick,1,&sample)==RF_OK);
    }
    {
        float idle[50][12],fire[50][12];uint32_t ticks;
        CHECK(rf_player_weapon_step(w,0,0)==RF_OK);memcpy(idle,w->pose,sizeof(idle));
        CHECK(rf_player_weapon_step(w,1,.08f)==RF_OK && w->current==1);memcpy(fire,w->pose,sizeof(fire));
        CHECK(memcmp(idle,fire,w->bone_count*48));
        CHECK(rf_player_weapon_step(w,2,.2f)==RF_OK && w->current==2);
        CHECK(memcmp(fire,w->pose,w->bone_count*48));
        for(ticks=0;ticks<600 && w->current!=0;ticks++)CHECK(rf_player_weapon_step(w,-1,1.0f/60)==RF_OK);
        CHECK(w->current==0 && ticks<600);
        for(i=0;i<50;i++){CHECK(rf_player_weapon_step(w,1,0)==RF_OK);CHECK(rf_player_weapon_step(w,2,0)==RF_OK);}
        CHECK(w->resources[0].references==0 && w->resources[1].references==0 && w->resources[2].references==1);
        CHECK(rf_player_weapon_step(w,3,0)==RF_RANGE);
        printf("Playback PASS reload-return=%u frames repeated-actions=100\n",ticks);
    }
    printf("PASS bones=%u vertices=%u materials=%u resident=%u peak=%u\n",w->bone_count,w->geometry.vertex_count,w->materials.count,w->resident_bytes,w->peak_bytes);
    rf_player_weapon_close(&w);rf_player_weapon_close(&w);CHECK(!w);return 0;
}
