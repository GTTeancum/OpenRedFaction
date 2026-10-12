/* First-playable received-message history. The authored Message callback is
 * the sole producer: no polling, event replay, voice restart or goal synthesis.
 * Level/name/text bytes are owned copies, never archive or subtitle borrowers.
 * This intentionally has no checkpoint codec. */
enum {SCENE_MESSAGE_HISTORY_CAPACITY=16,SCENE_MESSAGE_HISTORY_TEXT_BYTES=512};
typedef struct scene_message_history_entry {
    char text[SCENE_MESSAGE_HISTORY_TEXT_BYTES],level[64];
    uint32_t event_uid,message_id;
} scene_message_history_entry;
static struct {
    scene_message_history_entry entries[SCENE_MESSAGE_HISTORY_CAPACITY];
    uint32_t next,count,dropped;
} scene_message_history;
typedef char scene_message_history_source_size[
    sizeof(((rf_level_message *)0)->text)==SCENE_MESSAGE_HISTORY_TEXT_BYTES?1:-1];
static void scene_message_history_reset(void)
{memset(&scene_message_history,0,sizeof(scene_message_history));}
static void scene_message_history_receive(const rf_level_event *event,const rf_level_message *message)
{
    scene_message_history_entry *entry;
    /* A load constructor can issue startup messages before final publication.
     * They cannot evict the old timeline if that restoration later fails. */
    if(rf_scene_quickload_pending() || !message->text[0])return;
    entry=scene_message_history.entries+scene_message_history.next;
    memcpy(entry->text,message->text,sizeof(entry->text));
    memcpy(entry->level,campaign_current_level,sizeof(entry->level));
    entry->text[sizeof(entry->text)-1]=0;entry->level[sizeof(entry->level)-1]=0;
    entry->event_uid=event->uid;entry->message_id=message->id;
    scene_message_history.next=(scene_message_history.next+1)%SCENE_MESSAGE_HISTORY_CAPACITY;
    if(scene_message_history.count<SCENE_MESSAGE_HISTORY_CAPACITY)++scene_message_history.count;
    else if(scene_message_history.dropped<UINT32_MAX)++scene_message_history.dropped;
}
uint32_t rf_scene_message_history_count(void){return scene_message_history.count;}
static const scene_message_history_entry *scene_message_history_get(uint32_t index)
{
    uint32_t oldest;
    if(index>=scene_message_history.count)return NULL;
    oldest=(scene_message_history.next+SCENE_MESSAGE_HISTORY_CAPACITY-scene_message_history.count)%SCENE_MESSAGE_HISTORY_CAPACITY;
    return scene_message_history.entries+(oldest+index)%SCENE_MESSAGE_HISTORY_CAPACITY;
}
