#include "rf/motion.h"
#include <math.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main(void)
{
    {
        rf_motion_playback_state state={0}, saved;
        rf_motion_playback_resource resources[2]={{{1,160,9600,0,0},1,{0,0},0},{{1,160,9600,0,0},1,{0,0},0}};
        state.completion.active.freeze_slot=state.completion.active.primary_slot=state.completion.active.dominant_slot=-1;
        CHECK(rf_motion_set_weight(&state,resources,2,0,1)==RF_OK);
        CHECK(rf_motion_update(&state,resources,2,.2f)==RF_OK);
        saved=state;
        CHECK(rf_motion_stop_looping(&state,resources,0)==RF_FORMAT && memcmp(&state,&saved,sizeof(state))==0);
        CHECK(rf_motion_stop_looping(&state,resources,2)==RF_OK);
        CHECK(state.completion.active.count==1 && state.completion.active.slots[0].weight==0 && resources[0].references==1);
        CHECK(state.phase==saved.phase && state.generation==saved.generation);
        CHECK(rf_motion_set_weight(&state,resources,2,1,1)==RF_OK);
        CHECK(rf_motion_update(&state,resources,2,.2f)==RF_OK);
        CHECK(state.completion.active.count==1 && state.completion.active.slots[0].motion==1);
        CHECK(resources[0].references==0 && resources[1].references==1);
        state.completion.active.primary_slot=0; saved=state;
        CHECK(rf_motion_stop_slot(&state,0)==RF_OK && memcmp(&state,&saved,sizeof(state))==0);
        CHECK(rf_motion_stop_slot(&state,1)==RF_OK && state.completion.active.primary_slot==-1);
        CHECK(state.completion.active.count==1 && resources[1].references==1);
    }
    {
        rf_motion_playback_state state={0}, saved;
        rf_motion_playback_resource resources[2]={{{1,160,9600,0,0},0,{0,0},0},{{1,160,9600,0,0},0,{0,0},INT32_MAX}};
        state.completion.active.freeze_slot=state.completion.active.primary_slot=state.completion.active.dominant_slot=-1;
        state.completion.primary_words[0]=123; state.completion.primary_words[1]=456;
        saved=state;
        CHECK(rf_motion_set_weight(&state,resources,2,1,1)==RF_RANGE && memcmp(&state,&saved,sizeof(state))==0);
        CHECK(rf_motion_set_weight(&state,resources,2,0,NAN)==RF_FORMAT && memcmp(&state,&saved,sizeof(state))==0);
        CHECK(rf_motion_start(&state,resources,2,0,1,1)==RF_OK);
        CHECK(state.completion.active.count==1 && state.completion.active.slots[0].tick==160 && resources[0].references==1);
        CHECK(state.completion.active.primary_slot==0 && state.completion.active.freeze_slot==0);
        CHECK(state.completion.primary_words[0]==0 && state.completion.primary_words[1]==456);
        state.completion.active.slots[0].tick=1000; state.completion.frozen=1;
        CHECK(rf_motion_set_weight(&state,resources,2,0,.5f)==RF_OK);
        CHECK(state.completion.active.slots[0].tick==1000 && state.completion.frozen==0 && resources[0].references==1);
        CHECK(rf_motion_start(&state,resources,2,0,-1,0)==RF_OK && state.completion.active.slots[0].tick==1000);
    }
    {
        rf_motion_slot_state active={0}; rf_motion_weight_envelope envelopes[16];
        float weights[16], saved[16]; unsigned i;
        active.count=16; active.primary_slot=-1;
        for (i=0;i<16;++i) {
            active.slots[i].weight=1;
            envelopes[i].weight=1; envelopes[i].start_tick=0; envelopes[i].end_tick=100;
            envelopes[i].fade_in=envelopes[i].fade_out=0;
        }
        CHECK(rf_motion_bone_weights(&active,envelopes,0,weights)==RF_OK);
        for (i=0;i<16;++i) CHECK(weights[i]==0.0625f);
        active.count=2; active.primary_slot=1; envelopes[0].weight=10; envelopes[1].weight=5;
        CHECK(rf_motion_bone_weights(&active,envelopes,1,weights)==RF_OK && weights[0]==.5f && weights[1]==.5f);
        envelopes[1].weight=10;
        CHECK(rf_motion_bone_weights(&active,envelopes,1,weights)==RF_OK && weights[0]==0 && weights[1]==1);
        envelopes[1].weight=12;
        CHECK(rf_motion_bone_weights(&active,envelopes,1,weights)==RF_OK && weights[0]==0 && weights[1]==1);
        memcpy(saved,weights,sizeof(saved)); active.primary_slot=-1; envelopes[1].fade_in=-1;
        CHECK(rf_motion_bone_weights(&active,envelopes,1,weights)==RF_FORMAT && memcmp(saved,weights,sizeof(saved))==0);
        active.count=0;
        CHECK(rf_motion_bone_weights(&active,NULL,0,weights)==RF_OK);
        for (i=0;i<16;++i) CHECK(weights[i]==0);
    }
    {
        rf_motion_playback_state state={0}, saved;
        rf_motion_playback_resource resources[2]={{{1,0,9600,0,0},0,{0,0},2},{{1,0,9600,0,0},0,{0,0},2}};
        state.completion.active.count=2;
        state.completion.active.freeze_slot=state.completion.active.primary_slot=state.completion.active.dominant_slot=-1;
        state.completion.active.slots[0].weight=1;
        state.completion.active.slots[1].motion=1;
        state.completion.active.slots[1].weight=1;
        state.completion.active.slots[1].tick=INT32_MAX;
        saved=state;
        /* Failure after the first cursor advances must roll back everything. */
        CHECK(rf_motion_update(&state,resources,2,1)==RF_RANGE);
        CHECK(memcmp(&state,&saved,sizeof(state))==0 && resources[0].references==2 && resources[1].references==2);
        CHECK(rf_motion_update(&state,resources,2,NAN)==RF_RANGE && memcmp(&state,&saved,sizeof(state))==0);
        CHECK(rf_motion_update(&state,resources,2,-1)==RF_RANGE && memcmp(&state,&saved,sizeof(state))==0);
        state.completion.active.slots[1].motion=0; saved=state;
        CHECK(rf_motion_update(&state,resources,2,0)==RF_FORMAT && memcmp(&state,&saved,sizeof(state))==0);
        state.completion.frozen=1; saved=state;
        CHECK(rf_motion_update(&state,NULL,2,NAN)==RF_OK && memcmp(&state,&saved,sizeof(state))==0);
        state.completion.frozen=0; saved=state;
        CHECK(rf_motion_update(&state,NULL,0,NAN)==RF_OK && memcmp(&state,&saved,sizeof(state))==0);
    }
    {
        rf_motion_completion_state state={0};
        state.active.count=1; state.active.primary_slot=-1; state.active.slots[0].weight=1;
        state.primary_words[0]=123; state.primary_words[1]=456;
        CHECK(rf_motion_advance_candidate(&state,0,10,NULL,0,NULL,0)==RF_OK);
        CHECK(state.active.primary_slot==0 && state.active.slots[0].tick==10 && state.primary_words[0]==0 && state.primary_words[1]==0);
        CHECK(rf_motion_advance_candidate(&state,0,10,NULL,0,NULL,0)==RF_RANGE && state.active.slots[0].tick==10);
    }
    {
        rf_motion_completion_state state={0}; int32_t ends[2]={100,100};
        state.active.count=2; state.active.freeze_slot=0; state.active.primary_slot=1; state.active.dominant_slot=-1;
        state.active.slots[0].tick=101; state.active.slots[1].tick=100;
        state.active.slots[0].weight=state.active.slots[1].weight=1;
        state.primary_flag=1; state.primary_vectors[0][0]=3;
        CHECK(rf_motion_complete_slots(&state,ends,0)==RF_OK);
        CHECK(state.frozen==1 && state.active.slots[0].weight==1 && state.active.slots[0].tick==100);
        CHECK(state.active.slots[1].weight==0 && state.active.primary_slot==-1 && state.primary_flag==0 && state.primary_vectors[0][0]==0);
    }
    {
        rf_motion_slot_state state={0}, saved; int32_t references=2;
        state.count=2; state.slots[0].motion=3; state.slots[1].motion=7;
        state.freeze_slot=0; state.primary_slot=1; state.dominant_slot=-1;
        saved=state;
        CHECK(rf_motion_remove_slot(&state,99,&references)==RF_OK && references==2);
        CHECK(memcmp(&state,&saved,sizeof(state))==0);
        CHECK(rf_motion_remove_slot(&state,3,&references)==RF_OK && references==1);
        CHECK(state.count==1 && state.slots[0].motion==7 && state.freeze_slot==-1 && state.primary_slot==0);
        state.primary_slot=1; saved=state;
        CHECK(rf_motion_remove_slot(&state,7,&references)==RF_FORMAT && references==1);
        CHECK(memcmp(&state,&saved,sizeof(state))==0);
    }
    {
        rf_motion_loop_result result={99,99}; int32_t markers[2]={20,80};
        CHECK(rf_motion_map_loop(0,100,.5f,0,markers,0,&result)==RF_OK && result.tick==50 && result.event_mask==1);
        CHECK(rf_motion_map_loop(0,100,.5f,90,markers,1,&result)==RF_OK && result.event_mask==1);
        CHECK(rf_motion_map_loop(0,100,NAN,0,markers,0,&result)==RF_RANGE && result.tick==50);
        CHECK(rf_motion_map_loop(100,0,.5f,0,markers,0,&result)==RF_FORMAT);
    }
    {
        rf_motion_phase_slot slots[2]={{100,1,1},{200,1,1}};
        rf_motion_phase_result phase={99,99,99};
        CHECK(rf_motion_advance_phase(slots,2,0,100,&phase)==RF_OK);
        CHECK(phase.phase==.75f && phase.dominant_slot==0 && !phase.wrapped);
        CHECK(rf_motion_advance_phase(slots,2,.5f,100,&phase)==RF_OK);
        CHECK(phase.phase==.25f && phase.wrapped);
        slots[0].looping=slots[1].looping=0;
        CHECK(rf_motion_advance_phase(slots,2,.5f,100,&phase)==RF_OK);
        CHECK(phase.phase==0 && phase.dominant_slot==-1 && !phase.wrapped);
        slots[0].duration=0;
        CHECK(rf_motion_advance_phase(slots,2,.5f,100,&phase)==RF_FORMAT && phase.dominant_slot==-1);
    }
    int32_t ticks=99;
    CHECK(rf_motion_elapsed_ticks(NAN,&ticks)==RF_RANGE && ticks==99);
    CHECK(rf_motion_elapsed_ticks(INFINITY,&ticks)==RF_RANGE && ticks==99);
    CHECK(rf_motion_elapsed_ticks(1000000,&ticks)==RF_RANGE && ticks==99);
    CHECK(rf_motion_elapsed_ticks(0.2f,&ticks)==RF_OK && ticks==960);
    CHECK(rf_motion_elapsed_ticks(-0.2f,&ticks)==RF_OK && ticks==-960);
    rf_motion_weight_envelope envelope = {1,0,100,20,20};
    float weight=99;
    CHECK(rf_motion_sample_weight(NULL,0,0,&weight)==RF_RANGE && weight==99);
    envelope.fade_in=-1;
    CHECK(rf_motion_sample_weight(&envelope,0,0,&weight)==RF_FORMAT && weight==99);
    envelope.fade_in=20; envelope.weight=NAN;
    CHECK(rf_motion_sample_weight(&envelope,0,0,&weight)==RF_FORMAT && weight==99);
    envelope.weight=1;
    CHECK(rf_motion_sample_weight(&envelope,10,0,&weight)==RF_OK && weight==0.5f);
    CHECK(rf_motion_sample_weight(&envelope,90,0,&weight)==RF_OK && weight==0.5f);
    CHECK(rf_motion_sample_weight(&envelope,-10,1,&weight)==RF_OK && weight==1);
    envelope.weight=-1;
    CHECK(rf_motion_sample_weight(&envelope,50,1,&weight)==RF_OK && weight==0);
    rf_motion_position_key keys[2] = {{0,{0,0,0},{0,0,0},{0,0,0}}, {100,{4,8,12},{4,8,12},{4,8,12}}};
    float out[3]={99,99,99}, sentinel[3]={99,99,99};
    unsigned char packed[8] = {0,0, 0,64, 0,192, 0,128};
    float rotation[4] = {99,99,99,99};
    int16_t qa[4] = {0,0,0,16383}, qb[4] = {0,0,0,16383};
    int16_t qr[4] = {99,99,99,99}, qs[4] = {99,99,99,99};
    rf_motion_rotation_key rk[2] = {{0,{0,0,0,16383},0,0,{0,0}}, {100,{0,10000,0,12000},0,0,{0,0}}};
    CHECK(rf_motion_sample_rotation(NULL,0,0,rotation)==RF_OK && rotation[3]==1);
    CHECK(rf_motion_sample_rotation(rk,1,-1,rotation)==RF_OK);
    CHECK(rotation[0]==0 && rotation[3]>0.99f);
    CHECK(rf_motion_sample_rotation(rk,1,0,rotation)==RF_OK);
    CHECK(rf_motion_sample_rotation(rk,2,50,rotation)==RF_OK);
    rk[1].tick=0;
    CHECK(rf_motion_sample_rotation(rk,2,0,rotation)==RF_FORMAT);
    rk[1].tick=100; rk[1].incoming=-1;
    CHECK(rf_motion_sample_rotation(rk,2,50,rotation)==RF_FORMAT);
    rotation[0]=rotation[1]=rotation[2]=rotation[3]=99;
    CHECK(rf_motion_interpolate_rotation(NULL,qb,0,qr)==RF_RANGE);
    CHECK(rf_motion_interpolate_rotation(qa,qb,NAN,qr)==RF_RANGE);
    CHECK(rf_motion_interpolate_rotation(qa,qb,-1,qr)==RF_RANGE);
    CHECK(rf_motion_interpolate_rotation(qa,qb,2,qr)==RF_RANGE);
    CHECK(memcmp(qr,qs,sizeof(qr))==0);
    CHECK(rf_motion_interpolate_rotation(qa,qb,0.5f,qr)==RF_OK);
    CHECK(rf_motion_interpolate_rotation(qa,qb,0.5f,qa)==RF_OK);
    CHECK(memcmp(qa,qr,sizeof(qa))==0);
    CHECK(rf_motion_decode_rotation(packed,7,rotation)==RF_RANGE);
    CHECK(rotation[0]==99 && rotation[3]==99);
    CHECK(rf_motion_decode_rotation(NULL,8,rotation)==RF_RANGE);
    CHECK(rf_motion_decode_rotation(packed,8,NULL)==RF_RANGE);
    CHECK(rf_motion_decode_rotation(packed,8,rotation)==RF_OK);
    CHECK(rotation[0]==0 && rotation[1]>1 && rotation[2]<-1 && rotation[3]<-2);
    CHECK(rf_motion_sample_position(NULL,1,0,out)==RF_RANGE);
    keys[1].tick=0;
    CHECK(rf_motion_sample_position(keys,2,0,out)==RF_FORMAT);
    keys[1].tick=100; keys[0].outgoing[0]=NAN;
    CHECK(rf_motion_sample_position(keys,2,0,out)==RF_FORMAT);
    CHECK(memcmp(out,sentinel,sizeof(out))==0);
    keys[0].outgoing[0]=0;
    CHECK(rf_motion_sample_position(keys,2,50,out)==RF_OK);
    CHECK(out[0]==2 && out[1]==4 && out[2]==6);
    CHECK(rf_motion_sample_position(keys,2,-1,out)==RF_OK && out[0]==0);
    CHECK(rf_motion_sample_position(keys,2,101,out)==RF_OK && out[0]==4);
    CHECK(rf_motion_sample_position(NULL,0,0,out)==RF_OK && out[0]==0 && out[1]==0 && out[2]==0);
    return 0;
}
