#ifndef RF_CLUTTER_H
#define RF_CLUTTER_H
#include "rf/object_registry.h"
/*4686c0: ten fixed material names, ASCII-insensitive, unknown -> default0.
 *415430: first exact bytewise glare name;497550: first ASCII-insensitive emitter
 * name. Empty names may match an empty slot. NULL slots are empty; NULL query
 * and invalid arrays/counts return-1 for the port, without dereferencing them.
 * These are pure name lookups, not table loading or resource registration. */
uint32_t rf_clutter_material_index(const char *name);
int32_t rf_glare_name_lookup(const char *const *names,uint32_t count,const char *name);
int32_t rf_emitter_name_lookup(const char *const *names,uint32_t count,const char *name);
/* Original513020 with40f4f0's nine-entry flag vocabulary. Read one quoted
 * parenthesized list; consumed stops immediately after its closing parenthesis.
 * ASCII-insensitive names, duplicate flags OR together; comments allowed.
 * Unknown/malformed input fails without modifying either output. The original
 * fatal parser diagnostic is returned as RF_FORMAT. No allocation. */
int rf_clutter_flags_read(const void *text,uint32_t bytes,uint32_t *flags,uint32_t *consumed);
typedef struct rf_clutter_definition {
    char name[64],model[64],corpse[64],material[64],sound[64],explosion[64],glare[64],rod[64];
    char emitters[16][64];uint32_t emitter_count,model_kind,flags;
    float emitter_lifetime,life,radius;uint32_t screen_width,screen_height,resource_fields;
} rf_clutter_definition;
enum {RF_CLUTTER_HAS_SOUND=1,RF_CLUTTER_HAS_EXPLOSION=2,RF_CLUTTER_HAS_GLARE=4,RF_CLUTTER_HAS_ROD=8};
/* Owned factory-facing metadata for the first ASCII-insensitive class match.
 * No allocation or borrowed text. Required model/material/life/flags; defaults
 * follow40f4f0 through40f9b7. Stops before skin overrides. Bounds:63-byte names,
 * 16 emitters,255-byte tokens. Output unchanged on errors. Resource IDs, skins,
 * damage/debris/use/light metadata and full original parser equivalence are
 * separate; this is not a complete runtime class or class resource loader. */
int rf_clutter_definition_read(const void *text,uint32_t bytes,const char *name,
    rf_clutter_definition *result);

typedef struct rf_clutter_class {
    const char *name,*model,*corpse;
    const int32_t *emitters;uint32_t emitter_count;
    float emitter_lifetime;uint32_t model_kind;float life,radius;
    uint32_t material,flags;int32_t sound,explosion,glare,rod;
    int32_t timer,coronas[4];uint32_t corona_count;int32_t light_tag;
    uint32_t screen_width,screen_height;
} rf_clutter_class;
typedef struct rf_clutter_class_binding {
    uint32_t material;int32_t sound,explosion,glare,rod;const int32_t *emitters;
} rf_clutter_class_binding;
struct rf_foley_owner;
typedef struct rf_clutter_resource_names {
    const char *const *emitters;uint32_t emitter_count;
    const char *const *glares;uint32_t glare_count;
    const char *const *vclips; /*64 slots when explosion is authored. */
    const struct rf_foley_owner *sounds;
} rf_clutter_resource_names;
typedef struct rf_clutter_catalogs {
    void *storage;rf_clutter_resource_names names;uint32_t allocated_bytes,peak_bytes;
} rf_clutter_catalogs;
/* Load names from emitters.tbl/effects.tbl/vclip.tbl in authored order using
 * one reusable archive scratch block and one retained allocation. Limits64
 * names per catalog and63 bytes/name; vclip always exposes64 slots. Budget
 * includes owner, retained bytes and scratch (not allocator overhead/stack).
 * Initially zero owner required, errors preserve it. Archive contents must
 * remain stable across passes. Foley is borrowed and must outlive catalog use.
 * This is a bounded name-catalog loader, not effect definition/resource loading.
 * Close frees only this storage and can be repeated after borrowers retire. */
int rf_clutter_catalogs_open(rf_vpp *tables,const struct rf_foley_owner *sounds,
    uint32_t budget,rf_clutter_catalogs *owner);
void rf_clutter_catalogs_close(rf_clutter_catalogs *owner);
/* Resolve factory-facing material/effect IDs with verified lookup rules.
 * Absent optional fields produce-1 without lookup; explicit empty glare names
 * can match an empty catalog entry. Missing names retain the original-1 result.
 * Caller supplies stable terminated catalogs; emitter IDs are copied into its
 * capacity-sized output, and binding.emitters points there (NULL when empty).
 * Both outputs must be disjoint and remain unchanged on error. No allocation,
 * resource creation or reference ownership; catalogs preserve original order. */
int rf_clutter_definition_bind(const rf_clutter_definition *definition,
    const rf_clutter_resource_names *names,int32_t *emitter_ids,uint32_t capacity,
    rf_clutter_class_binding *binding);
typedef struct rf_clutter_classes {
    void *storage;rf_clutter_class *items;uint32_t count,allocated_bytes;
} rf_clutter_classes;
/* Compact port ownership: one allocation holds runtime classes, copied emitter
 * IDs and exact terminated name/model/corpse strings. Class order and duplicate
 * names are preserved. Bindings supply externally resolved IDs, not resources
 * owned by this allocation; no lookup or resource side effects occur here.
 * Budget includes owner plus allocation, excludes caller inputs/stack. Source
 * arrays may be released after success. Initially zero owner required; errors
 * preserve it. Close requires all borrowing objects retired and is repeatable.
 * Factory caches start empty; timer0/light tag-1 are port initial storage. */
int rf_clutter_classes_open(const rf_clutter_definition *definitions,
    const rf_clutter_class_binding *bindings,uint32_t count,uint32_t budget,rf_clutter_classes *owner);
typedef int (*rf_clutter_class_fetch)(void *context,uint32_t index,
    const rf_clutter_definition **definition,const rf_clutter_class_binding **binding);
/* Same owner, with rows fetched in two ordered passes. Fetch is read-only and
 * must return identical row content across passes; returned storage need only
 * survive until the next fetch. Callback errors free partial storage and leave
 * output unchanged. Caller-owned source workspace is excluded from budget. */
int rf_clutter_classes_open_source(rf_clutter_class_fetch fetch,void *context,
    uint32_t count,uint32_t budget,rf_clutter_classes *owner);
/* Archive composition, retaining all authored rows including duplicate names.
 * One scratch allocation holds table text,450 span slots and a reusable parsed
 * row/binding. Budget/peak include that workspace and final owner, excluding
 * caller-owned resource catalogs and stack. Outputs unchanged on error.
 * Resources and archive are borrowed only during load; resulting IDs still
 * refer to externally owned resources. No generic objects are instantiated. */
int rf_clutter_classes_load(rf_vpp *tables,const rf_clutter_resource_names *resources,
    uint32_t budget,rf_clutter_classes *owner,uint32_t *peak_bytes);
void rf_clutter_classes_close(rf_clutter_classes *owner);
typedef struct rf_clutter_state {
    uint32_t token,first_word,handle,model,flags,physics_flags;
    float position[3],health,armor;
    const char *name;rf_clutter_class *definition;int32_t class_index,corpse,sound;
    int32_t timer_a4,word_a8,timer_b0,timer_b4;uint32_t emitter_head,word_b8;
    int32_t skin,sound_d0;uint8_t byte_cc;uint16_t slot;
    rf_object_link link;
} rf_clutter_state;
typedef struct rf_clutter_create_descriptor {
    const char *model;uint32_t kind,material,flags,allocation_flags;
    int32_t identifier;float position[3],matrix[9],radius;
} rf_clutter_create_descriptor;
enum rf_clutter_create_operation {
    RF_CLUTTER_SOUND,RF_CLUTTER_SOUND_HANDLE,RF_CLUTTER_EMITTER,
    RF_CLUTTER_EMITTER_PREPEND,RF_CLUTTER_TAG,RF_CLUTTER_GLARE,
    RF_CLUTTER_ROD,RF_CLUTTER_SCREEN,RF_CLUTTER_EXPLOSION,
    RF_CLUTTER_COLLISION,RF_CLUTTER_SLOT
};
typedef struct rf_clutter_create_request {
    uint32_t values[5];const char *text;const float *position;
} rf_clutter_create_request;
typedef struct rf_clutter_create_backend {
    int (*allocate)(void *context,const rf_clutter_create_descriptor *descriptor,
        rf_clutter_state **state);
    int (*call)(void *context,rf_clutter_state *state,uint32_t operation,
        const rf_clutter_create_request *request,int32_t *result);
    void *context;
} rf_clutter_create_backend;
/* Full4104a0 control flow with caller-owned class/name/resource storage.
 * allocate supplies generic type4 object with handle-1 and final0, including
 * world position/model/flags and an unlinked link. NULL success means no object.
 * On successful allocation *out publishes ownership immediately, even if a
 * subsequent service fails: caller must retire partial resources/object. No
 * rollback or inferred release policy. Errors before allocation preserve out.
 * Class strings, emitter IDs and backend must remain stable during callbacks.
 * Classes retain original timer/corona/light caches. List append precedes slot
 * registration; on slot failure the owner remains linked. Original malformed
 * rod assertion becomes RF_FORMAT. Class indices must be in bounds, finite
 * geometry/life/lifetime and valid shared timer domain are required.
 * Requests: SOUND(sound,1.0 bits,0),position; SOUND_HANDLE(playback);
 * EMITTER(parent,class,first_word,1),position; PREPEND(new,old);
 * TAG(model),text; GLARE(parent,tag,class,0); ROD(parent,class,tags1/2,-1);
 * SCREEN(parent,-1,width,height,1); EXPLOSION(class); COLLISION(token);
 * SLOT(slot,1). Return handles/IDs as signed32; -1 is absent, emitter0 fails.
 * Filename compilation, generic allocation, actual effects and live dispatch
 * remain separate resource services. This function allocates no memory. */
int rf_clutter_create(rf_clutter_class *classes,uint32_t count,int32_t index,
    int32_t shield_class,const char *name,int32_t identifier,const float position[3],
    const float matrix[9],uint32_t persistent,int32_t now_ms,int32_t *next_slot,
    rf_object_list *list,const rf_clutter_create_backend *backend,rf_clutter_state **out);
#endif
