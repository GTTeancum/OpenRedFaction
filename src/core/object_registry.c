#include "rf/object_registry.h"
#include <string.h>
void rf_object_registry_init(rf_object_registry *r)
{
    uint32_t i;if(!r)return;memset(r,0,sizeof(*r));
    for(i=0;i<RF_OBJECT_CAPACITY;++i)r->free_slots[i]=i;
    r->count=RF_OBJECT_CAPACITY;r->generation=1;
}
int rf_object_registry_insert(rf_object_registry *r,void *object,uint32_t *handle)
{
    uint32_t slot,value;
    if(!r || !object || !handle || !r->count)return RF_RANGE;
    slot=r->free_slots[r->head];value=(r->generation<<16)|slot;
    r->head=(r->head+1)%RF_OBJECT_CAPACITY;--r->count;
    r->slots[slot].object=object;r->slots[slot].handle=value;
    if(++r->generation>=0x752f)r->generation=1;
    *handle=value;return RF_OK;
}
void *rf_object_registry_lookup(const rf_object_registry *r,uint32_t handle)
{
    uint32_t slot=handle&0xffff;
    if(!r || handle==UINT32_MAX || slot>=RF_OBJECT_CAPACITY)return NULL;
    return r->slots[slot].handle==handle?r->slots[slot].object:NULL;
}
int rf_object_registry_remove(rf_object_registry *r,uint32_t handle)
{
    uint32_t slot=handle&0xffff;
    if(!r)return RF_RANGE;
    if(!rf_object_registry_lookup(r,handle))return RF_NOT_FOUND;
    r->slots[slot].object=NULL;r->slots[slot].handle=0;
    r->free_slots[(r->head+r->count)%RF_OBJECT_CAPACITY]=slot;++r->count;
    return RF_OK;
}

void rf_object_list_init(rf_object_list *list)
{
    list->sentinel.next=list->sentinel.previous=&list->sentinel;list->count=list->peak=0;
}
void rf_object_list_append(rf_object_list *list,rf_object_link *node)
{
    ++list->count;if((int32_t)list->count>(int32_t)list->peak)list->peak=list->count;
    node->previous=list->sentinel.previous;node->next=&list->sentinel;
    list->sentinel.previous->next=node;list->sentinel.previous=node;
}
void rf_object_list_remove(rf_object_list *list,rf_object_link *node)
{
    rf_object_link *previous=node->previous,*next=node->next;
    node->next=node->previous=NULL;previous->next=next;next->previous=previous;--list->count;
}

int rf_attachment_update(rf_attachment_node *node,const rf_attachment_backend *backend,
    rf_attachment_node **scratch,uint32_t capacity)
{
    rf_attachment_node *parent;uint32_t depth=0,i;int status;
    if(!node || !backend || !backend->lookup || !backend->publish || !scratch || !capacity)return RF_RANGE;
    while(node) {
        if(!node->flags)return RF_RANGE;
        if(*node->flags&0x01000000u)break;
        if(depth==capacity)return RF_RANGE;
        for(i=0;i<depth;++i)if(scratch[i]==node)return RF_RANGE;
        scratch[depth++]=node;node=backend->lookup(backend->context,node->parent);
    }
    parent=node;
    while(depth) {
        node=scratch[--depth];
        if(parent && (*parent->flags&0x04000000u)) {
            *node->flags|=0x04000000u;
            status=backend->publish(backend->context,node,parent);if(status)return status;
        }
        *node->flags|=0x01000000u;parent=node;
    }
    return RF_OK;
}
