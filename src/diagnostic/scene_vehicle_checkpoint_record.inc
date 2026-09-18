#ifndef SCENE_VEHICLE_CHECKPOINT_RECORD_INC
#define SCENE_VEHICLE_CHECKPOINT_RECORD_INC
#include "rf/apc_checkpoint.h"
#include "rf/jeep_checkpoint.h"
#include "rf/fighter_checkpoint.h"
typedef struct scene_vehicle_checkpoint_record {
    uint32_t profile;
    union {rf_vehicle_checkpoint vehicle;rf_apc_checkpoint apc;rf_jeep_checkpoint jeep;rf_fighter_checkpoint fighter;} value;
} scene_vehicle_checkpoint_record;
static const void *scene_vehicle_checkpoint_typed(const scene_vehicle_checkpoint_record *r)
{
    if(r->profile==1)return &r->value.vehicle;
    if(r->profile==2)return &r->value.apc;
    if(r->profile==3)return &r->value.jeep;
    if(r->profile==5)return &r->value.fighter;
    return NULL;
}
static int scene_vehicle_checkpoint_valid(const scene_vehicle_checkpoint_record *r)
{
    uint32_t occupied;
    if(!r)return RF_RANGE;
    return rf_player_checkpoint_vehicle_profile_validate(r->profile,scene_vehicle_checkpoint_typed(r),&occupied);
}
#endif
