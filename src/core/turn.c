#include "rf/turn.h"
#include <math.h>
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
