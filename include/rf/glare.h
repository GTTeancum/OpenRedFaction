#ifndef RF_GLARE_H
#define RF_GLARE_H
#include "rf/visibility.h"
#include "rf/object_registry.h"
#include "rf/physics.h"
typedef struct rf_glare_definition {
    char name[64],corona[64],volumetric[64],reflection[64];uint32_t color[3];
    float cone_degrees,intensity,radius_distance,radius_scale,diminish,height,length;
    uint32_t fields;
} rf_glare_definition;
/* First exact name in #Glares. Owned authored metadata, no bitmap loading or
 * degree conversion. fields bits1/2/4 indicate corona/volume/reflection keys.
 * Required light color; optional bitmap blocks require their numeric fields.
 * Absent fields zero. Errors preserve output; bounded63-byte strings. */
int rf_glare_definition_read(const void *text,uint32_t bytes,const char *name,rf_glare_definition *result);
typedef struct rf_glare_class {
    float size_first,size_second;const void *definition;
} rf_glare_class;
typedef struct rf_glare_classes {
    rf_glare_definition *definitions;rf_glare_class *items;
    uint32_t count,allocated_bytes,peak_bytes;
} rf_glare_classes;
/* Own #Glares rows in authored order, including duplicate names. Factory
 * views borrow their corresponding owned definition and volumetric sizes.
 * One retained allocation plus transient effects.tbl buffer, both budgeted;
 * no archive borrowing after success. Max64 rows, empty destination required.
 * Bitmap names remain metadata; no renderer resource IDs are assigned. */
int rf_glare_classes_open(rf_vpp *tables,uint32_t budget,rf_glare_classes *owner);
void rf_glare_classes_close(rf_glare_classes *owner);
/* Original290: actor handle;294: moving-solid pointer token;298: face token.
 * Keep cache words separate from the four per-view sample floats29c..2a8. */
typedef struct rf_glare_state {
    uint32_t parent;int32_t tag;uint8_t active,reserved[3];
    int32_t occluder;uint32_t cached_solid,cached_face;float samples[4];const void *definition;int32_t class_index;
    uint32_t flags;rf_object_link link;float last_position[3];uint32_t word_2cc;
    uint8_t byte_2d0,padding[3];float vectors[2][3];
} rf_glare_state;
typedef struct rf_glare_create_descriptor {
    uint32_t parent;float radius,position[3],matrix[9];
} rf_glare_create_descriptor;
typedef struct rf_glare_create_backend {
    int (*tag_pose)(void *,uint32_t parent,int32_t tag,float result[12]);
    int (*allocate)(void *,const rf_glare_create_descriptor *,rf_glare_state **);
    void *context;
} rf_glare_create_backend;
/*413d20 parented glare. Maximum class size precedes parent/tag lookup and type10
 * allocation (identifier-1,parent,flags30000,final0; descriptor flags0/scale1).
 * Backend supplies generic object ownership with unlinked glare state. NULL
 * allocation succeeds without insertion. Class out of range returns NULL
 * without callbacks. Success appends in factory order; class storage/list and
 * allocated state must outlive their borrowers. No rendering/deletion here.
 * Callback failures stop with output preserved; allocation errors must clean
 * untransferred ownership. Successful nonnull allocation is published before
 * initialization. Reserved bytes retain generic allocator values. No heap
 * allocation in this function. Finite inputs and disjoint outputs required. */
int rf_glare_create(const rf_glare_class *classes,uint32_t count,int32_t index,
    uint32_t parent,int32_t tag,uint32_t flag,rf_object_list *list,
    const rf_glare_create_backend *backend,rf_glare_state **out);
typedef struct rf_glare_base_owner {
    rf_glare_state state;rf_object_link object_link;
    uint32_t handle,uid,flags,kind,parent_handle,parent_byte,parent_group;
    int32_t identifier;float radius,health,position[3],matrix[9];
    rf_physics_body body;uint32_t allocated_bytes;
} rf_glare_base_owner;
/* Type10 generic ownership for413d20: no model, flags30000, descriptor
 * flags0/scale1/material0, no ordinary room binding. Resolved parent byte/group
 * and material coefficients supplied. One owner allocation; registry/list
 * publication precedes physics. Empty registry returns NULL successfully.
 * UID/generation consumed after publication are not rolled back on failure.
 * Budget counts owner/body storage once, excludes registry/list/allocator.
 * Zero output required. Full glare factory/effect state is initialized later. */
int rf_glare_base_open(const rf_glare_create_descriptor *descriptor,
    rf_object_registry *registry,rf_object_list *objects,uint32_t *uid_cursor,
    uint32_t parent_byte,uint32_t parent_group,const float material[3],
    uint32_t budget,rf_glare_base_owner **out);
/* Retire glare family link first (4153b0, via rf_object_list_remove).
 * Then release body/global link/heap and recycle the handle. NULL repeats OK;
 * linked glare state rejects close. External effects must already be retired. */
int rf_glare_base_close(rf_glare_base_owner **owner,rf_object_registry *registry,rf_object_list *objects);
typedef struct rf_glare_services {
    int (*tag_pose)(void *,uint32_t,int32_t,float[12]);void *context;
} rf_glare_services;
/* Compose413d20 with concrete type10 ownership. External services supply
 * registered parent pose; all other creation/lifetime storage
 * is owned here. Parent/material inputs are resolved by caller. Classes and
 * lists outlive the owner. Empty output required; successful NULL means no
 * object. Callback errors preserve output; prior callback effects are not undone. */
int rf_glare_owned_open(const rf_glare_class *classes,uint32_t count,int32_t index,
    uint32_t parent,int32_t tag,uint32_t flag,rf_object_registry *registry,
    rf_object_list *objects,rf_object_list *glares,uint32_t *uid_cursor,
    uint32_t parent_byte,uint32_t parent_group,const float material[3],uint32_t budget,
    const rf_glare_services *services,rf_glare_base_owner **out);
/* Valid linked family owner required; unlinks glare state before base close.
 * NULL owner is repeatable. No renderer/resource borrower teardown implied. */
int rf_glare_owned_close(rf_glare_base_owner **owner,rf_object_registry *registry,
    rf_object_list *objects,rf_object_list *glares);
typedef struct rf_glare_render_backend {
    int (*enable)(void *,uint32_t);
    int (*corona)(void *,rf_glare_base_owner *,uint32_t);
    int (*reflection)(void *,rf_glare_base_owner *);
    void *context;
} rf_glare_render_backend;
/*4154f0 corona/reflection dispatch. First matching view wins; at most two
 * sample slots are represented by the owned state. Hidden base flag1 skips
 * the owner. Marker0x80000000 selects corona, otherwise clear that view's
 * two samples. Optional reflection uses the low byte; consume the marker
 * after successful callbacks. No volume pass or visibility producer here.
 * Backend must not mutate list ownership. After enable succeeds, later callback
 * errors disable rendering and preserve prior progress; the failed owner's
 * marker is not consumed. An enable failure returns directly. */
int rf_glare_render_pass(rf_object_list *glares,const void *const *views,
    uint32_t count,const void *current,uint32_t reflections,const rf_glare_render_backend *backend);
/*48847e..4884ec for one resolved owner. Match room token and active byte;
 * positive volume bitmap ID selects sorted volume callback, otherwise cull
 * without enqueueing. Accepted culling ORs marker0x80000000; rejection leaves
 * existing marker intact. Resolved cull position includes world/instance offset.
 * Callback is an opaque queue identifier, required nonzero for volume>0.
 * No allocation, room lookup, list traversal, geometry or draw dispatch. */
int rf_glare_collect(rf_glare_base_owner *owner,uint32_t room,uint32_t current_room,
    int32_t volume,uint32_t callback,const rf_visibility_frustum *frustum,
    const float cull_position[3],rf_render_queue_record *records,uint32_t capacity,
    uint32_t *count,uint32_t *accepted);
/*415280 candidate occlusion: require flag0x10/model, exclude the selected
 * object token and glare's parent handle; box-test glare->camera, then query
 * model camera->glare with flags1/reset1. Successful low-byte hit normalizes
 * to0/1. Uses only backend.model. Errors preserve blocked; candidate/glare
 * geometry is borrowed. Full world/actor search and cached-handle update
 * in414e00 are separate. */
int rf_glare_occluder_test(const rf_collision_visibility_object *candidate,
    uint32_t candidate_handle,uint32_t excluded,const rf_glare_base_owner *glare,
    const float camera[3],const rf_collision_visibility_backend *backend,uint32_t *blocked);
typedef struct rf_glare_visibility_object {
    rf_collision_visibility_object geometry;uint32_t handle,solid;
} rf_glare_visibility_object;
typedef struct rf_glare_visibility_list {
    const rf_glare_visibility_object *items;uint32_t count;
} rf_glare_visibility_list;
typedef struct rf_glare_solid_query {
    uint32_t preferred_face;rf_collision_solid_response_query input;
} rf_glare_solid_query;
typedef struct rf_glare_visibility_backend {
    int (*lookup)(void *,uint32_t,const rf_glare_visibility_object **);
    int (*solid_owner)(void *,uint32_t,const rf_glare_visibility_object **);
    int (*room)(void *,const rf_glare_visibility_object *,uint32_t *);
    int (*state)(void *,const rf_glare_visibility_object *,uint32_t *);
    int (*associated)(void *,const rf_glare_visibility_object *,const rf_glare_visibility_object **);
    int (*solid_query)(void *,uint32_t,const rf_glare_solid_query *,rf_collision_solid_response_hit *,uint32_t);
    int (*model)(void *,const rf_collision_visibility_object *,rf_collision_model_part_query *,
        rf_collision_model_response_hit *,uint32_t,uint32_t *);
    void *context;
} rf_glare_visibility_backend;
/* Full414e00 search with stable borrowed views in original traversal order.
 * Nonzero unique object tokens identify live owners; face tokens stay stable.
 * Cache resolution must return the matching owner; callbacks must not mutate lists
 * or glare. Lookup returns NULL for missing owners. Errors preserve visible,
 * while completed cache invalidations remain. Solid callbacks implement reset1.
 * Deliberate original defect correction: cached solid query is initialized to
 * origin0/identity/radius0/flags5 or85, rather than reading previous stack bytes.
 * The selected object is the original5cb054 role, supplied without guessing it.
 * No allocation, alternating-frame scheduling, or rendering in this entry. */
int rf_glare_visibility_search(rf_glare_base_owner *glare,const float camera[3],
    const rf_glare_visibility_list *movers,const rf_glare_visibility_list *actors,
    const rf_glare_visibility_object *selected,uint32_t world,uint32_t special,
    const rf_glare_visibility_backend *backend,uint32_t *visible);
#endif
