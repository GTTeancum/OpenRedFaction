#ifndef RF_XBOX_SETTINGS_STORAGE_H
#define RF_XBOX_SETTINGS_STORAGE_H
#include "input.h"
/* Independent, single-threaded controller settings owner. S: must be unowned.
 * No campaign/checkpoint APIs, formats, selections or diagnostics are used.
 * Exact36-byte RFCS1/2/3/4 or48-byte RFCS5 records in controller-settings.0/.1
 * under OpenRedFaction on Partition1. Startup is read-only; only explicit Save
 * creates/writes files. RFCS1/2 retain default buttons and Standard/selected
 * analog layout. RFCS3 stores a strict 10-control permutation rank plus layout
 * in word28. RFCS4 packs that word times3 plus text language. Older records
 * imply English. RFCS5 retains RFCS4 word28, stores master/effects/music gains
 * in words32/36/40 and moves the checksum to44. RFCS1..4 imply100% audio;
 * default audio still writes canonical RFCS1..4. No campaign save/size change.
 * Reads publish outputs only after all native handles and our alias close. */
int rf_xbox_settings_storage_load(rf_xbox_input_options *options,uint32_t *degraded);
/* Revalidate both slots; refuse corrupt/unknown records, ambiguous generations
 * and wrap. Write only the absent/older slot, flush, close, reread/compare and
 * unmount before success. The newest previously validated slot is never written.
 * Exact version/length and reopened inactive bytes are checked before writes;
 * only a recognized revalidated inactive48-byte record may shrink to36.
 * A failed save may leave new/partial inactive bytes; never silently repair them.
 * Native flush success is not a guarantee against physical power loss. */
int rf_xbox_settings_storage_save(const rf_xbox_input_options *options);
/* A close/unmount failure retains ownership and terminally disables this owner
 * until reboot. It must never reopen over a handle/alias with uncertain lifetime. */
uint32_t rf_xbox_settings_storage_locked(void);
/* Read-only dashboard admission also rejects an in-flight handle or alias. */
int rf_xbox_settings_storage_idle_status(void);
/* operation(1load/2save), phase(1mount/2read/3select/4directory/5write/6flush/
 * 7verify), RF status, Win32 error, NTSTATUS, selected generation, selected slot,
 * valid mask, invalid mask, flags(bit0 alias owned/bit1 file owned/bit2 locked),
 * failed-close NTSTATUS, failed-unmount Win32 error. Separate from game saves. */
extern uint32_t rf_xbox_settings_storage_state[12];
#endif
