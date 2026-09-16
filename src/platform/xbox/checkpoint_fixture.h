#ifndef RF_XBOX_CHECKPOINT_FIXTURE_H
#define RF_XBOX_CHECKPOINT_FIXTURE_H
#include "checkpoint_storage.h"
/* TEST ONLY: caller must establish explicit DEV mode and a disposable private
 * harness HDD. Guest cannot infer the host backing-file path. No normal scene
 * calls these helpers. Session must own R:; seed additionally requires writable.
 * Without D:\\geomod-fallback-seed.flag seed returns RF_NOT_FOUND without I/O
 * to save slots. With the flag, both destination slots MUST be absent.
 * Staged inputs D:\\geomod-fallback0.rfsg and ...1.rfsg must have valid RFSG
 * checksums and generations1/2. Semantic RFDS validation is deliberately absent:
 * the newer fixture is intended to fail the real scene selector later. */
int rf_xbox_checkpoint_fixture_seed(rf_xbox_checkpoint_storage *);
/* Explicit before/after probe; reads both slots only. Partial/missing fails. */
int rf_xbox_checkpoint_fixture_hash_slots(rf_xbox_checkpoint_storage *);
/* phase(1 gate,2 verify sources/empty destination,3 write/flush,4 readback,5 ready),
 * RF status,Win32 error,NTSTATUS, slot0 envelopeFNV,slot1 envelopeFNV,
 * slot0 bytes,slot1 bytes. Preserved by later hash_slots calls. */
extern uint32_t rf_xbox_checkpoint_fixture_seed_state[8];
/* Last copy operation: substage(1 source open,2 create,3 read,4 write,
 * 5 source close,6 flush,7 output close,8 complete), slot, bytes written,
 * latest read bytes,latest written bytes,errno,ferror,Win32 error.
 * Captures the first failed operation before cleanup can replace its error. */
extern uint32_t rf_xbox_checkpoint_fixture_copy_state[8];
/* RF status,Win32 error,slot0 envelopeFNV,slot0 bytes,slot1 envelopeFNV,slot1 bytes.
 * Record twice externally or copy first sample before normal save. */
extern uint32_t rf_xbox_checkpoint_fixture_slots[6];
#endif
