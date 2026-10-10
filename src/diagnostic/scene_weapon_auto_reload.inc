#ifndef RF_SCENE_WEAPON_AUTO_RELOAD_INCLUDED
#define RF_SCENE_WEAPON_AUTO_RELOAD_INCLUDED

/* RF.exe41e7ef calls419bc0 during ordinary entity update.419bed..419ca8
 * requests reload for an empty magazine after fire/alt_fire presentation and
 * pending selection permit it, independently of trigger input and the
 * Autoswitch preference.425312 reads real reserve before reload admission.
 * Include after scene_vehicle_equipment.inc, before campaign_combat_tick.
 * This read-only subset feeds the EXISTING conventional reload request and
 * trigger-inhibit branches, and Fusion/Flamethrower existing reload inputs.
 * It never advances a pose, timer or inventory. Riot charge, other special
 * projectile controllers and active forms are excluded.
 * See docs/WEAPON-AUTO-RELOAD.md for exact scope and parent integration. */
static uint32_t scene_weapon_auto_reload_ready(const scene_stream *s,uint32_t on_foot)
{
    const rf_weapon_acquire_definition *supply;
    const rf_player_weapon *view;
    uint32_t slot=campaign_equipped_slot,view_slot;
    int32_t base,weapon,reserve;
    if(!s || on_foot!=1 || campaign_explicit_unarmed || campaign_player_form.active ||
       campaign_cutscene_runtime.active || scene_driller_active(s) || scene_turret_player_active() ||
       !(campaign_player_damage.state.effects.health>0) || rf_scene_enemy_combat[6] ||
       (campaign_player_view.flags_810&(1u|0x800u)) || rf_scene_combat[6] ||
       combat_trigger.remaining || combat_trigger.delay)return 0;
    switch(slot){
    case 0:case 1:case 3:case 4:case 6:case 7:
    case 10:case 12:case 13:case 14:case 15:case 16:break;
    default:return 0;
    }
    if(!scene_weapon_available(slot) || (slot==4 && !scene_rocket_resources) ||
       (slot==10 && !scene_flame_resources) || (slot==12 && !scene_fusion_resources))return 0;
    if(slot==10 && (rf_scene_player_swim[2] || scene_flame_canister_busy() ||
       scene_flame_input.reload_left))return 0;
    if(slot==13 && (campaign_machine_mode.special>1 || campaign_machine_mode.pending))return 0;
    if(slot==16 && (!s->undercover || s->undercover->mode.pending))return 0;
    if(campaign_weapon_supply.names.count>64)return 0;
    base=campaign_slot_weapon(slot);weapon=campaign_selected_weapon();
    if(base<0 || base>=64 || weapon<0 || weapon>=64 ||
       (uint32_t)base>=campaign_weapon_supply.names.count ||
       (uint32_t)weapon>=campaign_weapon_supply.names.count ||
       !campaign_player_inventory.owned[base])return 0;
    supply=campaign_weapon_supply.definitions+weapon;
    if(supply->magazine<=0 || supply->capacity<=0 || supply->ammo_type<0 || supply->ammo_type>=32 ||
       campaign_player_inventory.loaded[weapon]!=0 ||
       (uint32_t)supply->magazine!=campaign_pistol.magazine ||
       !pistol_reload_ticks || pistol_reload_ticks>3600)return 0;
    reserve=campaign_player_inventory.reserve[supply->ammo_type];
    if(reserve<=0 || reserve>supply->capacity)return 0;
    view_slot=campaign_view_slot();
    if(view_slot>=SCENE_WEAPON_SLOTS || s->player_slot!=view_slot || !s->player_pose_frame ||
       s->player_shots!=rf_scene_combat[0])return 0;
    view=s->player_weapon[view_slot];
    /* rf_player_weapon_step returns completed actions to the real idle clip.
     * Require that existing transition, including continuous-loop release;
     * do not estimate fire duration from a cooldown or advance animation here.
     * A newly selected or invalidated view first publishes its normal idle. */
    if(!view || !view->initialized || view->current!=0 || view->clip_count<3 ||
       !view->payloads[2])return 0;
    return 1;
}

#endif
