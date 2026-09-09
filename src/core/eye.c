#include "rf/eye.h"
#include <math.h>
#include <string.h>
static int crouched(int32_t state) { return state >= 8 && state <= 10; }
static void transform(const float offset[3], const float matrix[3][3], float out[3])
{
    unsigned i;
    /* Original x87 order is z product + y product + x product, rounded on store. */
    for (i = 0; i < 3; ++i) out[i] = (float)(((double)offset[2]*matrix[2][i] +
        (double)offset[1]*matrix[1][i]) + (double)offset[0]*matrix[0][i]);
}
int rf_eye_position(const rf_eye_input *in, float result[3])
{
    float offset[3], standing[3], crouching[3], t;
    unsigned i, j;
    int current;
    if (!in || !result) return RF_RANGE;
    for (i = 0; i < 3; ++i) {
        if (!isfinite(in->position[i]) || !isfinite(in->standing_offset[i]) || !isfinite(in->crouching_offset[i])) return RF_FORMAT;
        for (j = 0; j < 3; ++j) if (!isfinite(in->orientation[i][j])) return RF_FORMAT;
    }
    if (!isfinite(in->transition_duration) || !isfinite(in->transition_elapsed)) return RF_FORMAT;
    if (in->eye_tag == -1 || (in->flags & 0x20)) { memcpy(result, in->position, 12); return RF_OK; }
    if (in->flags & 0x40) return RF_NOT_FOUND;
    current = crouched(in->current_state);
    if (in->transition_duration <= 0 || current == crouched(in->previous_state)) {
        transform(current ? in->crouching_offset : in->standing_offset, in->orientation, offset);
    } else {
        t = in->transition_elapsed / in->transition_duration;
        if (current) t = 1.0f-t;
        transform(in->standing_offset, in->orientation, standing);
        transform(in->crouching_offset, in->orientation, crouching);
        for (i = 0; i < 3; ++i) {
            float a = standing[i]*(1.0f-t), b = crouching[i]*t;
            offset[i] = a+b;
        }
    }
    for (i = 0; i < 3; ++i) { offset[i] += in->position[i]; if (!isfinite(offset[i])) return RF_RANGE; }
    memcpy(result, offset, sizeof(offset));
    return RF_OK;
}

int rf_first_person_pose_copy(const float eye[3],const float body_orientation[3][3],
    const float eye_orientation[3][3],rf_first_person_pose *result)
{
    rf_first_person_pose value;uint32_t i,j;
    if(!eye || !body_orientation || !eye_orientation || !result)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(eye[i]))return RF_FORMAT;
        for(j=0;j<3;++j)if(!isfinite(body_orientation[i][j]) || !isfinite(eye_orientation[i][j]))return RF_FORMAT;
    }
    memcpy(value.position,eye,12);memcpy(value.body_orientation,body_orientation,36);
    memcpy(value.eye_orientation,eye_orientation,36);*result=value;return RF_OK;
}
