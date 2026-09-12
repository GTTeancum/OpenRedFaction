#include "rf/clutter.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_object_registry registry,reference_registry;
static rf_model_file file;
static void normalize(rf_clutter_base_owner *v)
{
    v->state.token=v->state.model=v->attachment.model=0;
    v->object_link.next=v->object_link.previous=NULL;v->body.spheres.items=NULL;
    v->allocated_bytes=v->peak_bytes=0;
}
int main(int argc,char **argv)
{
    rf_static_render_resource resource={0};rf_model_collision_sphere spheres[4]={0};
    rf_clutter_shared_static_model model={"fixture.v3m",&resource,0};
    rf_clutter_base_owner *a=NULL,*b=NULL,*failed=NULL,*reference=NULL;
    rf_clutter_create_descriptor d={0};rf_object_list list,reference_list;
    rf_vpp archive={0};uint32_t uid=UINT32_MAX,reference_uid=UINT32_MAX,i,peak,flags;
    float material[3]={.25f,.5f,2};const char *name=argc==3?argv[2]:"fixture.v3d";
    if(argc==3) {
        CHECK(rf_vpp_open(&archive,argv[1])==RF_OK);
        CHECK(rf_model_file_open(&file,&archive,name)==RF_OK);
        CHECK(rf_static_render_resource_open(&file,4*1024*1024,&resource)==RF_OK);
        model.filename=file.entry.name;
    } else {
        resource.bound[3]=2;resource.spheres=spheres;resource.sphere_count=4;
        for(i=0;i<4;++i){spheres[i].parent=-1;spheres[i].center[0]=(float)i*.25f;spheres[i].radius=.5f;}
    }
    d.model=name;d.kind=1;d.material=2;d.identifier=-1;d.radius=-1;
    d.position[0]=3;d.position[1]=2;d.position[2]=-1;d.matrix[0]=d.matrix[4]=d.matrix[8]=1;
    for(flags=0;flags<=0x20;flags+=0x20) {
        rf_object_registry_init(&registry);rf_object_list_init(&list);uid=UINT32_MAX;d.flags=flags;
        CHECK(rf_clutter_shared_static_base_open(&model,&d,&registry,&list,&uid,0,0,1,material,65536,&a)==RF_OK && a);
        CHECK(model.references==1);peak=a->peak_bytes;
        if(argc==3) {
            rf_clutter_base_owner left,right;
            rf_object_registry_init(&reference_registry);rf_object_list_init(&reference_list);reference_uid=UINT32_MAX;
            CHECK(rf_clutter_static_base_open(&archive,&d,&reference_registry,&reference_list,&reference_uid,0,0,1,material,65536,&reference)==RF_OK && reference);
            left=*a;right=*reference;normalize(&left);normalize(&right);
            CHECK(!memcmp(&left,&right,sizeof(left)));
            CHECK(!a->body.spheres.count || !memcmp(a->body.spheres.items,reference->body.spheres.items,a->body.spheres.count*sizeof(rf_physics_sphere)));
            CHECK(rf_clutter_static_base_close(&reference,&reference_registry,&reference_list)==RF_OK);
        }
        CHECK(rf_clutter_shared_static_base_open(&model,&d,&registry,&list,&uid,0,0,1,material,peak,&b)==RF_OK && b);
        CHECK(model.references==2 && list.count==2);
        if(flags)CHECK(a->body.spheres.items!=b->body.spheres.items);
        CHECK(rf_clutter_shared_static_base_open(&model,&d,&registry,&list,&uid,0,0,1,material,peak-1,&failed)==RF_RANGE);
        CHECK(!failed && model.references==2 && list.count==2);
        d.model="missing-fixture.v3d";
        CHECK(rf_clutter_shared_static_base_open(&model,&d,&registry,&list,&uid,0,0,1,material,65536,&failed)==RF_OK && !failed);
        CHECK(model.references==2 && list.count==2);d.model=name;
        CHECK(rf_clutter_shared_static_base_close(&model,&a,&registry,&list)==RF_OK && !a && model.references==1);
        CHECK(rf_object_registry_lookup(&registry,b->state.handle)==&b->state);
        CHECK(rf_clutter_shared_static_base_close(&model,&b,&registry,&list)==RF_OK && !b && !model.references);
        CHECK(registry.count==1024 && list.count==0);
        CHECK(rf_clutter_shared_static_base_close(&model,&b,&registry,&list)==RF_OK);
    }
    /* Overflow must not acquire or leak a model borrow, even after registration. */
    model.references=UINT32_MAX;
    CHECK(rf_clutter_shared_static_base_open(&model,&d,&registry,&list,&uid,0,0,1,material,65536,&failed)==RF_RANGE);
    CHECK(!failed && model.references==UINT32_MAX && registry.count==1024 && !list.count);model.references=0;
    if(argc==3){rf_vpp_close(&archive);rf_static_render_resource_close(&resource);}
    puts("Shared static bases: independent bodies, reference lifetime, budgets, rollback and cleanup passed.");return 0;
}
