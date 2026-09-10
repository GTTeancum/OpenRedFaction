#include "rf/entity.h"
#include "rf/player.h"
#include "rf/level.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
static rf_entity_room_result room_result;
static uint32_t room_queries,room_notices,room_notice_kind;
static void binding_select(void *context,rf_player_local_binding *local,int32_t weapon)
{
    uint32_t *out=context;
    memcpy(out,local->entity,20);out[5]=(uint32_t)local->entity_handle;
    out[6]=(uint32_t)weapon;out[7]=local->inventory==&local->entity->inventory;
    ++out[8];
}
static int room_locate(void *context,const float position[3],rf_entity_room_result *result)
{(void)context;(void)position;++room_queries;*result=room_result;return RF_OK;}
static void room_notify(void *context,const char *name)
{(void)context;++room_notices;room_notice_kind=!strcmp(name,"underwater")?2:1;}
static void climb_sound(void *context,const rf_player_climb_state *state,const rf_player_sound_request *sound)
{
    uint32_t *out=context;
    out[0]++;out[1]=state->previous_region==NULL;out[2]=state->region!=NULL;
    out[3]=state->speed.mode;out[4]=sound->sound_id;out[5]=sound->spatial;
}
static int climb_stand(void *context,uint32_t *stood)
{uint32_t *v=context;++v[1];*stood=!v[0];return RF_OK;}
int main(int argc,char **argv)
{
    if(argc==4 && !strcmp(argv[1],"--owned-level-regions")) {
        rf_vpp archive;rf_level level;rf_level_owned_regions regions={0};int status;
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 3;
        status=rf_level_owned_regions_open(&level,64*1024,&regions);rf_vpp_close(&archive);
        if(status==RF_NOT_FOUND)return 0;if(status)return 4;
        memset(&level,0xa5,sizeof(level)); /* Owned data must outlive the loader. */
        if(fwrite(regions.items,sizeof(*regions.items),regions.count,stdout)!=regions.count)return 5;
        rf_level_owned_regions_close(&regions);rf_level_owned_regions_close(&regions);
        return regions.items || regions.count || regions.allocated_bytes?6:0;
    }
    if(argc==2 && !strcmp(argv[1],"--climb-exit")) {
        int32_t v[6];uint32_t out[10];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(v,sizeof(v),1,stdin)==1) {
            rf_player_movement_region region={0};rf_movement_descriptor table[16]={{0}};
            rf_movement_config config={0};rf_player_climb_state state={0};rf_player_climb_exit_input input={0};
            float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};uint32_t selected=77,standing[2]={(uint32_t)v[2],0};
            config.flags=v[0];config.base_speed=3.5f;config.slow_factor=.5f;
            if(v[3]>=0)table[v[3]].enabled=v[4];
            state.previous_region=state.region=&region;state.movement=table+2;state.contact_handle=123;state.step_offset=7;
            input.config=&config;input.descriptors=table;input.identity=identity;input.default_index=v[3];
            input.forced_action=v[5];input.entity_scale=1;input.crouched=v[1];
            if(rf_player_climb_exit(&state,&input,&selected,climb_stand,standing))return 3;
            out[0]=state.previous_region==NULL;out[1]=state.region==&region;
            out[2]=state.movement?(uint32_t)(state.movement-table):UINT32_MAX;out[3]=state.orientation==identity;
            out[4]=state.contact_handle;out[5]=state.speed.mode;out[6]=selected;memcpy(out+7,&state.step_offset,4);
            out[8]=standing[1];memcpy(out+9,&state.speed.speed,4);
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--climb-enter")) {
        uint32_t values[4],out[12];
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(values,sizeof(values),1,stdin)==1) {
            rf_player_movement_region region={0};rf_movement_descriptor descriptors[16]={{0}};
            rf_movement_config config={0};rf_player_climb_input input={0};rf_player_climb_state state={0};
            uint32_t selected=77;memset(out,0,sizeof(out));region.kind=values[2];descriptors[2].enabled=values[3];
            config.flags=values[0]*4;config.base_speed=3.5f;
            state.previous_region=&region;state.movement=descriptors+1;state.contact_handle=123;
            input.region=&region;input.descriptors=descriptors;input.config=&config;input.forced_action=-1;
            input.entity_scale=1;input.free_motion=values[1];input.sound.owner_present=1;
            if(rf_player_climb_enter(&state,&input,&selected,climb_sound,out))return 3;
            out[6]=state.previous_region==NULL;out[7]=state.region==&region;
            out[8]=state.movement-descriptors;out[9]=state.orientation==region.matrix;
            out[10]=state.speed.mode;out[11]=selected;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-sound")) {
        rf_player_sound_input input;rf_player_sound_request result;
        _Static_assert(sizeof(input)==36 && sizeof(result)==32,"Player sound wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(rf_player_sound_route(&input,&result))return 3;
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==4 && !strcmp(argv[1],"--level-regions")) {
        rf_vpp archive;rf_level level;rf_level_entity_reader reader;rf_player_movement_region region;int status;
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[2]) || rf_level_open(&level,&archive,argv[3]))return 3;
        status=rf_level_regions_begin(&level,&reader);
        if(status==RF_NOT_FOUND){rf_vpp_close(&archive);return 0;}if(status)return 4;
        while((status=rf_level_region_next(&reader,&region))==RF_OK)
            if(fwrite(&region,sizeof(region),1,stdout)!=1)return 5;
        rf_vpp_close(&archive);return status==RF_NOT_FOUND?0:6;
    }
    if(argc==2 && !strcmp(argv[1],"--player-regions")) {
        struct {uint32_t count;float point[3];rf_player_movement_region regions[3];} input;
        _Static_assert(sizeof(input)==208,"Movement region wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            uint32_t index;
            if(input.count>3 || rf_player_movement_region_find(input.regions,input.count,input.point,&index))return 3;
            if(fwrite(&index,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-stance-gate")) {
        rf_player_stance_gate input;
        _Static_assert(sizeof(input)==32,"Player stance gate wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            uint32_t result=rf_player_stance_enabled(&input);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-motion")) {
        rf_player_motion_input input;int32_t state;
        _Static_assert(sizeof(input)==52,"Player motion wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(rf_player_motion_choose(&input,&state))return 3;
            if(fwrite(&state,sizeof(state),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-crouch")) {
        rf_player_crouch_input input;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&input,sizeof(input),1,stdin)==1) {
            uint32_t result=rf_player_can_crouch(&input);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-bind")) {
        rf_player_entity_binding entity;rf_player_local_binding local;
        uint32_t out[9];
        _Static_assert(sizeof(entity)==20,"Player binding wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&entity,sizeof(entity),1,stdin)==1) {
            memset(&local,0,sizeof(local));memset(out,0,sizeof(out));
            if(rf_player_bind_local(&local,&entity,binding_select,out))return 3;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 4;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-spawn")) {
        struct {rf_player_spawn_state player;int32_t local,count;
            rf_player_position_override override;rf_player_spawn_request request;} in;
        _Static_assert(sizeof(in)==48,"Player spawn wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            if(rf_player_spawn_prepare(&in.player,in.local,in.count,&in.override,&in.request))return 3;
            if(fwrite(&in,sizeof(in),1,stdout)!=1)return 4;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--room-refresh")) {
        struct {rf_entity_room_state state;float position[3];uint32_t local,room,liquid;float minimum_y,depth;} in;
        uint32_t out[8];
        _Static_assert(sizeof(in)==52,"Room wire input");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            room_queries=room_notices=room_notice_kind=0;
            room_result.room=in.room;room_result.liquid=in.liquid;room_result.minimum_y=in.minimum_y;
            room_result.liquid_depth=in.depth;room_result.name="test-room";
            if(rf_entity_room_refresh(&in.state,in.position,in.local,room_locate,room_notify,0))return 3;
            memcpy(out,&in.state,20);out[5]=room_queries;out[6]=room_notices;out[7]=room_notice_kind;
            if(fwrite(out,sizeof(out),1,stdout)!=1)return 4;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--sphere-overrides")) {
        rf_entity_class_sphere spheres[8];rf_entity_sphere_override overrides[8];uint32_t header[3];
        _Static_assert(sizeof(*spheres)==64 && sizeof(*overrides)==44,"Sphere override wire layout");
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(header,sizeof(header),1,stdin)==1) {
            if(header[0]>8 || header[1]>8 || fread(spheres,64,header[0],stdin)!=header[0] || fread(overrides,44,header[1],stdin)!=header[1])return 2;
            if(rf_entity_sphere_overrides(spheres,header[0],overrides,header[1],(uint8_t)header[2]))return 3;
            if(fwrite(spheres,64,header[0],stdout)!=header[0])return 4;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--physics-flags")) {
        uint32_t in[5],out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            out=rf_entity_creation_physics_flags(in[0],in[1],in[2],in[3],(uint8_t)in[4]);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--creation-flags")) {
        uint32_t in[2],out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(in,sizeof(in),1,stdin)==1) {
            out=rf_entity_creation_object_flags(in[0],in[1]);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    struct { int32_t target,query,attached[8]; struct {
        int32_t slot,handle,type,kind; uint32_t flags[3];
        int32_t action,linked,weapons[2]; float speed;
        int32_t owner,occupants[3];
    } nodes[8]; } input;
    rf_entity_registry registry; rf_entity_view nodes[8];
    int32_t output[6]; unsigned i;
    _Static_assert(sizeof(input)==552,"Entity predicate fixture layout");
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
    while (fread(&input,sizeof(input),1,stdin)==1) {
        const rf_entity_view *p;
        memset(&registry,0,sizeof(registry)); memset(nodes,0,sizeof(nodes));
        for (i=0;i<8;++i) {
            nodes[i].handle=input.nodes[i].handle; nodes[i].type=input.nodes[i].type; nodes[i].class_type=input.nodes[i].kind;
            nodes[i].flags_7c=input.nodes[i].flags[0]; nodes[i].flags_810=input.nodes[i].flags[1]; nodes[i].flags_7d0=input.nodes[i].flags[2];
            nodes[i].action_520=input.nodes[i].action; nodes[i].linked_handle=input.nodes[i].linked;
            memcpy(nodes[i].weapons,input.nodes[i].weapons,8); nodes[i].base_speed=input.nodes[i].speed;
            nodes[i].weapon_owner=input.nodes[i].owner>=0 && input.nodes[i].owner<8 ? &nodes[input.nodes[i].owner] : 0;
            nodes[i].occupants=input.nodes[i].occupants; nodes[i].occupant_count=3;
            if (input.nodes[i].slot>=0 && input.nodes[i].slot<RF_OBJECT_SLOTS) registry.slots[input.nodes[i].slot]=&nodes[i];
        }
        p=rf_object_lookup(&registry,input.query); output[0]=p ? (int32_t)(p-nodes) : -1;
        p=rf_entity_lookup(&registry,input.query); output[1]=p ? (int32_t)(p-nodes) : -1;
        p=input.target>=0 && input.target<8 ? &nodes[input.target] : 0;
        output[3]=output[4]=-99;
        output[2]=rf_entity_combat_predicates(&registry,p,input.attached,8,&output[3],&output[4]);
        output[5]=-99;
        if (rf_entity_has_weapon(&registry,p,&output[5])!=RF_OK) output[5]=-2;
        if (fwrite(output,sizeof(output),1,stdout)!=1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
