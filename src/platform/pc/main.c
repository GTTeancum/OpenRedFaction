#include "rf/vpp.h"
#include "rf/level.h"
#include <stdio.h>

static int show_entry(const rf_vpp_entry *entry, void *context)
{
    (void)context;
    printf("%10u %10u %s\n", entry->offset, entry->size, entry->name);
    return 0;
}

int main(int argc, char **argv)
{
    rf_vpp archive;
    int result;
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Red Faction reconstruction: asset diagnostic, not a playable game.\nUsage: %s archive.vpp [level.rfl]\n", argv[0]);
        return 2;
    }
    result = rf_vpp_open(&archive, argv[1]);
    if (result != RF_OK) { fprintf(stderr, "Archive validation failed: %d\n", result); return 1; }
    if (argc == 3) {
        rf_level level;
        uint32_t i;
        result = rf_level_open(&level, &archive, argv[2]);
        if (result == RF_OK) {
            printf("RFL %u %u %u %s\n", level.version, level.entry.size, level.section_count, level.name);
            printf("spawn %.9g %.9g %.9g\n", level.player_position[0], level.player_position[1], level.player_position[2]);
            for (i = 0; i < 3; ++i) printf("row %.9g %.9g %.9g\n", level.player_orientation[i][0], level.player_orientation[i][1], level.player_orientation[i][2]);
            for (i = 0; i < level.section_count; ++i) printf("section %08x %u %u\n", level.sections[i].type, level.sections[i].offset, level.sections[i].size);
        } else fprintf(stderr, "Level validation failed: %d\n", result);
    } else {
        printf("VPP v1: %u entries, %u bytes; streaming directory access\n", archive.count, archive.length);
        result = rf_vpp_visit(&archive, show_entry, NULL);
    }
    rf_vpp_close(&archive);
    return result == RF_OK ? 0 : 1;
}
