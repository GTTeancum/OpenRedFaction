#include "rf/model.h"
#include "rf/entity_assets.h"
#include "rf/model_file.h"
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    rf_vpp meshes,motions; rf_model_file model; rf_motion_file motion, files[16];
    const rf_motion_file *handles[16]; rf_motion_playback_resource resources[16]={0};
    struct { rf_motion_playback_state state; uint32_t looping; float elapsed, displacement[3]; int32_t extra_root; } input;
    _Static_assert(sizeof(input)==284,"Playback skeleton wire layout");
    rf_model_bone bones[256]; float matrices[256][12]; uint32_t count=0,i; int32_t tick;
    rf_model_attachment eye; int found=0;
    int pose_only=getenv("RF_PROBE_POSE_ONLY")!=NULL;
    int cache_mode=getenv("RF_PROBE_CACHE")!=NULL;
    int setup_mode=getenv("RF_PROBE_EYE_SETUP")!=NULL;
    uint16_t generations[256];
    if (argc<5 || argc>20) return 1;
    if (setup_mode && (cache_mode || argc!=6)) return 1;
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    if (rf_vpp_open(&meshes,argv[1])!=RF_OK || rf_vpp_open(&motions,argv[2])!=RF_OK) return 2;
    if (rf_model_file_open(&model,&meshes,argv[3])!=RF_OK || rf_motion_file_open(&motion,&motions,argv[4])!=RF_OK) return 3;
    for (i=0;i<(uint32_t)argc-4;++i) {
        rf_motion_track track;
        if (rf_motion_file_open(&files[i],&motions,argv[i+4])!=RF_OK || rf_motion_file_track(&files[i],0,&track)!=RF_OK) return 3;
        handles[i]=&files[i]; resources[i].comparison=track.envelope; resources[i].references=2;
    }
    for (i=0;i<model.section_count;++i) if (model.sections[i].type==0x424f4e45) {
        void *payload=malloc(model.sections[i].size); int status;
        if (!payload) return 4;
        status=rf_vpp_read(&meshes,&model.entry,model.sections[i].offset,payload,model.sections[i].size);
        if (status==RF_OK) status=rf_model_decode_bones(payload,model.sections[i].size,bones,256,&count);
        free(payload); if (status!=RF_OK) return 5;
    }
    if (!count) return 6;
    for (i=0;i<model.lods[0].attachment_count;++i) {
        if (rf_model_file_attachment(&model,0,i,&eye)!=RF_OK) return 6;
        if (!strcmp(eye.name,"eye")) { found=1; break; }
    }
    if(getenv("RF_PROBE_OVERRIDES")) {
        struct {int32_t tick,bone;rf_model_bone_override override;} request;
        _Static_assert(sizeof(request)==52,"Override probe wire");
        while(fread(&request,sizeof(request),1,stdin)==1) {
            rf_model_bone_override overrides[256]={0};rf_motion_playback_state state;float displacement[3]={0};unsigned pass;
            if(request.bone<0 || (uint32_t)request.bone>=count)return 7;
            overrides[request.bone]=request.override;rf_motion_playback_initialize(&state);
            state.generation=1;state.completion.active.count=1;
            state.completion.active.slots[0].motion=0;state.completion.active.slots[0].tick=request.tick;state.completion.active.slots[0].weight=1;
            memset(generations,0,sizeof(generations));memset(matrices,0,sizeof(matrices));
            for(pass=0;pass<3;++pass) {
                if(pass)overrides[request.bone].enabled=0;
                if(pass==2)state.generation=2;
                if(rf_model_evaluate_overrides(bones,count,&state,handles,resources,(uint32_t)argc-4,displacement,matrices,generations,256,overrides))return 7;
                if(fwrite(matrices,48,count,stdout)!=count || fwrite(generations,2,count,stdout)!=count)return 8;
            }
        }
        rf_vpp_close(&meshes);rf_vpp_close(&motions);return ferror(stdin)?9:0;
    }
    if(pose_only) {
        rf_motion_playback_state state;float displacement[3]={0};
        while(fread(&state,sizeof(state),1,stdin)==1) {
            uint32_t looping;float elapsed;
            if(fread(&looping,4,1,stdin)!=1 || fread(&elapsed,4,1,stdin)!=1)return 9;
            for(i=0;i<(uint32_t)argc-4;++i)resources[i].looping=(looping>>i)&1;
            memset(generations,0,sizeof(generations));memset(matrices,0,sizeof(matrices));
            {
                rf_entity_model_motion items[32]={0};rf_entity_model_motions model_motions={items,NULL,32};
                rf_motion_playback_resource owned[32]={0};rf_entity_playback_model view={owned,NULL,32};rf_entity_playback_resources owner={0};
                uint16_t saved_generations[256];
                rf_entity_motion_catalog catalog={0};rf_entity_skeleton skeleton={0};rf_entity_skeletons skeletons={0};rf_entity_pose pose={0};
                catalog.models=&model_motions;catalog.model_count=1;skeleton.bones=bones;skeleton.count=count;
                skeletons.items=&skeleton;skeletons.count=1;pose.bone_count=count;pose.playback=state;pose.matrices=matrices;pose.generations=generations;
                for(i=0;i<(uint32_t)argc-4;++i){items[2*i+1].file=files[i];items[2*i+1].comparison=resources[i].comparison;items[2*i+1].looping=(uint8_t)resources[i].looping;}
                for(i=0;i<pose.playback.completion.active.count;++i)pose.playback.completion.active.slots[i].motion=2*pose.playback.completion.active.slots[i].motion+1;
                owner.models=&view;owner.model_count=1;
                for(i=0;i<32;++i){owned[i].comparison=items[i].comparison;owned[i].looping=items[i].looping;}
                for(i=0;i<pose.playback.completion.active.count;++i)owned[pose.playback.completion.active.slots[i].motion].references=2;
                if(rf_entity_pose_advance(&pose,&skeletons,&catalog,&owner,elapsed,displacement))return 7;
                memcpy(saved_generations,generations,sizeof(generations));
                if(rf_entity_pose_release(&pose,&owner) || rf_entity_pose_release(&pose,&owner))return 7;
                for(i=0;i<state.completion.active.count;++i)if(owned[2*state.completion.active.slots[i].motion+1].references!=1)return 7;
                for(i=0;i<count;++i)if(generations[i])return 7;
                memcpy(generations,saved_generations,sizeof(generations));
            }
            if(fwrite(&count,4,1,stdout)!=1 || fwrite(matrices,48,count,stdout)!=count || fwrite(generations,2,count,stdout)!=count)return 8;
        }
        rf_vpp_close(&meshes);rf_vpp_close(&motions);return ferror(stdin)?9:0;
    }
    if (!found || eye.parent<0 || (uint32_t)eye.parent>=count) return 6;
    while (argc==5 ? fread(&tick,4,1,stdin)==1 : fread(&input,sizeof(input),1,stdin)==1) {
        float local[12], evaluated[12];
        int32_t saved_parent=-1;
        if (argc==5) {
            if (rf_model_sample_single_motion(bones,count,&motion,tick,0,matrices,256)!=RF_OK) return 7;
        } else {
            if (input.extra_root < -1 || input.extra_root>=(int32_t)count) return 7;
            if (input.extra_root>=0) { saved_parent=bones[input.extra_root].parent; bones[input.extra_root].parent=-1; }
            for (i=0;i<(uint32_t)argc-4;++i) {
                resources[i].looping=(input.looping>>i)&1; resources[i].references=2;
            }
            if (setup_mode && rf_motion_set_weight(&input.state,resources,2,0,1)!=RF_OK) return 7;
            if ((!setup_mode || input.elapsed!=0) && rf_motion_update(&input.state,resources,(uint32_t)argc-4,input.elapsed)!=RF_OK) return 7;
            for (i=0;i<count;++i) generations[i]=(uint16_t)(input.state.generation-1);
            if (cache_mode || setup_mode) {
                if (rf_model_evaluate_playback(bones,count,&input.state,handles,resources,(uint32_t)argc-4,input.displacement,matrices,generations,256)!=RF_OK) return 7;
            } else if (rf_model_sample_playback(bones,count,&input.state,handles,resources,(uint32_t)argc-4,input.displacement,matrices,256)!=RF_OK) return 7;
            if (fwrite(&input.state,sizeof(input.state),1,stdout)!=1) return 8;
            if (fwrite(input.displacement,sizeof(input.displacement),1,stdout)!=1) return 8;
        }
        if (rf_model_attachment_transform(eye.rotation,eye.position,local)!=RF_OK ||
            rf_model_compose_transform(local,matrices[eye.parent],evaluated)!=RF_OK) return 7;
        if (fwrite(matrices,48,count,stdout)!=count) return 8;
        if (fwrite(evaluated,48,1,stdout)!=1) return 8;
        if (setup_mode) {
            unsigned query;
            for (query=0;query<2;++query) {
                if (rf_motion_stop_looping(&input.state,resources,2)!=RF_OK ||
                    rf_motion_set_weight(&input.state,resources,2,query==0 ? 1 : 0,1)!=RF_OK ||
                    rf_motion_update(&input.state,resources,2,.2f)!=RF_OK ||
                    rf_model_evaluate_playback(bones,count,&input.state,handles,resources,2,input.displacement,matrices,generations,256)!=RF_OK ||
                    rf_model_compose_transform(local,matrices[eye.parent],evaluated)!=RF_OK) return 7;
                if (fwrite(&input.state,260,1,stdout)!=1 || fwrite(input.displacement,12,1,stdout)!=1 ||
                    fwrite(matrices,48,count,stdout)!=count || fwrite(evaluated,48,1,stdout)!=1) return 8;
            }
        }
        if (cache_mode && argc>5) {
            unsigned query;
            for (query=0;query<3;++query) {
                if (query==0) { input.displacement[0]=1; input.displacement[1]=2; input.displacement[2]=3; }
                else if (query==1) input.state.generation=(input.state.generation+1)&65535u;
                else {
                    generations[0]=(uint16_t)(input.state.generation-1);
                    input.displacement[0]=-1; input.displacement[1]=-2; input.displacement[2]=-3;
                }
                if (rf_model_evaluate_playback(bones,count,&input.state,handles,resources,(uint32_t)argc-4,input.displacement,matrices,generations,256)!=RF_OK) return 7;
                if (rf_model_compose_transform(local,matrices[eye.parent],evaluated)!=RF_OK) return 7;
                if (fwrite(input.displacement,12,1,stdout)!=1 || fwrite(generations,2,count,stdout)!=count ||
                    fwrite(matrices,48,count,stdout)!=count || fwrite(evaluated,48,1,stdout)!=1) return 8;
            }
        }
        if (argc>5 && input.extra_root>=0) bones[input.extra_root].parent=saved_parent;
    }
    rf_vpp_close(&meshes); rf_vpp_close(&motions); return ferror(stdin) ? 9 : 0;
}
