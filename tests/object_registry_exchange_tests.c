#include "rf/object_registry.h"
#include <stdio.h>
#include <string.h>

/* No game/runtime dependencies. Whole-state snapshots establish rejection
 * atomicity and that publication cannot consume generations or FIFO slots. */
static rf_object_registry registry,before,expected;
static unsigned char objects[RF_OBJECT_CAPACITY+4];
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}} while(0)
#define REJECT(code,expression) do {memcpy(&before,&registry,sizeof(before)); \
    CHECK((expression)==(code));CHECK(!memcmp(&before,&registry,sizeof(before)));} while(0)

int main(void)
{
    uint32_t a,b,c,again,handles[RF_OBJECT_CAPACITY],i;
    rf_object_registry_init(&registry);
    CHECK(rf_object_registry_insert(&registry,objects,&a)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,objects+1,&b)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,objects+2,&c)==RF_OK);

    REJECT(RF_NOT_FOUND,rf_object_registry_exchange(&registry,a,objects,objects+3,
        b,objects+2,objects+4)); /* The first valid row must not publish early. */
    REJECT(RF_NOT_FOUND,rf_object_registry_exchange(&registry,a^0x10000u,objects,objects+3,
        b,objects+1,objects+4));
    REJECT(RF_NOT_FOUND,rf_object_registry_exchange(&registry,a,objects,objects+3,
        UINT32_MAX,objects+1,objects+4));
    REJECT(RF_RANGE,rf_object_registry_exchange(&registry,a,objects,objects+3,
        b,objects+1,objects+3));
    REJECT(RF_RANGE,rf_object_registry_exchange(&registry,a,objects,objects+2,
        b,objects+1,objects+3)); /* Third live owner cannot be stolen. */
    REJECT(RF_RANGE,rf_object_registry_exchange(&registry,a,objects,objects+3,
        b,objects+1,objects+2)); /* Same check on the second replacement. */
    REJECT(RF_RANGE,rf_object_registry_exchange(&registry,a,objects,objects+3,
        a,objects,objects+4));
    REJECT(RF_RANGE,rf_object_registry_exchange(&registry,a,NULL,objects+3,
        b,objects+1,objects+4));
    REJECT(RF_RANGE,rf_object_registry_exchange(&registry,a,objects,NULL,
        b,objects+1,objects+4));
    CHECK(rf_object_registry_exchange(NULL,a,objects,objects+3,b,objects+1,objects+4)==RF_RANGE);

    memcpy(&expected,&registry,sizeof(expected));
    expected.slots[a&0xffffu].object=objects+1;expected.slots[b&0xffffu].object=objects;
    CHECK(rf_object_registry_exchange(&registry,a,objects,objects+1,b,objects+1,objects)==RF_OK);
    CHECK(!memcmp(&expected,&registry,sizeof(expected)));
    CHECK(rf_object_registry_lookup(&registry,a)==objects+1);
    CHECK(rf_object_registry_lookup(&registry,b)==objects);
    CHECK(rf_object_registry_lookup(&registry,c)==objects+2);
    memcpy(&expected,&registry,sizeof(expected));
    CHECK(rf_object_registry_exchange(&registry,a,objects+1,objects+1,b,objects,objects)==RF_OK);
    CHECK(!memcmp(&expected,&registry,sizeof(expected))); /* Checked no-op. */
    expected.slots[a&0xffffu].object=objects+3;expected.slots[b&0xffffu].object=objects+4;
    CHECK(rf_object_registry_exchange(&registry,a,objects+1,objects+3,b,objects,objects+4)==RF_OK);
    CHECK(!memcmp(&expected,&registry,sizeof(expected)));

    /* The registry's historical insert API permits duplicate pointers. Even
     * then, exchange must reject retaining an owner aliased in a third slot. */
    CHECK(rf_object_registry_insert(&registry,objects+3,&again)==RF_OK);
    REJECT(RF_RANGE,rf_object_registry_exchange(&registry,a,objects+3,objects+3,
        b,objects+4,objects+4));

    /* Fill all slots so release/reinsert immediately reuses the released slot.
     * The old generation must fail even when its expected pointer is reused. */
    rf_object_registry_init(&registry);
    for(i=0;i<RF_OBJECT_CAPACITY;++i)
        CHECK(rf_object_registry_insert(&registry,objects+i,handles+i)==RF_OK);
    a=handles[0];b=handles[1];
    CHECK(rf_object_registry_remove(&registry,a)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,objects,&again)==RF_OK);
    CHECK((a&0xffffu)==(again&0xffffu) && a!=again);
    REJECT(RF_NOT_FOUND,rf_object_registry_exchange(&registry,a,objects,objects+1,
        b,objects+1,objects));
    memcpy(&expected,&registry,sizeof(expected));
    expected.slots[again&0xffffu].object=objects+1;expected.slots[b&0xffffu].object=objects;
    CHECK(rf_object_registry_exchange(&registry,again,objects,objects+1,b,objects+1,objects)==RF_OK);
    CHECK(!memcmp(&expected,&registry,sizeof(expected)));
    CHECK(rf_object_registry_remove(&registry,b)==RF_OK);
    CHECK(rf_object_registry_remove(&registry,again)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,objects+RF_OBJECT_CAPACITY,&a)==RF_OK);
    CHECK(rf_object_registry_insert(&registry,objects+RF_OBJECT_CAPACITY+1,&c)==RF_OK);
    CHECK((a&0xffffu)==(b&0xffffu));CHECK((c&0xffffu)==(again&0xffffu));
    CHECK((a>>16)==expected.generation);CHECK((c>>16)==expected.generation+1u);
    puts("object registry checked exchange: PASS");return 0;
}
