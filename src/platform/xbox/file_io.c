/* Native Xbox archive file access. See file_io.h for the contract.
 * Path handling mirrors NXDK lib/winapi/fileio.c CreateFileA (ANSI path
 * relative to ObDosDevicesDirectory, case-insensitive) so a path accepted by
 * fopen is accepted here. NtCreateFile is called directly so that FileAttributes
 * carries only FILE_ATTRIBUTE_NORMAL; NXDK CreateFileA forwards FILE_FLAG_* bits
 * into FileAttributes as well. */
#include "file_io.h"
#include <string.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

int rf_xbox_file_open(const char *path, uint32_t unbuffered, void **handle)
{
    ANSI_STRING name;
    OBJECT_ATTRIBUTES attributes;
    IO_STATUS_BLOCK io;
    HANDLE native = NULL;
    NTSTATUS status;
    ULONG options = FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE | FILE_RANDOM_ACCESS;
    if (!handle) return RF_XBOX_FILE_ERROR;
    *handle = NULL;
    if (!path || !path[0] || strlen(path) >= MAX_PATH) return RF_XBOX_FILE_ERROR;
    if (unbuffered) options |= FILE_NO_INTERMEDIATE_BUFFERING;
    RtlInitAnsiString(&name, path);
    InitializeObjectAttributes(&attributes, &name, OBJ_CASE_INSENSITIVE, ObDosDevicesDirectory(), NULL);
    memset(&io, 0, sizeof(io));
    status = NtCreateFile(&native, GENERIC_READ | FILE_READ_ATTRIBUTES | SYNCHRONIZE, &attributes, &io, NULL,
                          FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_DELETE, FILE_OPEN, options);
    if (!NT_SUCCESS(status) || !native) return RF_XBOX_FILE_ERROR;
    *handle = (void *)native;
    return RF_XBOX_FILE_OK;
}

int rf_xbox_file_read(void *handle, uint32_t offset, void *data, uint32_t size, uint32_t *transferred)
{
    IO_STATUS_BLOCK io;
    LARGE_INTEGER position;
    NTSTATUS status;
    if (transferred) *transferred = 0;
    if (!handle || !transferred || (size && !data)) return RF_XBOX_FILE_ERROR;
    if (!size) return RF_XBOX_FILE_OK;
    memset(&io, 0, sizeof(io));
    position.QuadPart = (LONGLONG)offset;
    /* Synchronous handle with an explicit ByteOffset: positional, no shared
     * file pointer, one kernel call per request. */
    status = NtReadFile((HANDLE)handle, NULL, NULL, NULL, &io, data, (ULONG)size, &position);
    if (status == (NTSTATUS)STATUS_END_OF_FILE) return RF_XBOX_FILE_OK;
    if (status == (NTSTATUS)STATUS_INVALID_PARAMETER) return RF_XBOX_FILE_ALIGNMENT;
    if (!NT_SUCCESS(status)) return RF_XBOX_FILE_ERROR;
    if (io.Information > size) return RF_XBOX_FILE_ERROR;
    *transferred = (uint32_t)io.Information;
    return RF_XBOX_FILE_OK;
}

int rf_xbox_file_close(void *handle)
{
    if (!handle) return RF_XBOX_FILE_OK;
    return NT_SUCCESS(NtClose((HANDLE)handle)) ? RF_XBOX_FILE_OK : RF_XBOX_FILE_ERROR;
}

uint64_t rf_xbox_file_ticks(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint64_t)now.QuadPart;
}

uint64_t rf_xbox_file_tick_rate(void)
{
    LARGE_INTEGER rate;
    QueryPerformanceFrequency(&rate);
    return rate.QuadPart > 0 ? (uint64_t)rate.QuadPart : 733333333u;
}

uint32_t rf_xbox_file_milliseconds(void) { return (uint32_t)GetTickCount(); }
