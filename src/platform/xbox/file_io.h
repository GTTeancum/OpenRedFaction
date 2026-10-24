#ifndef RF_XBOX_FILE_IO_H
#define RF_XBOX_FILE_IO_H
#include <stdint.h>

/* Native read-only file access for archive loading on Xbox (NXDK).
 *
 * Follows the XDK "Maximizing Xbox File Performance" guidance: kernel
 * NtCreateFile/NtReadFile instead of PDCLib stdio (1 KiB buffer, byte copy
 * loop, seek+read syscall pair per call), positional reads with no shared
 * file pointer, and optional FILE_NO_INTERMEDIATE_BUFFERING.
 *
 * Unbuffered handles require sector-multiple offsets and lengths. Callers use
 * RF_XBOX_FILE_SECTOR (2048, the DVD sector and a multiple of the 512-byte HDD
 * sector). UNVERIFIED on hardware: the destination-alignment rule is assumed
 * to be DWORD (XDK documentation), not sector as on desktop Windows. A kernel
 * STATUS_INVALID_PARAMETER is reported as RF_XBOX_FILE_ALIGNMENT so the caller
 * can fall back to a path without alignment requirements.
 *
 * Single-threaded use per handle is assumed (loading is main-thread only). */
enum { RF_XBOX_FILE_OK = 0, RF_XBOX_FILE_ERROR = -1, RF_XBOX_FILE_ALIGNMENT = -2 };
#define RF_XBOX_FILE_SECTOR 2048u

int rf_xbox_file_open(const char *path, uint32_t unbuffered, void **handle);
/* Reads up to size bytes at offset. transferred receives the byte count; a
 * read at or beyond end of file succeeds with zero bytes. */
int rf_xbox_file_read(void *handle, uint32_t offset, void *data, uint32_t size, uint32_t *transferred);
int rf_xbox_file_close(void *handle);
/* Cheap timestamp (rdtsc through QueryPerformanceCounter) and its rate. */
uint64_t rf_xbox_file_ticks(void);
uint64_t rf_xbox_file_tick_rate(void);
uint32_t rf_xbox_file_milliseconds(void);
#endif
