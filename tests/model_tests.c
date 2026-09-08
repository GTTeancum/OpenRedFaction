#include "rf/model.h"
#include <limits.h>
#include <string.h>
#include <math.h>
#include <float.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main(void)
{
    {
        rf_model_material_record source={{0}}; rf_model_material_instance instance={0},snapshot;
        uint32_t a[]={11,22,33},b[]={44,55},c[]={66};
        const uint32_t *arrays[]={a,b,c}; uint32_t capacities[]={3,2,1};
        int32_t counts[]={3,2,1}; uint32_t exact=(uint32_t)sizeof(instance)+24;
        memcpy(source.bytes+0x7c,&counts[0],4);memcpy(source.bytes+0xb8,&counts[1],4);memcpy(source.bytes+0xc0,&counts[2],4);
        CHECK(rf_model_material_instance_open(&instance,&source,3,arrays,capacities,exact-1)==RF_RANGE);
        CHECK(!instance.storage && !instance.accounted_bytes);
        CHECK(rf_model_material_instance_open(&instance,&source,3,arrays,capacities,exact)==RF_OK);
        CHECK(instance.accounted_bytes==exact && instance.counts[0]==3 && instance.counts[1]==2 && instance.counts[2]==1);
        CHECK(instance.arrays[0][2]==33 && instance.arrays[1][1]==55 && instance.arrays[2][0]==66);
        a[0]=99; CHECK(instance.arrays[0][0]==11); instance.arrays[1][0]=77; CHECK(b[0]==44);
        CHECK(!memcmp(instance.record.bytes+0x80,"\0\0\0\0",4));
        snapshot=instance;
        CHECK(rf_model_material_instance_open(&instance,&source,1,arrays,capacities,exact)==RF_RANGE);
        CHECK(!memcmp(&snapshot,&instance,sizeof(instance)));
        rf_model_material_instance_close(&instance); rf_model_material_instance_close(&instance);
        CHECK(rf_model_material_instance_open(&instance,&source,1,arrays,capacities,(uint32_t)sizeof(instance)+12)==RF_OK);
        CHECK(instance.counts[0]==1 && instance.counts[1]==1 && instance.counts[2]==1 && instance.arrays[0][0]==99);
        rf_model_material_instance_close(&instance);
        capacities[0]=2; CHECK(rf_model_material_instance_open(&instance,&source,3,arrays,capacities,exact)==RF_RANGE);
        counts[0]=INT32_MAX;memcpy(source.bytes+0x7c,&counts[0],4);capacities[0]=INT32_MAX;
        CHECK(rf_model_material_instance_open(&instance,&source,3,arrays,capacities,UINT32_MAX)==RF_RANGE);
        memset(source.bytes+0x14,'x',36);
        CHECK(rf_model_material_instance_open(&instance,&source,1,arrays,capacities,exact)==RF_FORMAT);
        CHECK(!instance.storage && !instance.accounted_bytes);
        memset(&source,0,sizeof(source));
        CHECK(rf_model_material_instance_open(&instance,&source,3,arrays,capacities,(uint32_t)sizeof(instance))==RF_OK);
        CHECK(!instance.storage && instance.accounted_bytes==sizeof(instance));rf_model_material_instance_close(&instance);
    }
    {
        rf_model_bone bones[2]={{0},{0}}; rf_motion_playback_state state={0};
        float displacement[3]={4,5,6}, matrices[2][12]={{1,0,0,0,1,0,0,0,1,7,8,9},{0}};
        uint16_t stamps[2]={1,0};
        bones[0].parent=bones[1].parent=-1; state.completion.active.primary_slot=-1; state.generation=1;
        CHECK(rf_model_evaluate_playback(bones,2,&state,NULL,NULL,0,displacement,matrices,stamps,2)==RF_OK);
        CHECK(matrices[0][9]==7 && matrices[1][9]==4 && matrices[1][10]==5 && stamps[1]==1 && displacement[0]==0);
        displacement[0]=2;
        CHECK(rf_model_evaluate_playback(bones,2,&state,NULL,NULL,0,displacement,matrices,stamps,2)==RF_OK);
        CHECK(matrices[0][9]==7 && matrices[1][9]==4 && displacement[0]==2);
        state.generation=65536;
        CHECK(rf_model_evaluate_playback(bones,2,&state,NULL,NULL,0,displacement,matrices,stamps,2)==RF_FORMAT && displacement[0]==2);
        state.generation=0;
        CHECK(rf_model_evaluate_playback(bones,2,&state,NULL,NULL,0,displacement,matrices,stamps,2)==RF_OK);
        CHECK(matrices[0][9]==2 && matrices[1][9]==0 && stamps[0]==0 && stamps[1]==0 && displacement[0]==0);
    }
    {
        rf_model_bone bone={0}; rf_motion_playback_state state={0};
        float displacement[3]={NAN,2,3}, matrices[1][12], saved[1][12];
        bone.parent=-1; state.completion.active.primary_slot=-1;
        memset(matrices,0x5a,sizeof(matrices)); memcpy(saved,matrices,sizeof(saved));
        CHECK(rf_model_sample_playback(&bone,1,&state,NULL,NULL,0,displacement,matrices,1)==RF_FORMAT);
        CHECK(memcmp(matrices,saved,sizeof(saved))==0 && isnan(displacement[0]) && displacement[1]==2);
        displacement[0]=1;
        CHECK(rf_model_sample_playback(&bone,1,&state,NULL,NULL,0,displacement,matrices,1)==RF_OK);
        CHECK(matrices[0][9]==1 && matrices[0][10]==2 && matrices[0][11]==3);
        CHECK(displacement[0]==0 && displacement[1]==0 && displacement[2]==0);
        CHECK(rf_model_sample_playback(&bone,1,&state,NULL,NULL,0,displacement,matrices,1)==RF_OK);
        CHECK(matrices[0][9]==0 && matrices[0][10]==0 && matrices[0][11]==0);
    }
    {
        float local[12]={1,0,0,0,1,0,0,0,1,1,2,3};
        float orientation[9]={1,0,0,0,1,0,0,0,1}, position[3]={4,5,6};
        float saved[12]; memcpy(saved,local,sizeof(saved));
        orientation[0]=NAN;
        CHECK(rf_model_place_tag(local,orientation,position,local)==RF_FORMAT);
        CHECK(memcmp(local,saved,sizeof(saved))==0);
        orientation[0]=1;
        CHECK(rf_model_place_tag(local,orientation,position,local)==RF_OK);
        CHECK(local[9]==5 && local[10]==7 && local[11]==9);
    }
    {
        float q[2][4]={{0,0,0,1},{0,0,0,1}}, p[2][3]={{1,2,3},{3,4,5}}, w[2]={.5f,.5f};
        float out[12], sentinel[12];
        memset(out,0x5a,sizeof(out)); memcpy(sentinel,out,sizeof(out));
        CHECK(rf_model_blend_pose(q,p,w,0,out)==RF_RANGE);
        CHECK(rf_model_blend_pose(q,p,w,17,out)==RF_RANGE);
        w[0]=NAN;
        CHECK(rf_model_blend_pose(q,p,w,2,out)==RF_FORMAT);
        CHECK(memcmp(out,sentinel,sizeof(out))==0);
        w[0]=.5f;
        CHECK(rf_model_blend_pose(q,p,w,2,out)==RF_OK);
        CHECK(out[9]==2 && out[10]==3 && out[11]==4);
    }
    rf_model_name_group groups[3] = {{0, 0}, {0, 0}, {0, 0}};
    rf_model_name query = {"eye", 3}, bad = {"e\0e", 3};
    int32_t index = 123;
    CHECK(rf_model_find_tag(NULL, query, &index) == RF_RANGE);
    CHECK(rf_model_find_tag(groups, query, NULL) == RF_RANGE);
    CHECK(rf_model_find_tag(groups, query, &index) == RF_NOT_FOUND);
    CHECK(rf_model_find_tag(groups, bad, &index) == RF_FORMAT);
    groups[0].count = 1;
    CHECK(rf_model_find_tag(groups, query, &index) == RF_RANGE);
    groups[0].names = &bad;
    CHECK(rf_model_find_tag(groups, query, &index) == RF_FORMAT);
    groups[0].names = &query;
    groups[0].count = INT32_MAX;
    groups[1].names = &query; groups[1].count = 1;
    CHECK(rf_model_find_tag(groups, query, &index) == RF_RANGE);
    CHECK(index == 123);
    groups[0].count = 0; groups[1].count = 0;
    query.data = NULL; query.length = 0;
    groups[2].names = &query; groups[2].count = 1;
    CHECK(rf_model_find_tag(groups, query, &index) == RF_OK && index == 0);
    {
        rf_model_bone bones[4] = {0};
        uint8_t order[4] = {99,99,99,99};
        const uint8_t expected[4] = {1,2,0,3};
        bones[0].parent = 2; bones[1].parent = -1;
        bones[2].parent = -1; bones[3].parent = 0;
        CHECK(rf_model_bone_order(bones, 4, order, 3) == RF_RANGE && order[0] == 99);
        CHECK(rf_model_bone_order(bones, 4, order, 4) == RF_OK);
        CHECK(memcmp(order, expected, 4) == 0);
        bones[2].parent = 3;
        CHECK(rf_model_bone_order(bones, 4, order, 4) == RF_FORMAT);
        CHECK(memcmp(order, expected, 4) == 0);
        bones[2].parent = -2;
        CHECK(rf_model_bone_order(bones, 4, order, 4) == RF_FORMAT);
        bones[2].parent = 4;
        CHECK(rf_model_bone_order(bones, 4, order, 4) == RF_FORMAT);
        CHECK(rf_model_bone_order(NULL, 0, NULL, 0) == RF_OK);
        CHECK(rf_model_bone_order(bones, 257, order, 257) == RF_RANGE);
    }
    {
        float a[12] = {1,0,0, 0,1,0, 0,0,1, 1,2,3};
        float b[12] = {1,0,0, 0,1,0, 0,0,1, 4,5,6};
        float out[12], sentinel[12];
        CHECK(rf_model_compose_transform(a, b, out) == RF_OK);
        CHECK(out[9] == 5 && out[10] == 7 && out[11] == 9);
        CHECK(rf_model_compose_transform(a, b, a) == RF_OK);
        CHECK(memcmp(a, out, sizeof(a)) == 0);
        CHECK(rf_model_compose_transform(b, b, b) == RF_OK);
        CHECK(b[9] == 8 && b[10] == 10 && b[11] == 12);
        memcpy(sentinel, out, sizeof(out));
        a[0] = NAN;
        CHECK(rf_model_compose_transform(a, b, out) == RF_FORMAT);
        CHECK(memcmp(out, sentinel, sizeof(out)) == 0);
        a[0] = FLT_MAX; b[0] = FLT_MAX;
        CHECK(rf_model_compose_transform(a, b, out) == RF_RANGE);
        CHECK(memcmp(out, sentinel, sizeof(out)) == 0);
        CHECK(rf_model_compose_transform(NULL, b, out) == RF_RANGE);
    }
    {
        float q[4] = {0, 0, 0, 0}, p[3] = {1, 2, 3}, out[12], sentinel[12];
        memset(out, 0xa5, sizeof(out)); memcpy(sentinel, out, sizeof(out));
        CHECK(rf_model_bone_transform(q, p, out) == RF_FORMAT);
        q[3] = NAN;
        CHECK(rf_model_bone_transform(q, p, out) == RF_FORMAT);
        q[3] = 1; p[0] = INFINITY;
        CHECK(rf_model_bone_transform(q, p, out) == RF_FORMAT);
        CHECK(memcmp(out, sentinel, sizeof(out)) == 0);
        p[0] = 1;
        CHECK(rf_model_bone_transform(q, p, out) == RF_OK);
        CHECK(out[0] == 1 && out[4] == 1 && out[8] == 1);
        CHECK(out[9] == 1 && out[10] == 2 && out[11] == 3);
        q[3] = 0;
        CHECK(rf_model_attachment_transform(q, p, out) == RF_OK);
        CHECK(out[0] == 1 && out[4] == 1 && out[8] == 1);
        q[2] = 1; q[3] = 1;
        CHECK(rf_model_attachment_transform(q, p, out) == RF_OK);
        CHECK(out[0] == -1 && out[1] == -2 && out[3] == 2);
        memcpy(sentinel, out, sizeof(out));
        q[2] = FLT_MAX;
        CHECK(rf_model_attachment_transform(q, p, out) == RF_RANGE);
        CHECK(memcmp(out, sentinel, sizeof(out)) == 0);
    }
    {
        unsigned char payload[60] = {1, 0, 0, 0, 'r', 'o', 'o', 't'};
        rf_model_bone bone, sentinel;
        uint32_t count = 99;
        memset(&bone, 0xa5, sizeof(bone)); memcpy(&sentinel, &bone, sizeof(bone));
        memset(payload + 56, 0xff, 4);
        CHECK(rf_model_decode_bones(payload, 59, &bone, 1, &count) == RF_FORMAT);
        CHECK(rf_model_decode_bones(payload, 60, &bone, 0, &count) == RF_RANGE);
        memset(payload + 56, 0, 4); /* Self-parent cycle. */
        CHECK(rf_model_decode_bones(payload, 60, &bone, 1, &count) == RF_FORMAT);
        payload[56] = 1; /* Out-of-range parent. */
        CHECK(rf_model_decode_bones(payload, 60, &bone, 1, &count) == RF_FORMAT);
        memset(payload + 56, 0xff, 4);
        payload[30] = 0xc0; payload[31] = 0x7f; /* NaN quaternion component. */
        CHECK(rf_model_decode_bones(payload, 60, &bone, 1, &count) == RF_FORMAT);
        CHECK(count == 99 && memcmp(&bone, &sentinel, sizeof(bone)) == 0);
        payload[30] = 0; payload[31] = 0;
        CHECK(rf_model_decode_bones(payload, 60, &bone, 1, &count) == RF_OK);
        CHECK(count == 1 && bone.parent == -1 && strcmp(bone.name, "root") == 0);
        payload[0] = 0;
        CHECK(rf_model_decode_bones(payload, 4, NULL, 0, &count) == RF_OK && count == 0);
    }
    return 0;
}
