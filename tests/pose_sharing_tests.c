#include "rf/entity_assets.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
static void put32(unsigned char *p,uint32_t value){memcpy(p,&value,4);}
static void putf(unsigned char *p,float value){memcpy(p,&value,4);}
int main(void)
{
    unsigned char bytes[328]={0};rf_model_bone bones[2]={{0}};
    rf_entity_skeleton skeleton={0};rf_entity_skeletons skeletons={0};
    rf_entity_model_motion motion={0};rf_entity_model_motions model={0};
    rf_entity_motion_catalog catalog={0};rf_motion_playback_resource resource={0};
    rf_entity_playback_model playback_model={0};rf_entity_playback_resources resources={0};
    static rf_entity_pose_batch batch;
    unsigned i,k,rep;uint32_t hits=0,misses=0,bypasses=0;
    bones[0].parent=-1;bones[1].parent=0;skeleton.bones=bones;skeleton.count=2;
    skeletons.items=&skeleton;skeletons.count=1;
    /* Two bones, each with two rotation and position keys. */
    put32(bytes+80,88);put32(bytes+84,208);
    for(i=0;i<2;++i) {
        unsigned char *track=bytes+88+i*120;
        putf(track,10);track[4]=track[6]=2;
        put32(track+8,0);put32(track+24,4800);
        track[8+11]=64;track[24+11]=64;
        put32(track+40,0);put32(track+80,4800);
        putf(track+44,(float)i);putf(track+84,(float)i+2);
        putf(track+68,(float)i+.5f);putf(track+96,(float)i+1.5f);
    }
    motion.file.resident=bytes;motion.file.entry.size=sizeof(bytes);
    motion.file.header[5]=4800;motion.file.header[6]=2;motion.file.header[18]=sizeof(bytes);
    motion.looping=1;motion.comparison.weight=10;motion.comparison.end_tick=4800;
    model.items=&motion;model.count=1;catalog.models=&model;catalog.model_count=1;
    resource.comparison=motion.comparison;resource.looping=1;resource.references=100;
    resource.markers[0]=160;resource.markers[1]=2400;
    playback_model.resources=&resource;playback_model.count=1;
    resources.models=&playback_model;resources.model_count=1;
    rf_entity_pose_batch_reset(&batch);
    /* Distinct ticks/displacements, hits, eviction, frozen partial generations,
     * enabled overrides, empty playback and generation wrap all compare against
     * the independent uncached evaluation path, including per-actor state. */
    for(k=0;k<80;++k)for(rep=0;rep<2;++rep) {
        rf_entity_pose a={0},b;rf_model_bone_override overrides[2]={{0}};
        float ma[2][12],mb[2][12],da[3]={0},db[3];uint16_t ga[2]={0},gb[2];
        rf_motion_playback_resource ra=resource,rb=resource;
        rf_entity_playback_model pma=playback_model,pmb=playback_model;
        rf_entity_playback_resources rsa=resources,rsb=resources;int sa,sb;
        pma.resources=&ra;pmb.resources=&rb;rsa.models=&pma;rsb.models=&pmb;
        memset(ma,0,sizeof(ma));ma[0][0]=ma[0][4]=ma[0][8]=1;ma[0][9]=99;
        a.bone_count=2;a.matrices=ma;a.generations=ga;a.overrides=overrides;
        rf_motion_playback_initialize(&a.playback);a.playback.phase=(float)(k%32)/32;
        a.playback.completion.active.count=1;a.playback.completion.active.slots[0].weight=1;
        if(k>=32)da[0]=(float)(k-32)*.25f;
        if(k==50)da[1]=-0.0f;
        if(k==51){a.playback.completion.frozen=1;ga[0]=1;}
        if(k==52){a.playback.completion.frozen=1;ga[0]=ga[1]=1;}
        if(k==53){overrides[0].enabled=1;overrides[0].weight=.5f;overrides[0].basis[0]=overrides[0].basis[4]=overrides[0].basis[8]=1;}
        if(k==54)a.playback.completion.active.count=0;
        if(k==55){a.playback.generation=65535;ga[0]=ga[1]=65535;}
        if(k==56){hits+=batch.hits;misses+=batch.misses;bypasses+=batch.bypasses;rf_entity_pose_batch_reset(&batch);}
        b=a;b.matrices=mb;b.generations=gb;memcpy(mb,ma,sizeof(ma));memcpy(gb,ga,sizeof(ga));memcpy(db,da,sizeof(da));
        sa=rf_entity_pose_advance_shared(&a,&skeletons,&catalog,&rsa,1.0f/60,da,&batch);
        sb=rf_entity_pose_advance(&b,&skeletons,&catalog,&rsb,1.0f/60,db);
        CHECK(sa==RF_OK && sb==sa);CHECK(!memcmp(ma,mb,sizeof(ma)));CHECK(!memcmp(ga,gb,sizeof(ga)));
        CHECK(!memcmp(da,db,sizeof(da)));CHECK(!memcmp(&a.playback,&b.playback,sizeof(a.playback)));
        CHECK(!memcmp(&ra,&rb,sizeof(ra)));CHECK(batch.count<=16);
    }
    hits+=batch.hits;misses+=batch.misses;bypasses+=batch.bypasses;
    CHECK(hits>60 && misses>60 && bypasses>=6);CHECK(sizeof(batch)<44u*1024u);
    printf("Shared pose differential: 160 cases, %u hits, %u misses, %u bypasses, %zu bytes\n",hits,misses,bypasses,sizeof(batch));
    return 0;
}
