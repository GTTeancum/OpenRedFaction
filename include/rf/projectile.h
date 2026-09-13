#ifndef RF_PROJECTILE_H
#define RF_PROJECTILE_H
#include "rf/weapon.h"
#include "rf/clutter.h"
/* Resources held beside each fixed pool slot. Model backend storage has its
 * own caller-enforced budget; these counters cover this owner and physics
 * records/scratch only. No effects, room membership or active simulation yet. */
typedef struct rf_projectile_resources {
    rf_object_model_attachment attachment;
    rf_physics_body body;
    uint32_t allocated_bytes,peak_bytes;
} rf_projectile_resources;
/* Prepared4c77a0 descriptor: zero initial tensor, no geometric mass grid or preinstalled spheres.
 * name resolves words[0], words[1] selects the model wrapper. Backend shares
 * generic object model/sphere services with clutter; failed load owns cleanup,
 * successful load transfers ownership. Zero owner required; failure preserves
 * it and releases partial resources. Descriptor remains caller-owned/unchanged.
 * Negative radius resolves from model; model-less radius defaults to1.
 * The model backend must outlive this owner and close cannot fail. */
int rf_projectile_resources_open(const rf_projectile_creation_descriptor *,const char *name,
    const float material[3],const rf_clutter_base_backend *,uint32_t budget,rf_projectile_resources *);
void rf_projectile_resources_close(rf_projectile_resources *,const rf_clutter_base_backend *);
/* Compose initialization with pool/list/registry ownership. resources is a
 * zero-initialized50-slot array paired with store for its entire lifetime.
 * budget covers the selected resource owner and its allocations; caller
 * accounts for the rest of the array, store and model storage separately. No automatic retirement on flag2.
 * Callers must use this close path for objects opened through this adapter. */
int rf_projectile_initialized_open(rf_projectile_store *,rf_projectile_resources resources[50],
    rf_object_registry *,rf_object_list *,uint32_t *uid,
    const rf_projectile_creation_descriptor *,const char *,const float material[3],
    const rf_clutter_base_backend *,uint32_t budget,rf_projectile_owner **);
int rf_projectile_initialized_close(rf_projectile_store *,rf_projectile_resources resources[50],
    rf_object_registry *,rf_object_list *,uint32_t handle,const rf_clutter_base_backend *);
#endif
