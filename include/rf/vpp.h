#ifndef RF_VPP_H
#define RF_VPP_H
#include <stdint.h>
#include <stdio.h>

/* VPP v1 disk layout: see docs/PROVENANCE.md. No archive-sized allocation. */
typedef struct rf_vpp {
    FILE *stream;
    uint32_t length;
    uint32_t count;
    uint32_t payload_offset;
} rf_vpp;
typedef struct rf_vpp_entry {
    char name[61];
    uint32_t offset;
    uint32_t size;
} rf_vpp_entry;

enum { RF_OK = 0, RF_IO = -1, RF_FORMAT = -2, RF_NOT_FOUND = -3, RF_RANGE = -4 };
typedef int (*rf_vpp_visitor)(const rf_vpp_entry *entry, void *context);
/* Open validates every directory entry. Close before reopening the same handle. */
int rf_vpp_open(rf_vpp *archive, const char *path);
void rf_vpp_close(rf_vpp *archive);
int rf_vpp_visit(rf_vpp *archive, rf_vpp_visitor visitor, void *context);
int rf_vpp_find(rf_vpp *archive, const char *name, rf_vpp_entry *entry);
/* Entry must originate from this archive; offsets and lengths are bounds checked. */
int rf_vpp_read(rf_vpp *archive, const rf_vpp_entry *entry, uint32_t offset, void *data, uint32_t size);
#endif
