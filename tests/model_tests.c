#include "rf/model.h"
#include <limits.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main(void)
{
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
