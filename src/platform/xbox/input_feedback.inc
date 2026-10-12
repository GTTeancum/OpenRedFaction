/* Xbox output policy, not recovered retail motor tuning. One request slot is
 * owned by input.c's selected controller; no SDL handles or PCM-like leases
 * are borrowed by the scene. Positive requests are submitted only by poll. */
enum {INPUT_RUMBLE_MAX_MS=250u,INPUT_RUMBLE_STOP_ATTEMPTS=3u};
static struct {
    uint32_t pending,active,blocked,pending_until,active_until,stop_attempts;
    Uint16 low,high;
} input_rumble;
static void input_rumble_cancel(void)
{
    input_rumble.pending=0;
    if(input_rumble.active && controller && SDL_GameControllerGetAttached(controller)){
        if(SDL_GameControllerRumble(controller,0,0,0)<0){
            /* A failed stop cannot authorize another positive command. Bound
             * retries so unsupported output cannot become an endless I/O loop.
             * Close still has SDL's native zeroing path; hardware failure cannot
             * be reported as proof that the physical motors stopped. */
            input_rumble.blocked=1;
            if(++input_rumble.stop_attempts<INPUT_RUMBLE_STOP_ATTEMPTS)return;
        }
    }
    input_rumble.active=0;input_rumble.active_until=0;input_rumble.stop_attempts=0;
}
void rf_xbox_input_player_feedback(void *context,float strength,float duration)
{
    uint32_t milliseconds;Uint16 low;(void)context;
    /* Zero is also the scene's immediate cancellation notification. */
    if(!isfinite(strength) || !isfinite(duration) || strength<=0 || duration<=0){
        input_rumble_cancel();return;
    }
    if(!initialized || !controller || pause_mode || pause_edge || disconnect_edge ||
       input_rumble.blocked || !SDL_GameControllerGetAttached(controller))return;
    low=(Uint16)(fminf(strength,1.f)*65535.f);
    if(!low){input_rumble_cancel();return;}
    milliseconds=duration>=.25f?INPUT_RUMBLE_MAX_MS:(uint32_t)(duration*1000.f);
    if(!milliseconds)milliseconds=1;
    /* Latest accepted feedback wins, like the camera owner. Time spent
     * waiting for the next poll consumes this pulse rather than extending it. */
    input_rumble.low=low;input_rumble.high=low/2;
    input_rumble.pending_until=SDL_GetTicks()+milliseconds;input_rumble.pending=1;
}
static void input_rumble_update(void)
{
    uint32_t now;
    if(pause_mode || pause_edge || disconnect_edge || input_rumble.blocked || !controller){
        input_rumble_cancel();return;
    }
    now=SDL_GetTicks();
    if(input_rumble.pending){
        uint32_t remaining;
        if(SDL_TICKS_PASSED(now,input_rumble.pending_until)){input_rumble_cancel();return;}
        remaining=input_rumble.pending_until-now;
        input_rumble.pending=0;input_rumble.active=1;
        input_rumble.active_until=input_rumble.pending_until;
        if(SDL_GameControllerRumble(controller,input_rumble.low,input_rumble.high,remaining)<0){
            /* Unsupported output never fails gameplay or retries positive
             * output on this controller. Even an uncertain write gets zero. */
            input_rumble.blocked=1;input_rumble_cancel();
        }
    }else if(input_rumble.active && SDL_TICKS_PASSED(now,input_rumble.active_until))
        input_rumble_cancel();
}
