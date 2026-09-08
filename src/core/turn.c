#include "rf/turn.h"
#include <math.h>
static int turn_magnitude_above(float x, float y, float z, float threshold, float factor)
{
    unsigned short saved,control,status;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm { fnstcw saved }
    control=(unsigned short)((saved & ~0x0f00u) | 0x0300u);
    __asm {
        fldcw control
        fld x
        fmul x
        fld y
        fmul y
        faddp st(1), st(0)
        fld z
        fmul z
        faddp st(1), st(0)
        fsqrt
        fld threshold
        fmul factor
        fcompp
        fnstsw status
        fldcw saved
    }
#elif defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("fnstcw %0" : "=m"(saved));
    control=(unsigned short)((saved & ~0x0f00u) | 0x0300u);
    __asm__ volatile ("fldcw %5\n\tflds %1\n\tfmuls %1\n\tflds %2\n\tfmuls %2\n\tfaddp\n\tflds %3\n\tfmuls %3\n\tfaddp\n\tfsqrt\n\tflds %4\n\tfmuls %7\n\tfcompp\n\tfnstsw %0\n\tfldcw %6"
        : "=m"(status) : "m"(x), "m"(y), "m"(z), "m"(threshold), "m"(control), "m"(saved), "m"(factor) : "st", "st(1)");
#else
#error Turn targeting requires the supported x86 PC or Xbox target.
#endif
    return (status & 0x0100u)!=0;
}

static int turn_target_far(const float source[3], const float target[3], int *far)
{
    float x,y,z; unsigned i;
    for (i=0;i<3;++i) if (!isfinite(source[i]) || !isfinite(target[i])) return RF_FORMAT;
    x=(float)((double)source[0]-target[0]); y=(float)((double)source[1]-target[1]); z=(float)((double)source[2]-target[2]);
    if (!isfinite(x) || !isfinite(y) || !isfinite(z)) return RF_RANGE;
    *far=turn_magnitude_above(x,y,z,8.2f,1); return RF_OK;
}

int rf_locomotion_choose_candidates(rf_locomotion_candidates *candidates,
                    const rf_locomotion_candidate_input *input, const int32_t motions[23],
                    rf_turn_effects *effects, rf_motion_playback_state *playback,
                    rf_motion_playback_resource *resources, uint32_t resource_count,
                    const int32_t actions[45], const int32_t sounds[45],
                    const rf_turn_context *context, const rf_turn_actor *actor,
                    rf_turn_reset_fn reset, void *user, int32_t *sound_class)
{
    rf_locomotion_candidates next={0,2,4,8}; int status; int32_t sound=-1;
    if (!candidates || !input || !motions || !effects || !playback || !resources ||
        !actions || !sounds || !context || !actor || !sound_class) return RF_RANGE;
    if (input->combat_eligible>1) return RF_FORMAT;
    if (input->action==17) {
        next.move=next.alternate=motions[7]==-1 ? 4 : 7;
    } else if (input->action==7 || (input->action==12 && actor->behavior==1)) {
        next.idle=13; next.move=6; next.alternate=motions[6]==-1 ? 4 : 6;
    } else if (input->combat_eligible) {
        status=rf_turn_update(effects,playback,resources,resource_count,actions,sounds,context,actor,reset,user,&sound);
        if (status!=RF_OK) return status;
        next.idle=1; next.move=effects->move_candidate; next.alternate=effects->alternate_candidate; next.special=9;
        /* These fields are read after helper/reset side effects in the original. */
        if (!(uint8_t)actor->network_mode) {
            if (!isfinite(input->velocity[0]) || !isfinite(input->velocity[1]) ||
                !isfinite(input->velocity[2]) || !isfinite(effects->movement.speed)) return RF_FORMAT;
            if (turn_magnitude_above(input->velocity[0],input->velocity[1],input->velocity[2],effects->movement.speed,.3f) &&
                input->state_740!=2) next.idle=next.alternate;
        }
    }
    *candidates=next; *sound_class=sound; return RF_OK;
}

int rf_turn_update(rf_turn_effects *effects, rf_motion_playback_state *playback,
                    rf_motion_playback_resource *resources, uint32_t resource_count,
                    const int32_t actions[45], const int32_t sounds[45],
                    const rf_turn_context *context, const rf_turn_actor *actor,
                    rf_turn_reset_fn reset, void *user, int32_t *sound_class)
{
    rf_turn_direction_result direction; rf_turn_finish_input finish;
    int active,status,far; int32_t a=-1,b=-1;
    if (!effects || !playback || !resources || !actions || !sounds || !context || !actor || !sound_class) return RF_RANGE;
    if ((uint8_t)actor->network_mode) { a=3; b=5; }
    else if ((actor->mode==12 || actor->mode==15 || actor->mode==13 || actor->mode==11 || actor->mode==9) &&
             (actor->info_flags & 0x20000u)) { a=b=1; }
    else {
        status=rf_motion_action_active(playback,resources,resource_count,actions,20,&active); if (status!=RF_OK) return status;
        if (!active) { status=rf_motion_action_active(playback,resources,resource_count,actions,19,&active); if (status!=RF_OK) return status; }
        if (active) a=b=9;
    }
    if (a!=-1) { effects->move_candidate=a; effects->alternate_candidate=b; *sound_class=-1; return RF_OK; }
    status=rf_turn_direction(&actor->direction,&direction); if (status!=RF_OK) return status;
    if (direction.eligible && actions[20]!=-1 && actions[19]!=-1) {
        /* These activity tests are repeated by the original before reset. */
        status=rf_motion_action_active(playback,resources,resource_count,actions,20,&active); if (status!=RF_OK) return status;
        if (!active) { status=rf_motion_action_active(playback,resources,resource_count,actions,19,&active); if (status!=RF_OK) return status; }
        if (!active) {
            if (!reset) return RF_NOT_FOUND;
            status=reset(user); if (status!=RF_OK) return status;
            if ((uint8_t)actor->target_valid) {
                status=turn_target_far(actor->source,actor->target,&far); if (status!=RF_OK) return status;
                if (far && ((uint8_t)actor->trigger_a || (uint8_t)actor->trigger_b))
                    return rf_turn_apply_selected(effects,playback,resources,resource_count,actions,sounds,context,direction.local[0],sound_class);
            }
        }
    }
    finish.local_x=direction.local[0]; finish.eligible=direction.eligible;
    finish.weapon=actor->weapon; finish.preferred_weapon=actor->preferred_weapon; finish.behavior=actor->behavior; finish.network_mode=actor->network_mode;
    return rf_turn_finish_candidates(effects,playback,resources,resource_count,actions,sounds,context,&finish,sound_class);
}

int rf_turn_finish_candidates(rf_turn_effects *effects, rf_motion_playback_state *playback,
                              rf_motion_playback_resource *resources, uint32_t resource_count,
                              const int32_t actions[45], const int32_t sounds[45],
                              const rf_turn_context *context, const rf_turn_finish_input *input,
                              int32_t *sound_class)
{
    rf_turn_effects next; int secondary,status,active; int32_t sound=-1;
    if (!effects || !playback || !resources || !actions || !sounds || !context || !input || !sound_class) return RF_RANGE;
    if (input->eligible>1 || !isfinite(input->local_x)) return RF_FORMAT;
    next=*effects;
    secondary=input->eligible && actions[18]!=-1 && actions[17]!=-1;
    status=rf_movement_set_mode(&next.movement,&context->movement,secondary ? 0 : 1,
                                context->forced_action,context->entity_scale,(uint8_t)context->override_enabled);
    if (status!=RF_OK) return status;
    next.move_candidate=3; next.alternate_candidate=5;
    if (secondary) {
        status=rf_motion_action_active(playback,resources,resource_count,actions,18,&active);
        if (status!=RF_OK) return status;
        if (!active) {
            status=rf_motion_action_active(playback,resources,resource_count,actions,17,&active);
            if (status!=RF_OK) return status;
            if (!active) {
                status=rf_motion_start_action(playback,resources,resource_count,actions,sounds,
                                              input->local_x>0 ? 18 : 17,1,0,1,&sound);
                if (status!=RF_OK) return status;
            }
        }
    } else if (input->weapon==input->preferred_weapon && input->behavior==1 &&
               !(uint8_t)input->network_mode && !(uint8_t)context->override_enabled) {
        next.move_candidate=2; next.alternate_candidate=4;
    }
    *effects=next; *sound_class=sound; return RF_OK;
}

static int turn_dot_inside(const float vector[3], const float axis[3])
{
    float x=vector[0],y=vector[1],z=vector[2],a=axis[0],b=axis[1],c=axis[2];
    float high_limit=.5f,low_limit=-.5f; unsigned short saved,control,upper,lower;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm { fnstcw saved }
    control=(unsigned short)((saved & ~0x0f00u) | 0x0300u);
    __asm {
        fldcw control
        fld c
        fmul z
        fld b
        fmul y
        faddp st(1), st(0)
        fld a
        fmul x
        faddp st(1), st(0)
        fcom high_limit
        fnstsw upper
        fcomp low_limit
        fnstsw lower
        fldcw saved
    }
#elif defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("fnstcw %0" : "=m"(saved));
    control=(unsigned short)((saved & ~0x0f00u) | 0x0300u);
    __asm__ volatile ("fldcw %10\n\tflds %7\n\tfmuls %4\n\tflds %6\n\tfmuls %3\n\tfaddp\n\tflds %5\n\tfmuls %2\n\tfaddp\n\tfcoms %8\n\tfnstsw %0\n\tfcomps %9\n\tfnstsw %1\n\tfldcw %11"
        : "=m"(upper), "=m"(lower)
        : "m"(x), "m"(y), "m"(z), "m"(a), "m"(b), "m"(c), "m"(high_limit), "m"(low_limit), "m"(control), "m"(saved)
        : "st", "st(1)");
#else
#error Turn direction currently requires the supported x86 PC or Xbox target.
#endif
    return (upper & 0x4100u)!=0 && (lower & 0x0100u)==0;
}

int rf_turn_direction(const rf_turn_direction_input *input, rf_turn_direction_result *result)
{
    rf_turn_direction_result next={{0,0,0},0};
    double x,y,z,length,reciprocal; float normalized[3]; unsigned i;
    if (!input || !result) return RF_RANGE;
    for (i=0;i<3;++i) if (!isfinite(input->vector[i])) return RF_FORMAT;
    for (i=0;i<9;++i) if (!isfinite(input->orientation[i])) return RF_FORMAT;
    if ((input->info_flags & 4u) || input->count<1 || !(input->entity_flags & 8u)) { *result=next; return RF_OK; }
    x=input->vector[0]; y=input->vector[1]; z=input->vector[2];
    length=sqrt((x*x+y*y)+z*z);
    if (length<(double).1f) { *result=next; return RF_OK; }
    reciprocal=1.0/length;
    normalized[0]=(float)(reciprocal*x); normalized[1]=(float)(reciprocal*y); normalized[2]=(float)(reciprocal*z);
    if (!turn_dot_inside(normalized,input->orientation+6)) { *result=next; return RF_OK; }
    for (i=0;i<3;++i) {
        next.local[i]=(float)(((double)input->orientation[i*3+2]*z+(double)input->orientation[i*3+1]*y)+
                              (double)input->orientation[i*3]*x);
        if (!isfinite(next.local[i])) return RF_RANGE;
    }
    next.eligible=1; *result=next; return RF_OK;
}

int rf_turn_apply_selected(rf_turn_effects *effects, rf_motion_playback_state *playback,
                           rf_motion_playback_resource *resources, uint32_t resource_count,
                           const int32_t actions[45], const int32_t sounds[45],
                           const rf_turn_context *context, float local_x, int32_t *sound_class)
{
    rf_turn_effects next; int32_t deadline,sound; unsigned i; int status;
    if (!effects || !playback || !resources || !actions || !sounds || !context || !sound_class) return RF_RANGE;
    if (!isfinite(local_x)) return RF_FORMAT;
    next=*effects;
    status=rf_timer_set(&deadline,context->now_ms,1200); if (status!=RF_OK) return status;
    status=rf_movement_set_mode(&next.movement,&context->movement,1,context->forced_action,
                                context->entity_scale,(uint8_t)context->override_enabled);
    if (status!=RF_OK) return status;
    /* Prepare numeric effects before mutating shared resource references. */
    for (i=0;i<5;++i) next.deadlines[i]=deadline;
    next.turning=1; next.move_candidate=next.alternate_candidate=9;
    status=rf_motion_start_action(playback,resources,resource_count,actions,sounds,
                                  local_x>0 ? 20 : 19,1,0,1,&sound);
    if (status!=RF_OK) return status;
    *effects=next; *sound_class=sound; return RF_OK;
}
