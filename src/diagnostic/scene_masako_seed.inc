#ifndef RF_SCENE_MASAKO_SEED_INCLUDED
#define RF_SCENE_MASAKO_SEED_INCLUDED
/* Original 464696..464744 creates a SECOND, hidden masako actor only in
 * L20S2. It is not authored Masako18301 and must never borrow that actor.
 * The port's indexed NPC loaders consume one explicit generated construction
 * descriptor appended after the unchanged authored prefix. It has no raw RFL
 * span. Its UID is a port persistence key, not an invented authored UID. */
enum {SCENE_MASAKO_GENERATED_UID=0x7f00126d,
    SCENE_MASAKO_DORMANT=0,SCENE_MASAKO_ACTIVE,SCENE_MASAKO_DYING,SCENE_MASAKO_FINISHED};
typedef struct scene_masako_binding {
    uint32_t enabled,parent_uid,parent_slot,actor_slot,class_index;
    uint32_t phase,end_effects,ready;
} scene_masako_binding;
static scene_masako_binding scene_masako;
uint32_t rf_scene_masako_phase[12]; /* created,parent UID,child key,child handle,phase,handoffs,deaths,trigger enables,music,status,class HP,class armor */
static int scene_masako_vehicle_destroy(uint32_t);
static int scene_masako_runtime_initialize(void);
static int scene_masako_runtime_tick(scene_stream *);
static int scene_masako_live_valid(void);
static int scene_masako_seed_is(uint32_t slot)
{return scene_masako.enabled&&slot==scene_masako.actor_slot;}
static int scene_masako_instance_vitals(const rf_level_owned_entity *record,rf_level_entity_vitals *out)
{
    if(scene_masako.enabled&&record==campaign_seeds.records.items+scene_masako.actor_slot){
        if(!out||record->raw||record->record.bytes||record->record.uid!=SCENE_MASAKO_GENERATED_UID)return RF_FORMAT;
        *out=(rf_level_entity_vitals){-1,-1};return RF_OK;
    }
    return rf_level_entity_vitals_read(record,out);
}
static void scene_masako_reset(void)
{memset(&scene_masako,0,sizeof(scene_masako));memset(rf_scene_masako_phase,0,sizeof(rf_scene_masako_phase));}
static int scene_masako_seeds_open(rf_entity_seeds *seeds,const rf_level *level,uint32_t budget)
{
    rf_level_owned_entities records={0};rf_entity_seed *items=NULL;
    uint32_t parent=UINT32_MAX,cls=UINT32_MAX,count;uint64_t raw_bytes=0,bytes,extra,peak;
    const char *wanted="L20S2.rfl";unsigned char *raw;
    if(!seeds||!level||scene_masako.enabled)return RF_RANGE;
    if(rf_emitter_name_lookup(&wanted,1,level->entry.name)!=0)return RF_OK;
    count=seeds->records.count;
    if(count&&(!seeds->records.items||!seeds->items||!seeds->classes))return RF_FORMAT;
    for(uint32_t i=0;i<count;i++){
        const rf_level_owned_entity *r=seeds->records.items+i;
        if(r->record.uid==SCENE_MASAKO_GENERATED_UID||!r->raw||!r->record.bytes)return RF_FORMAT;
        if(!strcmp(r->record.class_name,"masako_fighter")){
            if(parent!=UINT32_MAX)return RF_RANGE;parent=i;
        }
        if(!strcmp(r->record.class_name,"masako"))cls=seeds->items[i].class_index;
        raw_bytes+=r->record.bytes;
    }
    if(parent==UINT32_MAX)return RF_OK;
    /* The shipped L20S2 already supplies the real masako class and all its
     * skeletal resources through actor18301. No model or motion clone. */
    if(cls==UINT32_MAX||cls>=seeds->class_count||seeds->classes[cls].model_kind!=2)return RF_NOT_FOUND;
    bytes=(uint64_t)(count+1)*sizeof(*records.items)+raw_bytes;
    extra=sizeof(*records.items)+sizeof(*items);
    peak=(uint64_t)seeds->resident_bytes+bytes+(uint64_t)(count+1)*sizeof(*items);
    if(bytes>UINT32_MAX-sizeof(records)||extra+seeds->resident_bytes>budget||peak>budget)return RF_RANGE;
    records.storage=calloc(1,(size_t)bytes);items=calloc(count+1,sizeof(*items));
    if(!records.storage||!items){free(records.storage);free(items);return RF_IO;}
    records.items=records.storage;records.count=count+1;records.allocated_bytes=(uint32_t)bytes+sizeof(records);
    raw=(unsigned char *)(records.items+records.count);
    for(uint32_t i=0;i<count;i++){
        records.items[i]=seeds->records.items[i];
        memcpy(raw,records.items[i].raw,records.items[i].record.bytes);records.items[i].raw=raw;
        raw+=records.items[i].record.bytes;items[i]=seeds->items[i];
    }
    rf_level_entity *generated=&records.items[count].record;
    generated->uid=SCENE_MASAKO_GENERATED_UID;generated->offset=UINT32_MAX;
    strcpy(generated->class_name,"masako");strcpy(generated->script_name,"masako_endgame");
    memcpy(generated->position,records.items[parent].record.position,12);
    memcpy(generated->orientation,records.items[parent].record.orientation,36);
    items[count].class_index=cls;items[count].spawn.creation_flags=2;
    items[count].spawn.seat_host_uid=-1;items[count].spawn.relationship_51c=1; /* 402e21 */
    /* Original shared class mutation, not a new instance immunity flag. */
    seeds->classes[cls].physics.flags2|=0x100u;
    free(seeds->records.storage);free(seeds->items);seeds->records=records;seeds->items=items;
    seeds->resident_bytes+=(uint32_t)extra;if(peak>seeds->peak_bytes)seeds->peak_bytes=(uint32_t)peak;
    scene_masako=(scene_masako_binding){1,(uint32_t)records.items[parent].record.uid,parent,count,cls,SCENE_MASAKO_DORMANT,0,0};
    rf_scene_masako_phase[0]=1;rf_scene_masako_phase[1]=scene_masako.parent_uid;
    rf_scene_masako_phase[2]=SCENE_MASAKO_GENERATED_UID;
    return RF_OK;
}
#endif
