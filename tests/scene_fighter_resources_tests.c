#include <stdio.h>
#include "../src/diagnostic/scene_fighter_resources.inc"
#include "../src/diagnostic/scene_fighter_weapon.inc"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Fighter resources line%d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    rf_vpp meshes={0},maps[4]={{0}};scene_fighter_resources *o=NULL;
    const float basis[9]={1,0,0,0,1,0,0,0,1},position[3]={0,5,0};
    float pose[12];char path[128];uint32_t i;int status;
    CHECK(!rf_vpp_open(&meshes,"Installed_Game/meshes.vpp"));
    for(i=0;i<4;i++){snprintf(path,sizeof(path),"Installed_Game/maps%u.vpp",i+1);CHECK(!rf_vpp_open(maps+i,path));}
    CHECK(scene_fighter_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,1,&o)==RF_RANGE && !o);
    status=scene_fighter_resources_open("Installed_Game/tables.vpp",&meshes,maps,4,8*1024*1024,&o);
    if(status)fprintf(stderr,"FIGHTER_RESOURCE_OPEN status=%d\n",status);
    CHECK(!status);
    CHECK(!strcmp(o->chassis->model,"Fighter01.v3m"));
    CHECK(o->chassis->physics.authored.mass==1500 && o->chassis->vitals.health==900);
    CHECK(o->chassis->physics.authored.use_kind==1 && o->chassis->physics.authored.use_radius==5);
    CHECK(o->chassis->movement.speed==20 && o->chassis->movement.acceleration==10);
    CHECK(o->chassis->rotation.maximum_velocity==4 && o->chassis->rotation.acceleration==4);
    CHECK(o->chassis->render.sphere_count==2 && o->chassis->tags.count==18);
    CHECK(o->chassis->seat==11 && o->primary==0 && o->secondary[0]==1 && o->secondary[1]==2 && o->muzzle==12);
    CHECK(fabsf(o->eye_limits.minimum[0]+70*.01745329238474369f)<1e-5f);
    CHECK(fabsf(o->eye_limits.maximum[0]-70*.01745329238474369f)<1e-5f);
    CHECK(o->eye_limits.minimum[1]==0 && o->eye_limits.maximum[1]==0);
    CHECK(o->cockpit->endpoint==20 && o->cockpit->geometry->count==21);
    CHECK(o->cockpit->geometry->dummy_count==2);
    CHECK(!strcmp(o->cockpit->geometry->dummies[0]->name,"chaingun_1"));
    CHECK(!strcmp(o->cockpit->geometry->dummies[1]->name,"muzzle_1"));
    for(i=0;i<2;i++){
        const rf_vfx_dummy *d=o->cockpit->geometry->dummies[i];uint32_t j;
        CHECK(!strcmp(d->parent,"Scene Root") && d->count==21 && !d->flag && d->poses);
        CHECK(d->allocated_bytes==sizeof(*d)+21*28);
        for(j=0;j<21*7;j++)CHECK(isfinite(d->poses[j]));
    }
    {rf_vfx_directory dir={0};rf_vfx_dummy *d=NULL;unsigned char *raw=NULL;uint32_t n;
     CHECK(!rf_vfx_directory_open(&meshes,"fighter01.vfx",65536,&dir));
     for(n=0;n<dir.count;n++)if(dir.chunks[n].type==0x594d4d44u)break;
     CHECK(n<dir.count);raw=malloc(dir.chunks[n].bytes);CHECK(raw);
     CHECK(!rf_vfx_chunk_read(&dir,n,0,raw,dir.chunks[n].bytes));
     CHECK(rf_vfx_dummy_open(raw,dir.chunks[n].bytes-1,dir.header.version,65536,&d)==RF_FORMAT && !d);
     CHECK(rf_vfx_dummy_open(raw,dir.chunks[n].bytes,dir.header.version,1,&d)==RF_RANGE && !d);
     /* Poison one actual sample, preserving its structural framing. */
     raw[dir.chunks[n].bytes-4]=0;raw[dir.chunks[n].bytes-3]=0;
     raw[dir.chunks[n].bytes-2]=0xc0;raw[dir.chunks[n].bytes-1]=0x7f;
     CHECK(rf_vfx_dummy_open(raw,dir.chunks[n].bytes,dir.header.version,65536,&d)==RF_FORMAT && !d);
     free(raw);rf_vfx_directory_close(&dir);
    }
    CHECK(o->peak_bytes>=o->resident_bytes && o->peak_bytes<=8*1024*1024);
    printf("FIGHTER_RESOURCES mass=1500 speed=20 acceleration=10 use=1 movement=9 health=900 spheres=2 tags=18 resident=%u peak=%u hull=%u cockpit=%u\n",
        o->resident_bytes,o->peak_bytes,o->chassis->resident_bytes,o->cockpit->resident_bytes);
    rf_vpp_close(&meshes);for(i=0;i<4;i++)rf_vpp_close(maps+i);
    CHECK(!scene_driller_seat_pose(o->chassis,position,basis,pose));
    CHECK(!rf_static_model_tag_place(&o->chassis->tags,o->secondary[1],basis,position,pose));
    {int32_t indices[4]={o->chassis->seat,o->muzzle,o->secondary[0],o->secondary[1]};uint32_t n;
     for(n=0;n<4;n++){
        CHECK(!rf_static_model_tag_place(&o->chassis->tags,indices[n],basis,position,pose));
        printf("FIGHTER_TAG %s position %.7g %.7g %.7g forward %.7g %.7g %.7g\n",
            o->chassis->tags.items[indices[n]].name,pose[9],pose[10],pose[11],pose[6],pose[7],pose[8]);
     }}
    {
        rf_vpp tables={0};scene_fighter_weapon_state weapon;
        const float orientations[2][9]={{1,0,0,0,1,0,0,0,1},{0,0,-1,0,1,0,1,0,0}};
        uint32_t orientation,side,k,fired;float primary[12],secondary[12];
        CHECK(!rf_vpp_open(&tables,"Installed_Game/tables.vpp"));
        CHECK(!scene_fighter_weapon_open(&tables,&o->chassis->tags,"muzzle_1","secondary_1",2*1024*1024,&weapon));
        rf_vpp_close(&tables);
        for(orientation=0;orientation<2;orientation++)for(side=0;side<2;side++){
            scene_fighter_weapon_state shot=weapon;
            CHECK(!rf_static_model_tag_place(&o->chassis->tags,o->muzzle,orientations[orientation],position,primary));
            CHECK(!rf_static_model_tag_place(&o->chassis->tags,o->secondary[side],orientations[orientation],position,secondary));
            /* Both actual secondary attachment forwards point down, including
             * a yawed host. They identify the launch position, not gun aim. */
            CHECK(secondary[7]<-.999f && fabsf(secondary[6])<.001f && fabsf(secondary[8])<.001f);
            for(k=0;k<3;k++)CHECK(fabsf(primary[6+k]-orientations[orientation][6+k])<.001f);
            shot.rocket_muzzle=o->secondary[side];shot.rocket_reserve=20;
            CHECK(!scene_fighter_weapon_fire(&shot,&o->chassis->tags,position,orientations[orientation],primary+6,
                1,2,1,1,1,1.f/60,NULL,NULL,&fired));
            CHECK(fired && shot.rocket_reserve==19 && shot.rockets[0].flight.active);
            for(k=0;k<3;k++){
                CHECK(fabsf(shot.rockets[0].flight.position[k]-secondary[9+k])<1e-6f);
                CHECK(fabsf(shot.rockets[0].flight.velocity[k]-primary[6+k]*weapon.rocket.speed)<.001f);
            }
        }
    }
    scene_fighter_resources_close(&o);CHECK(!o);
    return 0;
}
