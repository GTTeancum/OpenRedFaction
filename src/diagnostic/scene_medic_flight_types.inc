#ifndef RF_SCENE_MEDIC_FLIGHT_TYPES_INCLUDED
#define RF_SCENE_MEDIC_FLIGHT_TYPES_INCLUDED
/* Reviewed staged runtime logical state. These are not RF.exe layouts or wire structures.
 * Include before campaign_npc_body; all fields use existing stdint types. */
enum {
    SCENE_FLIGHT_NODE_NONE=0,
    SCENE_FLIGHT_NODE_AUTHORED=1,
    SCENE_FLIGHT_NODE_OWN_START=2
};
enum {
    SCENE_FLIGHT_TARGET_NONE=0,
    SCENE_FLIGHT_TARGET_PLAYER=1,
    SCENE_FLIGHT_TARGET_UNRESOLVED=2
};
typedef struct scene_medic_flight_node {
    uint32_t kind,ordinal; /* ordinal is zero-based only for AUTHORED. */
} scene_medic_flight_node;
typedef struct scene_medic_flight_start {
    float position[3],query_point[3],radius,height;
    uint32_t word_040;
} scene_medic_flight_start;
typedef struct scene_medic_flight_nav {
    scene_medic_flight_node pair[2],prefix[4];
    uint32_t count;
    int32_t previous,current,wait_134;
    float turn_138[3],request_eye_14c[3],bias_158,steering_174[3];
    uint32_t approach_15c,activity_170,start_valid,eye_valid;
} scene_medic_flight_nav;
typedef struct scene_medic_flight_state {
    uint32_t qualified,entered,target_kind,target_handle;
    int32_t deadline_250,sight_deadline_294,no_route_deadline_4d4;
    int32_t reset_deadline_1370,stall_deadline_4d0;
    uint32_t visible_published_29c,visible_eye_29d;
    scene_medic_flight_nav nav;
} scene_medic_flight_state;
/* Portable capture image. target_handle must be zero here; target_kind owns
 * logical identity. Runtime PLAYER resolves the actual full registry handle.
 * OWN_START always aliases owner.navigation.start, not this copied image. */
typedef struct scene_medic_flight_snapshot {
    scene_medic_flight_state state;
    scene_medic_flight_start start;
    float command_714[3]; /* Existing actor command, retained until next AI service. */
    float angular_150[3],torque_174[3]; /* Existing body vectors, entered-only. */
} scene_medic_flight_snapshot;
typedef struct scene_medic_flight_rng {
    uint32_t present,value;
} scene_medic_flight_rng;
/* Interval-borrowed, prevalidated actual local player facts. Never serialized. */
typedef struct scene_medic_flight_threat_view {
    uint32_t handle;
    const rf_entity_view *view;
    float position[3],eye[3],model_radius;
} scene_medic_flight_threat_view;
/* RF status is the function return. The separate source_result is only
 * -2 (body blocker), -1 (route missing), or0 (including emitted movement).
 * no_route requests the immediate408580 side effect after short-prefix clear;
 * runtime publishes actor4d0 before that call for this short-prefix result. */
typedef struct scene_medic_flight_nav_result {
    int32_t source_result;
    uint32_t no_route;
    float command[3],turn_point[3];
    uint32_t turn_valid,command_valid;
} scene_medic_flight_nav_result;
#endif
