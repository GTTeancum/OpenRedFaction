#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "rf/vpp.h"
/* Minimal owner seams compile the production callback/codec unchanged. The
 * Xbox fixture separately exercises real registrations and authored events. */
#define RF_OBJECT_CAPACITY 1024
#define RF_CHECKPOINT_FILE_MAX 1048576
struct rf_damage_effect_state {uint32_t handle,affiliation,health_guard,flags_guard;};
typedef struct rf_damage_effect_state rf_damage_effect_state;
typedef struct damage {struct {rf_damage_effect_state effects;} state;} damage;
typedef struct scene_passive_vehicle {uint32_t uid,handle;damage damage;} scene_passive_vehicle;
typedef struct entry {struct {uint32_t handle;} registration;} entry;
typedef struct scene_driller_damage_runtime {damage damage;entry *entry;uint32_t active;} scene_driller_damage_runtime;
static scene_driller_damage_runtime *scene_driller_damage_owner;
static scene_passive_vehicle *campaign_passive_vehicles;
static uint32_t campaign_passive_vehicle_count,campaign_authored_vehicle_uid,campaign_authored_vehicle_handle,campaign_registry;
static struct {struct {uint32_t count;struct {struct {int32_t uid;} record;} items[3];} records;
    struct {struct {uint32_t friendliness;} spawn;} items[3];} campaign_seeds;
static void *registered[4];static uint32_t handles[4];
static void *rf_object_registry_lookup(const void *registry,uint32_t handle)
{(void)registry;for(uint32_t i=0;i<4;i++)if(handles[i]==handle)return registered[i];return NULL;}
static uint32_t scene_driller_damage_runtime_valid(const scene_driller_damage_runtime *p)
{return p&&p->active&&p->entry&&p->entry->registration.handle==p->damage.state.effects.handle;}
static uint32_t scene_history_word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void scene_history_put(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
#include "../src/diagnostic/scene_vehicle_allegiance_decl.inc"
#include "../src/diagnostic/scene_vehicle_allegiance.inc"
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"allegiance check%u line%d\n",checks,__LINE__);return 1;}}while(0)
int main(void)
{
    uint32_t checks=0,bytes=0,nested;unsigned char wire[128],baseline[128],bad[128];const unsigned char *inner;
    scene_vehicle_allegiance_stage *stage=NULL;entry e={{0x10001}};
    scene_driller_damage_runtime active={{{{0x10001,0,5000,0x4000}}},&e,1};
    scene_passive_vehicle passive[2]={{26,0x20002,{{{0x20002,1,900,0}}}},
                                    {27,0x30003,{{{0x30003,2,400,0}}}}};
    campaign_authored_vehicle_uid=9627;campaign_authored_vehicle_handle=0x10001;scene_driller_damage_owner=&active;
    campaign_passive_vehicle_count=2;campaign_passive_vehicles=passive;campaign_seeds.records.count=3;
    campaign_seeds.records.items[0].record.uid=9627;campaign_seeds.items[0].spawn.friendliness=0;
    campaign_seeds.records.items[1].record.uid=26;campaign_seeds.items[1].spawn.friendliness=1;
    campaign_seeds.records.items[2].record.uid=27;campaign_seeds.items[2].spawn.friendliness=2;
    handles[0]=0x10001;registered[0]=&e.registration;handles[1]=0x20002;registered[1]=passive;
    handles[2]=0x30003;registered[2]=passive+1;
    memset(wire,0x7c,sizeof(wire));memcpy(wire,"RFPV",4);memcpy(baseline,wire,sizeof(wire));
    CHECK(scene_vehicle_allegiance_legacy_save_allowed());
    CHECK(!scene_vehicle_allegiance_capture(wire,sizeof(wire),12,&bytes)&&bytes==12&&!memcmp(wire,baseline,sizeof(wire)));
    CHECK(!scene_vehicle_allegiance_prepare(wire,bytes,0,&stage,&inner,&nested)&&!stage&&inner==wire&&nested==bytes);
    CHECK(scene_vehicle_set_friendliness(0x90002,0)==RF_NOT_FOUND&&passive[0].damage.state.effects.affiliation==1);
    registered[1]=passive+1;CHECK(scene_vehicle_set_friendliness(0x20002,0)==RF_NOT_FOUND);registered[1]=passive;
    passive[0].damage.state.effects.handle=0x90002;CHECK(scene_vehicle_set_friendliness(0x20002,0)==RF_NOT_FOUND);passive[0].damage.state.effects.handle=0x20002;
    CHECK(!scene_vehicle_set_friendliness(0x20002,0)&&passive[0].damage.state.effects.affiliation==0);
    CHECK(!scene_vehicle_set_friendliness(0x20002,0)&&passive[0].damage.state.effects.affiliation==0);
    CHECK(passive[0].damage.state.effects.health_guard==900&&passive[0].damage.state.effects.flags_guard==0);
    CHECK(!scene_vehicle_set_friendliness(0x10001,UINT32_MAX)&&active.damage.state.effects.affiliation==UINT32_MAX);
    CHECK(active.damage.state.effects.health_guard==5000&&active.damage.state.effects.flags_guard==0x4000);
    CHECK(!scene_vehicle_allegiance_legacy_save_allowed());
    CHECK(scene_vehicle_allegiance_capture(wire,27,12,&bytes)==RF_RANGE&&!memcmp(wire,baseline,sizeof(wire)));
    CHECK(!scene_vehicle_allegiance_capture(wire,sizeof(wire),12,&bytes)&&bytes==44);
    CHECK(!memcmp(wire,"RFAL",4)&&scene_history_word(wire+4)==1&&scene_history_word(wire+12)==2);
    CHECK(!memcmp(wire+16,baseline,12)&&scene_history_word(wire+28)==9627&&scene_history_word(wire+32)==UINT32_MAX&&
        scene_history_word(wire+36)==26&&scene_history_word(wire+40)==0);
    CHECK(scene_vehicle_allegiance_prepare(wire,bytes,1,&stage,&inner,&nested)==RF_FORMAT&&!stage);
    CHECK(!scene_vehicle_allegiance_prepare(wire,bytes,4096,&stage,&inner,&nested)&&stage&&nested==12&&inner==wire+16);
    CHECK(scene_vehicle_allegiance_bytes(stage)==sizeof(*stage)+3*sizeof(*stage->rows));
    active.damage.state.effects.affiliation=0;passive[0].damage.state.effects.affiliation=1;passive[1].damage.state.effects.affiliation=0xabababab;
    CHECK(stage->rows[0].value==UINT32_MAX&&stage->rows[1].value==0&&stage->rows[2].value==2);
    scene_vehicle_allegiance_assign(stage);
    CHECK(active.damage.state.effects.affiliation==UINT32_MAX&&passive[0].damage.state.effects.affiliation==0&&passive[1].damage.state.effects.affiliation==2);
    scene_vehicle_allegiance_close(&stage);CHECK(!stage);
    for(uint32_t fault=0;fault<8;fault++){
        memcpy(bad,wire,bytes);uint32_t n=bytes;
        if(fault==0)scene_history_put(bad+4,2);
        if(fault==1)scene_history_put(bad+8,UINT32_MAX);
        if(fault==2)scene_history_put(bad+12,0);
        if(fault==3)scene_history_put(bad+28,27+1000);
        if(fault==4)scene_history_put(bad+36,9627);
        if(fault==5)--n;
        if(fault==6)scene_history_put(bad+12,UINT32_MAX);
        if(fault==7){scene_history_put(bad+8,0);n=32;memcpy(bad+16,wire+28,16);}
        CHECK(scene_vehicle_allegiance_prepare(bad,n,4096,&stage,&inner,&nested)==RF_FORMAT&&!stage);
        CHECK(active.damage.state.effects.affiliation==UINT32_MAX&&passive[0].damage.state.effects.affiliation==0);
    }
    /* Ownership changes do not change identity: selected26, parked9627 use
     * the same saved UID rows. These are real damage-owner transfer semantics. */
    passive[0].uid=9627;campaign_authored_vehicle_uid=26;
    CHECK(!scene_vehicle_allegiance_prepare(wire,bytes,4096,&stage,&inner,&nested));scene_vehicle_allegiance_assign(stage);
    CHECK(active.damage.state.effects.affiliation==0&&passive[0].damage.state.effects.affiliation==UINT32_MAX);
    scene_vehicle_allegiance_close(&stage);
    registered[0]=NULL;CHECK(scene_vehicle_set_friendliness(0x10001,7)==RF_NOT_FOUND);registered[0]=&e.registration;
    campaign_seeds.records.items[2].record.uid=26;
    CHECK(scene_vehicle_allegiance_prepare(wire,bytes,4096,&stage,&inner,&nested)==RF_FORMAT&&!stage);
    printf("vehicle allegiance PASS: %u callback/codec checks; exact words, stable owner state, sparse/legacy bytes, atomic malformed rejection and UID transfer\n",checks);
    return 0;
}
