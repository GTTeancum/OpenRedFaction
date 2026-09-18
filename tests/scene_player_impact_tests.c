#include "rf/entity.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../src/diagnostic/scene_player_impact.inc"
typedef struct fixture {uint32_t damage,feedback,lethal,suppress;float health;rf_damage_request request;} fixture;
static int suppressed(void *p,rf_entity_impact_actor *a,uint32_t *out){(void)a;*out=((fixture*)p)->suppress;return RF_OK;}
static int damage(void *p,rf_entity_impact_actor *a,const rf_damage_request *r){fixture *f=p;f->request=*r;++f->damage;f->health-=r->amount;a->health=f->health;return RF_OK;}
static int sound(void *p,rf_entity_impact_actor *a){(void)a;++((fixture*)p)->lethal;return RF_OK;}
static int feedback(void *p,rf_entity_impact_actor *a,float amount){(void)a;assert(amount>0);++((fixture*)p)->feedback;return RF_OK;}
int main(void)
{
    scene_player_impact impact={0};rf_entity_impact_actor actor={0};fixture f={0};
    rf_entity_impact_backend backend={suppressed,damage,sound,feedback,&f};uint32_t frame;
    actor.handle=7;actor.health=f.health=100;actor.movement_mode=3;actor.use_kind=3;
    actor.contact_material=2;actor.support_material=-1;
    /* Spawn/teleport/checkpoint snaps and diagnostics explicitly disable a frame. */
    scene_player_impact_begin(&impact,0,0);
    assert(!scene_player_impact_record(&impact,0,1,&actor,100));
    assert(!scene_player_impact_flush(&impact,0,&actor,&backend) && !f.damage);
    scene_player_impact_begin(&impact,1,1);
    assert(!scene_player_impact_record(&impact,1,0,&actor,100)); /* camera/support update */
    assert(!scene_player_impact_flush(&impact,1,&actor,&backend) && !f.damage);
    scene_player_impact_begin(&impact,2,1);
    assert(!scene_player_impact_record(&impact,2,1,&actor,12));
    assert(!scene_player_impact_record(&impact,2,1,&actor,11));
    assert(!scene_player_impact_record(&impact,2,1,&actor,13));
    actor.movement_mode=1; /* landing already published before damage callback */
    assert(!scene_player_impact_flush(&impact,2,&actor,&backend));
    assert(f.damage==1 && f.health==64 && f.feedback==1 && !f.lethal);
    assert(f.request.kind==9 && f.request.source==UINT32_MAX && f.request.amount==36);
    assert(!scene_player_impact_record(&impact,2,1,&actor,20));
    assert(!scene_player_impact_flush(&impact,2,&actor,&backend) && f.damage==1);
    for(frame=3;frame<63;frame++){
        scene_player_impact_begin(&impact,frame,1);
        assert(!scene_player_impact_record(&impact,frame,1,&actor,5));
        assert(!scene_player_impact_flush(&impact,frame,&actor,&backend));
    }
    assert(f.damage==1); /* ordinary grounded motion does not repeat damage */
    scene_player_impact_begin(&impact,63,1);f.suppress=1;
    assert(!scene_player_impact_record(&impact,63,1,&actor,20));
    assert(!scene_player_impact_flush(&impact,63,&actor,&backend) && f.damage==1);
    f.suppress=0;scene_player_impact_begin(&impact,64,1);
    assert(!scene_player_impact_record(&impact,64,1,&actor,20));actor.object_flags=4;
    assert(!scene_player_impact_flush(&impact,64,&actor,&backend) && f.damage==1);
    actor.object_flags=0;scene_player_impact_begin(&impact,65,1);
    assert(!scene_player_impact_record(&impact,65,1,&actor,20));
    assert(!scene_player_impact_flush(&impact,65,&actor,&backend) && f.damage==2 && f.lethal==1);
    return 0;
}
