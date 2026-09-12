#ifndef RF_CLUTTER_H
#define RF_CLUTTER_H
#include "rf/object_registry.h"

typedef struct rf_clutter_class {
    const char *name,*model,*corpse;
    const int32_t *emitters;uint32_t emitter_count;
    float emitter_lifetime;uint32_t model_kind;float life,radius;
    uint32_t material,flags;int32_t sound,explosion,glare,rod;
    int32_t timer,coronas[4];uint32_t corona_count;int32_t light_tag;
    uint32_t screen_width,screen_height;
} rf_clutter_class;
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
