#ifndef RF_SCENE_REMOTE_CHECKPOINT_INC
#define RF_SCENE_REMOTE_CHECKPOINT_INC
#include "rf/remote_checkpoint.h"
/* Include after scene_remote_gameplay.inc; earlier checkpoint callers only
 * need prototypes. Main thread only; two bounded snapshots avoid stack pools.
 * Parent stages this before RFDS commit, discards on any failed transaction,
 * publishes after ALL geometry/player checks, and keeps host identities stable.
 * Runtime object_id/actor indices are never durable save identities. */
#define SCENE_REMOTE_PLAYER_KEY 0xfffffffeu
static rf_remote_checkpoint scene_remote_capture_state,scene_remote_restore_state;
static uint32_t scene_remote_restore_ready,scene_remote_restore_keep,scene_remote_restore_select;
static uint32_t scene_remote_checkpoint_level_hash(void)
{return npc_hash_bytes(2166136261u,campaign_current_level,(uint32_t)strlen(campaign_current_level));}
/* Diagnostic: per call, supply word, remote-definition hash, primary-prefix
 * hash; [0..2] first call this session, [3..5] latest, [6] calls. */
uint32_t rf_scene_remote_hash_trace[7];
/* RFRM1/2 catalog identity predates appended piercing metadata. Remote
 * charges do not consume it; retain the exact legacy primary prefix. */
_Static_assert(offsetof(rf_weapon_primary_definition,piercing_enabled)==76u,
               "RFRM legacy primary catalog prefix");
/* FNV-1a over zero bytes, identical to npc_hash_bytes on a zero buffer. */
static uint32_t scene_remote_checkpoint_hash_zero(uint32_t hash,uint32_t bytes)
{while(bytes--)hash*=16777619u;return hash;}
/* variant0: the remote definitions as currently resident in the process-wide
 * statics (the original, legacy formula). variant1: the same formula over
 * zero-filled definitions, i.e. the value produced by a process that never
 * loaded Remote Charge resources.
 * Those statics are filled only when a scene demands remote charges and are
 * never cleared, so variant0 depended on process history: a save written by a
 * process that never loaded remote charges (variant1) failed RFWM/RFRM
 * catalog admission in any later process that had (2026-10-24 hardware
 * death-reload failures in L14S3/L19S2A/L19S2B/L19S3). */
static uint32_t scene_remote_checkpoint_catalog_hash_variant(uint32_t zero)
{
    const uint32_t prefix=(uint32_t)offsetof(rf_weapon_primary_definition,piercing_enabled);
    uint32_t hash=rf_scene_weapon_supply[3];
    if(zero){hash=scene_remote_checkpoint_hash_zero(hash,(uint32_t)sizeof(campaign_remote));
        return scene_remote_checkpoint_hash_zero(hash,prefix);}
    hash=npc_hash_bytes(hash,&campaign_remote,sizeof(campaign_remote));
    return npc_hash_bytes(hash,&campaign_remote_primary,prefix);
}
/* Canonical encoder identity: definitions count only when THIS scene loaded
 * them (campaign demand, or the DEV room which loads them unconditionally).
 * Stale statics left by an earlier scene are hashed as zero. In the DEV room
 * and in any scene with remote demand this equals the legacy value. */
static uint32_t scene_remote_checkpoint_catalog_hash(void)
{
    uint32_t loaded=scene_remote_resources||rf_scene_dev_room_enabled;
    {uint32_t parts[3]={rf_scene_weapon_supply[3],loaded?npc_hash_bytes(2166136261u,&campaign_remote,sizeof(campaign_remote)):0u,
        loaded?npc_hash_bytes(2166136261u,&campaign_remote_primary,(uint32_t)offsetof(rf_weapon_primary_definition,piercing_enabled)):0u};
     if(!rf_scene_remote_hash_trace[6]++)memcpy(rf_scene_remote_hash_trace,parts,12);
     memcpy(rf_scene_remote_hash_trace+3,parts,12);}
    return scene_remote_checkpoint_catalog_hash_variant(!loaded);
}
/* Decoder admission for a stored catalog word. Returns the expected value to
 * pass to the codec: the stored word when it is one of the admitted
 * identities, otherwise the canonical value (so the codec rejects as before).
 * Canonical and legacy-resident values are always admitted. The zero-
 * definition value is admitted when the caller states that the payload does
 * not consume remote definitions (RFWM always; RFRM only without charges).
 * The world envelope identity independently binds rf_scene_weapon_supply[3]. */
static uint32_t scene_remote_checkpoint_catalog_admit(uint32_t stored,uint32_t definitions_unused)
{
    uint32_t canonical=scene_remote_checkpoint_catalog_hash();
    if(stored==canonical||stored==scene_remote_checkpoint_catalog_hash_variant(0))return stored;
    if(definitions_unused&&stored==scene_remote_checkpoint_catalog_hash_variant(1))return stored;
    return canonical;
}
static uint32_t scene_remote_checkpoint_stored_word(const void *data,uint32_t bytes,uint32_t offset)
{
    const unsigned char *p=data;
    if(!p||bytes<offset+4)return 0;
    return (uint32_t)p[offset]|(uint32_t)p[offset+1]<<8|(uint32_t)p[offset+2]<<16|(uint32_t)p[offset+3]<<24;
}
/* RFWM carries flags/RNG/shield only; remote definitions never affect it.
 * Catalog word is at byte16 (rf_weapon_modes_checkpoint.h). */
static uint32_t scene_weapon_modes_checkpoint_catalog(const void *data,uint32_t bytes)
{return scene_remote_checkpoint_catalog_admit(scene_remote_checkpoint_stored_word(data,bytes,16),1);}
/* RFRM: charge count at byte12, catalog word at byte20 (remote_checkpoint.c). */
static uint32_t scene_remote_checkpoint_catalog(const void *data,uint32_t bytes)
{return scene_remote_checkpoint_catalog_admit(scene_remote_checkpoint_stored_word(data,bytes,20),
    bytes>=24&&!scene_remote_checkpoint_stored_word(data,bytes,12));}
static int scene_remote_checkpoint_npc_key(uint32_t index,uint32_t *key)
{
    if(!key || index>=campaign_npc_body_count || !campaign_seeds.records.items || index>=campaign_seeds.records.count)return RF_FORMAT;
    *key=(uint32_t)campaign_seeds.records.items[index].record.uid;
    return *key>=SCENE_REMOTE_PLAYER_KEY?RF_FORMAT:RF_OK;
}
static int scene_remote_checkpoint_owner_key(uint32_t handle,uint32_t *key)
{
    uint32_t i;
    if(handle==campaign_player_object.handle){*key=SCENE_REMOTE_PLAYER_KEY;return RF_OK;}
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].registration.view &&
        campaign_npc_bodies[i].registration.handle==handle &&
        rf_object_registry_lookup(&campaign_registry,handle)==&campaign_npc_bodies[i].registration)
        return scene_remote_checkpoint_npc_key(i,key);
    return RF_FORMAT;
}
static int scene_remote_checkpoint_npc(uint32_t key,uint32_t *index,uint32_t *handle)
{
    uint32_t i,found=0,candidate;
    if(key>=SCENE_REMOTE_PLAYER_KEY)return RF_FORMAT;
    for(i=0;i<campaign_npc_body_count;i++){
        if(scene_remote_checkpoint_npc_key(i,&candidate))return RF_FORMAT;
        if(candidate!=key)continue;
        if(found || !campaign_npc_bodies[i].registration.view ||
           rf_object_registry_lookup(&campaign_registry,campaign_npc_bodies[i].registration.handle)!=&campaign_npc_bodies[i].registration)return RF_FORMAT;
        found=1;*index=i;*handle=campaign_npc_bodies[i].registration.handle;
    }
    return found?RF_OK:RF_FORMAT;
}
static int scene_remote_checkpoint_capture(void *output,uint32_t capacity,uint32_t *written)
{
    rf_remote_checkpoint *state=&scene_remote_capture_state;uint32_t i;int status;
    memset(state,0,sizeof(*state));state->held=scene_remote_held;state->pending=scene_remote_pending;
    state->delay=scene_remote_delay;state->cooldown=scene_remote_cooldown;
    state->selected_mode=campaign_equipped_slot==8?1u:campaign_equipped_slot==9?2u:0u;
    state->followup_target=scene_remote_followup_target;state->followup_ticks=scene_remote_followup_ticks;
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++)if(scene_remote_charges[i].flight.lifecycle.active){
        rf_remote_charge *charge=state->charges+i;uint32_t tag=scene_remote_host_tags[i],index=tag&0xffffu;
        *charge=scene_remote_charges[i];state->tags[i]=UINT32_MAX;state->host_keys[i]=UINT32_MAX;
        if(campaign_remote_id<0 || charge->type!=(uint32_t)campaign_remote_id)return RF_FORMAT;
        status=scene_remote_checkpoint_owner_key(charge->owner,state->owner_keys+i);if(status)return status;
        if(charge->host_bound){
            if((tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
                if(index>=campaign_npc_body_count || charge->host!=campaign_npc_bodies[index].registration.handle)return RF_FORMAT;
                status=scene_remote_checkpoint_npc_key(index,state->host_keys+i);if(status)return status;
                state->tags[i]=SCENE_ACTOR_ROCKET_OWNER;
            }else if((tag&0xffff0000u)==SCENE_MOVER_ROCKET_OWNER){
                if(index>=campaign_movers.count || !campaign_movers.uids || charge->host!=campaign_movers.views[index].object_id)return RF_FORMAT;
                state->host_keys[i]=(uint32_t)campaign_movers.uids[index];if(state->host_keys[i]==UINT32_MAX)return RF_FORMAT;
                state->tags[i]=SCENE_MOVER_ROCKET_OWNER;
            }else return RF_FORMAT;
        }else{
            /* Unknown fragment attachment cannot be remapped durably yet. */
            if(charge->attached && charge->contact.object!=UINT32_MAX)return RF_NOT_FOUND;
            charge->contact.object=UINT32_MAX;charge->host=UINT32_MAX;
        }
    }
    return rf_remote_checkpoint_encode(state,scene_remote_checkpoint_level_hash(),scene_remote_checkpoint_catalog_hash(),output,capacity,written);
}
static int scene_remote_checkpoint_holster_admit(uint32_t holstered)
{
    if(!scene_remote_restore_ready)return RF_RANGE;
    return holstered && (scene_remote_restore_state.pending || scene_remote_restore_state.delay ||
        scene_remote_restore_state.followup_target)?RF_FORMAT:RF_OK;
}
static void scene_remote_checkpoint_discard(void)
{scene_remote_restore_ready=0;}
/* Resolve attachment transport against the same candidate poses that the world
 * transaction will publish. Live poses may differ after travel or a fresh boot. */
static int scene_remote_checkpoint_world_host(const scene_world_restore_stage *world,
    uint32_t tag,uint32_t expected,rf_remote_charge_host *host)
{
    uint32_t index=tag&0xffffu,i;
    if(!world || !host)return RF_RANGE;
    memset(host,0,sizeof(*host));host->handle=expected;
    if((tag&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
        if(!world->npcs)return RF_FORMAT;
        for(i=0;i<world->npcs->count;i++){
            const scene_npc_checkpoint_restore_entry *e=world->npcs->entries+i;
            if(e->slot!=index)continue;
            if(!e->registered || e->handle!=expected || e->saved.retired ||
               e->saved.class_id>=campaign_seeds.class_count)return RF_FORMAT;
            host->found=host->entity=1;
            host->entity_flags=e->saved.death_flags_810|(e->saved.health<=0?1u:0u);
            host->class_flags=campaign_seeds.classes[e->saved.class_id].physics.flags;
            memcpy(host->position,e->body.position,12);memcpy(host->orientation,e->body.orientation,36);
            return RF_OK;
        }
        return RF_FORMAT;
    }
    if((tag&0xffff0000u)==SCENE_MOVER_ROCKET_OWNER){
        const rf_collision_solid_view *view;
        if(index>=world->movers.mover_count || !world->movers.views)return RF_FORMAT;
        view=world->movers.views+index;if(view->object_id!=expected)return RF_FORMAT;
        host->found=1;memcpy(host->position,view->output_origin,12);memcpy(host->orientation,view->output_matrix,36);
        return RF_OK;
    }
    return RF_FORMAT;
}
static int scene_remote_checkpoint_prepare(scene_stream *stream,const void *data,uint32_t bytes,
    const scene_world_restore_stage *world)
{
    rf_remote_checkpoint *state=&scene_remote_restore_state;uint32_t i,index,handle;int status;
    int32_t remote_id=rf_weapon_name_find(&campaign_weapon_supply.names,"Remote Charge");
    scene_remote_restore_ready=0;if(!stream)return RF_RANGE;
    if(!bytes){if(data)return RF_RANGE;memset(state,0,sizeof(*state));scene_remote_restore_ready=1;return RF_OK;}
    status=rf_remote_checkpoint_decode(data,bytes,scene_remote_checkpoint_level_hash(),scene_remote_checkpoint_catalog(data,bytes),state);if(status)return status;
    if(state->pending && state->selected_mode!=1)return RF_FORMAT;
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++)if(state->charges[i].flight.lifecycle.active){
        rf_remote_charge *charge=state->charges+i;rf_remote_charge_host host;uint32_t action;
        if(remote_id<0 || charge->type!=(uint32_t)remote_id)return RF_FORMAT;
        if(state->owner_keys[i]==SCENE_REMOTE_PLAYER_KEY)charge->owner=campaign_player_object.handle;
        else {status=scene_remote_checkpoint_npc(state->owner_keys[i],&index,&handle);if(status)return status;charge->owner=handle;}
        if(charge->host_bound){
            if(state->tags[i]==SCENE_ACTOR_ROCKET_OWNER){
                status=scene_remote_checkpoint_npc(state->host_keys[i],&index,&handle);if(status)return status;
                state->tags[i]=SCENE_ACTOR_ROCKET_OWNER|index;
            }else if(state->tags[i]==SCENE_MOVER_ROCKET_OWNER){
                uint32_t j,found=0;if(!campaign_movers.uids || !campaign_movers.views)return RF_FORMAT;
                for(j=0;j<campaign_movers.count;j++)if((uint32_t)campaign_movers.uids[j]==state->host_keys[i]){
                    if(found)return RF_FORMAT;found=1;index=j;handle=campaign_movers.views[j].object_id;}
                if(!found || handle==UINT32_MAX)return RF_FORMAT;state->tags[i]=SCENE_MOVER_ROCKET_OWNER|index;
            }else return RF_FORMAT;
            charge->host=handle;charge->contact.object=state->tags[i];
            status=world?scene_remote_checkpoint_world_host(world,state->tags[i],handle,&host):
                scene_remote_host(stream,state->tags[i],handle,&host);if(status || !host.found)return status?status:RF_FORMAT;
            status=rf_remote_charge_update_host(charge,&host,&action);if(status || action!=1)return status?status:RF_FORMAT;
        }else{
            if(state->tags[i]!=UINT32_MAX || state->host_keys[i]!=UINT32_MAX || charge->contact.object!=UINT32_MAX)return RF_FORMAT;
            charge->host=UINT32_MAX;
        }
    }
    scene_remote_restore_ready=1;return RF_OK;
}
static int scene_remote_checkpoint_preflight(scene_stream *stream,const void *data,uint32_t bytes)
{return scene_remote_checkpoint_prepare(stream,data,bytes,NULL);}
static int scene_remote_checkpoint_world_preflight(scene_stream *stream,const void *data,uint32_t bytes,
    const scene_world_restore_stage *world)
{if(!world)return RF_RANGE;return scene_remote_checkpoint_prepare(stream,data,bytes,world);}
/* Publication evidence: active, bound, first durable host UID, first position. */
volatile uint32_t rf_scene_remote_restored[6];
/* Infallible after successful preflight; call only once all other stage checks
 * pass. No callbacks, allocation, lookup or geometry mutation on publication. */
static void scene_remote_checkpoint_publish(void)
{
    uint32_t i;if(!scene_remote_restore_ready)return;
    memcpy(scene_remote_charges,scene_remote_restore_state.charges,sizeof(scene_remote_charges));
    memcpy(scene_remote_host_tags,scene_remote_restore_state.tags,sizeof(scene_remote_host_tags));
    scene_remote_held=scene_remote_restore_state.held;scene_remote_pending=scene_remote_restore_state.pending;
    scene_remote_delay=scene_remote_restore_state.delay;scene_remote_cooldown=scene_remote_restore_state.cooldown;
    /* Selection restoration below may cancel the live scheduler. Keep it inert
     * until the saved charge/detonator mode has been assigned successfully. */
    scene_remote_followup_target=scene_remote_followup_ticks=0;
    memset(rf_scene_remote,0,sizeof(rf_scene_remote));for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++)rf_scene_remote[4]+=scene_remote_charges[i].flight.lifecycle.active!=0;
    memset((void *)rf_scene_remote_restored,0,sizeof(rf_scene_remote_restored));
    for(i=0;i<RF_REMOTE_CHARGE_CAPACITY;i++)if(scene_remote_charges[i].flight.lifecycle.active){
        ++rf_scene_remote_restored[0];
        if(scene_remote_charges[i].host_bound){
            if(!rf_scene_remote_restored[1]){
                rf_scene_remote_restored[2]=scene_remote_restore_state.host_keys[i];
                memcpy((void *)(rf_scene_remote_restored+3),scene_remote_charges[i].flight.position,12);
            }
            ++rf_scene_remote_restored[1];
        }
    }
    scene_remote_restore_ready=0;scene_remote_restore_keep=scene_remote_restore_select=1;
}
/* Parent's frame0 reset becomes if(!keep_restored())scene_remote_reset(). */
static uint32_t scene_remote_checkpoint_keep_restored(void)
{uint32_t keep=scene_remote_restore_keep;scene_remote_restore_keep=0;return keep;}
/* Call frame0 AFTER campaign_inventory_initialize/import. Shared inventory ID
 * does not itself distinguish charge from detonator's firstperson/control slot. */
static void scene_remote_checkpoint_apply_selection(void)
{
    uint32_t mode=scene_remote_restore_state.selected_mode;if(!scene_remote_restore_select)return;scene_remote_restore_select=0;
    scene_remote_followup_target=scene_remote_followup_ticks=0;
    if(!mode || campaign_explicit_unarmed || campaign_remote_id<0 || campaign_remote_id>=64 || campaign_selected_weapon()!=campaign_remote_id ||
       !campaign_player_inventory.owned[campaign_remote_id])return;
    campaign_select_primary(mode==2?9:8);campaign_ammo_publish();
    /* Assignment only: do not expire, request, launch or invoke followup
     * callbacks while restoring. Version1/absent state supplies both zeros. */
    scene_remote_followup_target=scene_remote_restore_state.followup_target;
    scene_remote_followup_ticks=scene_remote_restore_state.followup_ticks;
}
/* Ordinary world load publishes after frame0's combat tick. Apply its saved
 * charge/detonator mode here, after restoring the shared weapon ID, and do
 * not leave a frame0 keep token for a later level. */
static void scene_remote_checkpoint_world_selection(void)
{scene_remote_restore_keep=0;scene_remote_checkpoint_apply_selection();}
static void scene_remote_checkpoint_frame0(void)
{
    if(scene_remote_checkpoint_keep_restored())scene_remote_checkpoint_apply_selection();
    else {scene_remote_restore_select=0;scene_remote_reset();}
}
#endif
