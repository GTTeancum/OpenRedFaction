#include "rf/vpp.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "line %d: %s\n", __LINE__, #expr); return 1; } } while (0)
static unsigned char fixture[8192];
static void put32(unsigned offset, uint32_t value)
{
    unsigned i;
    for (i = 0; i < 4; ++i) fixture[offset + i] = (unsigned char)(value >> (8 * i));
}
static int save(unsigned length)
{
    FILE *file = fopen("vpp-fixture.tmp", "wb");
    int ok;
    if (!file) return 0;
    ok = fwrite(fixture, 1, length, file) == length;
    return fclose(file) == 0 && ok;
}
static int read_during_visit(const rf_vpp_entry *entry, void *context)
{
    unsigned char bytes[3];
    return rf_vpp_read((rf_vpp *)context, entry, 0, bytes, sizeof(bytes));
}
int main(void)
{
    rf_vpp archive;
    rf_vpp_entry entry;
    char data[4] = {0};
    put32(0, 0x51890aceu); put32(4, 1); put32(8, 2); put32(12, sizeof(fixture));
    memcpy(fixture + 2048, "First.TBL", 10); put32(2048 + 60, 3);
    memcpy(fixture + 2112, "second.tbl", 11); put32(2112 + 60, 3);
    memcpy(fixture + 4096, "one", 3); memcpy(fixture + 6144, "two", 3);
    CHECK(save(sizeof(fixture)));
    CHECK(rf_vpp_open(&archive, "vpp-fixture.tmp") == RF_OK);
    CHECK(rf_vpp_find(&archive, "FIRST.tbl", &entry) == RF_OK);
    CHECK(entry.offset == 4096 && entry.size == 3);
    CHECK(rf_vpp_read(&archive, &entry, 0, data, 3) == RF_OK && strcmp(data, "one") == 0);
    CHECK(rf_vpp_read(&archive, &entry, 2, data, 2) == RF_RANGE);
    CHECK(rf_vpp_read(&archive, &entry, 0xffffffffu, data, 1) == RF_RANGE);
    CHECK(rf_vpp_read(&archive, &entry, 3, NULL, 0) == RF_OK);
    CHECK(rf_vpp_visit(&archive, read_during_visit, &archive) == RF_OK);
    CHECK(rf_vpp_find(&archive, "second.tbl", &entry) == RF_OK && entry.offset == 6144);
    CHECK(rf_vpp_read(&archive, &entry, 0, data, 3) == RF_OK && strcmp(data, "two") == 0);
    CHECK(rf_vpp_find(&archive, "missing.tbl", &entry) == RF_NOT_FOUND);
    rf_vpp_close(&archive);
    put32(2112 + 60, 0xffffffffu); CHECK(save(sizeof(fixture)));
    CHECK(rf_vpp_open(&archive, "vpp-fixture.tmp") == RF_FORMAT && !archive.stream);
    put32(2112 + 60, 3); memset(fixture + 2048, 'x', 60); CHECK(save(sizeof(fixture)));
    CHECK(rf_vpp_open(&archive, "vpp-fixture.tmp") == RF_FORMAT);
    fixture[2048 + 9] = 0; put32(8, 0xffffffffu); CHECK(save(sizeof(fixture)));
    CHECK(rf_vpp_open(&archive, "vpp-fixture.tmp") == RF_FORMAT);
    put32(8, 2); CHECK(save(4096));
    CHECK(rf_vpp_open(&archive, "vpp-fixture.tmp") == RF_FORMAT);
    put32(4, 2); CHECK(save(sizeof(fixture)));
    CHECK(rf_vpp_open(&archive, "vpp-fixture.tmp") == RF_FORMAT);
    CHECK(remove("vpp-fixture.tmp") == 0);
    puts("VPP bounds, malformed inputs, lookup, and interleaved payload reads passed");
    return 0;
}
