#ifndef RF_SCENE_FIGHTER_RESOURCES_INC
#define RF_SCENE_FIGHTER_RESOURCES_INC
#include "scene_vehicle_resources.inc"
#include "scene_driller_cockpit.inc"
#include "scene_vehicle_aim.inc"
/* Exact installed Fighter01 resources. Weapon simulation and thruster/corona
 * effects are separate. No substituted cockpit or inferred launch tags.
 * fighter01.vfx contains21 Scene Root meshes plus2 DMMY transform nodes;
 * the shared geometry loader retains both markers without drawing them. */
typedef struct scene_fighter_resources {
    scene_driller_resources *chassis;
    scene_driller_cockpit *cockpit;
    rf_entity_eye_limits eye_limits;
    int32_t primary,secondary[2],muzzle;
    uint32_t resident_bytes,peak_bytes;
} scene_fighter_resources;
static void scene_fighter_resources_close(scene_fighter_resources **out)
{
    scene_fighter_resources *o;if(!out || !(o=*out))return;
    scene_driller_resources_close(&o->chassis);scene_driller_cockpit_close(&o->cockpit);
    free(o);*out=NULL;
}
static int scene_fighter_resources_open(const char *tables_path,rf_vpp *meshes,
    rf_vpp *maps,uint32_t map_count,uint32_t budget,scene_fighter_resources **out)
{
    scene_fighter_resources *o=NULL;rf_vpp tables={0};rf_vpp_entry entry;
    uint32_t used,i,peak;int status;
    if(!tables_path || !meshes || !maps || !map_count || !out || *out || budget<sizeof(*o))return RF_RANGE;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;used=sizeof(*o);
    o->primary=o->secondary[0]=o->secondary[1]=o->muzzle=-1;
    status=scene_vehicle_resources_open(tables_path,"Fighter01","interface_1",meshes,maps,map_count,budget-used,&o->chassis);
    if(status)goto fail;
    o->peak_bytes=used+o->chassis->peak_bytes;used+=o->chassis->resident_bytes;
    if(o->chassis->physics.authored.movement_index!=9 ||
       (o->chassis->physics.authored.flags&0x802u)!=0x802u){status=RF_FORMAT;goto fail;}
    for(i=0;i<o->chassis->tags.count;i++){
        const rf_model_attachment *t=o->chassis->tags.items+i;
        if(!strcmp(t->name,"primary_1"))o->primary=(int32_t)i;
        if(!strcmp(t->name,"secondary_1"))o->secondary[0]=(int32_t)i;
        if(!strcmp(t->name,"secondary_2"))o->secondary[1]=(int32_t)i;
        if(!strcmp(t->name,"muzzle_1"))o->muzzle=(int32_t)i;
    }
    if(o->primary<0 || o->secondary[0]<0 || o->secondary[1]<0 || o->muzzle<0){status=RF_NOT_FOUND;goto fail;}
    status=rf_vpp_open(&tables,tables_path);if(status)goto fail;
    status=rf_vpp_find(&tables,"entity.tbl",&entry);if(status)goto fail;
    status=scene_vehicle_aim_limits_load(&tables,"Fighter01",budget-used,&o->eye_limits);if(status)goto fail;
    peak=used+entry.size;if(peak>o->peak_bytes)o->peak_bytes=peak;
    rf_vpp_close(&tables);
    status=scene_vehicle_cockpit_open("fighter01.vfx",meshes,maps,map_count,budget-used,&o->cockpit);
    if(status)goto fail;
    peak=used+o->cockpit->peak_bytes;if(peak>o->peak_bytes)o->peak_bytes=peak;
    o->resident_bytes=used+o->cockpit->resident_bytes;*out=o;return RF_OK;
fail:
    rf_vpp_close(&tables);scene_fighter_resources_close(&o);return status;
}
#endif
