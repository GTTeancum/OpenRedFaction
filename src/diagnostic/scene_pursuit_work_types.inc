#ifndef RF_SCENE_PURSUIT_WORK_TYPES_INCLUDED
#define RF_SCENE_PURSUIT_WORK_TYPES_INCLUDED
/* Transient tick values only. Include after campaign_npc_body and tactical receipt. */
typedef struct scene_pursuit_ordinary_token {
    uint64_t epoch;campaign_npc_body *owners;
    uint32_t count,frame,slot,handle,target,mode;
    int32_t weapon;
    scene_tactical_receipt tactical;
} scene_pursuit_ordinary_token;
typedef struct scene_pursuit_work_dispatch_result {
    uint64_t epoch;
    uint32_t kind,slot,ordinary_valid;
    int service_status;
    scene_pursuit_ordinary_token ordinary;
    uint32_t cancelled_valid; /* empty-receipt completion/cancellation suppression */
    scene_pursuit_ordinary_token cancelled_owner;
} scene_pursuit_work_dispatch_result;
#endif
