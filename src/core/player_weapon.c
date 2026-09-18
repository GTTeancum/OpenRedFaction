#include "rf/player_weapon.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
void rf_player_weapon_close(rf_player_weapon **weapon)
{
    uint32_t i;rf_player_weapon *w;if(!weapon || !(w=*weapon))return;
    for(i=0;i<4;i++)free(w->payloads[i]);
    rf_model_geometry_close(&w->geometry);rf_model_materials_close(&w->materials);
    free(w);*weapon=NULL;
}
int rf_player_weapon_open_view(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    const rf_weapon_view_definition *definition,uint32_t budget,rf_player_weapon **result)
{
    rf_player_weapon *w=NULL;rf_model_file *model=NULL;unsigned char *bones=NULL;
    uint32_t i,j,used=sizeof(rf_player_weapon),scratch=sizeof(rf_model_file)+4+50*56;int status;
    if(!meshes || !motions || !maps || !map_count || !definition || !result || *result)return RF_RANGE;
    if(!memchr(definition->mesh,0,64) || !definition->mesh[0])return RF_RANGE;
    for(i=0;i<4;i++)if(!memchr(definition->clips[i],0,64) || (i<2 && !definition->clips[i][0]))return RF_RANGE;
    if(budget<used || budget-used<scratch)return RF_RANGE;
    w=calloc(1,sizeof(*w));model=calloc(1,sizeof(*model));bones=malloc(4+50*56);
    if(!w || !model || !bones){status=RF_IO;goto done;}
    w->clip_count=definition->clips[3][0]?4:definition->clips[2][0]?3:2;
    status=rf_model_file_open(model,meshes,definition->mesh);if(status)goto done;
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
    for(i=0;i<w->clip_count;i++) {
        rf_motion_track track;
        if(!definition->clips[i][0])continue;
        status=rf_motion_file_open(w->clips+i,motions,definition->clips[i]);if(status)goto done;
        if(w->clips[i].entry.size>budget-used-scratch){status=RF_RANGE;goto done;}
        w->payloads[i]=malloc(w->clips[i].entry.size);if(!w->payloads[i]){status=RF_IO;goto done;}
        status=rf_vpp_read(motions,&w->clips[i].entry,0,w->payloads[i],w->clips[i].entry.size);if(status)goto done;
        status=rf_motion_file_bind_memory(w->clips+i,w->payloads[i],w->clips[i].entry.size);if(status)goto done;
        for(j=0;j<w->bone_count;j++){status=rf_motion_file_track(w->clips+i,j,&track);if(status)goto done;}
        status=rf_motion_file_track(w->clips+i,0,&track);if(status)goto done;
        w->resources[i].comparison=track.envelope;w->resources[i].looping=i==0 || (i==3 && definition->alt_loop);
        w->clips[i].archive=NULL;used+=w->clips[i].entry.size;
    }
    w->resident_bytes=used;if(used+scratch>w->peak_bytes)w->peak_bytes=used+scratch;
    *result=w;w=NULL;status=RF_OK;
done:
    free(bones);free(model);rf_player_weapon_close(&w);return status;
}

int rf_player_weapon_open(rf_vpp *meshes,rf_vpp *motions,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,rf_player_weapon **result)
{
    const rf_weapon_view_definition pistol={"fp_glock.v3c",{"fp_glock_idle.rfa","fp_glock_fire.rfa","fp_glock_reload.rfa"},0};
    return rf_player_weapon_open_view(meshes,motions,maps,map_count,&pistol,budget,result);
}

static int player_weapon_start(rf_player_weapon *w,uint32_t clip)
{
    uint32_t i;rf_motion_playback_initialize(&w->playback);
    for(i=0;i<w->clip_count;i++)w->resources[i].references=0; /* This owner is the only consumer. */
    memset(w->generations,0xff,sizeof(w->generations));memset(w->prepared_generations,0xff,sizeof(w->prepared_generations));
    w->current=clip;w->initialized=1;
    /* Recovered action-start deliberately ignores loop resources; loops enter
     * through the state-weight operation instead (also fixes idle playback). */
    if(w->resources[clip].looping)return rf_motion_set_weight(&w->playback,w->resources,w->clip_count,(int32_t)clip,1);
    return rf_motion_start(&w->playback,w->resources,w->clip_count,(int32_t)clip,1,0);
}
int rf_player_weapon_step(rf_player_weapon *w,int32_t request,float elapsed)
{
    const rf_motion_file *files[4]={0};float displacement[3]={0};uint32_t i;int status;
    if(!w || request<-1 || request>3 || (request>=0 && ((uint32_t)request>=w->clip_count || !w->payloads[request])) || !isfinite(elapsed) || elapsed<0 || elapsed>1)return RF_RANGE;
    if(!w->initialized || request>=0){status=player_weapon_start(w,request<0?0:(uint32_t)request);if(status)return status;}
    status=rf_motion_update(&w->playback,w->resources,w->clip_count,elapsed);if(status)return status;
    if(!w->playback.completion.active.count){status=player_weapon_start(w,0);if(status)return status;}
    for(i=0;i<w->clip_count;i++)if(w->payloads[i])files[i]=w->clips+i;
    status=rf_model_evaluate_playback(w->bones,w->bone_count,&w->playback,files,w->resources,w->clip_count,
        displacement,w->pose,w->generations,50);if(status)return status;
    return rf_model_prepare_skinning(w->stored,w->pose,w->bone_count,(uint16_t)w->playback.generation,
        w->prepared,w->prepared_generations,50);
}
