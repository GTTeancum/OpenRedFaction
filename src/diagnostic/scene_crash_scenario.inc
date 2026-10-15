/* Diagnostic-only process-contained input. Normal gameplay owns all effects.
 * Phases use live input ticks, excluding loading and death/restart input.
 * State persists across normal scene reloads; the host supplies a bounded replay
 * budget and watchdog. This is an encounter sequence, not a campaign route.
 */
uint32_t rf_scene_crash_scenario_trace[8];
/* version, phase, live ticks, dead ticks, phase transitions, cycles,
 * phase-local tick, normal restart requests. Phase 0 idle, 1 engage,
 * 2 strafe right, 3 strafe left, 4 reload, 5 engage/jump, 6 cycle, 7 dead. */
static void scene_crash_scenario_input(uint32_t frame,rf_scene_input *input)
{
    uint32_t tick,phase,local;
    if(!rf_scene_crash_scenario)return;
    memset(input,0,sizeof(*input));
    rf_scene_crash_scenario_trace[0]=1;
    if(!frame || !campaign_spawn)return;
    if(campaign_player_damage.state.effects.health<=0) {
        tick=rf_scene_crash_scenario_trace[3]++;
        phase=7;local=tick;
        /* Release between requests, preserving the normal death timer/gates. */
        if(tick%60==30){input->use=1;++rf_scene_crash_scenario_trace[7];}
    } else {
        tick=rf_scene_crash_scenario_trace[2]++%360;
        if(tick<60){phase=1;local=tick;}
        else if(tick<150){phase=2;local=tick-60;input->move[0]=.35f;}
        else if(tick<180){phase=3;local=tick-150;input->move[0]=-.35f;}
        else if(tick<240){phase=4;local=tick-180;input->reload=local==0;}
        else if(tick<330){phase=5;local=tick-240;input->jump=local==60;}
        else {phase=6;local=tick-330;input->cycle_weapon=local==0;}
        if(phase<=3 || phase==5)input->fire=tick%12<6;
        if(tick==359)++rf_scene_crash_scenario_trace[5];
    }
    if(phase!=rf_scene_crash_scenario_trace[1])++rf_scene_crash_scenario_trace[4];
    rf_scene_crash_scenario_trace[1]=phase;
    rf_scene_crash_scenario_trace[6]=local;
}
