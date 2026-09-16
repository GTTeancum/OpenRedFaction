#ifndef RF_SWIM_H
#define RF_SWIM_H
#include "rf/movement.h"
typedef struct rf_swim_state {
    uint32_t mode,object_flags,entity_flags,body_flags,query_flags;
} rf_swim_state;
typedef struct rf_swim_transition {
    rf_swim_state state;
    int32_t posture_request; /* -1 unchanged; apply427450 before motion. */
    uint32_t entered,exited,reset_parent_basis;
} rf_swim_transition;
/*429100/428270/4281a0. Caller supplies actual room/body/eye membership,
 * class eligibility and the external42a020 falling/support admission result.
 * Ordinary actor only: vehicle/crouch side effects must be resolved by caller.
 * No-room clears wet flags but does not force fall; dry existing room does.
 * Mode is descriptor index; descriptor fallback follows original4339d0. */
int rf_swim_transition_prepare(const rf_swim_state *state,
    const rf_movement_descriptor descriptors[16],uint32_t class_flags,
    int has_room,int body_wet,int eye_wet,int outer_fall_admission,
    rf_swim_transition *output);
typedef struct rf_swim_controls {
    float right,left,up,down,forward,backward,jump,crouch;
    uint32_t jump_press_mode,crouch_press_mode;
} rf_swim_controls;
typedef struct rf_swim_motion {
    float local[3],acceleration[3],resistance;
} rf_swim_motion;
/*430760/4307a0,49f6cd..49f7c3; ordinary authored mode4 only.
 * Requires dt>0, actual drag after posture427450, mass>0. No integrator:
 * feed acceleration/resistance into rf_physics_ground_propose and body sweep.
 * Inputs alias safely; errors leave output unchanged. */
int rf_swim_motion_prepare(const rf_swim_controls *controls,const float eye[9],
    float class_acceleration,float drag,float mass,float dt,int is_player,
    rf_swim_motion *output);
typedef struct rf_swim_jump {
    rf_swim_state state;float velocity_y;uint32_t applied,reset_parent_basis;
} rf_swim_jump;
/*Whole4288b0 numeric/mode4 gate; caller supplies all other actor/action gates.
 * Only surface mode4 handled; ordinary ground jump remains existing service.
 * global frame time (not remaining collision time) drives swim jump scale. */
int rf_swim_surface_jump(const rf_swim_state *state,
    const rf_movement_descriptor descriptors[16],uint32_t class_flags,
    int other_gates_allow,float velocity_y,float jump_speed,float frame_time,
    rf_swim_jump *output);
#endif
