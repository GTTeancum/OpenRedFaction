#include "rf/vpp.h"
#include <limits.h>
#include <string.h>

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t aligned(uint32_t n) { return ((uint64_t)n + 2047u) & ~(uint64_t)2047u; }

int rf_vpp_visit(rf_vpp *archive, rf_vpp_visitor visitor, void *context)
{
    uint32_t index;
    uint64_t cursor;
    if (!archive || !archive->stream) return RF_IO;
    cursor = archive->payload_offset;
    for (index = 0; index < archive->count; ++index) {
        unsigned char raw[64];
        rf_vpp_entry entry;
        /* Seek for each entry: a visitor may read payloads without disrupting traversal. */
        if (fseek(archive->stream, (long)(2048u + index * 64u), SEEK_SET) ||
            fread(raw, 1, sizeof(raw), archive->stream) != sizeof(raw)) return RF_IO;
        if (!memchr(raw, 0, 60) || raw[0] == 0) return RF_FORMAT;
        memcpy(entry.name, raw, 60);
        entry.name[60] = 0;
        entry.size = le32(raw + 60);
        if (cursor + aligned(entry.size) > archive->length) return RF_FORMAT;
        entry.offset = (uint32_t)cursor;
        if (visitor) {
            int result = visitor(&entry, context);
            if (result) return result;
        }
        cursor += aligned(entry.size);
    }
    return RF_OK;
}

int rf_vpp_open(rf_vpp *archive, const char *path)
{
    unsigned char header[16];
    long length;
    int result;
    if (!archive || !path) return RF_IO;
    memset(archive, 0, sizeof(*archive));
    archive->stream = fopen(path, "rb");
    if (!archive->stream) return RF_IO;
    if (fseek(archive->stream, 0, SEEK_END) || (length = ftell(archive->stream)) < 2048 ||
        (uint64_t)length > INT32_MAX || fseek(archive->stream, 0, SEEK_SET) ||
        fread(header, 1, sizeof(header), archive->stream) != sizeof(header)) {
        rf_vpp_close(archive);
        return RF_IO;
    }
    archive->length = (uint32_t)length;
    archive->count = le32(header + 8);
    if (le32(header) != 0x51890aceu || le32(header + 4) != 1 ||
        archive->count > 65536 || le32(header + 12) != archive->length) {
        rf_vpp_close(archive);
        return RF_FORMAT;
    }
    archive->payload_offset = 2048u + (uint32_t)aligned(archive->count * 64u);
    if (archive->payload_offset > archive->length) {
        rf_vpp_close(archive);
        return RF_FORMAT;
    }
    result = rf_vpp_visit(archive, NULL, NULL);
    if (result != RF_OK) rf_vpp_close(archive);
    return result;
}

void rf_vpp_close(rf_vpp *archive)
{
    if (archive) {
        if (archive->stream) fclose(archive->stream);
        memset(archive, 0, sizeof(*archive));
    }
}

static unsigned char fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c; }
typedef struct find_context { const char *name; rf_vpp_entry *entry; } find_context;
static int find_entry(const rf_vpp_entry *entry, void *context)
{
    find_context *find = (find_context *)context;
    const unsigned char *a = (const unsigned char *)entry->name;
    const unsigned char *b = (const unsigned char *)find->name;
    while (*a && fold(*a) == fold(*b)) { ++a; ++b; }
    if (*a || *b) return 0;
    *find->entry = *entry;
    return 1;
}

int rf_vpp_find(rf_vpp *archive, const char *name, rf_vpp_entry *entry)
{
    find_context context;
    int result;
    if (!name || !entry) return RF_RANGE;
    context.name = name;
    context.entry = entry;
    result = rf_vpp_visit(archive, find_entry, &context);
    return result == 1 ? RF_OK : result == RF_OK ? RF_NOT_FOUND : result;
}

int rf_vpp_read(rf_vpp *archive, const rf_vpp_entry *entry, uint32_t offset, void *data, uint32_t size)
{
    if (!archive || !archive->stream) return RF_IO;
    if (!entry || (size && !data) || offset > entry->size || size > entry->size - offset ||
        entry->offset < archive->payload_offset || (uint64_t)entry->offset + entry->size > archive->length) return RF_RANGE;
    if (!size) return RF_OK;
    if (fseek(archive->stream, (long)(entry->offset + offset), SEEK_SET) ||
        fread(data, 1, size, archive->stream) != size) return RF_IO;
    return RF_OK;
}
