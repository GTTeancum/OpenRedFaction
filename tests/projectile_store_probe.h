typedef struct projectile_store_test {
 rf_projectile_store store;rf_object_registry registry;rf_object_list list;
 uint32_t uid,initialized,cleaned,resources,fail,errors;
} projectile_store_test;
static int projectile_store_initialize(void *v,rf_projectile_owner *owner,uint32_t record[197],const rf_projectile_creation_descriptor *d)
{
 projectile_store_test *c=v;(void)d;++c->initialized;
 if(rf_object_registry_lookup(&c->registry,owner->handle)!=owner || !owner->object_link.next || c->list.count!=c->store.pool.live_count || owner->flags!=0x06100000u)++c->errors;
 record[100]=++c->resources;return c->fail?RF_IO:RF_OK;
}
static void projectile_store_cleanup(void *v,rf_projectile_owner *owner,uint32_t record[197])
{
 projectile_store_test *c=v;++c->cleaned;
 if(rf_object_registry_lookup(&c->registry,owner->handle)!=owner || owner->object_link.next || owner->object_link.previous || c->list.count+1!=c->store.pool.live_count || !record[100] || !c->resources)++c->errors;
 --c->resources;record[100]=0;
}
static int projectile_store_probe(void)
{
 static projectile_store_test c;rf_projectile_owner_ops ops={projectile_store_initialize,projectile_store_cleanup};
 rf_projectile_creation_descriptor d={{0},0};rf_projectile_owner *owner;uint32_t handles[50],i,saved,status;
 memset(&c,0,sizeof(c));rf_projectile_store_init(&c.store);rf_object_registry_init(&c.registry);rf_object_list_init(&c.list);c.uid=1000;
 for(i=0;i<50;++i) {
  owner=NULL;if(rf_projectile_store_open(&c.store,&c.registry,&c.list,&c.uid,&d,&ops,&c,&owner) || !owner || owner->slot!=i || owner->flags!=0x06500000u || owner->uid!=1000-i)return 1;
  handles[i]=owner->handle;owner->flags|=2;if(rf_object_registry_lookup(&c.registry,handles[i])!=owner)return 2;
 }
 owner=NULL;if(rf_projectile_store_open(&c.store,&c.registry,&c.list,&c.uid,&d,&ops,&c,&owner)!=RF_NOT_FOUND || owner || c.uid!=950)return 3;
 for(i=0;i<50;++i)if(rf_projectile_store_close(&c.store,&c.registry,&c.list,handles[i],&ops,&c))return 4;
 owner=NULL;if(rf_projectile_store_open(&c.store,&c.registry,&c.list,&c.uid,&d,&ops,&c,&owner) || owner->slot!=49)return 5;
 if(rf_projectile_store_close(&c.store,&c.registry,&c.list,handles[49],&ops,&c)!=RF_NOT_FOUND)return 6;
 if(rf_projectile_store_close(&c.store,&c.registry,&c.list,owner->handle,&ops,&c))return 7;
 c.fail=1;owner=NULL;status=rf_projectile_store_open(&c.store,&c.registry,&c.list,&c.uid,&d,&ops,&c,&owner);
 if(status!=RF_IO || owner || c.uid!=948 || c.resources || c.list.count || c.store.pool.live_count || c.registry.count!=1024)return 8;
 saved=c.registry.count;c.registry.count=0;
 if(rf_projectile_store_open(&c.store,&c.registry,&c.list,&c.uid,&d,&ops,&c,&owner)!=RF_NOT_FOUND || c.uid!=948)return 9;
 c.registry.count=saved;
 if(c.errors || c.initialized!=52 || c.cleaned!=52)return 10;
 printf("PROJECTILE_STORE %u %u %u %u %u\n",c.initialized,c.cleaned,c.registry.generation,c.store.pool.peak,(uint32_t)sizeof(c.store));return 0;
}
