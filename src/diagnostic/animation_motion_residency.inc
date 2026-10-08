#ifndef RF_ANIMATION_MOTION_RESIDENCY_INCLUDED
#define RF_ANIMATION_MOTION_RESIDENCY_INCLUDED
/* Optional player-only immutable clip cache. The public state-set descriptors
 * stay borrowed and unchanged; sampling uses private copies while their bytes
 * are owned here. Cache pressure or allocation/I/O failure retains the exact
 * archive-backed sampler rather than changing animation or failing admission. */
#define ANIMATION_MOTION_BUDGET (128u*1024u)
#define ANIMATION_MOTION_CAPACITY 23u
typedef struct animation_motion_entry {
    const rf_motion_file *source;rf_motion_file file;
    void *data;uint32_t bytes,unavailable;
} animation_motion_entry;
typedef struct animation_motion_cache {
    animation_motion_entry entries[ANIMATION_MOTION_CAPACITY];uint32_t bytes;
} animation_motion_cache;
/* resident bytes,peak bytes,loads,resident selections,archive fallbacks,
 * evictions,payload budget,owner bytes. Reset for each animation stream. */
uint32_t rf_animation_motion_residency[8];
static void animation_motion_release(animation_motion_cache *cache,uint32_t slot)
{
    animation_motion_entry *entry=cache->entries+slot;
    if(entry->data){cache->bytes-=entry->bytes;free(entry->data);++rf_animation_motion_residency[5];}
    memset(entry,0,sizeof(*entry));
}
static void animation_motion_close(animation_motion_cache *cache)
{
    if(!cache)return;
    for(uint32_t i=0;i<ANIMATION_MOTION_CAPACITY;i++)free(cache->entries[i].data);
    free(cache);rf_animation_motion_residency[0]=0;
}
static const rf_motion_file *const *animation_motion_bind(animation_motion_cache *cache,
    const rf_motion_file *const *sources,uint32_t count,const rf_motion_playback_state *state,
    const rf_motion_file **selected)
{
    uint8_t active[ANIMATION_MOTION_CAPACITY]={0};uint32_t i,j;
    /* Caller has already validated the resource count. Leave original handles
     * selected on malformed playback; the ordinary evaluator owns rejection. */
    if(!cache || count>ANIMATION_MOTION_CAPACITY || state->completion.active.count>16)return sources;
    for(i=0;i<count;i++)selected[i]=sources[i];
    for(i=0;i<state->completion.active.count;i++) {
        int32_t id=state->completion.active.slots[i].motion;
        if(id<0 || (uint32_t)id>=count)return sources;
        active[id]=1;
    }
    /* A form replacement may assign different source files to the same IDs.
     * Release the old private descriptors before considering the new set. */
    for(i=0;i<ANIMATION_MOTION_CAPACITY;i++) {
        animation_motion_entry *entry=cache->entries+i;
        if(i>=count || entry->source!=sources[i])animation_motion_release(cache,i);
    }
    for(i=0;i<count;i++)if(active[i] && sources[i] && !sources[i]->resident) {
        animation_motion_entry *entry=cache->entries+i;const rf_motion_file *source=sources[i];
        void *data;rf_motion_file file;int status;
        if(entry->data) {
            selected[i]=&entry->file;++rf_animation_motion_residency[3];continue;
        }
        entry->source=source;
        if(entry->unavailable || source->entry.size>ANIMATION_MOTION_BUDGET || !source->archive) {
            ++rf_animation_motion_residency[4];continue;
        }
        /* Never evict any descriptor needed by this frame. Its exact active
         * slots, including zero-weight slots retained by the model evaluator,
         * determine residency; no visibility, frame number or benchmark gate. */
        for(j=0;j<ANIMATION_MOTION_CAPACITY && source->entry.size>ANIMATION_MOTION_BUDGET-cache->bytes;j++)
            if(!active[j] && cache->entries[j].data)animation_motion_release(cache,j);
        if(source->entry.size>ANIMATION_MOTION_BUDGET-cache->bytes) {
            ++rf_animation_motion_residency[4];continue;
        }
        data=malloc(source->entry.size);
        if(!data){entry->unavailable=1;++rf_animation_motion_residency[4];continue;}
        file=*source;
        status=rf_vpp_read(source->archive,&source->entry,0,data,source->entry.size);
        if(!status)status=rf_motion_file_bind_memory(&file,data,source->entry.size);
        if(status){free(data);entry->unavailable=1;++rf_animation_motion_residency[4];continue;}
        entry->data=data;entry->bytes=source->entry.size;entry->file=file;
        cache->bytes+=entry->bytes;selected[i]=&entry->file;
        ++rf_animation_motion_residency[2];++rf_animation_motion_residency[3];
        if(cache->bytes>rf_animation_motion_residency[1])rf_animation_motion_residency[1]=cache->bytes;
    }
    rf_animation_motion_residency[0]=cache->bytes;return selected;
}
#endif
