#include "rf/physics.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    float in[3];struct {rf_physics_fallback value;int32_t status;} out;
    _Static_assert(sizeof(out)==28,"Physics probe wire format");
    _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--run")) {
        struct {rf_physics_body_state state;float dt,speed,acceleration,traction,input[3],normal[3],support[3];} input;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(rf_physics_run_propose(&input.state,input.dt,input.speed,input.acceleration,input.traction,input.input,input.normal,input.support))return 3;
            if(fwrite(&input.state,sizeof(input.state),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--stance")) {
        struct {uint32_t count,flags,crouching;rf_physics_sphere spheres[8];float centers[8][3];float position[3],height;} input;
        struct {int32_t status;uint32_t flags;rf_physics_sphere spheres[8];float end[3];} result;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            rf_physics_spheres owner={input.spheres,input.count,sizeof(input.spheres)};
            result.status=rf_physics_stance_centers(&owner,input.centers,input.count,&input.flags,input.crouching);
            if(result.status)return 3;
            result.status=rf_physics_stand_endpoint(input.position,input.height,result.end);
            result.flags=input.flags;memcpy(result.spheres,input.spheres,sizeof(result.spheres));
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ground-motion")) {
        float v[18];
        while(fread(v,sizeof(v),1,stdin)==1) {
            rf_physics_body_state state={0};state.mass=v[0];
            memcpy(state.position,v+3,12);memcpy(state.velocity,v+6,12);memcpy(state.vector_e0,v+9,12);
            if(rf_physics_ground_propose(&state,v[1],v[2],v+12,v+15))return 3;
            if(fwrite(state.velocity,12,1,stdout)!=1 || fwrite(state.next_position,12,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && (!strcmp(argv[1],"--land") || !strcmp(argv[1],"--support"))) {
        struct {rf_physics_body_state state;rf_physics_ground_probe probe;float fraction;} input;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if((!strcmp(argv[1],"--land")?rf_physics_static_land:rf_physics_static_support)(&input.state,&input.probe,input.fraction))return 3;
            if(fwrite(&input.state,sizeof(input.state),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--ground")) {
        struct {rf_physics_sphere spheres[3];float position[3];uint32_t flags,falling;float dt,speed,support_y;} input;
        rf_physics_ground_probe value;
        _Static_assert(sizeof(value)==84,"Ground probe wire format");
        while(fread(&input,sizeof(input),1,stdin)==1) {
            if(rf_physics_ground_prepare(input.spheres,3,input.position,input.flags,input.falling,input.dt,input.speed,input.support_y,&value))return 3;
            if(fwrite(&value,sizeof(value),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--advance")) {
        struct {float dt,fraction,position[3],next[3];uint32_t flags;} input;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            rf_physics_body_state state={0};float remaining;
            memcpy(state.position,input.position,12);memcpy(state.next_position,input.next,12);state.flags=input.flags;
            if(rf_physics_contact_advance(&state,input.dt,input.fraction,&remaining))return 3;
            if(fwrite(state.position,12,1,stdout)!=1 || fwrite(&state.scalar_144,4,1,stdout)!=1 || fwrite(&remaining,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--player-contact")) {
        struct {float values[27];uint32_t mode,free_tangent;} input;
        while(fread(&input,sizeof(input),1,stdin)==1) {
            rf_physics_body_state state={0};float impact;
            memcpy(state.velocity,input.values,12);memcpy(state.vector_c8,input.values+3,12);
            memcpy(state.orientation,input.values+18,36);state.flags=0x80;
            if(rf_physics_player_contact(&state,input.values+6,input.values+9,input.values+12,input.values+15,
                input.mode,input.free_tangent,&impact))return 3;
            if(fwrite(state.velocity,12,1,stdout)!=1 || fwrite(state.vector_c8,12,1,stdout)!=1 || fwrite(&impact,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--contact")) {
        float values[15];
        while(fread(values,sizeof(values),1,stdin)==1) {
            rf_physics_body_state state={0};float impact;
            memcpy(state.velocity,values,12);memcpy(state.vector_c8,values+3,12);
            if(rf_physics_static_contact(&state,values+6,values+9,values+12,&impact))return 3;
            if(fwrite(state.velocity,12,1,stdout)!=1 || fwrite(state.vector_c8,12,1,stdout)!=1 || fwrite(&impact,4,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && (!strcmp(argv[1],"--fall") || !strcmp(argv[1],"--fall-repeat"))) {
        float values[15];
        while(fread(values,sizeof(values),1,stdin)==1) {
            rf_physics_body_state state={0};state.mass=values[0];
            if(!strcmp(argv[1],"--fall-repeat"))state.flags=0x1000000;
            memcpy(state.position,values+3,12);memcpy(state.velocity,values+6,12);memcpy(state.vector_e0,values+9,12);
            if(rf_physics_fall_propose(&state,values[1],values[2],values+12))return 3;
            if(fwrite(state.velocity,12,1,stdout)!=1 || fwrite(state.next_position,12,1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--replace-guards")) {
        rf_physics_body body={0},saved;rf_physics_body_parameters p={0};
        rf_physics_sphere old={{0,0,0},1,-1,0x12345678},next[2]={{{0,3,0},2,.5f,7},{{0,0,0},1,-1,8}};
        uint32_t peak=(uint32_t)(sizeof(body)+3*sizeof(old));rf_physics_body_state expected;
        p.mass=100;p.flags=0x80000078;p.position[0]=10;p.position[1]=20;p.position[2]=30;
        p.local_tensor[0]=2;p.local_tensor[4]=3;p.local_tensor[8]=4;
        p.orientation[0]=p.orientation[4]=p.orientation[8]=1;
        if(rf_physics_body_replace_spheres(&body,next,2,peak)!=RF_RANGE)return 3;
        if(rf_physics_body_open(&p,&old,1,peak,&body))return 3;
        saved=body;expected=body.state;
        if(rf_physics_body_replace_spheres(&body,next,2,peak-1)!=RF_RANGE || memcmp(&body,&saved,sizeof(body)) || memcmp(body.spheres.items,&old,sizeof(old)))return 3;
        next[1].radius=-1;
        if(rf_physics_body_replace_spheres(&body,next,2,peak)!=RF_RANGE || memcmp(&body,&saved,sizeof(body)))return 3;
        next[1].radius=1;
        if(rf_physics_body_replace_spheres(&body,next,2,peak))return 3;
        expected.flags|=0x2000;expected.bounds.radius=5;
        expected.bounds.minimum[0]=5;expected.bounds.minimum[1]=15;expected.bounds.minimum[2]=25;
        expected.bounds.maximum[0]=15;expected.bounds.maximum[1]=25;expected.bounds.maximum[2]=35;
        if(memcmp(&body.state,&expected,sizeof(expected)) || memcmp(body.spheres.items,next,sizeof(next)) || body.allocated_bytes!=sizeof(body)+sizeof(next))return 3;
        /* Aliasing the old allocation must copy before freeing it. */
        if(rf_physics_body_replace_spheres(&body,body.spheres.items+1,1,peak))return 3;
        if(body.state.flags&0x2000 || memcmp(body.spheres.items,next+1,sizeof(old)))return 3;
        if(rf_physics_body_replace_spheres(&body,NULL,0,(uint32_t)(sizeof(body)+sizeof(old))) || body.state.bounds.radius!=0 || body.allocated_bytes!=sizeof(body))return 3;
        rf_physics_body_close(&body);rf_physics_body_close(&body);puts("sphere replacement guards PASS");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--body")) {
        rf_physics_body_parameters parameters;rf_physics_sphere source[32];uint32_t count;
        _Static_assert(sizeof(parameters)==128 && sizeof(rf_physics_body_state)==308,"Body wire layout");
        while(fread(&parameters,sizeof(parameters),1,stdin)==1) {
            rf_physics_body body={0},empty={0};uint32_t budget,used;
            if(fread(&count,4,1,stdin)!=1 || count>32 || fread(source,sizeof(*source),count,stdin)!=count)return 2;
            used=(parameters.flags&0x70)?count:0;budget=(uint32_t)(sizeof(body)+used*sizeof(*source));
            if(rf_physics_body_open(&parameters,source,count,budget-1,&body)!=RF_RANGE || memcmp(&body,&empty,sizeof(body)))return 3;
            if(rf_physics_body_open(&parameters,source,count,budget,&body) || body.allocated_bytes!=budget || body.spheres.count!=used)return 3;
            if(rf_physics_body_open(&parameters,source,count,budget,&body)!=RF_RANGE)return 3;
            memset(source,0xa5,sizeof(source));memset(&parameters,0xa5,sizeof(parameters));
            if(fwrite(&body.state,sizeof(body.state),1,stdout)!=1 || fwrite(&used,4,1,stdout)!=1 ||
                fwrite(body.spheres.items,sizeof(*source),used,stdout)!=used)return 1;
            rf_physics_body_close(&body);rf_physics_body_close(&body);
            if(memcmp(&body,&empty,sizeof(body)))return 3;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--bounds")) {
        rf_physics_sphere source[32];uint32_t count;float position[3];
        struct {rf_physics_bounds value;int32_t status;} result;
        while(fread(&count,4,1,stdin)==1) {
            if(count>32 || fread(position,sizeof(position),1,stdin)!=1 || fread(source,sizeof(*source),count,stdin)!=count)return 2;
            memset(&result,0xa5,sizeof(result));result.status=rf_physics_spheres_bounds(source,count,position,&result.value);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--world")) {
        float matrices[18];struct {float matrix[9];int32_t status;} result;
        while(fread(matrices,sizeof(matrices),1,stdin)==1) {
            memset(&result,0xa5,sizeof(result));result.status=rf_physics_tensor_world(matrices,matrices+9,result.matrix);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--inverse")) {
        float matrix[9];struct {float matrix[9];int32_t status;} result;
        while(fread(matrix,sizeof(matrix),1,stdin)==1) {
            memset(&result,0xa5,sizeof(result));result.status=rf_physics_tensor_inverse(matrix,result.matrix);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && (!strcmp(argv[1],"--accumulate") || !strcmp(argv[1],"--prepare"))) {
        rf_physics_sphere source[32];rf_physics_mass_tensor initial;uint32_t count;float density;
        struct {rf_physics_mass_tensor value;int32_t status;} result;
        _Static_assert(sizeof(result)==44,"Mass tensor wire format");
        while(fread(&count,4,1,stdin)==1) {
            if(count>32 || fread(&density,4,1,stdin)!=1 || fread(&initial,sizeof(initial),1,stdin)!=1 ||
                fread(source,sizeof(*source),count,stdin)!=count)return 2;
            memset(&result,0xa5,sizeof(result));
            result.status=!strcmp(argv[1],"--prepare")?
                rf_physics_spheres_prepare(source,count,density,&initial,&result.value):
                rf_physics_spheres_accumulate(source,count,density,&initial,&result.value);
            if(fwrite(&result,sizeof(result),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--spheres")) {
        rf_physics_sphere source[32];uint32_t count;
        {
            rf_physics_spheres owned={0},empty={0};uint32_t bad=0x7fc00000;
            memset(source,0,sizeof(source));source[0].radius=1;
            memcpy(&source[0].center[1],&bad,4);
            if(rf_physics_spheres_open(source,1,1024,&owned)!=RF_RANGE || memcmp(&owned,&empty,sizeof(owned)))return 3;
            source[0].center[1]=0;source[0].radius=-1;
            if(rf_physics_spheres_open(source,1,1024,&owned)!=RF_RANGE || memcmp(&owned,&empty,sizeof(owned)))return 3;
            if(rf_physics_spheres_open(NULL,1,1024,&owned)!=RF_RANGE || memcmp(&owned,&empty,sizeof(owned)))return 3;
        }
        while(fread(&count,4,1,stdin)==1) {
            rf_physics_spheres owned={0},small={0},empty={0};uint32_t budget;
            if(count>32 || fread(source,sizeof(*source),count,stdin)!=count)return 2;
            budget=(uint32_t)(sizeof(owned)+count*sizeof(*source));
            if(rf_physics_spheres_open(source,count,budget-1,&small)!=RF_RANGE || memcmp(&small,&empty,sizeof(small)))return 3;
            if(rf_physics_spheres_open(source,count,budget,&owned) || owned.allocated_bytes!=budget)return 3;
            if(rf_physics_spheres_open(source,count,budget,&owned)!=RF_RANGE)return 3;
            memset(source,0xa5,sizeof(source));
            if(fwrite(owned.items,sizeof(*source),count,stdout)!=count)return 1;
            rf_physics_spheres_close(&owned);rf_physics_spheres_close(&owned);
            if(memcmp(&owned,&empty,sizeof(owned)))return 3;
        }
        return ferror(stdin)?1:0;
    }
    while(fread(in,sizeof(in),1,stdin)==1) {
        memset(&out,0xa5,sizeof(out));
        out.status=rf_physics_fallback_prepare(in[0],in[1],in[2],&out.value);
        if(fwrite(&out,sizeof(out),1,stdout)!=1)return 1;
    }
    return ferror(stdin)?1:0;
}
