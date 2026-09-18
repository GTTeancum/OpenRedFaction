#ifndef RF_SCENE_SUBMARINE_RESOURCES_INC
#define RF_SCENE_SUBMARINE_RESOURCES_INC
#include <stdio.h>
#include "scene_vehicle_resources.inc"
#include "scene_driller_cockpit.inc"
#include "scene_submarine_weapon.inc"
/* Installed sub/Sub_Mini01/sub.vfx resources. This owns no physics, ammo or
 * homing state. Both authored launch tags are retained; firing policy decides
 * which tube to use. All owners retain pixels/geometry after archives close. */
typedef struct scene_submarine_resources {
    scene_driller_resources *chassis,*torpedo;
    scene_driller_cockpit *cockpit;
    int32_t primary[2],eye,water;
    uint32_t resident_bytes,peak_bytes;
} scene_submarine_resources;
static void scene_submarine_resources_close(scene_submarine_resources **out)
{
    scene_submarine_resources *o;
    if(!out || !(o=*out))return;
    scene_driller_resources_close(&o->chassis);
    scene_driller_resources_close(&o->torpedo);
    scene_driller_cockpit_close(&o->cockpit);free(o);*out=NULL;
}
static int scene_submarine_resources_open(const char *tables_path,rf_vpp *meshes,
    rf_vpp *maps,uint32_t map_count,uint32_t budget,scene_submarine_resources **out)
{
    scene_submarine_resources *o=NULL;
    uint32_t used,i,peak,stage=1;int status;
    if(!tables_path || !meshes || !maps || !map_count || !out || *out ||
       budget<sizeof(*o)+sizeof(scene_driller_resources))return RF_RANGE;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;
    o->primary[0]=o->primary[1]=o->eye=o->water=-1;used=sizeof(*o);
    status=scene_vehicle_resources_open(tables_path,"sub","interface_1",meshes,maps,map_count,budget-used,&o->chassis);
    if(status)goto fail;
    o->peak_bytes=used+o->chassis->peak_bytes;used+=o->chassis->resident_bytes;
    stage=2;
    if(o->chassis->physics.authored.movement_index!=7 ||
       (o->chassis->physics.authored.flags&0x440u)!=0x440u){status=RF_FORMAT;goto fail;}
    for(i=0;i<o->chassis->tags.count;i++){
        const rf_model_attachment *t=o->chassis->tags.items+i;
        if(!strcmp(t->name,"primary_1"))o->primary[0]=(int32_t)i;
        if(!strcmp(t->name,"primary_2"))o->primary[1]=(int32_t)i;
        if(!strcmp(t->name,"eye"))o->eye=(int32_t)i;
        if(!strcmp(t->name,"water"))o->water=(int32_t)i;
    }
    if(o->primary[0]<0 || o->primary[1]<0 || o->eye<0 || o->water<0){status=RF_NOT_FOUND;goto fail;}
    stage=3;
    status=scene_vehicle_cockpit_open("sub.vfx",meshes,maps,map_count,budget-used,&o->cockpit);
    if(status)goto fail;
    peak=used+o->cockpit->peak_bytes;if(peak>o->peak_bytes)o->peak_bytes=peak;
    used+=o->cockpit->resident_bytes;
    stage=4;
    status=scene_submarine_weapon_model_open(meshes,maps,map_count,budget-used,&o->torpedo);
    if(status)goto fail;
    peak=used+o->torpedo->peak_bytes;if(peak>o->peak_bytes)o->peak_bytes=peak;
    o->resident_bytes=used+o->torpedo->resident_bytes;
    *out=o;return RF_OK;
fail:
    fprintf(stderr,"SUB_RESOURCE_FAIL stage=%u status=%d used=%u budget=%u\n",stage,status,used,budget);
    scene_submarine_resources_close(&o);return status;
}
#endif
