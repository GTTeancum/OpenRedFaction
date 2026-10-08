#ifndef RF_SCENE_VEHICLE_STATIC_EDGES_CHECKS_INCLUDED
#define RF_SCENE_VEHICLE_STATIC_EDGES_CHECKS_INCLUDED
#include "scene_vehicle_static_edges.inc"
/* Synthetic source-only checks shared by the focused host harness and the
 * opt-in Xbox Fighter fixture. No live owners, engine state or allocation.
 * out: attempted, passed, failure mask, first failing source line,
 * baseline found, complement found, parallel fraction bits, outward fraction
 * bits, escaping primitive found, late-error status, transformed sphere,
 * transformed room, transformed source face, frame92 primitive found,
 * frame92 complement found, frame94/tangent mask (expected31). */
typedef struct scene_vehicle_static_edges_check_world {
    rf_geometry_collision_world world;rf_collision_room_view rooms[2];
    rf_collision_tree empty,tree;rf_collision_node node;rf_collision_face faces[2];
    float vertices[4][3];uint32_t primary,child,source[2],stack;
    rf_collision_body_sphere spheres[2];rf_collision_body_query query;
} scene_vehicle_static_edges_check_world;
static void scene_vehicle_static_edges_check_init(scene_vehicle_static_edges_check_world *f)
{
    static const float vertices[4][3]={{0,0,0},{0,0,4},{4,0,4},{4,0,0}};
    memset(f,0,sizeof(*f));memcpy(f->vertices,vertices,sizeof(vertices));
    f->world.views=f->rooms;f->world.room_count=2;f->world.primary=&f->primary;
    f->world.primary_count=1;f->world.children=&f->child;f->world.child_count=1;f->child=1;
    f->rooms[0].tree=&f->empty;f->rooms[0].child_count=1;f->rooms[1].tree=&f->tree;
    for(uint32_t r=0;r<2;r++)for(uint32_t j=0;j<3;j++){
        f->rooms[r].minimum[j]=-10;f->rooms[r].maximum[j]=10;
    }
    f->tree.nodes=&f->node;f->tree.faces=f->faces;f->tree.source_indices=f->source;
    f->tree.stack=&f->stack;f->tree.node_count=f->tree.node_capacity=f->tree.face_count=1;
    f->source[0]=73;f->source[1]=74;f->node.face_count=1;f->node.left=f->node.right=UINT32_MAX;
    f->node.maximum[0]=f->node.maximum[2]=4;
    f->faces[0].plane[1]=1;f->faces[0].maximum[0]=f->faces[0].maximum[2]=4;
    f->faces[0].vertices=(const float(*)[3])f->vertices;f->faces[0].count=4;
    f->faces[0].filter.query_flags=4;
    f->spheres[0].radius=1;f->query.spheres=f->spheres;f->query.count=1;
    f->query.radius=f->query.limit=1;f->query.flags=4;
    f->query.start[0]=-2;f->query.end[0]=1;f->query.start[1]=f->query.end[1]=.5f;
    f->query.start[2]=f->query.end[2]=2;
    f->query.matrix[0][0]=f->query.matrix[1][1]=f->query.matrix[2][2]=1;
}
static void scene_vehicle_static_edges_check_ramp(scene_vehicle_static_edges_check_world *f)
{
    static const float vertices[4][3]={{410.5f,14.625f,-395},{410.5f,10.625f,-386},
        {414.5f,10.625f,-386},{414.5f,14.625f,-395}};
    scene_vehicle_static_edges_check_init(f);memcpy(f->vertices,vertices,sizeof(vertices));
    f->source[0]=918;f->faces[0].filter.face_flags=8;
    f->faces[0].plane[1]=.9138115644454956f;f->faces[0].plane[2]=.40613847970962524f;
    f->faces[0].plane[3]=147.06021118164062f;f->spheres[0].radius=1.7374999523162842f;
    f->query.radius=f->spheres[0].radius;
    for(uint32_t j=0;j<3;j++){
        float lo=vertices[0][j],hi=lo;
        for(uint32_t v=1;v<4;v++){if(vertices[v][j]<lo)lo=vertices[v][j];if(vertices[v][j]>hi)hi=vertices[v][j];}
        f->node.minimum[j]=f->faces[0].minimum[j]=lo;f->node.maximum[j]=f->faces[0].maximum[j]=hi;
        for(uint32_t r=0;r<2;r++){f->rooms[r].minimum[j]=lo-4;f->rooms[r].maximum[j]=hi+4;}
    }
}
static int scene_vehicle_static_edges_checks(uint32_t out[16])
{
    scene_vehicle_static_edges_check_world f;rf_geometry_body_hit hit={0},saved;
    rf_collision_sweep_hit baseline={0};float delta[3]={3,0,0},fraction=0,point[3];
    uint32_t found=0,plain=0,ok;int status;
    if(!out)return RF_RANGE;memset(out,0,16*sizeof(*out));
#define SCENE_STATIC_EDGE_CHECK(bit,condition) do {out[0]++;if(condition)out[1]++;else{out[2]|=1u<<(bit);if(!out[3])out[3]=__LINE__;}} while(0)
    scene_vehicle_static_edges_check_init(&f);
    status=rf_collision_sweep_face(f.faces,f.query.start,delta,delta,1,1,&baseline,&plain);
    out[4]=plain;ok=!status&&!plain;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);out[5]=found;
    memcpy(out+6,&hit.contact.fraction,4);fraction=hit.contact.fraction;
    SCENE_STATIC_EDGE_CHECK(0,ok&&!status&&found&&fabsf(fraction-.377991527f)<1e-6f&&
        fabsf(hit.contact.normal[0]+.8660254f)<1e-6f&&fabsf(hit.contact.normal[1]-.5f)<1e-6f&&
        hit.contact.normal[2]==0&&hit.room==1&&hit.face==73&&hit.sphere==0&&hit.edge==1&&
        hit.contact.object_id==UINT32_MAX&&hit.solid==UINT32_MAX&&hit.contact.face_token==73);

    /* Outward from the plane, but still approaching its finite edge. */
    f.query.end[1]=.75f;delta[1]=.25f;plain=found=0;
    status=rf_collision_sweep_face(f.faces,f.query.start,delta,delta,1,1,&baseline,&plain);ok=!status&&!plain;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);memcpy(out+7,&hit.contact.fraction,4);
    SCENE_STATIC_EDGE_CHECK(1,ok&&!status&&found&&fabsf(hit.contact.fraction-.4f)<1e-6f&&
        fabsf(hit.contact.normal[0]+.8f)<1e-6f&&fabsf(hit.contact.normal[1]-.6f)<1e-6f);

    /* 5072e0 clamps this slightly negative entering root even while leaving. */
    scene_vehicle_static_edges_check_init(&f);f.query.start[0]=-.85f;f.query.end[0]=-40.85f;
    delta[0]=-40;delta[1]=0;plain=found=0;saved=hit;
    status=rf_collision_sphere_edge(f.query.start,delta,1,f.vertices[0],f.vertices[1],1,&fraction,point,&plain);
    out[8]=plain;ok=!status&&plain;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(2,ok&&!status&&!found&&!memcmp(&hit,&saved,sizeof(hit)));

    /* Existing nearer and equal contacts retain all of their metadata. */
    scene_vehicle_static_edges_check_init(&f);hit.contact.fraction=.2f;hit.contact.object_id=19;
    hit.contact.material=91;hit.contact.texture=27;saved=hit;found=1;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok=!status&&found&&!memcmp(&hit,&saved,sizeof(hit));
    memcpy(&hit.contact.fraction,out+6,4);saved=hit;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(3,ok&&!status&&found&&!memcmp(&hit,&saved,sizeof(hit)));

    /* Primary skip suppresses its children; detail-child skip is ignored. */
    scene_vehicle_static_edges_check_init(&f);f.rooms[0].skip=1;found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok=!status&&!found;
    f.query.flags|=8;f.rooms[1].skip=255;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok&=!status&&found;
    f.rooms[0].skip=0;f.query.flags=4;found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok&=!status&&found;
    f.rooms[0].child_count=0;found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(4,ok&&!status&&!found);

    /* Solid filters still exclude liquid, portal and positive-property faces. */
    scene_vehicle_static_edges_check_init(&f);f.faces[0].filter.face_flags=4;f.query.flags|=0x1000;found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok=!status&&!found;
    f.faces[0].filter.face_flags=1;f.query.flags=4;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok&=!status&&!found;
    f.faces[0].filter.face_flags=0;f.faces[0].filter.property_34=1;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(5,ok&&!status&&!found);

    /* Each retained broadphase bound can exclude the synthetic edge. */
    scene_vehicle_static_edges_check_init(&f);f.rooms[1].minimum[0]=20;f.rooms[1].maximum[0]=30;found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok=!status&&!found;
    f.rooms[1].minimum[0]=-10;f.rooms[1].maximum[0]=10;f.node.minimum[0]=20;f.node.maximum[0]=30;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok&=!status&&!found;
    f.node.minimum[0]=0;f.node.maximum[0]=4;f.faces[0].minimum[0]=20;f.faces[0].maximum[0]=30;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(6,ok&&!status&&!found);

    /* Actual offset/radius and matrix columns select sphere1, not body radius. */
    scene_vehicle_static_edges_check_init(&f);f.spheres[0].radius=.25f;
    f.spheres[1].radius=1;f.spheres[1].center[0]=2;f.query.count=2;f.query.radius=3;
    memset(f.query.matrix,0,sizeof(f.query.matrix));f.query.matrix[0][2]=1;f.query.matrix[1][1]=1;f.query.matrix[2][0]=-1;
    f.query.start[2]=f.query.end[2]=0;found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    out[10]=hit.sphere;out[11]=hit.room;out[12]=hit.face;memcpy(&fraction,out+6,4);
    SCENE_STATIC_EDGE_CHECK(7,!status&&found&&hit.sphere==1&&hit.room==1&&hit.face==73&&
        fabsf(hit.contact.fraction-fraction)<1e-6f&&fabsf(hit.contact.point[2]-2)<1e-6f);

    scene_vehicle_static_edges_check_init(&f);memcpy(f.query.end,f.query.start,12);found=0;saved=hit;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);ok=!status&&!found&&!memcmp(&hit,&saved,sizeof(hit));
    f.query.end[0]=1;f.spheres[0].radius=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(8,ok&&!status&&!found&&!memcmp(&hit,&saved,sizeof(hit)));

    /* An error after an accepted local candidate must not publish that hit. */
    scene_vehicle_static_edges_check_init(&f);f.faces[1]=f.faces[0];f.faces[1].vertices=NULL;
    f.tree.face_count=f.node.face_count=2;found=0;saved=hit;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);out[9]=(uint32_t)status;
    ok=status==RF_RANGE&&!found&&!memcmp(&hit,&saved,sizeof(hit));
    f.query.matrix[0][0]=NAN;found=77;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    ok&=status==RF_FORMAT&&found==77&&!memcmp(&hit,&saved,sizeof(hit));
    scene_vehicle_static_edges_check_init(&f);f.query.flags|=0x80;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    ok&=status==RF_NOT_FOUND&&found==77&&!memcmp(&hit,&saved,sizeof(hit));
    scene_vehicle_static_edges_check_init(&f);f.tree.node_capacity=0;found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(9,ok&&status==RF_RANGE&&!found&&!memcmp(&hit,&saved,sizeof(hit)));

    /* Source005739 frame92 rotation: sequential-float transformed endpoints
     * start 5.22 micrometres inside edge918. Its entering root is -.06285,
     * beyond the primitive clamp; the opposite translation must remain free. */
    scene_vehicle_static_edges_check_ramp(&f);
    f.query.start[0]=416.1899719238281f;f.query.start[1]=12.887480735778809f;f.query.start[2]=-390.09686279296875f;
    delta[0]=-.00018310546875f;delta[1]=0;delta[2]=.001007080078125f;
    for(uint32_t j=0;j<3;j++)f.query.end[j]=f.query.start[j]+delta[j];plain=found=0;
    status=rf_collision_sphere_edge(f.query.start,delta,f.spheres[0].radius,f.vertices[2],f.vertices[3],1,&fraction,point,&plain);
    out[13]=plain;ok=!status&&!plain;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);out[14]=found;
    ok&=!status&&found&&hit.contact.fraction==0&&hit.face==918&&hit.contact.normal[0]>.97f;
    hit.contact.material=123;saved=hit;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    ok&=!status&&found&&!memcmp(&hit,&saved,sizeof(hit)); /* Existing zero-time tie. */
    for(uint32_t j=0;j<3;j++)f.query.end[j]=f.query.start[j]-delta[j];found=0;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    SCENE_STATIC_EDGE_CHECK(10,ok&&!status&&!found&&!memcmp(&hit,&saved,sizeof(hit)));

    /* Source005739 frame93->94: body-offset translation, then sequential-
     * float rotation endpoints. Both already overlap and deepen the contact. */
    {
        static const float starts[2][3]={{416.1936950683594f,12.887483596801758f,-390.1390075683594f},
            {416.1950988769531f,12.887497901916504f,-390.18341064453125f}};
        static const float moves[2][3]={{.001373291015625f,.0000133514404296875f,-.044403076171875f},
            {-.00018310546875f,0,.000946044921875f}};
        for(uint32_t c=0;c<2;c++){
            scene_vehicle_static_edges_check_ramp(&f);memcpy(f.query.start,starts[c],12);
            for(uint32_t j=0;j<3;j++)f.query.end[j]=starts[c][j]+moves[c][j];found=0;
            status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
            if(!status&&found&&hit.contact.fraction==0&&hit.face==918)out[15]|=1u<<c;
            saved=hit;for(uint32_t j=0;j<3;j++)f.query.end[j]=starts[c][j]-moves[c][j];found=0;
            status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
            if(!status&&!found&&!memcmp(&hit,&saved,sizeof(hit)))out[15]|=1u<<(c+2);
        }
    }
    scene_vehicle_static_edges_check_init(&f);f.query.start[0]=f.query.end[0]=-.5f;
    f.query.start[1]=f.query.end[1]=0;f.query.end[2]=2.25f;found=0;saved=hit;
    status=scene_vehicle_static_edges_compose(&f.world,&f.query,&hit,&found);
    if(!status&&!found&&!memcmp(&hit,&saved,sizeof(hit)))out[15]|=16;
    SCENE_STATIC_EDGE_CHECK(11,out[15]==31);
#undef SCENE_STATIC_EDGE_CHECK
    return out[2]?RF_FORMAT:RF_OK;
}
#endif
