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
