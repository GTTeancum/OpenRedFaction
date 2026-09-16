#ifndef RF_GEOMOD_NOTIFY_H
#define RF_GEOMOD_NOTIFY_H
#include "rf/vpp.h"
typedef struct rf_geomod_changed_box {float minimum[3],maximum[3];} rf_geomod_changed_box;
typedef struct rf_geomod_notify_object {
    uint32_t kind,object_flags,physics_flags,parent_is_kind8;
    rf_geomod_changed_box bounds;
} rf_geomod_notify_object;
typedef struct rf_geomod_notify_change {
    const rf_geomod_changed_box *boxes;uint32_t count,retirement_enabled;
    float radius;
} rf_geomod_notify_change;
/* Actions are bits: a full box+radius notification can both wake and retire. */
enum {RF_GEOMOD_NOTIFY_NONE=0,RF_GEOMOD_NOTIFY_WAKE=1,RF_GEOMOD_NOTIFY_RETIRE=2};
typedef struct rf_geomod_notify_result {uint32_t action,object_flags,physics_flags,overlaps;} rf_geomod_notify_result;
/* Pure487370 changed-AABB branch, real48b450/46afa0/46c340/40a420/48ab40.
 * Caller supplies resolved parent-kind8 predicate and immutable actual bounds.
 * Strict overlap excludes boundary touching. Room-bound400000 required;
 * object80000/parent-kind8 excludes. kind!=4 and physics&71 wakes80000000 +
 * object06000000; otherwise enabled LOW BYTE and !object4 marks retirement2.
 * 0..32 boxes; finite ordered bounds. Positive radius returns RF_NOT_FOUND:
 * radial/class/network fallback is EXPLICITLY UNSUPPORTED, never silently skipped.
 * Finite radius<=0 selects this proven branch. RF_OK publishes even no-action.
 * All errors preserve result. No owner writes, allocation, lookup, lifecycle or
 * scene publication. Output must not alias inputs. Apply only after successful
 * complete collision/render publication; retired objects are NOT freed here. */
int rf_geomod_notify_changed_boxes(const rf_geomod_notify_object *,const rf_geomod_notify_change *,rf_geomod_notify_result *);
/* Additional immutable inputs for487370 positive-radius branch. scalar_78 is
 * the original object+78 threshold field, NOT inferred from body_radius+180.
 * class_present/flags represent actual459a20 lookup. Byte globals preserve their
 * original values; only a PRESENT class consults suppression/network gates. */
typedef struct rf_geomod_notify_radial {
    float center[3],position[3],body_radius,scalar_78;
    uint32_t class_present,class_flags,suppress_retirement,network_active;
} rf_geomod_notify_radial;
/* Full per-object box then radius decision, without external world-owner calls.
 * For radius>0 radial must be valid: strict squared distance from physics
 * position to center against(radius+body_radius)^2. Kinds1/7 and kind4 scalar<3
 * may retire; others with physics&71 wake. Class bit1/global suppression and
 * object4/4000 gate radial retirement. Final enable byte gates BOX retirement
 * only. RF_OK publishes combined action bits; errors preserve out. No effects,
 * registry mutations or global auxiliary42e4a0/42e560 dispatch are performed. */
int rf_geomod_notify_object_change(const rf_geomod_notify_object *,const rf_geomod_notify_change *,
    const rf_geomod_notify_radial *,rf_geomod_notify_result *);
#endif
