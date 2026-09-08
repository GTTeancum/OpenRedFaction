#include "rf/motion.h"
#include <math.h>
#include <string.h>
int rf_motion_remaining(const rf_motion_playback_state *state, const rf_motion_playback_resource *resources,
                         uint32_t resource_count, int32_t motion, float *seconds)
{
    uint32_t i; int64_t ticks;
    if (!state || !resources || !seconds || state->completion.active.count>16) return RF_RANGE;
    for (i=0;i<state->completion.active.count;++i) if (state->completion.active.slots[i].motion==motion) break;
    if (i==state->completion.active.count) { *seconds=0; return RF_OK; }
    if (motion<0 || (uint32_t)motion>=resource_count) return RF_RANGE;
    ticks=(int64_t)resources[motion].comparison.end_tick-state->completion.active.slots[i].tick;
    if (ticks<INT32_MIN || ticks>INT32_MAX) return RF_RANGE;
    *seconds=ticks>0 ? (float)(((double)ticks*(double)(1.0f/160.0f))*(double)(1.0f/30.0f)) : 0;
    return RF_OK;
}

int rf_motion_action_active(const rf_motion_playback_state *state, const rf_motion_playback_resource *resources,
                             uint32_t resource_count, const int32_t actions[45], int32_t action, int *active)
{
    float seconds; int status;
    if (!actions || !active) return RF_RANGE;
    if (action<0 || action>=45 || actions[action]<0) { *active=0; return RF_OK; }
    status=rf_motion_remaining(state,resources,resource_count,actions[action],&seconds);
    if (status==RF_OK) *active=seconds!=0;
    return status;
}

int rf_motion_has_state(const rf_motion_controller *controller, int32_t requested)
{
    return controller && (controller->current==requested || controller->next==requested);
}

static int motion_priority_speed(float x, float y, float z)
{
    float threshold=0.01f; unsigned short saved,control,status;
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
        fcomp threshold
        fnstsw ax
        mov status, ax
        fldcw saved
    }
#elif defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("fnstcw %0" : "=m"(saved));
    control=(unsigned short)((saved & ~0x0f00u) | 0x0300u);
    __asm__ volatile ("fldcw %5\n\tflds %1\n\tfmuls %1\n\tflds %2\n\tfmuls %2\n\tfaddp\n\tflds %3\n\tfmuls %3\n\tfaddp\n\tfsqrt\n\tfcomps %4\n\tfnstsw %%ax\n\tfldcw %6"
        : "=a"(status) : "m"(x), "m"(y), "m"(z), "m"(threshold), "m"(control), "m"(saved) : "st", "st(1)");
#else
#error Motion selection requires the supported x86 PC or Xbox target.
#endif
    return (status & 0x4100u)==0;
}

int rf_motion_select_priority(rf_motion_controller *controller, const int32_t motions[23],
                              const rf_motion_priority *priority, int *handled)
{
    int32_t selected; int unconditional=0,status;
    if (!controller || !motions || !priority || !handled) return RF_RANGE;
    if (priority->linked_present>1 || !isfinite(priority->velocity[0]) ||
        !isfinite(priority->velocity[1]) || !isfinite(priority->velocity[2])) return RF_FORMAT;
    if (priority->forced_state!=-1) selected=priority->forced_state;
    else if (priority->flags & 0x02000000u) selected=22;
    else if (priority->linked_present && priority->linked_class_type==4) selected=15;
    else if (priority->linked_present && (priority->linked_flags & 0x00400000u) &&
             priority->linked_occupant_handle==priority->entity_handle) selected=20;
    /* The original calls the same first-occupant predicate twice. Its state
     * 21 branch is unreachable for a stable linked entity; do not substitute
     * the different second-occupant predicate at 0x42acd0. */
    else if (priority->mode==3 || priority->mode==8 || (priority->class_type==1 && priority->action==-1)) selected=14;
    else if ((priority->physics_flags & 0x8000u) && (priority->flags & 0x400u)) {
        selected=motion_priority_speed(priority->velocity[0],priority->velocity[1],priority->velocity[2]) ? 10 : 9;
        unconditional=1;
    } else { *handled=0; return RF_OK; }
    if (unconditional || !rf_motion_has_state(controller,selected)) {
        status=rf_motion_request_state(controller,motions,selected,.25f);
        if (status!=RF_OK) return status;
    }
    *handled=1; return RF_OK;
}

int rf_motion_select_movement(rf_motion_controller *controller, const int32_t motions[23],
                              const rf_motion_movement *movement)
{
    int moving; int32_t selected=-1;
    if (!controller || !motions || !movement) return RF_RANGE;
    if (!isfinite(movement->vector[0]) || !isfinite(movement->vector[1]) || !isfinite(movement->vector[2]) ||
        movement->idle_state<0 || movement->idle_state>=23 || movement->move_state<0 || movement->move_state>=23 ||
        movement->alternate_state<0 || movement->alternate_state>=23) return RF_FORMAT;
    moving=movement->vector[0]!=0 || movement->vector[1]!=0 || movement->vector[2]!=0;
    if (movement->mode==4 || movement->mode==7) selected=moving ? 19 : 18;
    else if (!moving) selected=movement->idle_state;
    else if (movement->mode==1 && movement->direction==0) selected=movement->move_state;
    else if ((movement->mode==1 && movement->direction==1) || movement->mode==12 || movement->mode==15 ||
             movement->mode==13 || movement->mode==11 || movement->mode==9) selected=movement->alternate_state;
    if (selected<0 || rf_motion_has_state(controller,selected)) return RF_OK;
    return rf_motion_request_state(controller,motions,selected,.25f);
}

int rf_motion_request_state(rf_motion_controller *controller, const int32_t motions[23],
                            int32_t requested, float duration)
{
    rf_motion_controller next; double fraction; unsigned i;
    if (!controller || !motions) return RF_RANGE;
    next=*controller;
    if (next.current<0 || next.current>=23 || next.next < -1 || next.next>=23 ||
        !isfinite(duration) || duration<0 || !isfinite(next.duration) || next.duration<0 ||
        !isfinite(next.elapsed) || next.elapsed<0 ||
        (next.duration>0 && (next.next<0 || next.elapsed>next.duration))) return RF_FORMAT;
    for (i=0;i<23;++i) if (motions[i]<-1) return RF_FORMAT;
    if (requested<0 || requested>=23 || motions[requested]==-1) requested=0;
    if (motions[requested]==-1) return RF_OK;
    if (next.duration==0) next.elapsed=0;
    else {
        fraction=(double)next.elapsed/next.duration;
        if (fraction>0.5) { next.current=next.next; fraction=1.0-fraction; }
        next.elapsed=(float)(fraction*duration);
    }
    next.next=requested; next.duration=duration;
    *controller=next; return RF_OK;
}

int rf_motion_apply_controller(rf_motion_controller *controller, const int32_t motions[23],
                               float elapsed, rf_motion_playback_state *state,
                               rf_motion_playback_resource *resources, uint32_t resource_count)
{
    rf_motion_controller next;
    rf_motion_playback_state staged;
    int32_t ids[2], refs[2]; float weights[2];
    unsigned count=0,i; int result;
    if (!controller || !motions || !state || !resources) return RF_RANGE;
    next=*controller; staged=*state;
    if (next.current<0 || next.current>=23 || next.next < -1 || next.next>=23 ||
        next.override_enabled>1 || (next.override_enabled && (next.override_state<0 || next.override_state>=23)) ||
        !isfinite(elapsed) || elapsed<0 || !isfinite(next.duration) || next.duration<0 ||
        !isfinite(next.elapsed) || next.elapsed<0 || (next.duration>0 && next.next<0)) return RF_FORMAT;
    for (i=0;i<23;++i) if (motions[i]<-1 || (motions[i]>=0 && (uint32_t)motions[i]>=resource_count)) return RF_RANGE;
    if (next.duration>0) {
        /* fst retains the unspilled sum for the original completion comparison. */
        double sum=(double)elapsed+(double)next.elapsed;
        next.elapsed=(float)sum;
        if (!isfinite(next.elapsed)) return RF_RANGE;
        if (sum>=next.duration) { next.duration=0; next.current=next.next; next.next=-1; }
    }
    if (next.override_enabled) {
        ids[0]=motions[next.override_state]; weights[0]=1; count=ids[0]>=0;
    } else if (next.duration==0) {
        ids[0]=motions[next.current]; weights[0]=1; count=ids[0]>=0;
    } else {
        ids[0]=motions[next.current]; ids[1]=motions[next.next];
        if (ids[0]>=0 && ids[1]>=0) {
            weights[1]=(float)((double)next.elapsed/next.duration);
            weights[0]=(float)(1.0-(double)weights[1]); count=2;
        }
    }
    result=rf_motion_stop_looping(&staged,resources,resource_count);
    if (result!=RF_OK) return result;
    for (i=0;i<count;++i) refs[i]=resources[ids[i]].references;
    for (i=0;i<count;++i) {
        result=rf_motion_set_weight(&staged,resources,resource_count,ids[i],weights[i]);
        if (result!=RF_OK) {
            unsigned j; for (j=0;j<count;++j) resources[ids[j]].references=refs[j];
            return result;
        }
    }
    *controller=next; *state=staged;
    return RF_OK;
}

int rf_motion_complete_slots(rf_motion_completion_state *state, const int32_t *end_ticks, uint32_t looping_mask)
{
    uint32_t i;
    if (!state || !end_ticks || state->active.count>16) return RF_RANGE;
    if (state->active.freeze_slot < -1 || state->active.primary_slot < -1 || state->active.dominant_slot < -1 ||
        state->active.freeze_slot>=(int32_t)state->active.count || state->active.primary_slot>=(int32_t)state->active.count ||
        state->active.dominant_slot>=(int32_t)state->active.count || state->frozen>1 || state->primary_flag>255)
        return RF_FORMAT;
    for (i=0;i<state->active.count;++i) if (state->active.slots[i].motion<0) return RF_FORMAT;
    for (i=0;i<state->active.count;++i) {
        rf_motion_active_slot *slot=&state->active.slots[i];
        if ((looping_mask & (1u<<i)) || slot->tick<end_ticks[i]) continue;
        slot->tick=end_ticks[i];
        if ((int32_t)i==state->active.freeze_slot) state->frozen=1;
        else {
            if ((int32_t)i==state->active.primary_slot) {
                state->active.primary_slot=-1;
                state->primary_flag=0; state->primary_words[0]=state->primary_words[1]=0;
                memset(state->primary_vectors,0,sizeof(state->primary_vectors));
            }
            slot->weight=0;
        }
    }
    return RF_OK;
}

static int32_t removed_slot_index(int32_t selected, uint32_t removed)
{
    if (selected==(int32_t)removed) return -1;
    return selected>(int32_t)removed ? selected-1 : selected;
}
int rf_motion_remove_slot(rf_motion_slot_state *state, int32_t motion, int32_t *references)
{
    uint32_t i,index;
    if (!state || !references || motion<0 || state->count>16) return RF_RANGE;
    if (*references<0 || state->freeze_slot < -1 || state->primary_slot < -1 || state->dominant_slot < -1 ||
        state->freeze_slot>=(int32_t)state->count || state->primary_slot>=(int32_t)state->count ||
        state->dominant_slot>=(int32_t)state->count) return RF_FORMAT;
    for (i=0;i<state->count;++i) if (state->slots[i].motion<0) return RF_FORMAT;
    for (index=0;index<state->count && state->slots[index].motion!=motion;++index) {}
    if (index==state->count) return RF_OK;
    if (*references>0) --*references;
    --state->count;
    for (i=index;i<state->count;++i) state->slots[i]=state->slots[i+1];
    state->freeze_slot=removed_slot_index(state->freeze_slot,index);
    state->primary_slot=removed_slot_index(state->primary_slot,index);
    state->dominant_slot=removed_slot_index(state->dominant_slot,index);
    return RF_OK;
}

int rf_motion_map_loop(int32_t start, int32_t end, float phase, int32_t previous,
                       const int32_t markers[2], int wrapped, rf_motion_loop_result *out)
{
    int64_t duration=(int64_t)end-start; unsigned i; rf_motion_loop_result result;
    if (!markers || !out || !isfinite(phase) || phase<0 || phase>1) return RF_RANGE;
    if (duration<=0) return RF_FORMAT;
    if (duration>INT32_MAX) return RF_RANGE;
    result.tick=(int32_t)((int64_t)floor((double)phase*(double)duration)+start);
    result.event_mask=0;
    for (i=0;i<2;++i)
        if ((markers[i]<=result.tick && previous<markers[i]) ||
            (wrapped && (previous<markers[i] || markers[i]<result.tick))) result.event_mask|=1u<<i;
    *out=result; return RF_OK;
}

/* 0x51bbde..0x51bc01 keeps division, multiply and add in x87 extended
 * precision. Even an exact rational evaluation differs at some float ties.
 * Both supported targets are x86; isolate the required precision here. */
static float motion_phase_extended(float rate, float total, int32_t delta, float phase, uint32_t *wrapped)
{
    float result, one=1.0f; unsigned short status, saved, control;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm { fnstcw saved }
    control=(unsigned short)((saved & ~0x0f00u) | 0x0300u);
    __asm {
        fldcw control
        fld rate
        fdiv total
        fimul delta
        fadd phase
        fst result
        fcomp one
        fnstsw ax
        mov status, ax
        fldcw saved
    }
#elif defined(__i386__) || defined(__x86_64__)
    __asm__ volatile ("fnstcw %0" : "=m"(saved));
    control=(unsigned short)((saved & ~0x0f00u) | 0x0300u);
    __asm__ volatile ("fldcw %7\n\tflds %2\n\tfdivs %3\n\tfimull %4\n\tfadds %5\n\tfsts %0\n\tfcomps %6\n\tfnstsw %%ax\n\tfldcw %8"
        : "=m"(result), "=a"(status)
        : "m"(rate), "m"(total), "m"(delta), "m"(phase), "m"(one), "m"(control), "m"(saved) : "st");
#else
#error Motion playback currently requires the supported x86 PC or Xbox target.
#endif
    *wrapped=(status & 0x0100u)==0;
    return result;
}

int rf_motion_advance_phase(const rf_motion_phase_slot *slots, uint32_t count, float phase,
                            int32_t delta_ticks, rf_motion_phase_result *out)
{
    float rate=0, total=0, greatest=0; uint32_t i;
    rf_motion_phase_result result={0,-1,0}; double advanced;
    if (!slots || !out || !count || count>16 || !isfinite(phase) || phase<0 || phase>1 || delta_ticks<0) return RF_RANGE;
    for (i=0;i<count;++i) {
        if (slots[i].duration<=0 || !isfinite(slots[i].weight) || slots[i].weight<0) return RF_FORMAT;
        if (!slots[i].looping) continue;
        rate=(float)(((1.0/(double)slots[i].duration)*slots[i].weight)+(double)rate);
        total+=slots[i].weight;
        if (slots[i].weight>greatest) { greatest=slots[i].weight; result.dominant_slot=(int32_t)i; }
    }
    if (!isfinite(rate) || !isfinite(total)) return RF_RANGE;
    if (total!=0) {
        advanced=((double)rate/total)*delta_ticks+phase;
        if (!isfinite(advanced) || advanced>=16777216.0) return RF_RANGE;
        result.phase=motion_phase_extended(rate,total,delta_ticks,phase,&result.wrapped);
        /* Below 2^24, repeated float subtraction of one is exact. */
        if (result.wrapped) {
            result.phase=(float)((double)result.phase-floor((double)result.phase));
        }
    }
    *out=result; return RF_OK;
}

int rf_motion_elapsed_ticks(float elapsed, int32_t *out)
{
    double ticks;
    if (!out || !isfinite(elapsed)) return RF_RANGE;
    ticks = ((double)elapsed * 30.0) * 160.0;
    if (ticks <= (double)INT32_MIN-1.0 || ticks >= (double)INT32_MAX+1.0) return RF_RANGE;
    *out = (int32_t)ticks;
    return RF_OK;
}

static int sample_weight_extended(const rf_motion_weight_envelope *envelope, int32_t tick, int bypass, double *out)
{
    int64_t duration, elapsed;
    double result;
    if (!envelope || !out) return RF_RANGE;
    if (!isfinite(envelope->weight) || envelope->fade_in < 0 || envelope->fade_out < 0 ||
        envelope->end_tick < envelope->start_tick) return RF_FORMAT;
    duration = (int64_t)envelope->end_tick - envelope->start_tick;
    elapsed = (int64_t)tick - envelope->start_tick;
    if (duration > INT32_MAX || elapsed < INT32_MIN || elapsed > INT32_MAX) return RF_RANGE;
    if (envelope->weight < 1.0e-5f) result = 0;
    else if (bypass) result = envelope->weight;
    else if (elapsed < 0) result = 0;
    /* Fade-in takes precedence, including when it extends beyond end_tick. */
    else if (elapsed < envelope->fade_in)
        result = ((double)elapsed / envelope->fade_in) * envelope->weight;
    else if (elapsed > duration) result = 0;
    else if (elapsed <= duration - envelope->fade_out) result = envelope->weight;
    else result = ((double)(duration - elapsed) / envelope->fade_out) * envelope->weight;
    if (result > envelope->weight && envelope->weight >= 1.0e-5f) result = envelope->weight;
    *out = result;
    return RF_OK;
}

int rf_motion_sample_weight(const rf_motion_weight_envelope *envelope, int32_t tick, int bypass, float *out)
{
    double value; int status;
    if (!out) return RF_RANGE;
    status=sample_weight_extended(envelope,tick,bypass,&value);
    if (status==RF_OK) *out=(float)value;
    return status;
}

int rf_motion_advance_candidate(rf_motion_completion_state *state, uint32_t index, int32_t delta,
                                const rf_motion_weight_envelope *candidate_envelope, int candidate_bypass,
                                const rf_motion_weight_envelope *primary_envelope, int primary_bypass)
{
    rf_motion_completion_state next; int64_t tick; int status, replace; double candidate, primary;
    if (!state || state->active.count>16 || index>=state->active.count || delta<0) return RF_RANGE;
    if (state->active.primary_slot < -1 || state->active.primary_slot>=(int32_t)state->active.count ||
        !isfinite(state->active.slots[index].weight)) return RF_FORMAT;
    if (state->active.slots[index].weight==0) return RF_OK;
    tick=(int64_t)state->active.slots[index].tick+delta;
    if (tick>INT32_MAX) return RF_RANGE;
    next=*state; next.active.slots[index].tick=(int32_t)tick;
    replace=next.active.primary_slot==-1;
    if (!replace) {
        status=sample_weight_extended(candidate_envelope,(int32_t)tick,candidate_bypass,&candidate);
        if (status!=RF_OK) return status;
        status=sample_weight_extended(primary_envelope,next.active.slots[next.active.primary_slot].tick,primary_bypass,&primary);
        if (status!=RF_OK) return status;
        replace=primary<(float)candidate;
    }
    if (replace) {
        next.active.primary_slot=(int32_t)index;
        next.primary_words[0]=next.primary_words[1]=0;
    }
    *state=next; return RF_OK;
}

int rf_motion_update(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                     uint32_t resource_count, float elapsed)
{
    rf_motion_playback_state next;
    rf_motion_phase_slot phases[16]; rf_motion_phase_result phase;
    int32_t ends[16], removed[16], delta; uint32_t i,j,mask=0,removed_count=0;
    int status;
    if (!state) return RF_RANGE;
    if (state->completion.frozen || !state->completion.active.count || !resource_count) return RF_OK;
    if (!resources || state->completion.active.count>16 || elapsed<0) return RF_RANGE;
    next=*state;
    if (next.generation>65535 || next.event_mask>3 || next.completion.frozen>1 ||
        next.completion.primary_flag>255 || next.completion.active.freeze_slot < -1 ||
        next.completion.active.primary_slot < -1 || next.completion.active.dominant_slot < -1 ||
        next.completion.active.freeze_slot>=(int32_t)next.completion.active.count ||
        next.completion.active.primary_slot>=(int32_t)next.completion.active.count ||
        next.completion.active.dominant_slot>=(int32_t)next.completion.active.count) return RF_FORMAT;
    status=rf_motion_elapsed_ticks(elapsed,&delta); if (status!=RF_OK) return status;
    for (i=0;i<next.completion.active.count;++i) {
        int32_t id=next.completion.active.slots[i].motion; int64_t duration;
        const rf_motion_playback_resource *r;
        if (id<0 || (uint32_t)id>=resource_count || i>=resource_count) return RF_FORMAT;
        for (j=0;j<i;++j) if (next.completion.active.slots[j].motion==id) return RF_FORMAT;
        r=&resources[id]; duration=(int64_t)r->comparison.end_tick-r->comparison.start_tick;
        if (duration<=0 || duration>INT32_MAX || r->references<0 || r->looping>255) return RF_FORMAT;
        phases[i].duration=(int32_t)duration; phases[i].weight=next.completion.active.slots[i].weight;
        phases[i].looping=r->looping; ends[i]=r->comparison.end_tick;
        if (r->looping) mask|=1u<<i;
    }
    status=rf_motion_advance_phase(phases,next.completion.active.count,next.phase,delta,&phase);
    if (status!=RF_OK) return status;
    next.phase=phase.phase; next.completion.active.dominant_slot=phase.dominant_slot;
    next.generation=(next.generation+1)&65535u;
    for (i=0;i<next.completion.active.count;++i) {
        rf_motion_active_slot *slot=&next.completion.active.slots[i];
        const rf_motion_playback_resource *r=&resources[slot->motion];
        if (slot->weight==0) continue;
        if (r->looping) {
            rf_motion_loop_result loop;
            status=rf_motion_map_loop(r->comparison.start_tick,r->comparison.end_tick,next.phase,
                                      slot->tick,r->markers,phase.wrapped,&loop);
            if (status!=RF_OK) return status;
            slot->tick=loop.tick;
            if ((int32_t)i==phase.dominant_slot) next.event_mask|=loop.event_mask;
        } else if (phase.dominant_slot>=0) {
            const rf_motion_playback_resource *primary=NULL;
            if (next.completion.active.primary_slot>=0)
                primary=&resources[next.completion.active.slots[next.completion.active.primary_slot].motion];
            status=rf_motion_advance_candidate(&next.completion,i,delta,&r->comparison,resources[i].looping,
                                               primary ? &primary->comparison : NULL,primary ? primary->looping : 0);
            if (status!=RF_OK) return status;
        } else {
            int64_t tick=(int64_t)slot->tick+delta;
            if (tick>INT32_MAX) return RF_RANGE;
            slot->tick=(int32_t)tick;
        }
    }
    status=rf_motion_complete_slots(&next.completion,ends,mask); if (status!=RF_OK) return status;
    for (i=0;i<next.completion.active.count;) {
        rf_motion_active_slot *slot=&next.completion.active.slots[i];
        if (slot->weight==0) {
            int32_t references=resources[slot->motion].references;
            removed[removed_count++]=slot->motion;
            status=rf_motion_remove_slot(&next.completion.active,slot->motion,&references);
            if (status!=RF_OK) return status;
        } else ++i;
    }
    for (i=0;i<removed_count;++i) if (resources[removed[i]].references>0) --resources[removed[i]].references;
    *state=next; return RF_OK;
}

static int motion_control(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                           uint32_t resource_count, int32_t motion, float weight, int restart, int freeze)
{
    uint32_t i,index; rf_motion_slot_state *active; rf_motion_playback_resource *resource;
    if (!state || !resources || motion<0 || (uint32_t)motion>=resource_count) return RF_RANGE;
    active=&state->completion.active; resource=&resources[motion];
    if (active->count>16) return RF_RANGE;
    if (!isfinite(weight) || (!restart && weight<0) || resource->references<0 || resource->looping>255 ||
        active->freeze_slot < -1 || active->primary_slot < -1 || active->dominant_slot < -1 ||
        active->freeze_slot>=(int32_t)active->count || active->primary_slot>=(int32_t)active->count ||
        active->dominant_slot>=(int32_t)active->count) return RF_FORMAT;
    for (i=0;i<active->count;++i) if (active->slots[i].motion<0 || (uint32_t)active->slots[i].motion>=resource_count) return RF_FORMAT;
    if (restart && (resource->looping==1 || weight<=0)) return RF_OK;
    for (index=0;index<active->count && active->slots[index].motion!=motion;++index) {}
    if (index==active->count) {
        if (index==16 || resource->references==INT32_MAX) return RF_RANGE;
        active->slots[index].motion=motion; active->slots[index].tick=0;
        active->slots[index].weight=0; ++active->count; ++resource->references;
    }
    state->completion.frozen=0; active->slots[index].weight=weight;
    if (restart) {
        active->slots[index].tick=resource->comparison.start_tick;
        if ((uint8_t)freeze==1) active->freeze_slot=(int32_t)index;
        if (active->primary_slot==-1) {
            active->primary_slot=(int32_t)index;
            state->completion.primary_words[0]=0;
        }
    }
    return RF_OK;
}
int rf_motion_set_weight(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                         uint32_t resource_count, int32_t motion, float weight)
{
    return motion_control(state,resources,resource_count,motion,weight,0,0);
}
int rf_motion_start(rf_motion_playback_state *state, rf_motion_playback_resource *resources,
                    uint32_t resource_count, int32_t motion, float weight, int freeze)
{
    return motion_control(state,resources,resource_count,motion,weight,1,freeze);
}

static int motion_stop_group(rf_motion_playback_state *state, const rf_motion_playback_resource *resources,
                              uint32_t resource_count, uint32_t loop_byte)
{
    uint32_t i; rf_motion_slot_state *active;
    if (!state) return RF_RANGE;
    active=&state->completion.active;
    if (active->count>16 || (active->count && !resources)) return RF_RANGE;
    for (i=0;i<active->count;++i) {
        int32_t id=active->slots[i].motion;
        if (id<0 || (uint32_t)id>=resource_count || resources[id].looping>255) return RF_FORMAT;
    }
    active->freeze_slot=-1; state->completion.frozen=0;
    if (!loop_byte) active->primary_slot=-1;
    for (i=0;i<active->count;++i)
        if (resources[active->slots[i].motion].looping==loop_byte) active->slots[i].weight=0;
    return RF_OK;
}
int rf_motion_stop_looping(rf_motion_playback_state *state, const rf_motion_playback_resource *resources, uint32_t resource_count)
{
    return motion_stop_group(state,resources,resource_count,1);
}
int rf_motion_stop_nonlooping(rf_motion_playback_state *state, const rf_motion_playback_resource *resources, uint32_t resource_count)
{
    return motion_stop_group(state,resources,resource_count,0);
}
int rf_motion_stop_slot(rf_motion_playback_state *state, int32_t motion)
{
    uint32_t i;
    if (!state || motion<0 || state->completion.active.count>16) return RF_RANGE;
    for (i=0;i<state->completion.active.count;++i)
        if (state->completion.active.slots[i].motion<0) return RF_FORMAT;
    for (i=0;i<state->completion.active.count;++i) if (state->completion.active.slots[i].motion==motion) {
        state->completion.active.slots[i].weight=0;
        state->completion.active.primary_slot=-1;
        break;
    }
    return RF_OK;
}

int rf_motion_bone_weights(const rf_motion_slot_state *active, const rf_motion_weight_envelope *envelopes,
                           uint32_t looping_mask, float weights[16])
{
    float result[16]={0}, total=0, attenuation=1; uint32_t i; double value; int status;
    if (!active || !weights || active->count>16 || (active->count && !envelopes)) return RF_RANGE;
    if (active->primary_slot < -1 || active->primary_slot>=(int32_t)active->count) return RF_FORMAT;
    for (i=0;i<active->count;++i)
        if (!isfinite(active->slots[i].weight) || active->slots[i].weight<0) return RF_FORMAT;
    if (active->primary_slot>=0 && active->slots[active->primary_slot].weight!=0) {
        i=(uint32_t)active->primary_slot;
        status=sample_weight_extended(&envelopes[i],active->slots[i].tick,0,&value);
        if (status!=RF_OK) return status;
        attenuation=(float)((10.0-value)*(double)0.10000000149011612f);
    }
    for (i=0;i<active->count;++i) {
        if (active->slots[i].weight==0) continue;
        status=sample_weight_extended(&envelopes[i],active->slots[i].tick,(looping_mask & (1u<<i))!=0,&value);
        if (status!=RF_OK) return status;
        value*=active->slots[i].weight;
        if (looping_mask & (1u<<i)) value*=attenuation;
        if (value>0) { total=(float)(value+total); result[i]=(float)value; }
    }
    if (!isfinite(total)) return RF_RANGE;
    for (i=0;i<active->count;++i) if (result[i]>0) result[i]/=total;
    memcpy(weights,result,sizeof(result)); return RF_OK;
}

static int32_t motion_wrap16(int32_t value)
{
    uint32_t bits = (uint32_t)value & 65535u;
    return bits < 32768u ? (int32_t)bits : (int32_t)bits - 65536;
}

static double motion_packed_dot(const int32_t a[4], const int32_t b[4])
{
    uint32_t sum = 0;
    int64_t signed_sum;
    unsigned i;
    for (i = 0; i < 4; ++i) sum += (uint32_t)(a[i] * b[i]);
    signed_sum = sum < 0x80000000u ? (int64_t)sum : (int64_t)sum - 4294967296LL;
    return (double)signed_sum * 3.725745045812800526619e-9;
}

int rf_motion_interpolate_rotation(const int16_t a[4], const int16_t b[4], float t, int16_t out[4])
{
    int32_t first[4], second[4], difference[4], sum[4];
    int16_t result[4];
    float dot;
    double wa, wb;
    unsigned i;
    int opposite;
    if (!a || !b || !out || !isfinite(t) || t < 0 || t > 1) return RF_RANGE;
    for (i = 0; i < 4; ++i) {
        first[i] = a[i]; second[i] = b[i];
        difference[i] = motion_wrap16(first[i] - second[i]);
        sum[i] = motion_wrap16(first[i] + second[i]);
    }
    if (motion_packed_dot(sum,sum) <= (float)motion_packed_dot(difference,difference))
        for (i = 0; i < 4; ++i) second[i] = motion_wrap16(-second[i]);
    dot = (float)motion_packed_dot(first,second);
    opposite = (double)dot + 1 <= (double)1.0e-6f;
    if (opposite) {
        wa = sin((1.0 - t) * (double)1.5707963705062866f);
        wb = sin((double)t * (double)1.5707963705062866f);
    } else if (1.0 - dot <= (double)1.0e-6f) {
        wa = 0; wb = 1;
    } else {
        double angle = acos((double)dot);
        float rounded_angle = (float)angle;
        float reciprocal = (float)(1.0 / sin(angle));
        if (!isfinite(angle) || !isfinite(reciprocal)) return RF_FORMAT;
        wa = sin((1.0 - t) * rounded_angle) * reciprocal;
        wb = sin((double)t * rounded_angle) * reciprocal;
    }
    for (i = 0; i < 4; ++i) {
        double value = first[i] * wa;
        if (opposite) value += ((i & 1) ? second[i-1] : -second[i+1]) * wb;
        else value += second[i] * wb;
        if (!isfinite(value) || value < INT32_MIN || value > INT32_MAX) return RF_RANGE;
        result[i] = (int16_t)motion_wrap16((int32_t)value);
    }
    if (!result[3]) result[3] = 1;
    memcpy(out,result,sizeof(result));
    return RF_OK;
}

int rf_motion_decode_rotation(const void *packed, size_t bytes, float out[4])
{
    const unsigned char *p = (const unsigned char *)packed;
    float result[4];
    unsigned i;
    if (!packed || !out || bytes < 8) return RF_RANGE;
    for (i = 0; i < 4; ++i) {
        int32_t value = (int32_t)p[i*2] | ((int32_t)p[i*2+1] << 8);
        if (value >= 32768) value -= 65536;
        /* RF.exe 0x589524: float bits 0x38800200, not exactly 1/16384. */
        result[i] = (float)value * 0.0000610388815402984619140625f;
    }
    memcpy(out, result, sizeof(result));
    return RF_OK;
}

static float motion_ease(float t, int8_t outgoing, int8_t incoming)
{
    float a = outgoing * 0.0078740157186985015869140625f;
    float b = incoming * 0.0078740157186985015869140625f;
    float sum = a + b;
    double scale, remaining;
    if (t == 0 || t == 1 || sum == 0) return t;
    if (sum > 1) { a /= sum; b /= sum; }
    scale = 1.0 / ((2.0 - a) - b);
    if (t < a) return (float)(((scale / a) * t) * t);
    if (t < 1.0 - b) return (float)(((t + (double)t) - a) * scale);
    remaining = 1.0 - t;
    return (float)(1.0 - ((scale / b) * remaining) * remaining);
}

int rf_motion_sample_rotation(const rf_motion_rotation_key *keys, uint32_t count, int32_t tick, float out[4])
{
    uint32_t i, upper;
    int16_t packed[4];
    unsigned char bytes[8];
    float t, result[4] = {0,0,0,1};
    int status;
    if (!out || (count && !keys)) return RF_RANGE;
    for (i = 0; i < count; ++i) {
        if ((i && keys[i].tick <= keys[i-1].tick) || keys[i].incoming < 0 || keys[i].outgoing < 0)
            return RF_FORMAT;
    }
    if (!count) { memcpy(out,result,sizeof(result)); return RF_OK; }
    if (count == 1) memcpy(packed,keys[0].packed,sizeof(packed));
    else {
        if (tick <= keys[0].tick) { upper = 1; t = 0; }
        else if (tick >= keys[count-1].tick) { upper = count-1; t = 1; }
        else {
            int64_t delta;
            for (upper = 1; tick >= keys[upper].tick; ++upper) {}
            delta = (int64_t)keys[upper].tick - keys[upper-1].tick;
            if (delta > INT32_MAX) return RF_RANGE;
            t = (float)(tick - keys[upper-1].tick) / (float)delta;
        }
        t = motion_ease(t,keys[upper-1].outgoing,keys[upper].incoming);
        status = rf_motion_interpolate_rotation(keys[upper-1].packed,keys[upper].packed,t,packed);
        if (status != RF_OK) return status;
    }
    for (i = 0; i < 4; ++i) {
        bytes[i*2] = (unsigned char)((uint16_t)packed[i] & 255);
        bytes[i*2+1] = (unsigned char)((uint16_t)packed[i] >> 8);
    }
    return rf_motion_decode_rotation(bytes,sizeof(bytes),out);
}

int rf_motion_sample_position(const rf_motion_position_key *keys, uint32_t count, int32_t tick, float out[3])
{
    uint32_t i, c, upper;
    float result[3] = {0,0,0}, t;
    if (!out || (count && !keys)) return RF_RANGE;
    for (i = 0; i < count; ++i) {
        if (i && keys[i].tick <= keys[i-1].tick) return RF_FORMAT;
        for (c = 0; c < 3; ++c)
            if (!isfinite(keys[i].position[c]) || !isfinite(keys[i].incoming[c]) || !isfinite(keys[i].outgoing[c])) return RF_FORMAT;
    }
    if (!count) { memcpy(out, result, sizeof(result)); return RF_OK; }
    if (tick <= keys[0].tick) { memcpy(out, keys[0].position, sizeof(result)); return RF_OK; }
    if (tick >= keys[count-1].tick) { memcpy(out, keys[count-1].position, sizeof(result)); return RF_OK; }
    for (upper = 1; upper < count && tick >= keys[upper].tick; ++upper) {}
    /* Avoid signed overflow in validation; valid original durations use int32. */
    {
        int64_t delta = (int64_t)keys[upper].tick - keys[upper-1].tick;
        if (delta > INT32_MAX) return RF_RANGE;
        t = (float)(tick - keys[upper-1].tick) / (float)delta;
    }
    return rf_motion_interpolate_position(&keys[upper-1],&keys[upper],t,out);
}

int rf_motion_interpolate_position(const rf_motion_position_key *previous, const rf_motion_position_key *next, float t, float out[3])
{
    uint32_t c; float s, result[3];
    if (!previous || !next || !out || !isfinite(t) || t<0 || t>1) return RF_RANGE;
    for (c=0;c<3;++c)
        if (!isfinite(previous->position[c]) || !isfinite(previous->outgoing[c]) ||
            !isfinite(next->position[c]) || !isfinite(next->incoming[c])) return RF_FORMAT;
    s = 1.0f - t;
    for (c = 0; c < 3; ++c) {
        float a = previous->position[c], b = previous->outgoing[c];
        float d = next->position[c], e = next->incoming[c];
        a *= s; a *= s; a *= s;
        b *= 3.0f; b *= t; b *= s; b *= s;
        e *= 3.0f; e *= t; e *= t; e *= s;
        d *= t; d *= t; d *= t;
        result[c] = ((a + b) + e) + d;
        if (!isfinite(result[c])) return RF_RANGE;
    }
    memcpy(out, result, sizeof(result));
    return RF_OK;
}
