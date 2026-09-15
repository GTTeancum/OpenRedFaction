# Missing live NPC damage feedback

Source inspection confirms an integration gap. `rf_entity_damage_effects` in
src/core/entity.c emits RF_DAMAGE_PAIN_ANIMATION for incoming damage above5,
and RF_DAMAGE_PAIN_SOUND for eligible damage fractions. The live scene's
`combat_notify` accepts only RF_DAMAGE_AI_REACTION; it returns immediately
for both pain notifications.

The retained implementations already exist: `rf_scene_npc_pain_retained`
calls the reconstructed428740 pain orchestration, while
`rf_scene_npc_pain_sound` handles sound. The only scene call to the retained
animation wrapper is in `campaign_damage_test_notify`, the diagnostic fixture.
Thus fixture coverage does not establish live campaign pain reactions.

The next gameplay change should connect those notifications using the live
simulation clock, retained owners and deterministic RNG, propagate failures,
and exercise normal player/enemy damage. Keep existing cooldown, AI/action and
animation eligibility gates; do not add unconditional stagger or change damage
to make an input recording win. Review enemy firing during an active reaction
against the existing lock/action machinery before changing its behavior.

Existing evidence includes `tools/verify_npc_pain_binding.py` and
`tools/verify_pain_reaction.py`. These compare reconstructed bindings and
orchestration with the original code, not the missing gameplay dispatch.
This gap is not yet fixed and is not proven to explain every return shot.
