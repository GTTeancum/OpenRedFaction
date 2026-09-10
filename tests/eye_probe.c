#include "rf/eye.h"
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    rf_eye_input input;
    struct { int32_t status; float position[3]; } output;
    _Static_assert(sizeof(input) == 96, "Probe wire layout");
    _setmode(_fileno(stdin), _O_BINARY); _setmode(_fileno(stdout), _O_BINARY);
    if(argc==2 && !strcmp(argv[1],"--look-pose")) {
        struct {rf_look_state state;float speed,dt;} in;
        struct {int32_t status;rf_look_pose pose;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            memset(&out,0,sizeof(out));out.status=rf_look_update_pose(&in.state,in.speed,in.dt,&out.pose);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--look-matrix")) {
        float angles[3];struct {int32_t status;float matrix[9];} out;
        while(fread(angles,sizeof(angles),1,stdin)==1) {
            memset(&out,0,sizeof(out));out.status=rf_look_orientation(angles,out.matrix);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--look")) {
        struct {rf_look_state state;float speed,dt;} in;
        struct {int32_t status;rf_look_state state;} out;
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;out.status=rf_look_update(&out.state,in.speed,in.dt);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--effect-random")) {
        struct {rf_camera_effect_state state;int32_t now;rf_random_state random;float orientation[9];} in;
        struct {int32_t status;rf_camera_effect_state state;float orientation[9];uint32_t active;rf_random_state random;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;out.random=in.random;memcpy(out.orientation,in.orientation,36);out.active=99;
            out.status=rf_camera_effect_apply_random(&out.state,in.now,&out.random,out.orientation,&out.active);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--effect-apply")) {
        struct {rf_camera_effect_state state;int32_t now;uint32_t draws[2];float orientation[9];} in;
        struct {int32_t status;rf_camera_effect_state state;float orientation[9];uint32_t active;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;memcpy(out.orientation,in.orientation,36);out.active=99;
            out.status=rf_camera_effect_apply(&out.state,in.now,in.draws[0],in.draws[1],out.orientation,&out.active);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--effect")) {
        struct {rf_camera_effect_state state;int32_t now;uint32_t reset;} in;
        struct {int32_t status;rf_camera_effect_state state;float cosine;uint32_t active;} out;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(&in,sizeof(in),1,stdin)==1) {
            out.state=in.state;out.cosine=17;out.active=99;
            out.status=in.reset?rf_camera_effect_reset(&out.state,in.now):rf_camera_effect_step(&out.state,in.now,&out.cosine,&out.active);
            if(fwrite(&out,sizeof(out),1,stdout)!=1)return 2;
        }
        return ferror(stdin)?1:0;
    }
    if(argc==2 && !strcmp(argv[1],"--camera-pose")) {
        rf_first_person_pose pose;struct {int32_t status;rf_first_person_pose pose;} value;
        _Static_assert(sizeof(pose)==84,"Camera pose wire layout");
        while(fread(&pose,sizeof(pose),1,stdin)==1) {
            memset(&value,0xa5,sizeof(value));
            value.status=rf_first_person_pose_copy(pose.position,pose.body_orientation,pose.eye_orientation,&value.pose);
            if(fwrite(&value,sizeof(value),1,stdout)!=1)return 1;
        }
        return ferror(stdin)?1:0;
    }
    if(argc!=1)return 2;
    while (fread(&input, sizeof(input), 1, stdin) == 1) {
        output.position[0] = output.position[1] = output.position[2] = 0;
        output.status = rf_eye_position(&input, output.position);
        if (fwrite(&output, sizeof(output), 1, stdout) != 1) return 1;
    }
    return ferror(stdin) ? 1 : 0;
}
