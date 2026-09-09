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
