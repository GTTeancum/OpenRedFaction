#ifndef RF_OBJECT_REGISTRY_H
#define RF_OBJECT_REGISTRY_H
#include "rf/vpp.h"
#define RF_OBJECT_CAPACITY 1024u
/* Original object10/14 intrusive links and73d880 sentinel. Stable caller-
 * owned nodes; initialized list, detached append nodes, and linked remove
 * nodes required. No allocation, identity lookup or destructor dispatch. */
typedef struct rf_object_link {struct rf_object_link *next,*previous;} rf_object_link;
typedef struct rf_object_list {rf_object_link sentinel;uint32_t count,peak;} rf_object_list;
void rf_object_list_init(rf_object_list *list);
/*4872eb..487321: tail append after successful allocation/room initialization;
 * peak uses original signed comparison, counters wrap at32 bits. */
void rf_object_list_append(rf_object_list *list,rf_object_link *node);
/*4867bc..4867e1: clear removed links, reconnect neighbors, decrement count.
 * Must precede object destruction and handle-slot release. */
void rf_object_list_remove(rf_object_list *list,rf_object_link *node);

typedef struct rf_object_slot { void *object;uint32_t handle; } rf_object_slot;
typedef struct rf_object_registry {
    rf_object_slot slots[RF_OBJECT_CAPACITY];
    uint32_t free_slots[RF_OBJECT_CAPACITY],head,count,generation;
} rf_object_registry;
/* Explicit fresh initialization, not an object destructor. No heap allocation.
 * Borrowed objects must stay alive until removal and may be inserted once.
 * Registry fields are internal; calls require an initialized intact registry.
 * FIFO slots and global generations follow 486ce9/486e35/48684f. Single-threaded.
 * Outputs must not overlap registry storage. Errors preserve state/output. */
void rf_object_registry_init(rf_object_registry *registry);
int rf_object_registry_insert(rf_object_registry *registry,void *object,uint32_t *handle);
void *rf_object_registry_lookup(const rf_object_registry *registry,uint32_t handle);
int rf_object_registry_remove(rf_object_registry *registry,uint32_t handle);
#endif
