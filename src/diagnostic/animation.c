#include "rf/animation_check.h"
#include "rf/model.h"
#include "rf/model_file.h"
#include "rf/turn.h"
#include "rf/entity.h"
#include "rf/weapon.h"
#include "rf/effect.h"
#include <stdlib.h>
#include <string.h>

static uint32_t hash_bytes(uint32_t hash, const void *bytes, size_t count)
{
    const unsigned char *p=bytes;
    while (count--) hash=(hash ^ *p++)*16777619u;
    return hash;
}
typedef struct animation_reset {
    rf_weapon_reset_state *state; rf_weapon_descriptor *descriptors;
    rf_weapon_reset_context *context; rf_motion_playback_state *playback;
    rf_motion_playback_resource *resources; rf_turn_actor *actor;
    rf_effect_pair *effects;
} animation_reset;
static int stop_reset_effect(void *user,int32_t handle)
{
    animation_reset *r=user;
    return rf_effect_set_enabled(r->effects,1,handle,0,0,0);
}
static int reset_loaded_weapon(void *user)
{
    animation_reset *r=user;
    const rf_weapon_reset_ops ops={NULL,NULL,stop_reset_effect,NULL};
    return rf_weapon_reset(r->state,r->actor->weapon,r->descriptors,r->context,r->playback,r->resources,4,&ops,r);
}
int rf_animation_check(const char *meshes_path, const char *motions_path, uint32_t out[8])
{
    static const char *names[4]={"ult2_stand.rfa","ult2_crouch.rfa",
        "ult2_sidestep_left.rfa","ult2_sidestep_right.rfa"};
    rf_vpp meshes, archive; rf_model_file model; rf_motion_file files[4];
    const rf_motion_file *handles[4]={&files[0],&files[1],&files[2],&files[3]};
    rf_motion_playback_resource resources[4]={0}; rf_motion_playback_state state={0};
    rf_turn_effects effects={0}; rf_turn_actor actor={0};
    rf_locomotion_candidate_input selection={0,1,{0,0,0},0};
    rf_locomotion_candidates candidates;
    rf_entity_registry registry={0}; rf_entity_view entity={0};
    rf_weapon_reset_state weapon_state={0}; rf_weapon_descriptor descriptors[64]={0};
    rf_weapon_reset_context weapon_context={0};
    rf_effect_switch effect_objects[2]={{1,{0,0,0},77},{1,{0,0,0},99}};
    rf_effect_pair effect_pair={{{&effect_objects[0],&effect_objects[1]},{NULL,NULL}}};
    animation_reset reset={&weapon_state,descriptors,&weapon_context,&state,resources,&actor,&effect_pair};
    uint32_t weapon_flags[1]={6};
    rf_weapon_inventory inventory={0}; rf_weapon_supply supply[64]={0};
    int32_t preference[32],replacement,reserve;
    int ready,eligible;
    rf_turn_context context={{0x800,6,.3f,1.5f,20,7,9},-1,1,0,0};
    int32_t actions[45],sounds[45],sound_class;
    rf_motion_controller controller={0,-1,0,0,0,0}; int32_t motions[23];
    rf_model_bone bones[256]; rf_model_attachment eye;
    float matrices[256][12], local[12], tag[12], displacement[3]={.125f,-.25f,.5f};
    uint16_t generations[256]={0}; uint32_t count=0,i,frame; int status,opened=0,found=0;
    void *payload=NULL;
    if (!out) return RF_RANGE;
    memset(out,0,8*4); out[0]=1;
    status=rf_vpp_open(&meshes,meshes_path); if (status!=RF_OK) goto done;
    opened=1; status=rf_vpp_open(&archive,motions_path); if (status!=RF_OK) goto done;
    opened=2; status=rf_model_file_open(&model,&meshes,"miner.v3c"); if (status!=RF_OK) goto done;
    for (i=0;i<model.section_count;++i) if (model.sections[i].type==0x424f4e45) {
        if (model.sections[i].size>4+256*56) { status=RF_RANGE; goto done; }
        out[7]=model.sections[i].size; payload=malloc(out[7]);
        if (!payload) { status=RF_RANGE; goto done; }
        status=rf_vpp_read(&meshes,&model.entry,model.sections[i].offset,payload,out[7]);
        if (status==RF_OK) status=rf_model_decode_bones(payload,out[7],bones,256,&count);
        free(payload); payload=NULL; if (status!=RF_OK) goto done;
    }
    if (!count || !model.lod_count) { status=RF_FORMAT; goto done; }
    out[1]=count;
    for (i=0;i<model.lods[0].attachment_count;++i) {
        status=rf_model_file_attachment(&model,0,i,&eye); if (status!=RF_OK) goto done;
        if (!strcmp(eye.name,"eye")) { found=1; break; }
    }
    if (!found || eye.parent<0 || (uint32_t)eye.parent>=count) { status=RF_FORMAT; goto done; }
    status=rf_model_attachment_transform(eye.rotation,eye.position,local); if (status!=RF_OK) goto done;
    state.completion.active.freeze_slot=-1;
    state.completion.active.primary_slot=state.completion.active.dominant_slot=-1;
    state.phase=.25f; state.generation=1;
    for (i=0;i<4;++i) {
        rf_motion_track track;
        status=rf_motion_file_open(&files[i],&archive,names[i]); if (status!=RF_OK) goto done;
        status=rf_motion_file_track(&files[i],0,&track); if (status!=RF_OK) goto done;
        resources[i].comparison=track.envelope; resources[i].looping=i<2;
        resources[i].markers[0]=3200; resources[i].markers[1]=6400;
    }
    for (i=0;i<23;++i) motions[i]=-1;
    motions[0]=0; motions[8]=1;
    for (i=0;i<45;++i) actions[i]=sounds[i]=-1;
    /* Original 0x4181d0 names actions 17/18 sidestep_left/right. Roll actions
     * 19/20 are absent in this profile, so the reset adapter is never reached. */
    actions[17]=2; actions[18]=3;
    actor.info_flags=context.movement.flags; actor.weapon=0;
    actor.direction.entity_flags=10;
    entity.handle=0x10000; entity.linked_handle=-1;
    entity.weapons[0]=0; entity.weapons[1]=-1; entity.weapon_owner=&entity;
    weapon_state.sound_81c=weapon_state.sound_820=-1;
    weapon_state.flags_7d0=10;
    weapon_state.character_present=1; weapon_context.weapon_count=1;
    descriptors[0].flags_264=6; descriptors[0].flags_268=0x40; descriptors[0].release_sound_class=-1;
    inventory.owned[0]=inventory.owned[1]=1;
    supply[0].ammo_type=0; supply[0].capacity=10;
    supply[1].ammo_type=-1; supply[1].flags_268=0x100;
    for (i=0;i<32;++i) preference[i]=-1;
    preference[0]=1; preference[1]=0;
    entity.flags_7d0=10; registry.slots[0]=&entity;
    actor.direction.orientation[0]=actor.direction.orientation[4]=actor.direction.orientation[8]=1;
    for (i=3;i<=6;++i) out[i]=2166136261u;
    for (frame=0;frame<64;++frame) {
        inventory.reserve[0]=frame<32 ? 1 : 0;
        status=rf_weapon_reserve(&inventory,supply,0,&reserve); if (status!=RF_OK) goto done;
        status=rf_weapon_choose_available(&inventory,supply,preference,1,&replacement); if (status!=RF_OK) goto done;
        /* Scripted diagnostic requests, not the unrecovered locomotion selector. */
        if (frame==4 || frame==7 || frame==20 || frame==40) {
            status=rf_motion_request_state(&controller,motions,(frame==4 || frame==20) ? 8 : 0,.25f);
            if (status!=RF_OK) goto done;
        }
        controller.override_enabled=frame>=22 && frame<26;
        status=rf_motion_apply_controller(&controller,motions,1.0f/30.0f,&state,resources,4); if (status!=RF_OK) goto done;
        actor.direction.count=frame>=8 && frame<56;
        actor.direction.vector[0]=frame<32 ? -1.0f : 1.0f;
        context.now_ms=(int32_t)frame*33;
        selection.action=frame>=62 ? 12 : frame>=60 ? 7 : frame>=58 ? 17 : 0;
        selection.velocity[0]=frame>=32 ? 1.0f : 0;
        actor.behavior=frame>=48;
        if (frame==48) weapon_state.active[0]=1;
        status=rf_locomotion_prepare(&effects.deadlines[3],-1,&context,&actor,weapon_flags,1,reset_loaded_weapon,&reset); if (status!=RF_OK) goto done;
        entity.action_520=selection.action;
        status=rf_entity_combat_predicates(&registry,&entity,NULL,0,&ready,&eligible); if (status!=RF_OK) goto done;
        selection.combat_eligible=(uint32_t)eligible;
        status=rf_locomotion_choose_candidates(&candidates,&selection,motions,&effects,&state,resources,4,
            actions,sounds,&context,&actor,NULL,NULL,&sound_class); if (status!=RF_OK) goto done;
        status=rf_motion_update(&state,resources,4,1.0f/30.0f); if (status!=RF_OK) goto done;
        status=rf_model_evaluate_playback(bones,count,&state,handles,resources,4,displacement,matrices,generations,256); if (status!=RF_OK) goto done;
        status=rf_model_compose_transform(local,matrices[eye.parent],tag); if (status!=RF_OK) goto done;
        out[3]=hash_bytes(out[3],matrices,count*48); out[4]=hash_bytes(out[4],&state,sizeof(state)); out[6]=hash_bytes(out[6],tag,48);
        out[4]=hash_bytes(out[4],&controller,sizeof(controller));
        for (i=0;i<4;++i) out[4]=hash_bytes(out[4],&resources[i].references,4);
        out[4]=hash_bytes(out[4],&effects,sizeof(effects));
        out[4]=hash_bytes(out[4],&sound_class,4);
        out[4]=hash_bytes(out[4],&candidates,sizeof(candidates));
        out[4]=hash_bytes(out[4],&ready,4); out[4]=hash_bytes(out[4],&eligible,4);
        out[4]=hash_bytes(out[4],&weapon_state,sizeof(weapon_state));
        out[4]=hash_bytes(out[4],effect_objects,sizeof(effect_objects));
        out[4]=hash_bytes(out[4],&reserve,4); out[4]=hash_bytes(out[4],&replacement,4);
        displacement[0]=1;
        status=rf_model_evaluate_playback(bones,count,&state,handles,resources,4,displacement,matrices,generations,256); if (status!=RF_OK) goto done;
        if (displacement[0]!=1) { status=RF_FORMAT; goto done; }
        out[5]=hash_bytes(out[5],matrices,count*48); out[5]=hash_bytes(out[5],displacement,12);
        out[5]=hash_bytes(out[5],generations,count*2); displacement[0]=0;
        out[2]=frame+1;
    }
done:
    free(payload);
    if (opened==2) rf_vpp_close(&archive);
    if (opened) rf_vpp_close(&meshes);
    out[0]=status==RF_OK ? 2u : 0x80000000u | (uint32_t)(-status);
    return status;
}
