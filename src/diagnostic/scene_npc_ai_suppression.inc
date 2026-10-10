#ifndef RF_SCENE_NPC_AI_SUPPRESSION_INCLUDED
#define RF_SCENE_NPC_AI_SUPPRESSION_INCLUDED
/* Goto4bab00 ->40cfc0 publishes entity7d0 / AI-relative530 bit40000000.
 * It is per registered actor, never a shared class flag or script_move.active.
 * Original408346/408634/408932/408ae2 gate noise, reactions and AI selection.
 * It is NOT a universal firing gate:401580 tests408e90 bit1; direct scripted
 *425830/426ca0 and already-owned409280 burst/reload work are separate owners.
 * Keep all three port views of the same source bit coherent, changing no
 * unrelated flags, timers, ammunition, motion, physics or scripted orders. */
#define SCENE_NPC_GOTO_AI_SUPPRESSION 0x40000000u
static uint32_t campaign_npc_ai_suppressed(const campaign_npc_body *o)
{return !!(o->ai_mode.flags_530&SCENE_NPC_GOTO_AI_SUPPRESSION);}
static uint32_t campaign_npc_ai_suppression_consistent(const campaign_npc_body *o)
{
    return !((o->view.flags_7d0^o->ai_mode.flags_530)&SCENE_NPC_GOTO_AI_SUPPRESSION)&&
        !((o->firing.flags_7d0^o->ai_mode.flags_530)&SCENE_NPC_GOTO_AI_SUPPRESSION);
}
/* Caller owns a fully qualified live actor, or a completely admitted saved
 * level/UID candidate at the assignment-only publication boundary. */
static void campaign_npc_ai_suppression_assign(campaign_npc_body *o,uint32_t value)
{
    uint32_t bit=value?SCENE_NPC_GOTO_AI_SUPPRESSION:0;
    o->ai_mode.flags_530=(o->ai_mode.flags_530&~SCENE_NPC_GOTO_AI_SUPPRESSION)|bit;
    o->view.flags_7d0=(o->view.flags_7d0&~SCENE_NPC_GOTO_AI_SUPPRESSION)|bit;
    o->firing.flags_7d0=(o->firing.flags_7d0&~SCENE_NPC_GOTO_AI_SUPPRESSION)|bit;
}
#endif
