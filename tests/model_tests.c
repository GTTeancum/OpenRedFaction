#include "rf/model.h"
#include <limits.h>
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
    return 0;
}
