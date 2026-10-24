#include "rf/vpp.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Fast archive path: default on NXDK (Xbox), opt-in elsewhere (-DRF_VPP_FAST).
 * The stdio-only path below is the original implementation, unchanged apart
 * from call counters and rf_vpp_read_raw. */
#if defined(NXDK) && !defined(RF_VPP_STDIO_ONLY) && !defined(RF_VPP_FAST)
#define RF_VPP_FAST 1
#endif
#if defined(NXDK) && defined(RF_VPP_FAST)
#define RF_VPP_NATIVE 1
#include "../platform/xbox/file_io.h"
#else
#include <time.h>
#endif

uint32_t rf_vpp_io_stats[RF_VPP_IO_COUNT];
uint32_t rf_vpp_io_stages[RF_VPP_IO_STAGES * 3];

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t aligned(uint32_t n) { return ((uint64_t)n + 2047u) & ~(uint64_t)2047u; }
static unsigned char fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c; }

#ifdef RF_VPP_NATIVE
static uint64_t vpp_now(void) { return rf_xbox_file_ticks(); }
static uint32_t vpp_to_ms(uint64_t ticks)
{
    static uint64_t per_ms;
    if (!per_ms) {
        per_ms = rf_xbox_file_tick_rate() / 1000u;
        if (!per_ms) per_ms = 1;
    }
    return (uint32_t)(ticks / per_ms);
}
#else
static uint64_t vpp_now(void)
{
    clock_t now = clock();
    return now == (clock_t)-1 ? 0 : (uint64_t)now;
}
static uint32_t vpp_to_ms(uint64_t ticks) { return (uint32_t)(ticks * 1000u / (uint64_t)CLOCKS_PER_SEC); }
#endif

static uint64_t vpp_stage_origin, vpp_open_ticks;
void rf_vpp_io_stage(uint32_t stage)
{
    uint64_t now = vpp_now();
    if (stage >= RF_VPP_IO_STAGES) return;
    if (!stage) {
        vpp_stage_origin = now;
        memset(rf_vpp_io_stages, 0, sizeof(rf_vpp_io_stages));
    }
    rf_vpp_io_stages[stage * 3] = vpp_to_ms(now - vpp_stage_origin);
    rf_vpp_io_stages[stage * 3 + 1] = rf_vpp_io_stats[RF_VPP_IO_NATIVE_MS];
    rf_vpp_io_stages[stage * 3 + 2] = rf_vpp_io_stats[RF_VPP_IO_KIB];
}
static void vpp_open_account(uint64_t started)
{
    vpp_open_ticks += vpp_now() - started;
    rf_vpp_io_stats[RF_VPP_IO_OPEN_MS] = vpp_to_ms(vpp_open_ticks);
}

/* A failed fclose may invalidate FILE while leaving native ownership uncertain.
 * Keep only process-lifetime evidence, never a pointer that could be retried. */
static uint32_t vpp_checked_close_uncertain;

int rf_vpp_checked_idle_status(void)
{
    return vpp_checked_close_uncertain ? RF_IO : RF_OK;
}

static int checked_destination_busy(const rf_vpp *archive)
{
    return archive->stream || archive->length || archive->count || archive->payload_offset ||
           archive->directory || archive->native || archive->serial || archive->flags;
}

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

#ifndef RF_VPP_FAST
/* ---------------------------------------------------------------------------
 * stdio-only path (PC default): original implementation. */
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

int rf_vpp_close_checked(rf_vpp *archive)
{
    FILE *stream;
    if (!archive) return RF_RANGE;
    stream = archive->stream;
    memset(archive, 0, sizeof(*archive));
    /* Do not let an earlier latch prevent retirement of another owned stream. */
    if (stream && fclose(stream)) vpp_checked_close_uncertain = 1;
    return rf_vpp_checked_idle_status();
}

static int vpp_open_failure(rf_vpp *archive, int status, uint32_t checked)
{
    if (checked) {
        if (rf_vpp_close_checked(archive)) return RF_IO;
    } else rf_vpp_close(archive);
    return status;
}

static int vpp_open_body(rf_vpp *archive, const char *path, uint32_t checked)
{
    unsigned char header[16];
    long length;
    int result;
    memset(archive, 0, sizeof(*archive));
    archive->stream = fopen(path, "rb");
    if (!archive->stream) return RF_IO;
    if (fseek(archive->stream, 0, SEEK_END) || (length = ftell(archive->stream)) < 2048 ||
        (uint64_t)length > INT32_MAX || fseek(archive->stream, 0, SEEK_SET) ||
        fread(header, 1, sizeof(header), archive->stream) != sizeof(header)) {
        return vpp_open_failure(archive, RF_IO, checked);
    }
    archive->length = (uint32_t)length;
    archive->count = le32(header + 8);
    if (le32(header) != 0x51890aceu || le32(header + 4) != 1 ||
        archive->count > 65536 || le32(header + 12) != archive->length) {
        return vpp_open_failure(archive, RF_FORMAT, checked);
    }
    archive->payload_offset = 2048u + (uint32_t)aligned(archive->count * 64u);
    if (archive->payload_offset > archive->length) {
        return vpp_open_failure(archive, RF_FORMAT, checked);
    }
    result = rf_vpp_visit(archive, NULL, NULL);
    if (result != RF_OK) return vpp_open_failure(archive, result, checked);
    return result;
}

void rf_vpp_close(rf_vpp *archive)
{
    if (archive) {
        if (archive->stream) fclose(archive->stream);
        memset(archive, 0, sizeof(*archive));
    }
}

int rf_vpp_find(rf_vpp *archive, const char *name, rf_vpp_entry *entry)
{
    find_context context;
    int result;
    if (!name || !entry) return RF_RANGE;
    ++rf_vpp_io_stats[RF_VPP_IO_FINDS];
    ++rf_vpp_io_stats[RF_VPP_IO_LINEAR_FINDS];
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
    ++rf_vpp_io_stats[RF_VPP_IO_READS];
    if (!size) return RF_OK;
    if (fseek(archive->stream, (long)(entry->offset + offset), SEEK_SET) ||
        fread(data, 1, size, archive->stream) != size) return RF_IO;
    return RF_OK;
}

int rf_vpp_read_raw(rf_vpp *archive, uint32_t offset, void *data, uint32_t size)
{
    if (!archive || !archive->stream) return RF_IO;
    if ((size && !data) || offset > archive->length || size > archive->length - offset) return RF_RANGE;
    ++rf_vpp_io_stats[RF_VPP_IO_READS];
    if (!size) return RF_OK;
    if (fseek(archive->stream, (long)offset, SEEK_SET) || fread(data, 1, size, archive->stream) != size) return RF_IO;
    return RF_OK;
}

#else
/* ---------------------------------------------------------------------------
 * Fast path. Evidence for the costs it removes (stdio path, NXDK PDCLib):
 * - PDCLib BUFSIZ is 1024 (platform/xbox/include/pdclib/_PDCLIB_config.h) and
 *   fread copies byte by byte, refilling with one 1 KiB ReadFile per KiB
 *   (functions/stdio/fread.c, platform/xbox/.../_PDCLIB_fillbuffer.c).
 * - Every rf_vpp_read began with fseek, discarding the buffer, so each read of
 *   any size cost SetFilePointer + ReadFile(1 KiB) at minimum.
 * - rf_vpp_find re-read the directory entry by entry (seek + 64-byte read per
 *   entry): up to 2568 syscall pairs per audio.vpp lookup, 886 per maps*.vpp
 *   lookup, repeated for each archive tried by texture resolution.
 * - rf_vpp_open re-validated the whole directory the same way on every open
 *   (tables.vpp is reopened dozens of times during a campaign load). */
#ifndef RF_VPP_WINDOW_BYTES
#define RF_VPP_WINDOW_BYTES 32768u
#endif
#ifndef RF_VPP_WINDOW_SLOTS
#define RF_VPP_WINDOW_SLOTS 4u
#endif
#ifndef RF_VPP_DIRECT_CHUNK
#define RF_VPP_DIRECT_CHUNK 131072u
#endif
#ifndef RF_VPP_DIRECTORY_BUDGET
/* All 15 retail archives together need about 320 KiB with 16-bit buckets. */
#define RF_VPP_DIRECTORY_BUDGET 393216u
#endif
#ifndef RF_VPP_UNBUFFERED
#define RF_VPP_UNBUFFERED 1
#endif
#define VPP_SECTOR 2048u
_Static_assert(RF_VPP_WINDOW_BYTES % VPP_SECTOR == 0 && RF_VPP_WINDOW_BYTES >= 4u * VPP_SECTOR,
               "read windows must hold whole sectors");
_Static_assert(RF_VPP_DIRECT_CHUNK % VPP_SECTOR == 0 && RF_VPP_DIRECT_CHUNK >= VPP_SECTOR,
               "direct requests must be sector multiples");
_Static_assert(RF_VPP_WINDOW_SLOTS >= 1u, "at least one read window");

enum { VPP_FLAG_NATIVE = 1, VPP_FLAG_UNBUFFERED = 2 };

typedef struct vpp_record { uint32_t offset, size, hash, name; } vpp_record;
struct rf_vpp_directory {
    struct rf_vpp_directory *next;
    uint32_t length, count, mask, bytes, name_capacity;
    vpp_record *records;
    uint16_t *buckets; /* record index + 1; 0 is empty */
    char *names;
    char path[];
};
static struct rf_vpp_directory *vpp_directories;
static uint32_t vpp_directory_bytes;

/* Shared read windows. Keyed by a per-open serial so a closed or abandoned
 * archive can never be served another file's bytes. */
typedef struct vpp_window { uint32_t serial, start, valid, used; } vpp_window;
static vpp_window vpp_windows[RF_VPP_WINDOW_SLOTS];
static _Alignas(4096) unsigned char vpp_window_data[RF_VPP_WINDOW_SLOTS][RF_VPP_WINDOW_BYTES];
static uint32_t vpp_window_clock, vpp_serial;

#ifdef RF_VPP_NATIVE
static uint64_t vpp_native_ticks, vpp_native_bytes;
static void vpp_native_account(uint64_t started, uint32_t bytes)
{
    vpp_native_ticks += vpp_now() - started;
    vpp_native_bytes += bytes;
    rf_vpp_io_stats[RF_VPP_IO_NATIVE_MS] = vpp_to_ms(vpp_native_ticks);
    rf_vpp_io_stats[RF_VPP_IO_KIB] = (uint32_t)(vpp_native_bytes >> 10);
}
static void vpp_native_retire(rf_vpp *archive)
{
    uint64_t started = vpp_now();
    void *native = archive->native;
    archive->native = NULL;
    archive->flags = 0;
    /* The FILE stream stays open on the same file and takes over reads. */
    if (native && rf_xbox_file_close(native)) vpp_checked_close_uncertain = 1;
    vpp_native_account(started, 0);
    ++rf_vpp_io_stats[RF_VPP_IO_FALLBACKS];
}
#endif

static uint32_t vpp_hash(const char *text, uint32_t *length)
{
    uint32_t hash = 2166136261u, n = 0;
    while (text[n]) {
        hash = (hash ^ fold((unsigned char)text[n])) * 16777619u;
        ++n;
    }
    if (length) *length = n;
    return hash;
}
/* Same equality as find_entry: case-folded A-Z, equal length. */
static int vpp_same(const char *left, const char *right)
{
    const unsigned char *a = (const unsigned char *)left, *b = (const unsigned char *)right;
    while (*a && fold(*a) == fold(*b)) { ++a; ++b; }
    return !*a && !*b;
}

/* Positional read of up to size bytes; *got may be short only at end of file. */
static int vpp_backend_read(rf_vpp *archive, uint32_t offset, void *data, uint32_t size, uint32_t *got)
{
    size_t count;
    *got = 0;
#ifdef RF_VPP_NATIVE
    if (archive->native) {
        uint64_t started = vpp_now();
        int status = rf_xbox_file_read(archive->native, offset, data, size, got);
        vpp_native_account(started, *got);
        if (status == RF_XBOX_FILE_OK) return RF_OK;
        /* Alignment rule mismatch (unverified on hardware) or device error:
         * retry this and later requests on the FILE stream. */
        vpp_native_retire(archive);
        *got = 0;
    }
#endif
    /* Archive length is checked against INT32_MAX at open. */
    if (fseek(archive->stream, (long)offset, SEEK_SET)) return RF_IO;
    count = fread(data, 1, size, archive->stream);
    if (count != size && ferror(archive->stream)) return RF_IO;
    *got = (uint32_t)count;
    return RF_OK;
}

static void vpp_windows_forget(uint32_t serial)
{
    uint32_t i;
    if (!serial) return;
    for (i = 0; i < RF_VPP_WINDOW_SLOTS; ++i)
        if (vpp_windows[i].serial == serial) memset(vpp_windows + i, 0, sizeof(vpp_windows[i]));
}

/* Requires 0 < size, (pos % VPP_SECTOR) + size <= RF_VPP_WINDOW_BYTES and
 * pos + size <= archive->length. */
static int vpp_window_read(rf_vpp *archive, uint32_t pos, void *out, uint32_t size)
{
    uint32_t i, pick = 0, start, request, got;
    vpp_window *window;
    int status;
    for (i = 0; i < RF_VPP_WINDOW_SLOTS; ++i) {
        window = vpp_windows + i;
        if (window->serial == archive->serial && pos >= window->start && pos - window->start <= window->valid &&
            size <= window->valid - (pos - window->start)) {
            window->used = ++vpp_window_clock;
            memcpy(out, vpp_window_data[i] + (pos - window->start), size);
            ++rf_vpp_io_stats[RF_VPP_IO_WINDOW_HITS];
            return RF_OK;
        }
    }
    for (i = 1; i < RF_VPP_WINDOW_SLOTS; ++i)
        if (vpp_windows[i].used < vpp_windows[pick].used) pick = i;
    window = vpp_windows + pick;
    memset(window, 0, sizeof(*window));
    start = pos & ~(VPP_SECTOR - 1u);
    if (start >= archive->length) return RF_IO;
    request = RF_VPP_WINDOW_BYTES;
    if (archive->length - start < request) request = (uint32_t)aligned(archive->length - start);
    status = vpp_backend_read(archive, start, vpp_window_data[pick], request, &got);
    if (status) return status;
    ++rf_vpp_io_stats[RF_VPP_IO_WINDOW_FILLS];
    if (got < pos - start || got - (pos - start) < size) return RF_IO;
    window->serial = archive->serial;
    window->start = start;
    window->valid = got;
    window->used = ++vpp_window_clock;
    memcpy(out, vpp_window_data[pick] + (pos - start), size);
    return RF_OK;
}

/* Large reads: sector-aligned middle straight into caller memory in large
 * requests; head, tail and DWORD-misaligned destinations through windows. */
static int vpp_span_read(rf_vpp *archive, uint32_t pos, unsigned char *out, uint32_t size)
{
    while (size) {
        uint32_t inner = pos & (VPP_SECTOR - 1u), piece, got;
        int status;
        if (!inner && size >= VPP_SECTOR && !((uintptr_t)out & 3u)) {
            piece = size & ~(VPP_SECTOR - 1u);
            if (piece > RF_VPP_DIRECT_CHUNK) piece = RF_VPP_DIRECT_CHUNK;
            status = vpp_backend_read(archive, pos, out, piece, &got);
            if (status) return status;
            ++rf_vpp_io_stats[RF_VPP_IO_DIRECT_REQUESTS];
            if (got != piece) return RF_IO;
        } else {
            piece = inner ? VPP_SECTOR - inner : RF_VPP_WINDOW_BYTES;
            if (piece > size) piece = size;
            status = vpp_window_read(archive, pos, out, piece);
            if (status) return status;
        }
        pos += piece;
        out += piece;
        size -= piece;
    }
    return RF_OK;
}

static int vpp_bytes(rf_vpp *archive, uint32_t pos, void *data, uint32_t size)
{
    ++rf_vpp_io_stats[RF_VPP_IO_READS];
    if (!size) return RF_OK;
    if ((pos & (VPP_SECTOR - 1u)) + (uint64_t)size <= RF_VPP_WINDOW_BYTES / 2u)
        return vpp_window_read(archive, pos, data, size);
    return vpp_span_read(archive, pos, (unsigned char *)data, size);
}

/* Validates the directory exactly as the stdio path does. Optionally reports
 * entries to a visitor, fills a cache record set, and totals name bytes. */
static int vpp_scan(rf_vpp *archive, rf_vpp_visitor visitor, void *context,
                    struct rf_vpp_directory *fill, uint32_t *name_bytes)
{
    uint32_t index, names = 0;
    uint64_t cursor = archive->payload_offset;
    for (index = 0; index < archive->count; ++index) {
        unsigned char raw[64];
        rf_vpp_entry entry;
        uint32_t length;
        if (vpp_window_read(archive, 2048u + index * 64u, raw, sizeof(raw))) return RF_IO;
        if (!memchr(raw, 0, 60) || raw[0] == 0) return RF_FORMAT;
        memset(entry.name, 0, sizeof(entry.name));
        length = (uint32_t)strlen((const char *)raw);
        memcpy(entry.name, raw, length);
        entry.size = le32(raw + 60);
        if (cursor + aligned(entry.size) > archive->length) return RF_FORMAT;
        entry.offset = (uint32_t)cursor;
        if (fill) {
            vpp_record *record = fill->records + index;
            if (length + 1u > fill->name_capacity - names) return RF_FORMAT;
            record->offset = entry.offset;
            record->size = entry.size;
            record->hash = vpp_hash(entry.name, NULL);
            record->name = names;
            memcpy(fill->names + names, entry.name, length + 1u);
        }
        names += length + 1u;
        if (visitor) {
            int result = visitor(&entry, context);
            if (result) return result;
        }
        cursor += aligned(entry.size);
    }
    if (name_bytes) *name_bytes = names;
    return RF_OK;
}

static void vpp_index(struct rf_vpp_directory *directory)
{
    uint32_t i;
    for (i = 0; i < directory->count; ++i) {
        const vpp_record *record = directory->records + i;
        const char *name = directory->names + record->name;
        uint32_t slot = record->hash & directory->mask;
        for (;; slot = (slot + 1u) & directory->mask) {
            uint16_t bucket = directory->buckets[slot];
            const vpp_record *other;
            if (!bucket) {
                directory->buckets[slot] = (uint16_t)(i + 1u);
                break;
            }
            other = directory->records + (bucket - 1u);
            /* Duplicate names: the first entry wins, as in a linear scan. */
            if (other->hash == record->hash && vpp_same(directory->names + other->name, name)) break;
        }
    }
}

static const vpp_record *vpp_lookup(const struct rf_vpp_directory *directory, const char *name)
{
    uint32_t length, hash = vpp_hash(name, &length), slot;
    if (!directory->count || !length || length >= 60u) return NULL;
    for (slot = hash & directory->mask;; slot = (slot + 1u) & directory->mask) {
        uint16_t bucket = directory->buckets[slot];
        const vpp_record *record;
        if (!bucket) return NULL;
        record = directory->records + (bucket - 1u);
        if (record->hash == hash && vpp_same(directory->names + record->name, name)) return record;
    }
}

static const struct rf_vpp_directory *vpp_cached(const char *path, uint32_t length, uint32_t count)
{
    const struct rf_vpp_directory *directory;
    for (directory = vpp_directories; directory; directory = directory->next)
        if (directory->length == length && directory->count == count && vpp_same(directory->path, path))
            return directory;
    return NULL;
}

/* Best effort: on budget or allocation failure the archive keeps linear
 * lookup over windowed reads. The directory was already validated. */
static void vpp_directory_build(rf_vpp *archive, const char *path, uint32_t name_bytes)
{
    struct rf_vpp_directory *directory;
    size_t path_length = strlen(path), head, buckets_at, names_at, total;
    uint32_t table = 1;
    if (archive->count > 65534u || path_length >= 260u) return;
    while (table < 2u * archive->count) table <<= 1;
    head = (sizeof(*directory) + path_length + 1u + 3u) & ~(size_t)3u;
    buckets_at = head + (size_t)archive->count * sizeof(vpp_record);
    names_at = buckets_at + (size_t)table * sizeof(uint16_t);
    total = names_at + name_bytes;
    if (total > RF_VPP_DIRECTORY_BUDGET || vpp_directory_bytes > RF_VPP_DIRECTORY_BUDGET - total) return;
    directory = (struct rf_vpp_directory *)calloc(1, total);
    if (!directory) return;
    directory->length = archive->length;
    directory->count = archive->count;
    directory->mask = table - 1u;
    directory->bytes = (uint32_t)total;
    directory->name_capacity = name_bytes;
    directory->records = (vpp_record *)((unsigned char *)directory + head);
    directory->buckets = (uint16_t *)((unsigned char *)directory + buckets_at);
    directory->names = (char *)directory + names_at;
    memcpy(directory->path, path, path_length + 1u);
    if (vpp_scan(archive, NULL, NULL, directory, NULL)) {
        free(directory);
        return;
    }
    vpp_index(directory);
    directory->next = vpp_directories;
    vpp_directories = directory;
    vpp_directory_bytes += (uint32_t)total;
    rf_vpp_io_stats[RF_VPP_IO_DIRECTORY_BYTES] = vpp_directory_bytes;
    ++rf_vpp_io_stats[RF_VPP_IO_DIRECTORY_BUILDS];
    archive->directory = directory;
}

/* Returns nonzero when any close failed. Ownership is cleared first. */
static int vpp_release(rf_vpp *archive)
{
    FILE *stream = archive->stream;
    void *native = archive->native;
    uint32_t serial = archive->serial;
    int failed = 0;
    memset(archive, 0, sizeof(*archive));
    vpp_windows_forget(serial);
#ifdef RF_VPP_NATIVE
    if (native) {
        uint64_t started = vpp_now();
        if (rf_xbox_file_close(native)) failed = 1;
        vpp_native_account(started, 0);
    }
#else
    (void)native;
#endif
    if (stream && fclose(stream)) failed = 1;
    return failed;
}

int rf_vpp_close_checked(rf_vpp *archive)
{
    if (!archive) return RF_RANGE;
    /* Do not let an earlier latch prevent retirement of another owned stream. */
    if (vpp_release(archive)) vpp_checked_close_uncertain = 1;
    return rf_vpp_checked_idle_status();
}

void rf_vpp_close(rf_vpp *archive)
{
    if (archive) (void)vpp_release(archive);
}

static int vpp_open_failure(rf_vpp *archive, int status, uint32_t checked)
{
    if (checked) {
        if (rf_vpp_close_checked(archive)) return RF_IO;
    } else rf_vpp_close(archive);
    return status;
}

static int vpp_open_body(rf_vpp *archive, const char *path, uint32_t checked)
{
    unsigned char header[16];
    const struct rf_vpp_directory *cached;
    uint32_t name_bytes = 0;
    long length;
    int result;
    memset(archive, 0, sizeof(*archive));
    archive->stream = fopen(path, "rb");
    if (!archive->stream) return RF_IO;
    if (fseek(archive->stream, 0, SEEK_END) || (length = ftell(archive->stream)) < 2048 ||
        (uint64_t)length > INT32_MAX || fseek(archive->stream, 0, SEEK_SET)) {
        return vpp_open_failure(archive, RF_IO, checked);
    }
    archive->length = (uint32_t)length;
    if (!++vpp_serial) ++vpp_serial;
    archive->serial = vpp_serial;
#ifdef RF_VPP_NATIVE
    {
        uint64_t started = vpp_now();
        void *native = NULL;
        if (rf_xbox_file_open(path, RF_VPP_UNBUFFERED, &native) == RF_XBOX_FILE_OK) {
            archive->native = native;
            archive->flags = VPP_FLAG_NATIVE | (RF_VPP_UNBUFFERED ? VPP_FLAG_UNBUFFERED : 0);
        } else ++rf_vpp_io_stats[RF_VPP_IO_NATIVE_OPEN_FAILURES];
        vpp_native_account(started, 0);
    }
#endif
    if (vpp_window_read(archive, 0, header, sizeof(header))) return vpp_open_failure(archive, RF_IO, checked);
    archive->count = le32(header + 8);
    if (le32(header) != 0x51890aceu || le32(header + 4) != 1 ||
        archive->count > 65536 || le32(header + 12) != archive->length) {
        return vpp_open_failure(archive, RF_FORMAT, checked);
    }
    archive->payload_offset = 2048u + (uint32_t)aligned(archive->count * 64u);
    if (archive->payload_offset > archive->length) {
        return vpp_open_failure(archive, RF_FORMAT, checked);
    }
    /* Same path, length and count as an archive validated earlier in this
     * process: retail archives are read-only, so reuse its directory. */
    cached = vpp_cached(path, archive->length, archive->count);
    if (cached) {
        archive->directory = cached;
        ++rf_vpp_io_stats[RF_VPP_IO_DIRECTORY_HITS];
        return RF_OK;
    }
    result = vpp_scan(archive, NULL, NULL, NULL, &name_bytes);
    if (result != RF_OK) return vpp_open_failure(archive, result, checked);
    vpp_directory_build(archive, path, name_bytes);
    return RF_OK;
}

int rf_vpp_visit(rf_vpp *archive, rf_vpp_visitor visitor, void *context)
{
    const struct rf_vpp_directory *directory;
    uint32_t index;
    if (!archive || !archive->stream) return RF_IO;
    directory = archive->directory;
    if (!directory) return vpp_scan(archive, visitor, context, NULL, NULL);
    for (index = 0; index < directory->count; ++index) {
        const vpp_record *record = directory->records + index;
        const char *name = directory->names + record->name;
        rf_vpp_entry entry;
        memset(entry.name, 0, sizeof(entry.name));
        memcpy(entry.name, name, strlen(name));
        entry.offset = record->offset;
        entry.size = record->size;
        if (visitor) {
            int result = visitor(&entry, context);
            if (result) return result;
        }
    }
    return RF_OK;
}

int rf_vpp_find(rf_vpp *archive, const char *name, rf_vpp_entry *entry)
{
    find_context context;
    int result;
    if (!name || !entry) return RF_RANGE;
    ++rf_vpp_io_stats[RF_VPP_IO_FINDS];
    if (archive && archive->stream && archive->directory) {
        const vpp_record *record = vpp_lookup(archive->directory, name);
        const char *found;
        if (!record) return RF_NOT_FOUND;
        found = archive->directory->names + record->name;
        memset(entry->name, 0, sizeof(entry->name));
        memcpy(entry->name, found, strlen(found));
        entry->offset = record->offset;
        entry->size = record->size;
        return RF_OK;
    }
    ++rf_vpp_io_stats[RF_VPP_IO_LINEAR_FINDS];
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
    return vpp_bytes(archive, entry->offset + offset, data, size);
}

int rf_vpp_read_raw(rf_vpp *archive, uint32_t offset, void *data, uint32_t size)
{
    if (!archive || !archive->stream) return RF_IO;
    if ((size && !data) || offset > archive->length || size > archive->length - offset) return RF_RANGE;
    return vpp_bytes(archive, offset, data, size);
}
#endif

static int vpp_open_policy(rf_vpp *archive, const char *path, uint32_t checked)
{
    uint64_t started;
    int result;
    if (!archive || !path) return RF_IO;
    if (checked) {
        if (rf_vpp_checked_idle_status()) return RF_IO;
        if (checked_destination_busy(archive)) return RF_RANGE;
    }
#ifdef RF_VPP_FAST
    rf_vpp_io_stats[RF_VPP_IO_FLAGS] = 1u
#ifdef RF_VPP_NATIVE
        | 2u | (RF_VPP_UNBUFFERED ? 4u : 0u)
#endif
        ;
#endif
    ++rf_vpp_io_stats[RF_VPP_IO_OPENS];
    started = vpp_now();
    result = vpp_open_body(archive, path, checked);
    vpp_open_account(started);
    return result;
}

int rf_vpp_open(rf_vpp *archive, const char *path)
{
    return vpp_open_policy(archive, path, 0);
}

int rf_vpp_open_checked(rf_vpp *archive, const char *path)
{
    return vpp_open_policy(archive, path, 1);
}
