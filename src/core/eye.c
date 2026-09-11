#include "rf/eye.h"
#include "rf/timer.h"
#include <math.h>
#include <string.h>
static int crouched(int32_t state) { return state >= 8 && state <= 10; }
static float spawn_atan(float y,float x)
{
    long double angle;
    if(x==0)return y==0?0:y>0?1.5707963705062866211f:-1.5707963705062866211f;
#if defined(__i386__) && !defined(_MSC_VER)
    __asm__ volatile("flds %1; fdivs %2; fld1; fpatan; fstpt %0"
        :"=m"(angle):"m"(y),"m"(x):"st","st(1)");
#else
    angle=atanl((long double)y/x);
#endif
    if(x<0)angle+=(long double)3.1415927410125732422f;
    return (float)angle;
}
static long double spawn_trig(float angle,int cosine)
{
#if defined(__i386__) && !defined(_MSC_VER)
    long double value;
    if(cosine) __asm__ volatile("flds %1; fcos; fstpt %0":"=m"(value):"m"(angle):"st");
    else __asm__ volatile("flds %1; fsin; fstpt %0":"=m"(value):"m"(angle):"st");
    return value;
#else
    return cosine?cosl(angle):sinl(angle);
#endif
}
int rf_look_spawn_angles(const float orientation[9],const float physics_orientation[9],
    const uint32_t rotation[3],rf_spawn_look_angles *result)
{
    rf_spawn_look_angles value={{0},{0}};float angles[3],c,denominator,a,b;long double s;
    unsigned i;
#if defined(__i386__) && !defined(_MSC_VER)
    unsigned short saved,control;
#endif
    if(!orientation || !physics_orientation || !rotation || !result)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(orientation[i]) || !isfinite(physics_orientation[i]))return RF_FORMAT;
#if defined(__i386__) && !defined(_MSC_VER)
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
#endif
    angles[1]=spawn_atan(orientation[6],orientation[8]);
    s=spawn_trig(angles[1],0);c=(float)spawn_trig(angles[1],1);
    denominator=(float)(fabsl(s)>fabsf(c)?s*orientation[6]:(long double)c*orientation[8]);
    angles[0]=spawn_atan(-orientation[7],denominator);
    a=denominator?orientation[1]/denominator:0;
    b=denominator?orientation[4]/denominator:0;
    angles[2]=spawn_atan(a,b);
    value.body[1]=angles[1];
    for(i=0;i<3;++i)if(rotation[i]==1)value.eye[i]=(float)(
        ((long double)physics_orientation[i*3+2]*angles[2]+
         (long double)physics_orientation[i*3+1]*angles[1])+
         (long double)physics_orientation[i*3]*angles[0]);
#if defined(__i386__) && !defined(_MSC_VER)
    __asm__ volatile("fldcw %0"::"m"(saved));
#endif
    for(i=0;i<3;++i)if(!isfinite(value.body[i]) || !isfinite(value.eye[i]))return RF_RANGE;
    *result=value;return RF_OK;
}
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

int rf_camera_effect_start(rf_camera_effect_state *state,float strength,float duration,int32_t now_ms)
{
    rf_camera_effect_state value;double milliseconds;int status;
    if(!state || !isfinite(strength) || !isfinite(duration))return RF_RANGE;
    milliseconds=(double)duration*1000.0;
    if(milliseconds < -RF_TIMER_PERIOD || milliseconds > RF_TIMER_PERIOD)return RF_RANGE;
    value.strength=strength;value.duration=duration;
    status=rf_timer_set(&value.deadline,now_ms,(int32_t)milliseconds);if(status)return status;
    *state=value;return RF_OK;
}
int rf_camera_effect_reset(rf_camera_effect_state *state,int32_t now_ms)
{
    rf_camera_effect_state value={0};int status;
    if(!state)return RF_RANGE;
    status=rf_timer_set(&value.deadline,now_ms,0);if(status)return status;
    *state=value;return RF_OK;
}
int rf_camera_effect_step(rf_camera_effect_state *state,int32_t now_ms,float *cosine,uint32_t *active)
{
    rf_camera_effect_state value;float cone;int expired,status;int32_t remaining;
    if(!state || !cosine || !active)return RF_RANGE;
    if(!isfinite(state->strength) || !isfinite(state->duration))return RF_FORMAT;
    status=rf_timer_expired(state->deadline,now_ms,&expired);if(status)return status;
    if(expired){*active=0;return RF_OK;}
    value=*state;cone=1.0f-value.strength;
    status=rf_timer_remaining(value.deadline,now_ms,&remaining);if(status)return status;
    if(remaining<1000)value.strength=(float)((double)value.strength*0.6002401113510132f);
    if(cone < -1)cone=-1;if(cone > 1)cone=1;
    *state=value;*cosine=cone;*active=1;return RF_OK;
}

static void camera_normalize(float normal[3])
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    unsigned short saved,control;
#if defined(_MSC_VER)
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,normal
        fld dword ptr [ecx]
        fmul dword ptr [ecx]
        fld dword ptr [ecx+4]
        fmul dword ptr [ecx+4]
        faddp st(1),st(0)
        fld dword ptr [ecx+8]
        fmul dword ptr [ecx+8]
        faddp st(1),st(0)
        fsqrt
        fld1
        fdivrp st(1),st(0)
        fld st(0)
        fmul dword ptr [ecx]
        fstp dword ptr [ecx]
        fld st(0)
        fmul dword ptr [ecx+4]
        fstp dword ptr [ecx+4]
        fmul dword ptr [ecx+8]
        fstp dword ptr [ecx+8]
        fldcw saved
    }
#else
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile(".intel_syntax noprefix\n\t"
        "fld dword ptr [ecx]\n\t"
        "fmul dword ptr [ecx]\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fmul dword ptr [ecx+4]\n\t"
        "faddp st(1),st(0)\n\t"
        "fld dword ptr [ecx+8]\n\t"
        "fmul dword ptr [ecx+8]\n\t"
        "faddp st(1),st(0)\n\t"
        "fsqrt\n\t"
        "fld1\n\t"
        "fdivrp st(1),st(0)\n\t"
        "fld st(0)\n\t"
        "fmul dword ptr [ecx]\n\t"
        "fstp dword ptr [ecx]\n\t"
        "fld st(0)\n\t"
        "fmul dword ptr [ecx+4]\n\t"
        "fstp dword ptr [ecx+4]\n\t"
        "fmul dword ptr [ecx+8]\n\t"
        "fstp dword ptr [ecx+8]\n\t"
        ".att_syntax prefix"::"c"(normal):"memory","st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#endif
#else
    long double r=1/sqrtl((long double)normal[0]*normal[0]+(long double)normal[1]*normal[1]+(long double)normal[2]*normal[2]);
    unsigned i;for(i=0;i<3;++i)normal[i]=(float)(normal[i]*r);
#endif
}

typedef struct camera_cone_terms {int32_t first,second;float minimum,scale,one,tau,radius;} camera_cone_terms;

static void camera_cone(camera_cone_terms *terms,float output[3])
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    unsigned short saved,control;
#if defined(_MSC_VER)
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,terms
        mov edx,output
        fild dword ptr [ecx]
        fmul dword ptr [ecx+12]
        fld dword ptr [ecx+16]
        fsub dword ptr [ecx+8]
        fmulp st(1),st(0)
        fadd dword ptr [ecx+8]
        fstp dword ptr [edx+8]
        fild dword ptr [ecx+4]
        fmul dword ptr [ecx+12]
        fmul dword ptr [ecx+20]
        fld dword ptr [edx+8]
        fmul dword ptr [edx+8]
        fsubr dword ptr [ecx+16]
        fsqrt
        fstp dword ptr [ecx+24]
        fld st(0)
        fcos
        fmul dword ptr [ecx+24]
        fstp dword ptr [edx]
        fsin
        fmul dword ptr [ecx+24]
        fstp dword ptr [edx+4]
        fldcw saved
    }
#else
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile(".intel_syntax noprefix\n\t"
        "fild dword ptr [ecx]\n\t"
        "fmul dword ptr [ecx+12]\n\t"
        "fld dword ptr [ecx+16]\n\t"
        "fsub dword ptr [ecx+8]\n\t"
        "fmulp st(1),st(0)\n\t"
        "fadd dword ptr [ecx+8]\n\t"
        "fstp dword ptr [edx+8]\n\t"
        "fild dword ptr [ecx+4]\n\t"
        "fmul dword ptr [ecx+12]\n\t"
        "fmul dword ptr [ecx+20]\n\t"
        "fld dword ptr [edx+8]\n\t"
        "fmul dword ptr [edx+8]\n\t"
        "fsubr dword ptr [ecx+16]\n\t"
        "fsqrt\n\t"
        "fstp dword ptr [ecx+24]\n\t"
        "fld st(0)\n\t"
        "fcos\n\t"
        "fmul dword ptr [ecx+24]\n\t"
        "fstp dword ptr [edx]\n\t"
        "fsin\n\t"
        "fmul dword ptr [ecx+24]\n\t"
        "fstp dword ptr [edx+4]\n\t"
        ".att_syntax prefix"::"c"(terms),"d"(output):"memory","st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#endif
#else
    long double a=(long double)terms->second*terms->scale*terms->tau;
    output[2]=(float)((long double)terms->first*terms->scale*(1-(long double)terms->minimum)+terms->minimum);
    terms->radius=(float)sqrtl(1-(long double)output[2]*output[2]);output[0]=(float)(cosl(a)*terms->radius);output[1]=(float)(sinl(a)*terms->radius);
#endif
}

static void camera_cross(const float a[3],const float b[3],float out[3])
{
    uint32_t i;for(i=0;i<3;++i)out[i]=(float)((double)a[(i+1)%3]*b[(i+2)%3]-(double)a[(i+2)%3]*b[(i+1)%3]);
}
static int camera_nonzero(const float v[3]) {return v[0]!=0 || v[1]!=0 || v[2]!=0;}
int rf_camera_effect_apply(rf_camera_effect_state *state,int32_t now_ms,
    uint32_t draw0,uint32_t draw1,float orientation[9],uint32_t *active)
{
    rf_camera_effect_state next;float cosine=0,basis[3][3]={{0}},local[3],changed[3],rebuilt[3][3]={{0}};
    camera_cone_terms terms;uint32_t enabled,i;int status;
    if(!state || !orientation || !active)return RF_RANGE;
    next=*state;status=rf_camera_effect_step(&next,now_ms,&cosine,&enabled);if(status)return status;
    if(!enabled){*active=0;return RF_OK;}
    if(draw0>32767 || draw1>32767)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(orientation[i]))return RF_FORMAT;
    memcpy(basis[2],orientation+6,12);
    if(basis[2][0]<.0001f && basis[2][0]>-.0001f && basis[2][2]<.0001f && basis[2][2]>-.0001f) {
        basis[0][0]=1;basis[2][0]=basis[2][2]=0;basis[2][1]=orientation[7]<0?-1.0f:1.0f;basis[1][2]=-basis[2][1];
    } else {
        basis[0][0]=basis[2][2];basis[0][2]=-basis[2][0];camera_normalize(basis[0]);camera_cross(basis[2],basis[0],basis[1]);
    }
    terms.first=(int32_t)draw0;terms.second=(int32_t)draw1;terms.minimum=cosine;
    terms.scale=1.0f/32768.0f;terms.one=1;terms.tau=6.2831854820251465f;terms.radius=0;
    camera_cone(&terms,local);transform(local,basis,changed);
    if(!camera_nonzero(changed))return RF_FORMAT;
    memcpy(rebuilt[2],changed,12);camera_normalize(rebuilt[2]);
    if(camera_nonzero(orientation+3)) {memcpy(rebuilt[1],orientation+3,12);camera_normalize(rebuilt[1]);}
    else if(camera_nonzero(orientation))camera_cross(rebuilt[2],orientation,rebuilt[1]);
    else if(rebuilt[2][0]==0 && rebuilt[2][2]==0 && rebuilt[2][1]!=0)rebuilt[1][2]=1;
    else rebuilt[1][1]=1;
    camera_cross(rebuilt[1],rebuilt[2],rebuilt[0]);camera_cross(rebuilt[2],rebuilt[0],rebuilt[1]);
    for(i=0;i<9;++i)if(!isfinite(((float*)rebuilt)[i]))return RF_FORMAT;
    memcpy(orientation,rebuilt,36);*state=next;*active=1;return RF_OK;
}

int rf_camera_effect_apply_random(rf_camera_effect_state *state,int32_t now_ms,
    rf_random_state *random,float orientation[9],uint32_t *active)
{
    rf_random_state next;uint32_t first=0,second=0;int expired,status;
    if(!state || !random || !orientation || !active)return RF_RANGE;
    status=rf_timer_expired(state->deadline,now_ms,&expired);if(status)return status;
    next=*random;
    if(!expired) {
        status=rf_random_next(&next,&first);if(status)return status;
        status=rf_random_next(&next,&second);if(status)return status;
    }
    status=rf_camera_effect_apply(state,now_ms,first,second,orientation,active);if(status)return status;
    *random=next;return RF_OK;
}

int rf_look_update(rf_look_state *state,float angular_speed,float dt)
{
    rf_look_state v;float pitch_delta,yaw_delta;double yaw,pitch;unsigned i;
    const float tau=6.2831854820251465f,limit=6283.185546875f,half_pi=1.5707963705062866f;
    if(!state)return RF_RANGE;
    if(!isfinite(angular_speed) || !isfinite(dt) || dt<=0)return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(state->command[i]) ||
        !isfinite(state->body_angles[i]) || !isfinite(state->eye_angles[i]))return RF_FORMAT;
    if(!isfinite(state->pending_pitch) || !isfinite(state->pending_yaw))return RF_FORMAT;
    v=*state;
    pitch_delta=(float)((double)angular_speed*dt*v.command[0]+v.pending_pitch);
    yaw_delta=(float)((double)v.command[1]*angular_speed*dt+v.pending_yaw);
    v.angular_velocity[0]=pitch_delta/dt;v.angular_velocity[1]=yaw_delta/dt;v.angular_velocity[2]=0;
    if(!isfinite(pitch_delta) || !isfinite(yaw_delta) ||
       !isfinite(v.angular_velocity[0]) || !isfinite(v.angular_velocity[1]))return RF_RANGE;
    yaw=(double)yaw_delta+v.body_angles[1];pitch=(double)pitch_delta+v.eye_angles[0];
    v.body_angles[0]=v.body_angles[2]=0;v.eye_angles[1]=v.eye_angles[2]=0;
    v.eye_angles[0]=(float)pitch;
    if(pitch>half_pi)v.eye_angles[0]=half_pi;
    if(v.eye_angles[0]<-half_pi)v.eye_angles[0]=-half_pi;
    v.body_angles[1]=(yaw>limit || yaw<-limit)?0:(float)yaw;
    while(v.body_angles[1]>tau)v.body_angles[1]=(float)((double)v.body_angles[1]-tau);
    while(v.body_angles[1]<-tau)v.body_angles[1]=(float)((double)v.body_angles[1]+tau);
    memset(v.command,0,sizeof(v.command));v.pending_pitch=v.pending_yaw=0;
    *state=v;return RF_OK;
}

/* 4a0d70 keeps sin(pitch) extended while deriving horizontal weight. */
static void look_basis_seed(const float angles[3],float basis[9])
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    unsigned short saved,control;
#if defined(_MSC_VER)
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,angles
        mov edx,basis
        fld dword ptr [ecx+4]
        fcos
        fstp dword ptr [edx]
        fld dword ptr [ecx+4]
        fsin
        fchs
        fstp dword ptr [edx+8]
        fld dword ptr [ecx]
        fsin
        fst dword ptr [edx+28]
        fabs
        fld1
        fsubrp st(1),st(0)
        fld st(0)
        fmul dword ptr [edx+8]
        fchs
        fstp dword ptr [edx+24]
        fmul dword ptr [edx]
        fstp dword ptr [edx+32]
        fldcw saved
    }
#else
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile(".intel_syntax noprefix\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fcos\n\t"
        "fstp dword ptr [edx]\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fsin\n\t"
        "fchs\n\t"
        "fstp dword ptr [edx+8]\n\t"
        "fld dword ptr [ecx]\n\t"
        "fsin\n\t"
        "fst dword ptr [edx+28]\n\t"
        "fabs\n\t"
        "fld1\n\t"
        "fsubrp st(1),st(0)\n\t"
        "fld st(0)\n\t"
        "fmul dword ptr [edx+8]\n\t"
        "fchs\n\t"
        "fstp dword ptr [edx+24]\n\t"
        "fmul dword ptr [edx]\n\t"
        "fstp dword ptr [edx+32]\n\t"
        ".att_syntax prefix"::"c"(angles),"d"(basis):"memory","st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#endif
#else
    long double sine=sinl(angles[0]),h=1-fabsl(sine);
    basis[0]=(float)cosl(angles[1]);basis[2]=(float)-sinl(angles[1]);
    basis[7]=(float)sine;basis[6]=(float)(-h*basis[2]);basis[8]=(float)(h*basis[0]);
#endif
}
static void look_cross(const float *a,const float *b,float *result)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved,control;
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,a
        mov edx,b
        mov eax,result
        fld dword ptr [ecx+4]
        fmul dword ptr [edx+8]
        fld dword ptr [ecx+8]
        fmul dword ptr [edx+4]
        fsubp st(1),st(0)
        fstp dword ptr [eax+0]
        fld dword ptr [ecx+8]
        fmul dword ptr [edx+0]
        fld dword ptr [ecx+0]
        fmul dword ptr [edx+8]
        fsubp st(1),st(0)
        fstp dword ptr [eax+4]
        fld dword ptr [ecx+0]
        fmul dword ptr [edx+4]
        fld dword ptr [ecx+4]
        fmul dword ptr [edx+0]
        fsubp st(1),st(0)
        fstp dword ptr [eax+8]
        fldcw saved
    }
#elif defined(__i386__)
    unsigned short saved,control;
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile(".intel_syntax noprefix\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fmul dword ptr [edx+8]\n\t"
        "fld dword ptr [ecx+8]\n\t"
        "fmul dword ptr [edx+4]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fstp dword ptr [eax+0]\n\t"
        "fld dword ptr [ecx+8]\n\t"
        "fmul dword ptr [edx+0]\n\t"
        "fld dword ptr [ecx+0]\n\t"
        "fmul dword ptr [edx+8]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fstp dword ptr [eax+4]\n\t"
        "fld dword ptr [ecx+0]\n\t"
        "fmul dword ptr [edx+4]\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fmul dword ptr [edx+0]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fstp dword ptr [eax+8]\n\t"
        ".att_syntax prefix"::"c"(a),"d"(b),"a"(result):"memory","st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#else
    camera_cross(a,b,result);
#endif
}
int rf_look_orientation(const float angles[3],float orientation[9])
{
    float basis[9]={0},out[9]={0};unsigned i;
    if(!angles || !orientation)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(angles[i]))return RF_FORMAT;
    /* Original wrapped look domain; roll is ignored by 4a0d70. */
    if(fabsf(angles[0])>1.5707963705062866f || fabsf(angles[1])>6.2831854820251465f)return RF_RANGE;
    look_basis_seed(angles,basis);
    look_cross(basis+6,basis,basis+3);
    memcpy(out+6,basis+6,12);camera_normalize(out+6);
    memcpy(out+3,basis+3,12);camera_normalize(out+3);
    look_cross(out+3,out+6,out);look_cross(out+6,out,out+3);
    memcpy(orientation,out,sizeof(out));return RF_OK;
}

int rf_look_update_pose(const rf_look_state *state,float angular_speed,float dt,rf_look_pose *result)
{
    rf_look_pose value;float seed[9]={0},angles[3],s,c,z;unsigned i;int status;
    if(!state || !result)return RF_RANGE;
    value.state=*state;status=rf_look_update(&value.state,angular_speed,dt);if(status)return status;
    /* 4fbee0/4fbe40 with the body's zero pitch and roll after 49de50.
     * Keep the zero products to retain the original signed-zero results. */
    look_basis_seed(value.state.body_angles,seed);s=-seed[2];c=seed[0];z=value.state.body_angles[0];
    value.body_orientation[0]=(float)((double)z*s*z+c);
    value.body_orientation[5]=(float)((double)c*z+(double)z*s);
    value.body_orientation[3]=(float)((double)s*z-(double)z*c);
    value.body_orientation[2]=(float)((double)z*c*z-s);
    value.body_orientation[6]=s;value.body_orientation[1]=z;value.body_orientation[4]=1;
    value.body_orientation[8]=c;value.body_orientation[7]=-z;
    for(i=0;i<3;++i)angles[i]=value.state.body_angles[i]+value.state.eye_angles[i];
    status=rf_look_orientation(angles,value.eye_orientation);if(status)return status;
    *result=value;return RF_OK;
}
