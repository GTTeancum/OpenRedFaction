#ifndef RF_VPP_H
#define RF_VPP_H
#include <stdint.h>
#include <stdio.h>

/* VPP v1 disk layout: see docs/PROVENANCE.md. No archive-sized allocation.
 *
 * Fast path (default on NXDK, opt-in elsewhere with -DRF_VPP_FAST; disable on
 * NXDK with -DRF_VPP_STDIO_ONLY): the validated directory is kept in a shared,
 * process-lifetime, budget-capped cache (RF_VPP_DIRECTORY_BUDGET) with a hash
 * index; entry reads use native positional reads (Xbox) through a small shared
 * pool of sector-aligned read windows, and large reads go straight into the
 * caller's buffer in large requests. Archives over the directory budget keep
 * linear lookup over the windowed reads. `stream` stays a valid FILE * opened on
 * the same file, so existing callers that test it or read it directly still
 * work. All formats checks and error codes are unchanged. Not thread-safe:
 * archive access is main-thread only. */
struct rf_vpp_directory;
typedef struct rf_vpp {
    FILE *stream;
    uint32_t length;
    uint32_t count;
    uint32_t payload_offset;
    /* Fast-path state; zero on the stdio-only path. Opaque to callers. */
    const struct rf_vpp_directory *directory;
    void *native;
    uint32_t serial;
    uint32_t flags;
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
/* Opt-in checked ownership; legacy open/close semantics are unchanged.
 * Checked open requires a zero-initialized empty destination. Any failed close
 * permanently blocks later checked opens, including after scene/menu teardown.
 * Checked open also checks closes on validation failure. Close clears ownership
 * before its single fclose attempt: never retry or retain the old FILE pointer.
 * A failed native handle close on the fast path latches the same way.
 * Cleanup of another owned archive remains allowed after uncertainty; it still
 * reports RF_IO while the process latch is set. No reset API is provided. */
int rf_vpp_checked_idle_status(void);
int rf_vpp_open_checked(rf_vpp *archive, const char *path);
int rf_vpp_close_checked(rf_vpp *archive);
int rf_vpp_visit(rf_vpp *archive, rf_vpp_visitor visitor, void *context);
int rf_vpp_find(rf_vpp *archive, const char *name, rf_vpp_entry *entry);
/* Entry must originate from this archive; offsets and lengths are bounds checked. */
int rf_vpp_read(rf_vpp *archive, const rf_vpp_entry *entry, uint32_t offset, void *data, uint32_t size);
/* Whole-archive byte range read (e.g. identity hashing); bounds checked against
 * the archive length. Prefer this to reading `stream` directly. */
int rf_vpp_read_raw(rf_vpp *archive, uint32_t offset, void *data, uint32_t size);

/* Load I/O accounting, plain words for the Xbox RAM log (zero on stdio-only
 * builds except the call counters). Indices: */
enum {
    RF_VPP_IO_OPENS = 0,          /* rf_vpp_open/_checked calls */
    RF_VPP_IO_DIRECTORY_BUILDS,   /* directories parsed and cached */
    RF_VPP_IO_DIRECTORY_HITS,     /* opens served by the directory cache */
    RF_VPP_IO_DIRECTORY_BYTES,    /* cache bytes held (process lifetime) */
    RF_VPP_IO_FINDS,              /* rf_vpp_find calls */
    RF_VPP_IO_LINEAR_FINDS,       /* finds without a cached index */
    RF_VPP_IO_READS,              /* rf_vpp_read/_raw calls */
    RF_VPP_IO_WINDOW_HITS,        /* small reads served from memory */
    RF_VPP_IO_WINDOW_FILLS,       /* window-sized native requests */
    RF_VPP_IO_DIRECT_REQUESTS,    /* large native requests into caller memory */
    RF_VPP_IO_KIB,                /* KiB transferred by native requests */
    RF_VPP_IO_NATIVE_MS,          /* ms inside native open/read/close calls */
    RF_VPP_IO_OPEN_MS,            /* ms inside rf_vpp_open (inclusive) */
    RF_VPP_IO_FALLBACKS,          /* archives switched to stdio after a native error */
    RF_VPP_IO_NATIVE_OPEN_FAILURES,
    RF_VPP_IO_FLAGS,              /* 1 fast path, 2 native, 4 unbuffered */
    RF_VPP_IO_COUNT
};
extern uint32_t rf_vpp_io_stats[RF_VPP_IO_COUNT];
/* Load-stage timer: rf_vpp_io_stage(n) for n < RF_VPP_IO_STAGES records
 * {ms since stage 0, cumulative native I/O ms, cumulative native KiB} into
 * rf_vpp_io_stages[n*3..n*3+2]. Stage 0 starts the clock and clears later
 * stages. Costs one tick read; safe to call from any load step. */
#define RF_VPP_IO_STAGES 16
extern uint32_t rf_vpp_io_stages[RF_VPP_IO_STAGES * 3];
void rf_vpp_io_stage(uint32_t stage);
#endif
