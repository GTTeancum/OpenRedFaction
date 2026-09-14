#include "rf/player_weapon.h"
#include <stdlib.h>
#include <string.h>
void rf_player_weapon_close(rf_player_weapon **weapon)
{
    uint32_t i;rf_player_weapon *w;if(!weapon || !(w=*weapon))return;
    for(i=0;i<3;i++)free(w->payloads[i]);
    rf_model_geometry_close(&w->geometry);rf_model_materials_close(&w->materials);
    free(w);*weapon=NULL;
}
int rf_player_weapon_open(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,rf_player_weapon **result)
{
    static const char *const names[3]={"fp_glock_idle.rfa","fp_glock_fire.rfa","fp_glock_reload.rfa"};
    rf_player_weapon *w=NULL;rf_model_file *model=NULL;unsigned char *bones=NULL;
    uint32_t i,j,used=sizeof(rf_player_weapon),scratch=sizeof(rf_model_file)+4+50*56;int status;
    if(!meshes || !motions || !maps || !map_count || !result || *result)return RF_RANGE;
    if(budget<used || budget-used<scratch)return RF_RANGE;
    w=calloc(1,sizeof(*w));model=calloc(1,sizeof(*model));bones=malloc(4+50*56);
    if(!w || !model || !bones){status=RF_IO;goto done;}
    status=rf_model_file_open(model,meshes,"fp_glock.v3c");if(status)goto done;
    for(i=0;i<model->section_count;i++)if(model->sections[i].type==0x424f4e45) {
        if(model->sections[i].size>4+50*56){status=RF_RANGE;goto done;}
        status=rf_vpp_read(meshes,&model->entry,model->sections[i].offset,bones,model->sections[i].size);if(status)goto done;
        status=rf_model_decode_bones(bones,model->sections[i].size,w->bones,50,&w->bone_count);if(status)goto done;
    }
    if(!w->bone_count || !model->lod_count){status=RF_FORMAT;goto done;}
    for(i=0;i<w->bone_count;i++){status=rf_model_bone_transform(w->bones[i].rotation,w->bones[i].position,w->stored[i]);if(status)goto done;}
    status=rf_model_geometry_open(&w->geometry,model,0,budget-used-scratch);if(status)goto done;
    used+=w->geometry.accounted_bytes;
    status=rf_model_materials_open(&w->materials,model,maps,map_count,budget-used-scratch);if(status)goto done;
    w->peak_bytes=used+w->materials.peak_bytes+scratch;used+=w->materials.resident_bytes;
    for(i=0;i<3;i++) {
        rf_motion_track track;
        status=rf_motion_file_open(w->clips+i,motions,names[i]);if(status)goto done;
        if(w->clips[i].entry.size>budget-used-scratch){status=RF_RANGE;goto done;}
        w->payloads[i]=malloc(w->clips[i].entry.size);if(!w->payloads[i]){status=RF_IO;goto done;}
        status=rf_vpp_read(motions,&w->clips[i].entry,0,w->payloads[i],w->clips[i].entry.size);if(status)goto done;
        status=rf_motion_file_bind_memory(w->clips+i,w->payloads[i],w->clips[i].entry.size);if(status)goto done;
        for(j=0;j<w->bone_count;j++){status=rf_motion_file_track(w->clips+i,j,&track);if(status)goto done;}
        w->clips[i].archive=NULL;used+=w->clips[i].entry.size;
    }
    w->resident_bytes=used;if(used+scratch>w->peak_bytes)w->peak_bytes=used+scratch;
    *result=w;w=NULL;status=RF_OK;
done:
    free(bones);free(model);rf_player_weapon_close(&w);return status;
}
