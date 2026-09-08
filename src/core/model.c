#include "rf/model.h"
#include <limits.h>

static int valid_name(rf_model_name name)
{
    size_t i;
    if (!name.data && name.length) return 0;
    for (i = 0; i < name.length; ++i)
        if (!name.data[i]) return 0;
    return 1;
}

static unsigned char fold(unsigned char c)
{
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

int rf_model_find_tag(const rf_model_name_group groups[3],
                      rf_model_name query, int32_t *index)
{
    uint32_t g, n, base = 0, total = 0;
    if (!groups || !index) return RF_RANGE;
    if (!valid_name(query)) return RF_FORMAT;
    for (g = 0; g < 3; ++g) {
        if ((groups[g].count && !groups[g].names) ||
            groups[g].count > (uint32_t)INT32_MAX - total) return RF_RANGE;
        total += groups[g].count;
    }
    /* Reconstructed from RF.exe 0x51d5b0 and default-locale 0x57c130.
     * Stop at the first match, including duplicates across groups. */
    for (g = 0; g < 3; ++g) {
        for (n = 0; n < groups[g].count; ++n) {
            rf_model_name name = groups[g].names[n];
            size_t i;
            if (!valid_name(name)) return RF_FORMAT;
            if (name.length != query.length) continue;
            for (i = 0; i < name.length; ++i)
                if (fold((unsigned char)name.data[i]) != fold((unsigned char)query.data[i])) break;
            if (i == name.length) {
                *index = (int32_t)(base + n);
                return RF_OK;
            }
        }
        base += groups[g].count;
    }
    return RF_NOT_FOUND;
}
