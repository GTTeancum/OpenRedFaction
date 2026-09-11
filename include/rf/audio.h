#ifndef RF_AUDIO_H
#define RF_AUDIO_H
#include "rf/vpp.h"
#define RF_SOUND_METADATA_CAPACITY 4096u
#define RF_AUDIO_ORDINARY_SLOTS 30u
typedef struct rf_audio_allocation_slot {uint32_t present,flags;} rf_audio_allocation_slot;
typedef struct rf_audio_allocation_backend {
    int32_t (*status)(void *context,uint32_t slot,uint32_t *bits);
    void (*release)(void *context,uint32_t slot);
} rf_audio_allocation_backend;
/* Original522470 ordinary allocator over the first30 records, not the entire
 * 55-record cleanup table. First absent slot returns immediately. Present
 * flags&5 skip status; otherwise status failure counts as not playing, status
 * bit0 means playing, and flags&2 prevents reuse. Reusable present slots call
 * release before return. No active-voice stealing. Caller supplies valid
 * callbacks and owns release effects; this helper never mutates slot facts. */
int32_t rf_audio_select_ordinary(const rf_audio_allocation_slot slots[RF_AUDIO_ORDINARY_SLOTS],
    const rf_audio_allocation_backend *backend,void *context);
typedef struct rf_sound_metadata {
    char name[120];uint32_t loop_flags,keyoff_flags;
} rf_sound_metadata;
/* Original56bb80/5749fa sorting and56baa0 sorted lookup. Compact rows retain
 * original name and words+a8/ac. Order includes all4096 entries; UINT16_MAX
 * represents an original zero padding record. No allocation or row mutation.
 * ASCII, terminated names only; count<=4096. Errors preserve order. Caller
 * keeps rows/order alive and unchanged together; no overlapping storage. */
int rf_sound_metadata_order(const rf_sound_metadata *rows,uint32_t count,
    uint16_t order[RF_SOUND_METADATA_CAPACITY]);
const rf_sound_metadata *rf_sound_metadata_find(const rf_sound_metadata *rows,
    const uint16_t order[RF_SOUND_METADATA_CAPACITY],const char *name);
/* Bounded Bluebeard adapter for original56bbc0 field order. Retains name and
 * packed loop/keyoff words; validates but discards directories/time/envelope/
 * incidental fields. ASCII text, // comments, decimal or hexadecimal integers.
 * No allocation; two passes preserve rows/count on error. NULL rows/capacity0
 * queries count. Input and outputs must not overlap. */
int rf_sound_metadata_read(const void *text,uint32_t bytes,rf_sound_metadata *rows,
    uint32_t capacity,uint32_t *count);
typedef struct rf_sound_metadata_owner {
    rf_sound_metadata *rows;uint16_t *order;uint32_t count,allocated_bytes;
} rf_sound_metadata_owner;
/* One allocation plus caller-owned owner; budget includes both, excludes
 * input text and allocator overhead. Result must be empty. No text borrowing.
 * Repeated close is safe; discard borrowed lookup results before closing. */
int rf_sound_metadata_open(const void *text,uint32_t bytes,uint32_t budget,rf_sound_metadata_owner *result);
void rf_sound_metadata_close(rf_sound_metadata_owner *owner);
struct rf_level_owned_ambient;
typedef struct rf_ambient_instance {
    uint32_t uid;int32_t sample,slot;float position[3];
    float near_distance,volume,rolloff;uint32_t authored_word;int32_t deadline;
} rf_ambient_instance;
typedef struct rf_ambient_instances {
    rf_ambient_instance *items;uint32_t count,rejected,allocated_bytes;
} rf_ambient_instances;
/* Register by name/near/volume/rolloff; every negative result omits that instance.
 * Caller controls the audio-enabled gate. No PCM preload is requested here. */
typedef int32_t (*rf_ambient_register)(void *context,const char *name,float near_distance,float volume,float rolloff);
/* Original45aca0 state and list order, with allocation/registration supplied.
 * Budget includes owner plus capacity for all authored rows. Preflights before
 * callbacks; callback side effects are caller-owned. No archive/name pointers
 * are retained. Output must be empty; original OOM crash is not reproduced. */
int rf_ambient_instances_open(const struct rf_level_owned_ambient *authored,uint32_t budget,
    rf_ambient_register registration,void *context,rf_ambient_instances *result);
void rf_ambient_instances_close(rf_ambient_instances *instances);
rf_ambient_instance *rf_ambient_find(rf_ambient_instances *instances,uint32_t uid);
#define RF_AMBIENT_SLOTS 25u
typedef struct rf_ambient_slot {
    int32_t sample,voice;float position[3],volume;
} rf_ambient_slot;
typedef struct rf_ambient_voice_backend {
    int32_t (*start)(void *context,int32_t sample,float gain,float pan,uint32_t looping);
    void (*stop)(void *context,int32_t voice);
    void (*refresh)(void *context,int32_t voice,int32_t sample,const float position[3]);
} rf_ambient_voice_backend;
/* Original505f75..50603c decision for one control slot. Spatial calculation and
 * gain/category ownership are supplied; finite floats are required. Compare
 * the unrounded product to binary32(.1), round only the start argument. Loop
 * selection uses the low byte. Start stores any returned signed handle; stop
 * clears it after callback. Active loops refresh through the backend's separate
 * original543c20/544390 gain path; active one-shots do not refresh. Callbacks
 * must not mutate the slot; all three functions must be valid. No PCM ownership. */
void rf_ambient_voice_update(rf_ambient_slot *slot,float spatial_gain,float pan,float category_gain,
    uint32_t looping,const rf_ambient_voice_backend *backend,void *context);
/* Original505ac0/505b50/505b80 ambient control table, separate from mixer voices.
 * enabled uses its low byte. Start returns first free index or -1; it does not
 * load PCM or start a device voice. Invalid/inactive slots are unchanged.
 * Position and volume are copied without normalization, as in the original. */
int32_t rf_ambient_slot_start(rf_ambient_slot slots[RF_AMBIENT_SLOTS],uint32_t enabled,
    int32_t sample,const float position[3],float volume);
void rf_ambient_slot_volume(rf_ambient_slot slots[RF_AMBIENT_SLOTS],uint32_t enabled,int32_t slot,float volume);
void rf_ambient_slot_position(rf_ambient_slot slots[RF_AMBIENT_SLOTS],uint32_t enabled,int32_t slot,const float position[3]);
/* Original45ade0 startup (initial=1) or45ae30 tick (initial=0), in instance
 * order. Startup only handles slot==-1; tick handles every negative slot.
 * A delayed allocation clears its timer even if no slot is available.
 * Caller initializes/owns the slot table. No PCM/device work. Valid timer
 * domain is the shared game clock's single period; invalid inputs preserve
 * both owners. Instances/table must not overlap. */
int rf_ambient_schedule(rf_ambient_instances *instances,rf_ambient_slot slots[RF_AMBIENT_SLOTS],
    uint32_t enabled,int32_t now,uint32_t initial);
/* DirectSound hundredths-of-dB adapter to linear L/R amplitude.
 * Volume -10000..0, pan -10000..10000; errors preserve output. */
int rf_audio_device_gains(int32_t volume,int32_t pan,float output[2]);
/* Original 521680/522420 device volume, finite input -1..2.
 * linear_mode selects the alternative table; result is device attenuation units.
 * This is not a linear PCM gain. No allocation or transcendental work per call. */
int32_t rf_audio_device_volume(float volume,uint32_t linear_mode);
/* Original 544960 cutoff, after registration normalizes near distance.
 * Finite near > 0, rolloff > 0, default_volume >= 0 are caller preconditions. */
float rf_audio_far_distance(float near_distance,float rolloff,float default_volume);
/* Original 505740 positional calculation. Caller supplies finite vectors,
 * near_distance > 0, finite far_distance, factor >= 0, volume >= 0.
 * Output order is pan, gain; listener_right is the listener orientation axis.
 * Does not apply the later volume-group/sample gain or device conversion. */
void rf_audio_position(const float position[3],const float listener[3],
    const float listener_right[3],float near_distance,float far_distance,
    float factor,float volume,float output[2]);
typedef struct rf_wave_pcm {
    const uint8_t *samples;uint32_t bytes,frames,rate,channels,bits;
} rf_wave_pcm;
/* Bounded RIFF/WAVE PCM resource adapter, not original audio-engine recovery.
 * Borrows immutable input; no allocation/conversion. Supports PCM8/16 mono or
 * stereo, skips bounded metadata chunks and rejects ambiguous fmt/data chunks.
 * RIFF extent must equal supplied size. Errors preserve output. */
int rf_wave_pcm_parse(const void *data,uint32_t size,rf_wave_pcm *result);
typedef struct rf_audio_parameters {float near_distance,far_distance,volume,rolloff;} rf_audio_parameters;
/* Original543a60 category/default/input multiplication, rounded at the caller. */
float rf_audio_sample_gain(float default_volume,float category_gain,float scale);
/* Original543c20 ambient refresh, including exact-position fast path, low-byte
 * enabled gate, rounded distance, unrounded attenuation and upper-only clamp.
 * Finite inputs/positive near distance are required. Equal positions apply
 * scale without clamping; elsewhere only scale<1 affects gain. Device544390
 * performs its own later0..1 clamp. No pan update or slot volume is applied. */
float rf_audio_ambient_gain(const rf_audio_parameters *parameters,const float position[3],
    const float listener[3],float category_gain,float scale,uint32_t enabled);
typedef struct rf_audio_declaration {char name[61];float near_distance,volume,rolloff;} rf_audio_declaration;
/* Port table adapter: bounded #Sounds Start/End, quoted archive names and three
 * finite decimal values per row. No allocation; max2048 rows. Errors preserve
 * rows/count. NULL rows with capacity0 queries count. Input/output must not overlap.
 * Does not register sounds or implement original table-parser error handling. */
int rf_sound_table_read(const void *text,uint32_t bytes,rf_audio_declaration *rows,
    uint32_t capacity,uint32_t *count);
/* Loads sounds.tbl through a borrowed open archive. scratch_budget bounds the
 * temporary text allocation; caller-owned rows are separate. Same output/query
 * contract as read, with no retained archive/text pointers. */
int rf_sound_table_load(rf_vpp *tables,uint32_t scratch_budget,
    rf_audio_declaration *rows,uint32_t capacity,uint32_t *count);
typedef struct rf_audio_sample { char name[61];void *storage;rf_wave_pcm pcm;uint32_t bytes;rf_audio_parameters parameters; } rf_audio_sample;
typedef struct rf_audio_bank {
    rf_vpp *archive;rf_audio_sample *samples;uint32_t count,capacity,bytes,budget;
} rf_audio_bank;
/* One-archive, level-lifetime PCM owner. Budget includes bank/slot storage and
 * whole retained files, excluding allocator overhead. Archive is borrowed for
 * loading; it may close after loading. Loaded PCM outlives the archive. Names
 * deduplicate case-insensitively. Missing/invalid/over-budget loads preserve
 * bank and index. Stop all borrowing voices before close or explicit unload. */
int rf_audio_bank_open(rf_vpp *archive,uint32_t capacity,uint32_t budget,rf_audio_bank *bank);
int rf_audio_bank_load(rf_audio_bank *bank,const char *name,uint32_t *index);
/* First successful registration wins, including parameters, on duplicate names.
 * Nonpositive near normalizes to one; other parameters must be finite with
 * positive rolloff and nonnegative volume. Duplicate registration/load returns
 * the existing index even if explicitly unloaded; use reload to restore PCM. */
int rf_audio_bank_register(rf_audio_bank *bank,const char *name,float near_distance,
    float volume,float rolloff,uint32_t *index);
/* Reserve a stable name/parameter entry after archive-directory lookup, without
 * reading or allocating PCM. First registration wins. Uses preallocated slots;
 * reload validates/loads the waveform later. Errors preserve bank and index. */
int rf_audio_bank_declare(rf_audio_bank *bank,const char *name,float near_distance,
    float volume,float rolloff,uint32_t *index);
const rf_audio_parameters *rf_audio_bank_parameters(const rf_audio_bank *bank,uint32_t index);
const rf_wave_pcm *rf_audio_bank_sample(const rf_audio_bank *bank,uint32_t index);
/* Explicit residency control; caller must release ALL device/mixer borrowers
 * first. An asynchronous device stop alone does not release buffer ownership.
 * Unload preserves index/name/parameters and is idempotent. Sample returns NULL
 * while unloaded. Reload borrows an open archive only for this call, preserves
 * metadata, and leaves the bank unchanged on failure. No automatic eviction. */
int rf_audio_bank_unload(rf_audio_bank *bank,uint32_t index);
int rf_audio_bank_reload(rf_audio_bank *bank,rf_vpp *archive,uint32_t index);
void rf_audio_bank_close(rf_audio_bank *bank);
#define RF_AUDIO_VOICES RF_AUDIO_ORDINARY_SLOTS
#define RF_AUDIO_RATE 48000u
/* Original1753c38 control records, separate from port mixer/device voices.
 * Game handles encode signed generation<<8 | slot, not mixer handles.
 * fields10/14 await recovery of their producers. */
typedef struct rf_audio_control_voice {
    int32_t device,sample;uint32_t category;int32_t generation;
    float field10,field14,position[3],volume;uint32_t positional;
} rf_audio_control_voice;
/*505a40 -> supplied5442b0 ->505680. Disabled low byte, invalid slot, negative
 * device or stale generation do nothing. Handle zero is valid. Stop precedes
 * reset; generation, position and positional marking survive reset.
 * Table/callback must be valid when a matching voice can be stopped. Callback
 * may mutate fields but must retain table storage. No allocation. */
void rf_audio_control_stop(rf_audio_control_voice voices[RF_AUDIO_VOICES],uint32_t enabled,
    int32_t handle,void (*stop_device)(void *context,int32_t device),void *context);
typedef struct rf_audio_voice {
    rf_wave_pcm pcm;uint32_t handle,frame,phase,left,right,loop,active;
} rf_audio_voice;
typedef struct rf_audio_mixer { rf_audio_voice voices[RF_AUDIO_VOICES];uint32_t generation; } rf_audio_mixer;
/* Xbox/PC output adapter, not original Miles mixer reconstruction. Caller
 * serializes access and retains PCM until the voice ends/stops. No heap, FPU,
 * callbacks or OS input. PCM rates1..192000, gain0..32768 (unity), stereo48kHz
 * signed16 output, integer linear interpolation and saturating summation. */
void rf_audio_mixer_init(rf_audio_mixer *mixer);
int rf_audio_voice_start(rf_audio_mixer *mixer,const rf_wave_pcm *pcm,
    uint32_t left,uint32_t right,uint32_t loop,uint32_t *handle);
/* Updates a live logical voice without changing sample/phase or restarting.
 * Stale handles and out-of-range Q15 gains leave the mixer unchanged. */
int rf_audio_voice_gain(rf_audio_mixer *mixer,uint32_t handle,uint32_t left,uint32_t right);
/* Releases a matching voice even after natural completion; clears its PCM
 * borrower. Zero/stale handles leave the mixer unchanged. Device borrowers
 * are separate and must also be released before bank PCM is unloaded. */
int rf_audio_voice_stop(rf_audio_mixer *mixer,uint32_t handle);
/* Port residency bridge. Caller serializes all borrowers. Rejects active or
 * looping shared voices, then asks release_device to release idle device
 * borrowers; failure preserves shared/bank ownership. NULL callback means no
 * device borrowers exist. Exact PCM base pointers only; no overlapping aliases.
 * Success clears completed shared voices and unloads PCM, preserving metadata. */
int rf_audio_bank_release_idle(rf_audio_bank *bank,rf_audio_mixer *mixer,uint32_t index,
    int (*release_device)(void *context,const uint8_t *samples),void *context);
/* Budget-pressure retry over caller-selected resident candidates in index
 * order. released[0/1] reports reclaimed sample count/bytes even on failure.
 * Same borrower contract as release_idle; original eviction order unverified. */
int rf_audio_bank_reload_idle(rf_audio_bank *bank,rf_audio_mixer *mixer,rf_vpp *archive,uint32_t index,
    const uint8_t *eligible,uint32_t eligible_count,int (*release_device)(void *,const uint8_t *),
    void *context,uint32_t released[2]);
int rf_audio_mix(rf_audio_mixer *mixer,int16_t *stereo,uint32_t frames);
#endif
