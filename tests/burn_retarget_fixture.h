#include "rf/burn.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define BURN_CHECK(x) do {if(!(x)){status=__LINE__;goto cleanup;}} while(0)
typedef struct burn_retarget_storage {
    rf_particle records[1600],saved_particles[1600];
    rf_particle_list lists[133],saved_lists[133];
    rf_emitter_slot slots[128],saved_slots[128];
} burn_retarget_storage;
static uint32_t calls[2],call_count;
static void voice(void *context,uint32_t id){(void)context;if(call_count<2)calls[call_count++]=id;}
static void owner(void *context,uint32_t id){(void)context;if(call_count<2)calls[call_count++]=id;}
static int burn_retarget_fixture(uint32_t out[8])
{
    burn_retarget_storage *storage=calloc(1,sizeof(*storage));int status=0;
    rf_particle *records,*saved_particles;rf_particle_list *lists,*saved_lists;
    rf_emitter_slot *slots,*saved_slots;
    memset(out,0,8*sizeof(*out));out[1]=sizeof(*storage);call_count=0;
    if(!storage){out[0]=(uint32_t)RF_IO;return RF_IO;}
    records=storage->records;saved_particles=storage->saved_particles;
    lists=storage->lists;saved_lists=storage->saved_lists;
    slots=storage->slots;saved_slots=storage->saved_slots;
    rf_particle_pool particles;rf_emitter_pool emitters;rf_burn_pool burns={0},saved;
    rf_burn_release_owner_backend release={voice,owner,NULL};uint32_t i,flags=0x1001;
    rf_model_name bones[2]={{"head",4},{"spine03",7}};
    BURN_CHECK(rf_particle_pool_init(&particles,records,lists,133)==RF_OK);
    BURN_CHECK(rf_emitter_pool_init(&emitters,slots,&particles)==RF_OK);
    /* Four live emitters, each with one particle already in flight. */
    emitters.lists[0].next=4;slots[4].previous=128;emitters.lists[1]=(rf_particle_list){0,3};emitters.live=4;
    for(i=0;i<4;i++) {
        slots[i].active=1;slots[i].runtime.enabled=1;
        slots[i].next=i==3?129:i+1;slots[i].previous=i?i-1:129;
        slots[i].runtime.emitter.owner=slots[i].bounds.owner=77;
        lists[5+i]=(rf_particle_list){500+i,500+i};
        records[500+i].next=records[500+i].previous=1605+i;
        records[500+i].owner=77;records[500+i].emitter=i+1;records[500+i].pool=1;
        records[500+i].flags=1;records[500+i].room=1;
    }
    lists[1].next=504;records[504].previous=1601;particles.live[1]=4;
    burns.active_head=1;burns.free_head=2;
    for(i=1;i<8;i++){burns.records[i].next=i==7?2:i+2;burns.records[i].previous=i==1?8:i;}
    burns.records[0].next=burns.records[0].previous=1;
    burns.records[0].target=77;burns.records[0].voice=91;burns.records[0].elapsed=7;
    for(i=0;i<4;i++)burns.records[0].emitters[i]=i+1;
    saved=burns;memcpy(saved_slots,slots,sizeof(storage->slots));memcpy(saved_particles,records,sizeof(storage->records));memcpy(saved_lists,lists,sizeof(storage->lists));
    /* Duplicate/free emitter references must fail before target or flags change. */
    burns.records[0].emitters[3]=1;
    BURN_CHECK(rf_burn_retarget_resolved(&burns,1,88,&flags,bones,2,&emitters,NULL)==RF_FORMAT);
    burns.records[0].emitters[3]=4;slots[3].active=0;
    BURN_CHECK(rf_burn_retarget_resolved(&burns,1,88,&flags,bones,2,&emitters,NULL)==RF_RANGE);slots[3].active=1;
    BURN_CHECK(!memcmp(&burns,&saved,sizeof(burns)) && !memcmp(slots,saved_slots,sizeof(storage->slots)) && flags==0x1001);
    BURN_CHECK(rf_burn_retarget_resolved(&burns,1,88,&flags,bones,2,&emitters,NULL)==RF_OK);
    BURN_CHECK(burns.records[0].attachments[0]==-1 && burns.records[0].attachments[1]==-1);
    BURN_CHECK(burns.records[0].attachments[2]==1 && burns.records[0].attachments[3]==0);
    BURN_CHECK(burns.records[0].target==88 && burns.records[0].fading==1 && burns.records[0].elapsed==0 && flags==0x1201);
    BURN_CHECK(burns.records[0].voice==91 && burns.free_head==2 && burns.active_head==1);
    for(i=0;i<4;i++) {
        BURN_CHECK(slots[i].runtime.emitter.owner==88 && slots[i].bounds.owner==88);
        saved_slots[i].runtime.emitter.owner=saved_slots[i].bounds.owner=88;
    }
    BURN_CHECK(!memcmp(slots,saved_slots,sizeof(storage->slots)) && !memcmp(records,saved_particles,sizeof(storage->records)) && !memcmp(lists,saved_lists,sizeof(storage->lists)));
    out[2]=emitters.live;out[3]=burns.records[0].target;
    /* Missing target releases emitters but leaves their existing particles alive. */
    BURN_CHECK(rf_burn_retarget_resolved(&burns,1,99,NULL,NULL,0,&emitters,&release)==RF_OK);
    BURN_CHECK(call_count==2 && calls[0]==91 && calls[1]==1);
    BURN_CHECK(emitters.live==0 && burns.active_head==0 && particles.live[1]==4);
    for(i=0;i<4;i++)BURN_CHECK(!slots[i].active && records[500+i].emitter==0 && records[500+i].owner==77);
    BURN_CHECK(lists[4].next==500 && lists[4].previous==503);
    BURN_CHECK(burns.records[0].target==UINT32_MAX && burns.records[0].voice==UINT32_MAX);
    out[4]=emitters.live;out[5]=particles.live[1];out[6]=call_count;out[7]=1;
cleanup:
    out[0]=(uint32_t)status;free(storage);return status;
}
#undef BURN_CHECK
