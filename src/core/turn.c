#include "rf/turn.h"
#include <math.h>
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
