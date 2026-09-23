#ifndef RF_SCENE_CAMPAIGN_HISTORY_CHECKPOINT_INCLUDED
#define RF_SCENE_CAMPAIGN_HISTORY_CHECKPOINT_INCLUDED
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* RFCH2 is session history, not the current-level NPC/event payload. Include
 * after all campaign stores/carry declarations. Caller identity must bind the
 * installed campaign/catalogs, not merely the current level. Current owners
 * merge into private copies; no session capture side effects. Max disk207720B.
 * Separate goals/pickups/triggers/startup/current-player and omitted sidecars must be
 * composed independently. Restore replaces store indices: dependent NPC/shield/
 * AI slots require composition-level UID rebinding before gameplay resumes. */
enum { SCENE_CAMPAIGN_HISTORY_MAX_BYTES=207720,
    SCENE_CAMPAIGN_HISTORY_OMIT_PLAYER=1,SCENE_CAMPAIGN_HISTORY_OMIT_WEAPON_MODES=2,
    SCENE_CAMPAIGN_HISTORY_OMIT_NPC_SHIELDS=4,SCENE_CAMPAIGN_HISTORY_OMIT_AI_MODES=8 };
typedef struct scene_campaign_history_checkpoint_stage {
    rf_campaign_pickups switches,events;rf_switch_state switch_saved[RF_CAMPAIGN_PICKUP_SLOTS];
    campaign_event_saved event_saved[RF_CAMPAIGN_PICKUP_SLOTS];rf_campaign_actors actors;
    uint32_t carry_mask;rf_campaign_player_state player_import,player_export;
    scene_machine_pistol_carry machine_import,machine_export;
    scene_undercover_carry undercover_import,undercover_export;
} scene_campaign_history_checkpoint_stage;
static uint32_t scene_campaign_history_checkpoint_omissions(void)
{
    uint32_t flags=0,i;
    if(scene_shield_history_count)flags|=SCENE_CAMPAIGN_HISTORY_OMIT_NPC_SHIELDS;
    for(i=0;i<RF_CAMPAIGN_ACTOR_SLOTS;i++)if(campaign_ai_saved_modes[i].flags&1){flags|=SCENE_CAMPAIGN_HISTORY_OMIT_AI_MODES;break;}
    return flags;
}
static uint32_t scene_history_word(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void scene_history_put(unsigned char *p,uint32_t v)
{uint32_t i;for(i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));}
static int32_t scene_history_signed(const unsigned char *p)
{uint32_t v=scene_history_word(p);return v<=INT32_MAX?(int32_t)v:-1-(int32_t)(UINT32_MAX-v);}
static float scene_history_float(const unsigned char *p)
{float f;uint32_t bits=scene_history_word(p);memcpy(&f,&bits,4);return f;}
static void scene_history_put_float(unsigned char *p,float f)
{uint32_t bits;memcpy(&bits,&f,4);scene_history_put(p,bits);}
static uint32_t scene_history_hash(const unsigned char *p,uint32_t bytes)
{uint32_t i,h=2166136261u;for(i=0;i<bytes;i++){h^=i>=12&&i<16?0:p[i];h*=16777619u;}return h;}
static int scene_history_same(const unsigned char *a,const unsigned char *b)
{for(;;a++,b++){unsigned char x=*a,y=*b;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 0;if(!x)return 1;}}
static int scene_history_keys(const unsigned char *levels,uint32_t nl,const unsigned char *rows,uint32_t nr,uint32_t stride)
{
    uint32_t i,j;
    for(i=0;i<nl;i++){
        const unsigned char *name=levels+i*64;
        if(!name[0]||!memchr(name,0,64))return RF_FORMAT;
        for(j=0;j<nl&&j<i;j++)if(scene_history_same(name,levels+j*64))return RF_FORMAT;
    }
    for(i=0;i<nr;i++){
        const unsigned char *r=rows+i*stride;
        if(scene_history_word(r)>=nl||scene_history_word(r+4)==UINT32_MAX||scene_history_word(r+8)>1)return RF_FORMAT;
        for(j=0;j<i;j++)if(scene_history_word(r)==scene_history_word(rows+j*stride)&&scene_history_word(r+4)==scene_history_word(rows+j*stride+4))return RF_FORMAT;
    }
    return RF_OK;
}
/* RFCH2 header72 mask: import/export player, import/export MP, import/export
 * Undercover. Header76 tail size; header80..95 reserved. Each present player
 * is464 bytes (owned64/reserve32/loaded64/health/armor/weapon/catalog). MP adds
 * base/special IDs + settled mode12B; Undercover adds ID + mode8B. Sidecars
 * deliberately retain their independent snapshots, including stale ones: the
 * existing exact-match guard decides whether a later handoff may apply them. */
static uint32_t scene_history_carry_bytes(uint32_t mask)
{uint32_t i,n=0;for(i=0;i<6;i++)if(mask&(1u<<i))n+=i<2?464:i<4?476:472;return n;}
static void scene_history_player_write(unsigned char *p,const rf_campaign_player_state *v)
{
    uint32_t i;memcpy(p,v->inventory.owned,64);
    for(i=0;i<32;i++)scene_history_put(p+64+i*4,(uint32_t)v->inventory.reserve[i]);
    for(i=0;i<64;i++)scene_history_put(p+192+i*4,(uint32_t)v->inventory.loaded[i]);
    scene_history_put_float(p+448,v->health);scene_history_put_float(p+452,v->armor);
    scene_history_put(p+456,v->weapon);scene_history_put(p+460,v->catalog_hash);
}
static void scene_history_player_read(const unsigned char *p,rf_campaign_player_state *v)
{
    uint32_t i;memcpy(v->inventory.owned,p,64);
    for(i=0;i<32;i++)v->inventory.reserve[i]=scene_history_signed(p+64+i*4);
    for(i=0;i<64;i++)v->inventory.loaded[i]=scene_history_signed(p+192+i*4);
    v->health=scene_history_float(p+448);v->armor=scene_history_float(p+452);
    v->weapon=scene_history_word(p+456);v->catalog_hash=scene_history_word(p+460);
}
static int scene_history_carry_read(const unsigned char *p,uint32_t mask,scene_campaign_history_checkpoint_stage *s)
{
    uint32_t i;rf_campaign_player_state value,checked;
    if(mask&~63u)return RF_FORMAT;
    for(i=0;i<6;i++)if(mask&(1u<<i)){
        scene_history_player_read(p,&value);
        if(rf_campaign_player_copy(&checked,&value,value.catalog_hash))return RF_FORMAT;
        if(i<2){if(s){if(i)s->player_export=value;else s->player_import=value;}}
        else if(i<4){
            int32_t base=scene_history_signed(p+464),special=scene_history_signed(p+468);uint32_t mode=scene_history_word(p+472);
            if(base<0||base>=64||special<0||special>=64||base==special||mode>1||value.inventory.owned[special]||
               value.weapon==(uint32_t)special||(mode&&!value.inventory.owned[base]))return RF_FORMAT;
            if(s){scene_machine_pistol_carry *v=i==2?&s->machine_import:&s->machine_export;
                v->player=value;v->base_weapon=base;v->special_weapon=special;v->special=mode;v->valid=1;}
        }else{
            int32_t id=scene_history_signed(p+464);uint32_t mode=scene_history_word(p+468);
            if(id<0||id>=64||mode>1||(mode&&!value.inventory.owned[id]))return RF_FORMAT;
            if(s){scene_undercover_carry *v=i==4?&s->undercover_import:&s->undercover_export;
                v->player=value;v->weapon=id;v->attached=mode;v->valid=1;}
        }
        p+=i<2?464:i<4?476:472;
    }
    if(s)s->carry_mask=mask;return RF_OK;
}
static void scene_history_carry_write(unsigned char *p,const scene_campaign_history_checkpoint_stage *s)
{
    uint32_t i;for(i=0;i<6;i++)if(s->carry_mask&(1u<<i)){
        if(i<2)scene_history_player_write(p,i?&s->player_export:&s->player_import);
        else if(i<4){const scene_machine_pistol_carry *v=i==2?&s->machine_import:&s->machine_export;
            scene_history_player_write(p,&v->player);scene_history_put(p+464,(uint32_t)v->base_weapon);
            scene_history_put(p+468,(uint32_t)v->special_weapon);scene_history_put(p+472,v->special);
        }else{const scene_undercover_carry *v=i==4?&s->undercover_import:&s->undercover_export;
            scene_history_player_write(p,&v->player);scene_history_put(p+464,(uint32_t)v->weapon);scene_history_put(p+468,v->attached);}
        p+=i<2?464:i<4?476:472;
    }
}
/* Header96: magic/version/bytes/checksum, identity32, three level/count pairs,
 * reserved24. Each section: level names64, then rows. Switch rows32=key12+
 * disabled/limit/unlimited/activations/mode. Event rows32=key12+type/count/
 * enabled/fired/remaining. Actor rows56=key12+valid/health/armor/affiliation/
 * flags/dropstate/weapon/quantity/position3. All integers/float bits LE. */
static int scene_campaign_history_checkpoint_preflight(const unsigned char identity[32],const void *data,uint32_t bytes)
{
    const unsigned char *p=data,*r,*levels;uint32_t counts[6],i,j,k,at=96;uint64_t expected=96;int status;
    if(!identity||!data)return RF_RANGE;
    if(bytes<96||bytes>SCENE_CAMPAIGN_HISTORY_MAX_BYTES||memcmp(p,"RFCH",4)||(scene_history_word(p+4)!=1&&scene_history_word(p+4)!=2)||
       scene_history_word(p+8)!=bytes||memcmp(p+16,identity,32)||scene_history_word(p+12)!=scene_history_hash(p,bytes))return RF_FORMAT;
    for(i=scene_history_word(p+4)==1?72:80;i<96;i+=4)if(scene_history_word(p+i))return RF_FORMAT;
    if(scene_history_word(p+4)==2){
        if((scene_history_word(p+72)&~63u)||scene_history_word(p+76)!=scene_history_carry_bytes(scene_history_word(p+72)))return RF_FORMAT;
        expected+=scene_history_word(p+76);
    }
    for(i=0;i<6;i++)counts[i]=scene_history_word(p+48+i*4);
    for(i=0;i<3;i++){
        if(counts[i*2]>RF_CAMPAIGN_PICKUP_LEVELS||counts[i*2+1]>(i==2?RF_CAMPAIGN_ACTOR_SLOTS:RF_CAMPAIGN_PICKUP_SLOTS))return RF_FORMAT;
        expected+=(uint64_t)counts[i*2]*64+(uint64_t)counts[i*2+1]*(i==2?56:32);
    }
    if(expected!=bytes)return RF_FORMAT;
    for(k=0;k<3;k++){
        uint32_t stride=k==2?56:32,n=counts[k*2+1];levels=p+at;at+=counts[k*2]*64;r=p+at;
        status=scene_history_keys(levels,counts[k*2],r,n,stride);if(status)return status;
        for(i=0;i<n;i++,r+=stride){
            if(k==0){if(scene_history_word(r+20)>255)return RF_FORMAT;}
            else if(k==1){uint32_t type=scene_history_word(r+12);
                if(!scene_history_word(r+8)){for(j=12;j<32;j+=4)if(scene_history_word(r+j))return RF_FORMAT;}
                else if(type==20){if(scene_history_word(r+20)>1||scene_history_word(r+24)||scene_history_signed(r+28)<-1||scene_history_signed(r+28)>RF_TIMER_PERIOD)return RF_FORMAT;}
                else if(type==87||type==88){if(scene_history_word(r+16)||scene_history_word(r+20)||scene_history_word(r+24)>1||scene_history_signed(r+28)!=-1)return RF_FORMAT;}
                else return RF_FORMAT;
            } else {
                if(scene_history_word(r+12)>1||!isfinite(scene_history_float(r+16))||!isfinite(scene_history_float(r+20))||scene_history_float(r+20)<0||
                   (scene_history_word(r+28)&~0x4004u)||scene_history_word(r+32)>2)return RF_FORMAT;
                if(scene_history_word(r+32)&&(scene_history_signed(r+36)<0||scene_history_signed(r+36)>=64||scene_history_signed(r+40)<0))return RF_FORMAT;
                for(j=44;j<56;j+=4)if(!isfinite(scene_history_float(r+j)))return RF_FORMAT;
            }
        }
        at+=n*stride;
    }
    return scene_history_word(p+4)==2?scene_history_carry_read(p+at,scene_history_word(p+72),NULL):RF_OK;
}
static int scene_history_snapshot(scene_campaign_history_checkpoint_stage *s,int32_t now)
{
    uint32_t i,slot;int status;
    if(now<0||now>RF_TIMER_PERIOD||campaign_events.count>RF_OBJECT_CAPACITY||
       (campaign_events.count&&!campaign_events.items)||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items)))return RF_RANGE;
    if(campaign_import_pending>1||campaign_export_valid>1||campaign_machine_import_sidecar.valid>1||
       campaign_machine_export_sidecar.valid>1||campaign_undercover_import_sidecar.valid>1||campaign_undercover_export_sidecar.valid>1)return RF_FORMAT;
    s->carry_mask=campaign_import_pending|(campaign_export_valid<<1)|(campaign_machine_import_sidecar.valid<<2)|
        (campaign_machine_export_sidecar.valid<<3)|(campaign_undercover_import_sidecar.valid<<4)|(campaign_undercover_export_sidecar.valid<<5);
    s->player_import=campaign_player_import;s->player_export=campaign_player_export;
    s->machine_import=campaign_machine_import_sidecar;s->machine_export=campaign_machine_export_sidecar;
    s->undercover_import=campaign_undercover_import_sidecar;s->undercover_export=campaign_undercover_export_sidecar;
    s->switches=campaign_switch_history;s->events=campaign_event_history;s->actors=rf_scene_defeated_actors;
    memcpy(s->switch_saved,campaign_switch_saved,sizeof(s->switch_saved));memcpy(s->event_saved,campaign_event_saved_states,sizeof(s->event_saved));
    if(s->switches.count>RF_CAMPAIGN_PICKUP_SLOTS||s->events.count>RF_CAMPAIGN_PICKUP_SLOTS||s->actors.count>RF_CAMPAIGN_ACTOR_SLOTS||
       s->switches.level_count>128||s->events.level_count>128||s->actors.level_count>128)return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *e=campaign_events.items+i;if(e->retired)continue;if(!e->authored)return RF_FORMAT;
        if(e->switch_state){status=rf_campaign_pickup_register(&s->switches,campaign_current_level,e->authored->record.uid,&slot);if(status)return status;
            s->switch_saved[slot]=*e->switch_state;s->switches.items[slot].retired=1;}
        if(e->state.type==20||e->state.type==87||e->state.type==88){campaign_event_saved value;
            status=campaign_event_capture(e,now,&value);if(status)return status;
            status=rf_campaign_pickup_register(&s->events,campaign_current_level,e->authored->record.uid,&slot);if(status)return status;
            s->event_saved[slot]=value;s->events.items[slot].retired=1;}
    }
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *o=campaign_npc_bodies+i;const rf_campaign_object_record *key;
        if(!o->persistence_registered)continue;slot=o->persistence_slot;if(slot>=s->actors.count||i>=campaign_seeds.records.count||
            !isfinite(o->damage.effects.health)||!isfinite(o->damage.effects.armor)||o->damage.effects.armor<0)return RF_FORMAT;
        /* Campaign registration canonicalizes names to lowercase; archive
         * entries/current level retain authored spelling (L1S1.rfl). */
        key=s->actors.items+slot;if(key->level>=s->actors.level_count||key->uid!=(uint32_t)campaign_seeds.records.items[i].record.uid||
            !memchr(s->actors.levels[key->level],0,64)||!memchr(campaign_current_level,0,64)||
            !scene_history_same((const unsigned char *)s->actors.levels[key->level],(const unsigned char *)campaign_current_level))return RF_FORMAT;
        if(o->registration.view&&o->damage.effects.health>0){
            if(o->registration.view!=&o->view||rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration)return RF_FORMAT;
            s->actors.vitals[slot].valid=1;s->actors.vitals[slot].health=o->damage.effects.health;s->actors.vitals[slot].armor=o->damage.effects.armor;
            s->actors.mission[slot].affiliation=o->damage.effects.affiliation;s->actors.mission[slot].flags=o->object_flags&0x4004u;
        }
        if(o->damage.effects.health<=0)s->actors.items[slot].retired=1;
    }
    return RF_OK;
}
static void scene_history_write_keys(unsigned char *r,const rf_campaign_object_record *key)
{scene_history_put(r,key->level);scene_history_put(r+4,key->uid);scene_history_put(r+8,key->retired);}
static int scene_campaign_history_checkpoint_encode(const unsigned char identity[32],int32_t now,
    void *out,uint32_t capacity,uint32_t *written)
{
    scene_campaign_history_checkpoint_stage *s;unsigned char *p=NULL,*r;uint32_t bytes,i,k,at=96;int status;
    if(!identity||!out||!written)return RF_RANGE;
    s=calloc(1,sizeof(*s));if(!s)return RF_IO;status=scene_history_snapshot(s,now);if(status)goto done;
    bytes=96+(s->switches.level_count+s->events.level_count+s->actors.level_count)*64+
        (s->switches.count+s->events.count)*32+s->actors.count*56+scene_history_carry_bytes(s->carry_mask);
    if(bytes>capacity||bytes>SCENE_CAMPAIGN_HISTORY_MAX_BYTES){status=RF_RANGE;goto done;}
    p=calloc(1,bytes);if(!p){status=RF_IO;goto done;}
    memcpy(p,"RFCH",4);scene_history_put(p+4,2);scene_history_put(p+8,bytes);memcpy(p+16,identity,32);
    scene_history_put(p+48,s->switches.level_count);scene_history_put(p+52,s->switches.count);
    scene_history_put(p+56,s->events.level_count);scene_history_put(p+60,s->events.count);
    scene_history_put(p+64,s->actors.level_count);scene_history_put(p+68,s->actors.count);
    scene_history_put(p+72,s->carry_mask);scene_history_put(p+76,scene_history_carry_bytes(s->carry_mask));
    for(k=0;k<3;k++){
        uint32_t nl=k==0?s->switches.level_count:k==1?s->events.level_count:s->actors.level_count;
        uint32_t nr=k==0?s->switches.count:k==1?s->events.count:s->actors.count;
        const void *names=k==0?(const void *)s->switches.levels:k==1?(const void *)s->events.levels:s->actors.levels;
        memcpy(p+at,names,nl*64);at+=nl*64;
        for(i=0;i<nr;i++){
            r=p+at;scene_history_write_keys(r,k==0?s->switches.items+i:k==1?s->events.items+i:s->actors.items+i);
            if(k==0){const rf_switch_state *v=s->switch_saved+i;scene_history_put(r+12,v->disabled);scene_history_put(r+16,(uint32_t)v->limit);
                scene_history_put(r+20,v->unlimited);scene_history_put(r+24,v->activations);scene_history_put(r+28,(uint32_t)v->mode);}
            else if(k==1){const campaign_event_saved *v=s->event_saved+i;scene_history_put(r+12,v->type);scene_history_put(r+16,v->count);
                scene_history_put(r+20,v->enabled);scene_history_put(r+24,v->fired);scene_history_put(r+28,(uint32_t)v->cycle_remaining);}
            else {const rf_campaign_weapon_drop *d=s->actors.drops+i;uint32_t j;
                scene_history_put(r+12,s->actors.vitals[i].valid);scene_history_put_float(r+16,s->actors.vitals[i].health);scene_history_put_float(r+20,s->actors.vitals[i].armor);
                scene_history_put(r+24,s->actors.mission[i].affiliation);scene_history_put(r+28,s->actors.mission[i].flags);scene_history_put(r+32,d->state);
                scene_history_put(r+36,(uint32_t)d->weapon);scene_history_put(r+40,(uint32_t)d->quantity);for(j=0;j<3;j++)scene_history_put_float(r+44+j*4,d->position[j]);}
            at+=k==2?56:32;
        }
    }
    scene_history_carry_write(p+at,s);
    scene_history_put(p+12,scene_history_hash(p,bytes));status=scene_campaign_history_checkpoint_preflight(identity,p,bytes);
    if(!status){memcpy(out,p,bytes);*written=bytes;}
 done:free(p);free(s);return status;
}
static inline void scene_campaign_history_checkpoint_close(scene_campaign_history_checkpoint_stage **s)
{if(s){free(*s);*s=NULL;}}
static inline int scene_campaign_history_checkpoint_prepare(const unsigned char identity[32],const void *data,uint32_t bytes,
    uint32_t budget,scene_campaign_history_checkpoint_stage **out)
{
    scene_campaign_history_checkpoint_stage *s;const unsigned char *p=data,*r;uint32_t i,k,j,at=96;int status;
    if(!out||*out)return RF_RANGE;status=scene_campaign_history_checkpoint_preflight(identity,data,bytes);if(status)return status;
    if(sizeof(*s)>budget)return RF_RANGE;s=calloc(1,sizeof(*s));if(!s)return RF_IO;
    for(k=0;k<3;k++){
        uint32_t nl=scene_history_word(p+48+k*8),nr=scene_history_word(p+52+k*8);
        char (*levels)[64]=k==0?s->switches.levels:k==1?s->events.levels:s->actors.levels;
        if(k==0){s->switches.level_count=nl;s->switches.count=nr;}else if(k==1){s->events.level_count=nl;s->events.count=nr;}else{s->actors.level_count=nl;s->actors.count=nr;}
        memcpy(levels,p+at,nl*64);at+=nl*64;
        for(i=0;i<nr;i++){
            rf_campaign_object_record *key=k==0?s->switches.items+i:k==1?s->events.items+i:s->actors.items+i;
            r=p+at;key->level=scene_history_word(r);key->uid=scene_history_word(r+4);key->retired=scene_history_word(r+8);
            if(k==0){rf_switch_state *v=s->switch_saved+i;v->disabled=scene_history_word(r+12);v->limit=scene_history_signed(r+16);v->unlimited=scene_history_word(r+20);v->activations=scene_history_word(r+24);v->mode=scene_history_signed(r+28);}
            else if(k==1){campaign_event_saved *v=s->event_saved+i;v->type=scene_history_word(r+12);v->count=scene_history_word(r+16);v->enabled=scene_history_word(r+20);v->fired=scene_history_word(r+24);v->cycle_remaining=scene_history_signed(r+28);}
            else {rf_campaign_weapon_drop *d=s->actors.drops+i;s->actors.vitals[i].valid=scene_history_word(r+12);s->actors.vitals[i].health=scene_history_float(r+16);s->actors.vitals[i].armor=scene_history_float(r+20);
                s->actors.mission[i].affiliation=scene_history_word(r+24);s->actors.mission[i].flags=scene_history_word(r+28);d->state=scene_history_word(r+32);d->weapon=scene_history_signed(r+36);d->quantity=scene_history_signed(r+40);for(j=0;j<3;j++)d->position[j]=scene_history_float(r+44+j*4);}
            at+=k==2?56:32;
        }
    }
    if(scene_history_word(p+4)==2){status=scene_history_carry_read(p+at,scene_history_word(p+72),s);if(status){free(s);return status;}}
    *out=s;return RF_OK;
}
/* Assignment-only publication after ALL composed components have preflighted.
 * Caller holds scene stable; this does not touch current runtime owners. */
static inline void scene_campaign_history_checkpoint_publish(const scene_campaign_history_checkpoint_stage *s)
{
    campaign_import_pending=s->carry_mask&1;campaign_export_valid=(s->carry_mask>>1)&1;
    campaign_player_import=s->player_import;campaign_player_export=s->player_export;
    campaign_machine_import_sidecar=s->machine_import;campaign_machine_export_sidecar=s->machine_export;
    campaign_undercover_import_sidecar=s->undercover_import;campaign_undercover_export_sidecar=s->undercover_export;
    campaign_switch_history=s->switches;campaign_event_history=s->events;rf_scene_defeated_actors=s->actors;
    memcpy(campaign_switch_saved,s->switch_saved,sizeof(s->switch_saved));memcpy(campaign_event_saved_states,s->event_saved,sizeof(s->event_saved));
}
/* Switch/event owners perform level+UID lookup on every revisit and have no
 * retained slot. NPC persistence_slot DOES retain an index. This second stage
 * binds current owners before replacing the all-level catalogs. AI-mode/shield
 * sidecars are explicitly unadmitted until their own disk components are staged. */
typedef struct scene_history_npc_rebind {
    uint32_t registered,old_slot,new_slot,uid,handle;rf_entity_view *view;
} scene_history_npc_rebind;
typedef struct scene_campaign_history_rebind_stage {
    const scene_campaign_history_checkpoint_stage *history;
    campaign_npc_body *bodies;rf_runtime_event *events;
    uint32_t body_count,event_count,bytes;
    scene_history_npc_rebind entries[];
} scene_campaign_history_rebind_stage;
static inline void scene_campaign_history_checkpoint_rebind_close(scene_campaign_history_rebind_stage **s)
{if(s){free(*s);*s=NULL;}}
static int scene_history_find(const char (*levels)[64],uint32_t nl,
    const rf_campaign_object_record *items,uint32_t count,const char *level,uint32_t uid,uint32_t *slot)
{
    uint32_t i,found=UINT32_MAX;
    if(nl>RF_CAMPAIGN_PICKUP_LEVELS||count>RF_CAMPAIGN_ACTOR_SLOTS||!memchr(level,0,64))return RF_FORMAT;
    for(i=0;i<count;i++){
        if(items[i].level>=nl||!memchr(levels[items[i].level],0,64))return RF_FORMAT;
        if(items[i].uid==uid&&scene_history_same((const unsigned char *)levels[items[i].level],(const unsigned char *)level)){
            if(found!=UINT32_MAX)return RF_FORMAT;found=i;}
    }
    if(found==UINT32_MAX)return RF_NOT_FOUND;*slot=found;return RF_OK;
}
static int scene_history_event_bindings(const scene_campaign_history_checkpoint_stage *s)
{
    uint32_t i,slot;int status;
    if(campaign_events.count>RF_OBJECT_CAPACITY||(campaign_events.count&&!campaign_events.items)||
       s->switches.count>RF_CAMPAIGN_PICKUP_SLOTS||s->events.count>RF_CAMPAIGN_PICKUP_SLOTS)return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *e=campaign_events.items+i;if(e->retired)continue;
        if(!e->authored)return RF_FORMAT;
        if(e->switch_state){
            status=scene_history_find(s->switches.levels,s->switches.level_count,s->switches.items,s->switches.count,
                campaign_current_level,e->authored->record.uid,&slot);if(status)return status;
            if(!s->switches.items[slot].retired||s->switch_saved[slot].limit!=e->switch_state->limit||
               s->switch_saved[slot].mode!=e->switch_state->mode||s->switch_saved[slot].unlimited!=e->switch_state->unlimited)return RF_FORMAT;
        }
        if(e->state.type==20||e->state.type==87||e->state.type==88){
            status=scene_history_find(s->events.levels,s->events.level_count,s->events.items,s->events.count,
                campaign_current_level,e->authored->record.uid,&slot);if(status)return status;
            if(!s->events.items[slot].retired||s->event_saved[slot].type!=e->state.type)return RF_FORMAT;
        }
    }
    return RF_OK;
}
static inline int scene_campaign_history_checkpoint_rebind_prepare(const scene_campaign_history_checkpoint_stage *history,
    uint32_t budget,scene_campaign_history_rebind_stage **out)
{
    scene_campaign_history_rebind_stage *s;uint64_t bytes;uint32_t i,slot,old_slot;int status;
    if(!history||!out||*out||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items)))return RF_RANGE;
    if(scene_campaign_history_checkpoint_omissions()&(SCENE_CAMPAIGN_HISTORY_OMIT_AI_MODES|SCENE_CAMPAIGN_HISTORY_OMIT_NPC_SHIELDS))return RF_NOT_FOUND;
    status=scene_history_event_bindings(history);if(status)return status;
    bytes=sizeof(*s)+(uint64_t)campaign_npc_body_count*sizeof(*s->entries);if(bytes>budget||bytes>UINT32_MAX)return RF_RANGE;
    s=calloc(1,(size_t)bytes);if(!s)return RF_IO;
    s->history=history;s->bodies=campaign_npc_bodies;s->events=campaign_events.items;
    s->body_count=campaign_npc_body_count;s->event_count=campaign_events.count;s->bytes=(uint32_t)bytes;
    for(i=0;i<s->body_count;i++){
        const campaign_npc_body *o=s->bodies+i;scene_history_npc_rebind *r=s->entries+i;
        r->registered=o->persistence_registered;r->old_slot=o->persistence_slot;r->handle=o->registration.handle;r->view=o->registration.view;
        if(!r->registered)continue;
        if(i>=campaign_seeds.records.count){status=RF_FORMAT;goto fail;}r->uid=campaign_seeds.records.items[i].record.uid;
        status=scene_history_find(rf_scene_defeated_actors.levels,rf_scene_defeated_actors.level_count,
            rf_scene_defeated_actors.items,rf_scene_defeated_actors.count,campaign_current_level,r->uid,&old_slot);if(status)goto fail;
        if(old_slot!=r->old_slot||(r->view&&(r->view!=&o->view||rf_object_registry_lookup(&campaign_registry,r->handle)!=&o->registration))){status=RF_FORMAT;goto fail;}
        status=scene_history_find(history->actors.levels,history->actors.level_count,history->actors.items,
            history->actors.count,campaign_current_level,r->uid,&slot);if(status)goto fail;r->new_slot=slot;
    }
    *out=s;return RF_OK;
 fail:free(s);return status;
}
static inline int scene_campaign_history_checkpoint_rebind_validate(const scene_campaign_history_rebind_stage *s)
{
    uint32_t i,slot;int status;
    if(!s||!s->history||s->bodies!=campaign_npc_bodies||s->body_count!=campaign_npc_body_count||
       s->events!=campaign_events.items||s->event_count!=campaign_events.count)return RF_FORMAT;
    if(scene_campaign_history_checkpoint_omissions()&(SCENE_CAMPAIGN_HISTORY_OMIT_AI_MODES|SCENE_CAMPAIGN_HISTORY_OMIT_NPC_SHIELDS))return RF_NOT_FOUND;
    status=scene_history_event_bindings(s->history);if(status)return status;
    for(i=0;i<s->body_count;i++){
        const scene_history_npc_rebind *r=s->entries+i;const campaign_npc_body *o=s->bodies+i;
        if(r->registered!=o->persistence_registered||r->old_slot!=o->persistence_slot||
           r->handle!=o->registration.handle||r->view!=o->registration.view)return RF_FORMAT;
        if(!r->registered)continue;
        if(i>=campaign_seeds.records.count||r->uid!=(uint32_t)campaign_seeds.records.items[i].record.uid||
           (r->view&&rf_object_registry_lookup(&campaign_registry,r->handle)!=&o->registration))return RF_FORMAT;
        status=scene_history_find(rf_scene_defeated_actors.levels,rf_scene_defeated_actors.level_count,rf_scene_defeated_actors.items,
            rf_scene_defeated_actors.count,campaign_current_level,r->uid,&slot);if(status)return status;if(slot!=r->old_slot)return RF_FORMAT;
        status=scene_history_find(s->history->actors.levels,s->history->actors.level_count,s->history->actors.items,
            s->history->actors.count,campaign_current_level,r->uid,&slot);if(status)return status;if(slot!=r->new_slot)return RF_FORMAT;
    }
    return RF_OK;
}
/* Composer-only assignment phase after rebind_validate and ALL other stages
 * pass while ownership is frozen. Apply after any NPC state assignment that
 * otherwise retains the fresh scene's old persistence slot. */
static inline void scene_campaign_history_checkpoint_assign_rebound(const scene_campaign_history_checkpoint_stage *history,
    const scene_campaign_history_rebind_stage *bindings)
{
    uint32_t i;
    scene_campaign_history_checkpoint_publish(history);
    for(i=0;i<bindings->body_count;i++)if(bindings->entries[i].registered)
        bindings->bodies[i].persistence_slot=bindings->entries[i].new_slot;
}
static inline int scene_campaign_history_checkpoint_publish_rebound(scene_campaign_history_checkpoint_stage *history,
    scene_campaign_history_rebind_stage *bindings)
{
    int status;if(!bindings||bindings->history!=history)return RF_RANGE;
    status=scene_campaign_history_checkpoint_rebind_validate(bindings);if(status)return status;
    scene_campaign_history_checkpoint_assign_rebound(history,bindings);
    return RF_OK;
}
#endif
