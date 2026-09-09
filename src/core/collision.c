#include "rf/collision.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Original 4cf500 arithmetic: squared spans, initial radius, expansion,
 * and 46b075 origin radius. state = radius, radius_squared, center[3], origin_radius. */
static void sphere_math(const float v[3],const float point[3],float state[6],int mode)
{
#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__i386__)
    unsigned short saved,control;const float half=.5f;
#if defined(_MSC_VER)
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,v
        mov edx,point
        mov esi,state
        mov edi,mode
        lea ebx,half
        fld dword ptr [ecx]
        fmul dword ptr [ecx]
        fld dword ptr [ecx+4]
        fmul dword ptr [ecx+4]
        faddp st(1),st(0)
        fld dword ptr [ecx+8]
        fmul dword ptr [ecx+8]
        faddp st(1),st(0)
        cmp edi,2
        je sm_expand
        cmp edi,3
        je sm_finish
        fst dword ptr [esi+4]
        cmp edi,0
        je sm_pop
        fsqrt
        fstp dword ptr [esi]
        jmp sm_done
        sm_finish:
        fsqrt
        fadd dword ptr [esi]
        fstp dword ptr [esi+20]
        jmp sm_done
        sm_expand:
        fcom dword ptr [esi+4]
        fnstsw ax
        test ah,0x41
        jne sm_pop
        fsqrt
        fld st(0)
        fadd dword ptr [esi]
        fmul dword ptr [ebx]
        fst dword ptr [esi]
        fmul dword ptr [esi]
        fstp dword ptr [esi+4]
        fld st(0)
        fsub dword ptr [esi]
        fld st(0)
        fmul dword ptr [edx+0]
        fld dword ptr [esi+8]
        fmul dword ptr [esi]
        faddp st(1),st(0)
        fdiv st(0),st(2)
        fstp dword ptr [esi+8]
        fld st(0)
        fmul dword ptr [edx+4]
        fld dword ptr [esi+12]
        fmul dword ptr [esi]
        faddp st(1),st(0)
        fdiv st(0),st(2)
        fstp dword ptr [esi+12]
        fld st(0)
        fmul dword ptr [edx+8]
        fld dword ptr [esi+16]
        fmul dword ptr [esi]
        faddp st(1),st(0)
        fdiv st(0),st(2)
        fstp dword ptr [esi+16]
        fstp st(0)
        sm_pop:
        fstp st(0)
        sm_done:
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
        "cmp edi,2\n\t"
        "je .Lsm_expand%=\n\t"
        "cmp edi,3\n\t"
        "je .Lsm_finish%=\n\t"
        "fst dword ptr [esi+4]\n\t"
        "cmp edi,0\n\t"
        "je .Lsm_pop%=\n\t"
        "fsqrt\n\t"
        "fstp dword ptr [esi]\n\t"
        "jmp .Lsm_done%=\n\t"
        ".Lsm_finish%=:\n\t"
        "fsqrt\n\t"
        "fadd dword ptr [esi]\n\t"
        "fstp dword ptr [esi+20]\n\t"
        "jmp .Lsm_done%=\n\t"
        ".Lsm_expand%=:\n\t"
        "fcom dword ptr [esi+4]\n\t"
        "fnstsw ax\n\t"
        "test ah,0x41\n\t"
        "jne .Lsm_pop%=\n\t"
        "fsqrt\n\t"
        "fld st(0)\n\t"
        "fadd dword ptr [esi]\n\t"
        "fmul dword ptr [ebx]\n\t"
        "fst dword ptr [esi]\n\t"
        "fmul dword ptr [esi]\n\t"
        "fstp dword ptr [esi+4]\n\t"
        "fld st(0)\n\t"
        "fsub dword ptr [esi]\n\t"
        "fld st(0)\n\t"
        "fmul dword ptr [edx+0]\n\t"
        "fld dword ptr [esi+8]\n\t"
        "fmul dword ptr [esi]\n\t"
        "faddp st(1),st(0)\n\t"
        "fdiv st(0),st(2)\n\t"
        "fstp dword ptr [esi+8]\n\t"
        "fld st(0)\n\t"
        "fmul dword ptr [edx+4]\n\t"
        "fld dword ptr [esi+12]\n\t"
        "fmul dword ptr [esi]\n\t"
        "faddp st(1),st(0)\n\t"
        "fdiv st(0),st(2)\n\t"
        "fstp dword ptr [esi+12]\n\t"
        "fld st(0)\n\t"
        "fmul dword ptr [edx+8]\n\t"
        "fld dword ptr [esi+16]\n\t"
        "fmul dword ptr [esi]\n\t"
        "faddp st(1),st(0)\n\t"
        "fdiv st(0),st(2)\n\t"
        "fstp dword ptr [esi+16]\n\t"
        "fstp st(0)\n\t"
        ".Lsm_pop%=:\n\t"
        "fstp st(0)\n\t"
        ".Lsm_done%=:\n\t"
        ".att_syntax prefix"
        ::"c"(v),"d"(point),"S"(state),"D"(mode),"b"(&half):"eax","cc","memory","st","st(1)","st(2)","st(3)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#endif
#else
    long double squared=((long double)v[0]*v[0]+(long double)v[1]*v[1])+(long double)v[2]*v[2];
    if(mode==3)state[5]=(float)(sqrtl(squared)+state[0]);
    else if(mode<2) {state[1]=(float)squared;if(mode==1)state[0]=(float)sqrtl(squared);}
    else if(squared>state[1]) {
        long double distance=sqrtl(squared),radius=(distance+state[0])*.5L,gap;unsigned j;
        state[0]=(float)radius;state[1]=(float)(radius*state[0]);gap=distance-state[0];
        for(j=0;j<3;j++)state[j+2]=(float)(((long double)state[j+2]*state[0]+gap*point[j])/distance);
    }
#endif
}
int rf_collision_vertex_bounds(const float (*vertices)[3],uint32_t count,rf_collision_bounds *result)
{
    rf_collision_bounds value;uint32_t low[3]={0},high[3]={0},i,j,axis=0;
    float span[3],diff[3],state[6]={0};
    if(!result || (count && !vertices))return RF_RANGE;
    if(!count)return RF_NOT_FOUND;
    for(i=0;i<count;i++)for(j=0;j<3;j++) {
        if(!isfinite(vertices[i][j]))return RF_FORMAT;
        if(vertices[i][j]<vertices[low[j]][j])low[j]=i;
        if(vertices[i][j]>vertices[high[j]][j])high[j]=i;
    }
    for(j=0;j<3;j++) {
        value.minimum[j]=vertices[low[j]][j]-.0001f;value.maximum[j]=vertices[high[j]][j]+.0001f;
        for(i=0;i<3;i++)diff[i]=vertices[high[j]][i]-vertices[low[j]][i];
        sphere_math(diff,NULL,state,0);span[j]=state[1];if(span[j]>span[axis])axis=j;
    }
    for(j=0;j<3;j++) {
        volatile float sum=vertices[low[axis]][j]+vertices[high[axis]][j];
        state[j+2]=sum*.5f;diff[j]=vertices[high[axis]][j]-state[j+2];
    }
    sphere_math(diff,NULL,state,1);
    for(i=0;i<count;i++) {
        for(j=0;j<3;j++)diff[j]=vertices[i][j]-state[j+2];
        sphere_math(diff,vertices[i],state,2);
        for(j=0;j<5;j++)if(!isfinite(state[j]))return RF_FORMAT;
    }
    sphere_math(state+2,NULL,state,3);
    if(!isfinite(state[5]))return RF_FORMAT;
    value.radius=state[0];memcpy(value.center,state+2,12);value.origin_radius=state[5];
    *result=value;return RF_OK;
}

/* Original edge quadratic, retaining x87 intermediates through sqrt/division.
 * terms: o.e, o.d, o.o, e.d, e.e, d.d, radius, A, B. */
static int edge_roots(float terms[9],float roots[2],int endpoint)
{
    unsigned short saved,control;int status;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,terms
        mov edx,roots
        mov esi,endpoint
        cmp esi,0
        jne alg_point
        fld dword ptr [ecx+12]
        fmul dword ptr [ecx+12]
        fld dword ptr [ecx+20]
        fmul dword ptr [ecx+16]
        fsubp st(1),st(0)
        fst dword ptr [ecx+28]
        ftst
        fnstsw ax
        test ah,0x40
        fstp st(0)
        jnz alg_parallel
        fld dword ptr [ecx+12]
        fmul dword ptr [ecx]
        fld dword ptr [ecx+16]
        fmul dword ptr [ecx+4]
        fsubp st(1),st(0)
        fadd st(0),st(0)
        fstp dword ptr [ecx+32]
        fld dword ptr [ecx+32]
        fld st(0)
        fmul st(0),st(0)
        fld dword ptr [ecx+16]
        fmul dword ptr [ecx+24]
        fmul dword ptr [ecx+24]
        fld dword ptr [ecx]
        fmul dword ptr [ecx]
        faddp st(1),st(0)
        fld dword ptr [ecx+16]
        fmul dword ptr [ecx+8]
        fsubp st(1),st(0)
        fmul dword ptr [ecx+28]
        fadd st(0),st(0)
        fadd st(0),st(0)
        fsubp st(1),st(0)
        jmp alg_disc
        alg_point:
        fld dword ptr [ecx+4]
        fadd st(0),st(0)
        fld st(0)
        fmul st(0),st(0)
        fld dword ptr [ecx+24]
        fmul dword ptr [ecx+24]
        fsubr dword ptr [ecx+8]
        fmul dword ptr [ecx+20]
        fadd st(0),st(0)
        fadd st(0),st(0)
        fsubp st(1),st(0)
        alg_disc:
        ftst
        fnstsw ax
        test ah,0x41
        jnz alg_miss
        fsqrt
        cmp esi,0
        jne alg_point_den
        fld dword ptr [ecx+28]
        jmp alg_den
        alg_point_den:
        fld dword ptr [ecx+20]
        alg_den:
        fadd st(0),st(0)
        fld st(1)
        fsub st(0),st(3)
        fdiv st(0),st(1)
        fstp dword ptr [edx]
        fld st(2)
        fchs
        fsub st(0),st(2)
        fdiv st(0),st(1)
        fstp dword ptr [edx+4]
        fstp st(0)
        fstp st(0)
        fstp st(0)
        mov eax,1
        jmp alg_done
        alg_miss:
        fstp st(0)
        fstp st(0)
        xor eax,eax
        jmp alg_done
        alg_parallel:
        mov eax,2
        alg_done:
        mov status,eax
        fldcw saved
    }
#elif defined(__i386__)
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile(".intel_syntax noprefix\n\t"
        "cmp esi,0\n\t"
        "jne alg_point_%=\n\t"
        "fld dword ptr [ecx+12]\n\t"
        "fmul dword ptr [ecx+12]\n\t"
        "fld dword ptr [ecx+20]\n\t"
        "fmul dword ptr [ecx+16]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fst dword ptr [ecx+28]\n\t"
        "ftst\n\t"
        "fnstsw ax\n\t"
        "test ah,0x40\n\t"
        "fstp st(0)\n\t"
        "jnz alg_parallel_%=\n\t"
        "fld dword ptr [ecx+12]\n\t"
        "fmul dword ptr [ecx]\n\t"
        "fld dword ptr [ecx+16]\n\t"
        "fmul dword ptr [ecx+4]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fadd st(0),st(0)\n\t"
        "fstp dword ptr [ecx+32]\n\t"
        "fld dword ptr [ecx+32]\n\t"
        "fld st(0)\n\t"
        "fmul st(0),st(0)\n\t"
        "fld dword ptr [ecx+16]\n\t"
        "fmul dword ptr [ecx+24]\n\t"
        "fmul dword ptr [ecx+24]\n\t"
        "fld dword ptr [ecx]\n\t"
        "fmul dword ptr [ecx]\n\t"
        "faddp st(1),st(0)\n\t"
        "fld dword ptr [ecx+16]\n\t"
        "fmul dword ptr [ecx+8]\n\t"
        "fsubp st(1),st(0)\n\t"
        "fmul dword ptr [ecx+28]\n\t"
        "fadd st(0),st(0)\n\t"
        "fadd st(0),st(0)\n\t"
        "fsubp st(1),st(0)\n\t"
        "jmp alg_disc_%=\n\t"
        "alg_point_%=:\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fadd st(0),st(0)\n\t"
        "fld st(0)\n\t"
        "fmul st(0),st(0)\n\t"
        "fld dword ptr [ecx+24]\n\t"
        "fmul dword ptr [ecx+24]\n\t"
        "fsubr dword ptr [ecx+8]\n\t"
        "fmul dword ptr [ecx+20]\n\t"
        "fadd st(0),st(0)\n\t"
        "fadd st(0),st(0)\n\t"
        "fsubp st(1),st(0)\n\t"
        "alg_disc_%=:\n\t"
        "ftst\n\t"
        "fnstsw ax\n\t"
        "test ah,0x41\n\t"
        "jnz alg_miss_%=\n\t"
        "fsqrt\n\t"
        "cmp esi,0\n\t"
        "jne alg_point_den_%=\n\t"
        "fld dword ptr [ecx+28]\n\t"
        "jmp alg_den_%=\n\t"
        "alg_point_den_%=:\n\t"
        "fld dword ptr [ecx+20]\n\t"
        "alg_den_%=:\n\t"
        "fadd st(0),st(0)\n\t"
        "fld st(1)\n\t"
        "fsub st(0),st(3)\n\t"
        "fdiv st(0),st(1)\n\t"
        "fstp dword ptr [edx]\n\t"
        "fld st(2)\n\t"
        "fchs\n\t"
        "fsub st(0),st(2)\n\t"
        "fdiv st(0),st(1)\n\t"
        "fstp dword ptr [edx+4]\n\t"
        "fstp st(0)\n\t"
        "fstp st(0)\n\t"
        "fstp st(0)\n\t"
        "mov eax,1\n\t"
        "jmp alg_done_%=\n\t"
        "alg_miss_%=:\n\t"
        "fstp st(0)\n\t"
        "fstp st(0)\n\t"
        "xor eax,eax\n\t"
        "jmp alg_done_%=\n\t"
        "alg_parallel_%=:\n\t"
        "mov eax,2\n\t"
        "alg_done_%=:\n\t"
        ".att_syntax prefix"
        :"=a"(status):"c"(terms),"d"(roots),"S"(endpoint):"cc","memory","st","st(1)","st(2)","st(3)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#else
    (void)saved;(void)control;
    {long double a,b,c,disc;
        if(endpoint) {a=terms[5];b=2*(long double)terms[1];c=(long double)terms[2]-(long double)terms[6]*terms[6];}
        else {
            terms[7]=(float)((long double)terms[3]*terms[3]-(long double)terms[5]*terms[4]);
            if(terms[7]==0)return 2;
            terms[8]=(float)(2*((long double)terms[3]*terms[0]-(long double)terms[4]*terms[1]));
            a=terms[7];b=terms[8];c=((long double)terms[4]*terms[6]*terms[6]+(long double)terms[0]*terms[0])-(long double)terms[4]*terms[2];
        }
        disc=b*b-4*a*c;if(!(disc>0))return 0;
        roots[0]=(float)((sqrtl(disc)-b)/(2*a));roots[1]=(float)((-b-sqrtl(disc))/(2*a));status=1;
    }
#endif
    return status;
}

static float edge_dot(const float *a,const float *b,float denominator,int *negative)
{
    float value;unsigned short comparison;
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved,control;
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,a
        mov edx,b
        lea esi,denominator
        lea edi,value
        fld dword ptr [ecx+8]
        fmul dword ptr [edx+8]
        fld dword ptr [ecx+4]
        fmul dword ptr [edx+4]
        faddp st(1),st(0)
        fld dword ptr [ecx]
        fmul dword ptr [edx]
        faddp st(1),st(0)
        fdiv dword ptr [esi]
        ftst
        fnstsw comparison
        fstp dword ptr [edi]
        fldcw saved
    }
#elif defined(__i386__)
    unsigned short saved,control;
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %0"::"m"(control));
    __asm__ volatile(".intel_syntax noprefix\n\t"
        "fld dword ptr [ecx+8]\n\t"
        "fmul dword ptr [edx+8]\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fmul dword ptr [edx+4]\n\t"
        "faddp st(1),st(0)\n\t"
        "fld dword ptr [ecx]\n\t"
        "fmul dword ptr [edx]\n\t"
        "faddp st(1),st(0)\n\t"
        "fdiv dword ptr [esi]\n\t"
        "ftst\n\t"
        "fnstsw ax\n\t"
        "fstp dword ptr [edi]\n\t"
        ".att_syntax prefix"
        :"=a"(comparison):"c"(a),"d"(b),"S"(&denominator),"D"(&value):"memory","st","st(1)");
    __asm__ volatile("fldcw %0"::"m"(saved));
#else
    {long double extended=(((long double)a[2]*b[2]+(long double)a[1]*b[1])+(long double)a[0]*b[0])/denominator;value=(float)extended;comparison=extended<0?0x100:0;}
#endif
    if(negative)*negative=(comparison&0x100u)!=0;
    return value;
}
/* 4df1c0..4df302: preserve endpoint stores before transforming displacement. */
int rf_collision_query_local(const float start[3],const float displacement[3],
    const float origin[3],const float matrix[3][3],uint32_t flags,
    float local_start[3],float local_displacement[3],uint32_t *active)
{
    float first[3],last[3],offset[3],endpoint[3];uint32_t i,j;
    if(!start || !displacement || !local_start || !local_displacement || !active)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(start[i]) || !isfinite(displacement[i]))return RF_FORMAT;
    if(displacement[0]==0 && displacement[1]==0 && displacement[2]==0) {*active=0;return RF_OK;}
    if(flags&4u) {memcpy(first,start,12);memcpy(last,displacement,12);}
    else {
        if(!origin || !matrix)return RF_RANGE;
        for(i=0;i<3;i++) {
            if(!isfinite(origin[i]))return RF_FORMAT;
            for(j=0;j<3;j++)if(!isfinite(matrix[i][j]))return RF_FORMAT;
            offset[i]=start[i]-origin[i];
            {volatile float end=start[i]+displacement[i];endpoint[i]=end-origin[i];}
            if(!isfinite(offset[i]) || !isfinite(endpoint[i]))return RF_FORMAT;
        }
        for(i=0;i<3;i++) {
            first[i]=edge_dot(offset,matrix[i],1,NULL);
            last[i]=edge_dot(endpoint,matrix[i],1,NULL);
            last[i]=last[i]-first[i];
            if(!isfinite(first[i]) || !isfinite(last[i]))return RF_FORMAT;
        }
    }
    memcpy(local_start,first,12);memcpy(local_displacement,last,12);*active=1;return RF_OK;
}

/* 498fb4..499011: output pose uses moving solid +e4/+fc, not +3c/+48. */
int rf_collision_contact_world(const rf_collision_ray_hit *local,const float origin[3],
    const float matrix[3][3],rf_collision_ray_hit *world)
{
    rf_collision_ray_hit value;float column[3];uint32_t i,j;
    if(!local || !origin || !matrix || !world)return RF_RANGE;
    if(!isfinite(local->fraction))return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(local->point[i]) || !isfinite(local->normal[i]) || !isfinite(origin[i]))return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(matrix[i][j]))return RF_FORMAT;
    }
    value.fraction=local->fraction;
    for(i=0;i<3;i++) {
        for(j=0;j<3;j++)column[j]=matrix[j][i];
        value.normal[i]=edge_dot(local->normal,column,1,NULL);
        {volatile float rotated=edge_dot(local->point,column,1,NULL);value.point[i]=origin[i]+rotated;}
        if(!isfinite(value.point[i]) || !isfinite(value.normal[i]))return RF_FORMAT;
    }
    *world=value;return RF_OK;
}

static float edge_length(const float *a)
{
    float reversed[3]={a[2],a[1],a[0]};return edge_dot(reversed,reversed,1,NULL);
}
int rf_collision_sphere_edge(const float start[3],const float delta[3],float radius,
    const float a[3],const float b[3],float limit,float *fraction,float point[3],uint32_t *hit)
{
    float e[3],o[3],terms[9],roots[2],time,contact[3];uint32_t j;int status;
    if(!start || !delta || !a || !b || !fraction || !point || !hit)return RF_RANGE;
    if(!isfinite(radius) || radius<0 || !isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    for(j=0;j<3;j++) {
        if(!isfinite(start[j]) || !isfinite(delta[j]) || !isfinite(a[j]) || !isfinite(b[j]))return RF_FORMAT;
        e[j]=b[j]-a[j];o[j]=start[j]-a[j];
    }
    terms[0]=edge_dot(o,e,1,NULL);terms[1]=edge_dot(o,delta,1,NULL);terms[2]=edge_length(o);
    terms[3]=edge_dot(e,delta,1,NULL);terms[4]=edge_length(e);terms[5]=edge_length(delta);terms[6]=radius;
    for(j=0;j<7;j++)if(!isfinite(terms[j]))return RF_FORMAT;
    status=edge_roots(terms,roots,0);
    if(!status)goto miss;
    if(status==1) {
        float later;time=roots[0];later=roots[1];if(later<time) {float swap=time;time=later;later=swap;}
        if(time>=-.05f) {
            if(time<0)time=1e-6f;
            if(time>1 || !(time<limit))goto miss;
            for(j=0;j<3;j++) {volatile float moved=delta[j]*time;volatile float center=start[j]+moved;o[j]=center-a[j];}
            {int negative;float along=edge_dot(o,e,terms[4],&negative);
                if(!negative && along<=1) {
                    for(j=0;j<3;j++) {volatile float offset=e[j]*along;contact[j]=a[j]+offset;}
                    goto accept;
                }
            }
        }
        else if(time>1 || later<0)goto miss;
    }
    status=edge_roots(terms,roots,1);if(!status)goto miss;
    time=roots[0]<roots[1]?roots[0]:roots[1];
    if(!(time>=0 && time<=1 && time<limit))goto miss;
    memcpy(contact,a,sizeof(contact));
 accept:
    *fraction=time;memcpy(point,contact,sizeof(contact));*hit=1;return RF_OK;
 miss:
    *hit=0;return RF_OK;
}



/* Preserve the original extended intermediates and single final float store. */
static float plane_component(float a,float b,float s,float e,float plane)
{
    float value;
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved,control;
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        fld e
        fsub s
        fld plane
        fsub s
        fld b
        fsub a
        fmul st(0),st(1)
        fdiv st(0),st(2)
        fadd a
        fstp value
        fstp st(0)
        fstp st(0)
        fldcw saved
    }
#elif defined(__i386__) || defined(__x86_64__)
    unsigned short saved,control;
    __asm__ volatile("fnstcw %0":"=m"(saved));
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %6\n\tflds %4\n\tfsubs %3\n\tflds %5\n\tfsubs %3\n\tflds %2\n\tfsubs %1\n\tfmul %%st(1),%%st\n\tfdiv %%st(2),%%st\n\tfadds %1\n\tfstps %0\n\tfstp %%st(0)\n\tfstp %%st(0)\n\tfldcw %7"
        :"=m"(value):"m"(a),"m"(b),"m"(s),"m"(e),"m"(plane),"m"(control),"m"(saved):"st","st(1)","st(2)");
#else
    value=(float)(((long double)b-a)*((long double)plane-s)/((long double)e-s)+a);
#endif
    return value;
}
static uint32_t outcode(const float lo[3],const float hi[3],const float p[3])
{
    return (p[0]>hi[0]?1u:p[0]<lo[0]?2u:0u) |
           (p[1]>hi[1]?4u:p[1]<lo[1]?8u:0u) |
           (p[2]>hi[2]?32u:p[2]<lo[2]?16u:0u);
}
int rf_collision_segment_box(const float minimum[3],const float maximum[3],
    const float start[3],const float end[3],float point[3],uint32_t *hit)
{
    uint32_t a,b,axis,j,valid;static const uint32_t masks[3]={3,12,48},high[3]={1,4,32};
    if(!minimum || !maximum || !start || !end || !point || !hit)return RF_RANGE;
    for(j=0;j<3;j++)if(!isfinite(minimum[j]) || !isfinite(maximum[j]) || !isfinite(start[j]) || !isfinite(end[j]) || minimum[j]>maximum[j])return RF_FORMAT;
    a=outcode(minimum,maximum,start);b=outcode(minimum,maximum,end);
    if(!a || !b) {memcpy(point,!a?start:end,12);*hit=1;return RF_OK;}
    *hit=0;if(a&b)return RF_OK;
    for(axis=0;axis<3;axis++)if(a&masks[axis]) {
        point[axis]=(a&high[axis])?maximum[axis]:minimum[axis];
        for(j=0;j<3;j++)if(j!=axis)point[j]=plane_component(start[j],end[j],start[axis],end[axis],point[axis]);
        valid=1;for(j=0;j<3;j++)if(j!=axis && !(point[j]<=maximum[j] && point[j]>=minimum[j]))valid=0;
        if(valid) {*hit=1;return RF_OK;}
    }
    return RF_OK;
}

int rf_collision_segment_plane(const float start[3],const float displacement[3],
    const float plane[4],float *fraction,uint32_t *hit)
{
    float distance,value=0;uint32_t accepted=0,j;
    if(!start || !displacement || !plane || !fraction || !hit)return RF_RANGE;
    for(j=0;j<4;j++)if(!isfinite(plane[j]))return RF_FORMAT;
    for(j=0;j<3;j++)if(!isfinite(start[j]) || !isfinite(displacement[j]))return RF_FORMAT;
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        unsigned short saved,control;
        __asm { fnstcw saved }
        control=(unsigned short)((saved&~0x0f00u)|0x0300u);
        __asm {
            fldcw control
            mov edx,plane
            mov ecx,start
            fld dword ptr [edx+8]
            fmul dword ptr [ecx+8]
            fld dword ptr [edx+4]
            fmul dword ptr [ecx+4]
            faddp st(1),st(0)
            fld dword ptr [edx]
            fmul dword ptr [ecx]
            faddp st(1),st(0)
            fadd dword ptr [edx+12]
            fst distance
            fldz
            fxch st(1)
            fcompp
            fnstsw ax
            test ah,1
            jnz plane_done
            mov ecx,displacement
            fld dword ptr [edx+8]
            fmul dword ptr [ecx+8]
            fld dword ptr [edx+4]
            fmul dword ptr [ecx+4]
            faddp st(1),st(0)
            fld dword ptr [edx]
            fmul dword ptr [ecx]
            faddp st(1),st(0)
            fchs
            fcom distance
            fnstsw ax
            test ah,1
            jnz plane_pop
            fld distance
            fdiv st(0),st(1)
            fstp value
            mov accepted,1
        plane_pop:
            fstp st(0)
        plane_done:
            fldcw saved
        }
    }
#elif defined(__i386__) || defined(__x86_64__)
    {
        unsigned short saved,control;
        __asm__ volatile("fnstcw %0":"=m"(saved));
        control=(unsigned short)((saved&~0x0f00u)|0x0300u);
        __asm__ volatile(
            "fldcw %[control]\n\t"
            "flds 8(%[p])\n\tfmuls 8(%[s])\n\tflds 4(%[p])\n\tfmuls 4(%[s])\n\tfaddp\n\tflds (%[p])\n\tfmuls (%[s])\n\tfaddp\n\tfadds 12(%[p])\n\tfsts %[distance]\n\tfldz\n\tfxch %%st(1)\n\tfcompp\n\tfnstsw %%ax\n\ttestb $1,%%ah\n\tjnz 2f\n\t"
            "flds 8(%[p])\n\tfmuls 8(%[d])\n\tflds 4(%[p])\n\tfmuls 4(%[d])\n\tfaddp\n\tflds (%[p])\n\tfmuls (%[d])\n\tfaddp\n\tfchs\n\tfcoms %[distance]\n\tfnstsw %%ax\n\ttestb $1,%%ah\n\tjnz 1f\n\t"
            "flds %[distance]\n\tfdiv %%st(1),%%st\n\tfstps %[value]\n\tmovl $1,%[accepted]\n1:\n\tfstp %%st(0)\n2:\n\tfldcw %[saved]"
            :[distance]"=m"(distance),[value]"+m"(value),[accepted]"+m"(accepted)
            :[s]"r"(start),[d]"r"(displacement),[p]"r"(plane),[control]"m"(control),[saved]"m"(saved)
            :"ax","cc","st","st(1)","memory");
    }
#else
    {
        long double a=((long double)plane[2]*start[2]+(long double)plane[1]*start[1])+(long double)plane[0]*start[0]+plane[3];
        long double b=-(((long double)plane[2]*displacement[2]+(long double)plane[1]*displacement[1])+(long double)plane[0]*displacement[0]);
        distance=(float)a;
        if(a>=0 && b>=distance) {value=(float)(distance/b);accepted=1;}
    }
#endif
    if(accepted)*fraction=value;
    *hit=accepted;return RF_OK;
}

int rf_collision_sphere_plane(const float start[3],const float displacement[3],
    float radius,const float plane[4],float *fraction,float point[3],uint32_t *hit)
{
    float approach=0,distance=0,value=0,contact[3];uint32_t accepted=0,j;
    if(!start || !displacement || !plane || !fraction || !point || !hit)return RF_RANGE;
    if(!isfinite(radius) || radius<0)return RF_FORMAT;
    for(j=0;j<4;j++)if(!isfinite(plane[j]))return RF_FORMAT;
    for(j=0;j<3;j++)if(!isfinite(start[j]) || !isfinite(displacement[j]))return RF_FORMAT;
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        unsigned short saved,control;
        __asm { fnstcw saved }
        control=(unsigned short)((saved&~0x0f00u)|0x0300u);
        __asm {
            fldcw control
            mov edx,plane
            mov ecx,displacement
            fld dword ptr [edx+8]
            fmul dword ptr [ecx+8]
            fld dword ptr [edx+4]
            fmul dword ptr [ecx+4]
            faddp st(1),st(0)
            fld dword ptr [edx]
            fmul dword ptr [ecx]
            faddp st(1),st(0)
            fchs
            fst approach
            fldz
            fxch st(1)
            fcompp
            fnstsw ax
            test ah,0x41
            jnz sphere_done
            mov ecx,start
            fld dword ptr [edx+8]
            fmul dword ptr [ecx+8]
            fld dword ptr [edx+4]
            fmul dword ptr [ecx+4]
            faddp st(1),st(0)
            fld dword ptr [edx]
            fmul dword ptr [ecx]
            faddp st(1),st(0)
            fadd dword ptr [edx+12]
            fst distance
            fldz
            fxch st(1)
            fcompp
            fnstsw ax
            test ah,1
            jnz sphere_done
            fld distance
            fcomp radius
            fnstsw ax
            test ah,1
            jnz sphere_overlap
            fld distance
            fsub radius
            fld approach
            fcomp st(1)
            fnstsw ax
            test ah,1
            jnz sphere_pop
            fdiv approach
            fstp value
        sphere_overlap:
            mov accepted,1
            jmp sphere_done
        sphere_pop:
            fstp st(0)
        sphere_done:
            fldcw saved
        }
    }
#elif defined(__i386__) || defined(__x86_64__)
    {
        unsigned short saved,control;
        __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
        __asm__ volatile(
            "fldcw %[control]\n\tflds 8(%[p])\n\tfmuls 8(%[d])\n\tflds 4(%[p])\n\tfmuls 4(%[d])\n\tfaddp\n\tflds (%[p])\n\tfmuls (%[d])\n\tfaddp\n\tfchs\n\tfsts %[approach]\n\tfldz\n\tfxch %%st(1)\n\tfcompp\n\tfnstsw %%ax\n\ttestb $0x41,%%ah\n\tjnz 3f\n\t"
            "flds 8(%[p])\n\tfmuls 8(%[s])\n\tflds 4(%[p])\n\tfmuls 4(%[s])\n\tfaddp\n\tflds (%[p])\n\tfmuls (%[s])\n\tfaddp\n\tfadds 12(%[p])\n\tfsts %[distance]\n\tfldz\n\tfxch %%st(1)\n\tfcompp\n\tfnstsw %%ax\n\ttestb $1,%%ah\n\tjnz 3f\n\t"
            "flds %[distance]\n\tfcomps %[radius]\n\tfnstsw %%ax\n\ttestb $1,%%ah\n\tjnz 1f\n\tflds %[distance]\n\tfsubs %[radius]\n\tflds %[approach]\n\tfcomp %%st(1)\n\tfnstsw %%ax\n\ttestb $1,%%ah\n\tjnz 2f\n\tfdivs %[approach]\n\tfstps %[value]\n1:\n\tmovl $1,%[accepted]\n\tjmp 3f\n2:\n\tfstp %%st(0)\n3:\n\tfldcw %[saved]"
            :[approach]"+m"(approach),[distance]"+m"(distance),[value]"+m"(value),[accepted]"+m"(accepted)
            :[p]"r"(plane),[s]"r"(start),[d]"r"(displacement),[radius]"m"(radius),[control]"m"(control),[saved]"m"(saved)
            :"ax","cc","st","st(1)","memory");
    }
#else
    {
        long double b=-(((long double)plane[2]*displacement[2]+(long double)plane[1]*displacement[1])+(long double)plane[0]*displacement[0]);
        long double a=((long double)plane[2]*start[2]+(long double)plane[1]*start[1])+(long double)plane[0]*start[0]+plane[3];
        approach=(float)b;distance=(float)a;
        if(b>0 && a>=0) {if(distance<radius)accepted=1;else if(approach>=(long double)distance-radius) {value=(float)(((long double)distance-radius)/approach);accepted=1;}}
    }
#endif
    if(accepted) {
        for(j=0;j<3;j++) {
            volatile float offset=plane[j]*(distance<radius?distance:radius);
            volatile float projected=start[j]-offset;
            if(distance<radius)contact[j]=projected;
            else {volatile float movement=displacement[j]*value;contact[j]=projected+movement;}
        }
        *fraction=value;memcpy(point,contact,sizeof(contact));
    }
    *hit=accepted;return RF_OK;
}

/* Compare against the extended edge intersection before any float store. */
static int edge_right(float px,float py,float x,float y,float previous_x,float previous_y)
{
    unsigned short flags;
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved,control;
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        fld previous_x
        fsub x
        fld py
        fsub y
        fmulp st(1),st(0)
        fld previous_y
        fsub y
        fdivp st(1),st(0)
        fadd x
        fcomp px
        fnstsw flags
        fldcw saved
    }
#elif defined(__i386__) || defined(__x86_64__)
    unsigned short saved,control;
    __asm__ volatile("fnstcw %0":"=m"(saved));
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %7\n\tflds %5\n\tfsubs %3\n\tflds %2\n\tfsubs %4\n\tfmulp\n\tflds %6\n\tfsubs %4\n\tfdivrp\n\tfadds %3\n\tfcomps %1\n\tfnstsw %0\n\tfldcw %8"
        :"=m"(flags):"m"(px),"m"(py),"m"(x),"m"(y),"m"(previous_x),"m"(previous_y),"m"(control),"m"(saved):"st","st(1)");
#else
    return (long double)px<(((long double)previous_x-x)*((long double)py-y))/((long double)previous_y-y)+x;
#endif
    return !(flags&0x4100u);
}
int rf_collision_polygon_contains(const float normal[3],const float point[3],
    const float (*vertices)[3],uint32_t count,uint32_t *inside)
{
    static const uint32_t axes[3][2]={{2,1},{0,2},{1,0}};
    uint32_t axis,x,y,i,j,result=0;float a,b,c,px,py,previous_x,previous_y;
    if(!normal || !point || !vertices || !inside || !count || count>65536)return RF_RANGE;
    for(j=0;j<3;j++)if(!isfinite(normal[j]) || !isfinite(point[j]))return RF_FORMAT;
    for(i=0;i<count;i++)for(j=0;j<3;j++)if(!isfinite(vertices[i][j]))return RF_FORMAT;
    a=fabsf(normal[0]);b=fabsf(normal[1]);c=fabsf(normal[2]);
    axis=a>b?(a>c?0:2):(b>c?1:2);
    x=axes[axis][normal[axis]>0?0:1];y=axes[axis][normal[axis]>0?1:0];
    px=point[x];py=point[y];previous_x=vertices[count-1][x];previous_y=vertices[count-1][y];
    for(i=0;i<count;i++) {
        float current_x=vertices[i][x],current_y=vertices[i][y];
        if((current_y>py)!=(previous_y>py))
            if(edge_right(px,py,current_x,current_y,previous_x,previous_y))result^=1;
        previous_x=current_x;previous_y=current_y;
    }
    *inside=result;return RF_OK;
}

int rf_collision_face_accept(const rf_collision_face_filter *filter,uint32_t *accepted)
{
    uint32_t q,f,value=0;
    if(!filter || !accepted || filter->property_34 < -32768 || filter->property_34 > 32767 ||
       filter->owner_present>1 || filter->owner_kind>255 || filter->owner_state>255)return RF_RANGE;
    q=filter->query_flags;f=filter->face_flags;
    if((q&0x20u) && (f&0x40u))goto done;
    if((q&0x40u) && (f&0x80u))goto done;
    if((q&0x400u) && filter->owner_present && filter->owner_kind==1 && !filter->owner_state)goto done;
    if(!(q&2u) && filter->property_34>0)goto done;
    if(!(q&0x1000u) && (f&4u))goto done;
    if(!(q&8u)) {
        if((f&0x2000u) && !(q&0x2000u))goto done;
        if((f&1u) && !(q&0x800u))goto done;
    }
    value=1;
 done:
    *accepted=value;return RF_OK;
}

int rf_collision_thin_face(const rf_collision_face *face,const float start[3],
    const float displacement[3],float limit,rf_collision_ray_hit *result,uint32_t *matched)
{
    rf_collision_ray_hit value;float end[3],box_point[3];uint32_t hit,j;int status;
    if(!face || !start || !displacement || !result || !matched)return RF_RANGE;
    if(!isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    status=rf_collision_face_accept(&face->filter,&hit);if(status)return status;
    if(!hit) {*matched=0;return RF_OK;}
    if(face->filter.query_flags&0x180u)return RF_NOT_FOUND;
    for(j=0;j<3;j++) {
        if(!isfinite(start[j]) || !isfinite(displacement[j]))return RF_FORMAT;
        end[j]=start[j]+displacement[j];
    }
    status=rf_collision_segment_box(face->minimum,face->maximum,start,end,box_point,&hit);if(status)return status;
    if(!hit) {*matched=0;return RF_OK;}
    status=rf_collision_segment_plane(start,displacement,face->plane,&value.fraction,&hit);if(status)return status;
    if(!hit) {*matched=0;return RF_OK;}
    if(!isfinite(value.fraction))return RF_FORMAT;
    for(j=0;j<3;j++) {volatile float scaled=displacement[j]*value.fraction;value.point[j]=start[j]+scaled;value.normal[j]=face->plane[j];}
    if(value.fraction>limit) {*matched=0;return RF_OK;}
    status=rf_collision_polygon_contains(face->plane,value.point,face->vertices,face->count,&hit);if(status)return status;
    if(hit)*result=value;
    *matched=hit;return RF_OK;
}

/* 4faaf0: reciprocal length remains extended through component stores. */
static void sweep_normalize(float normal[3])
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
    long double reciprocal=1/sqrtl(((long double)normal[0]*normal[0]+(long double)normal[1]*normal[1])+(long double)normal[2]*normal[2]);
    unsigned j;for(j=0;j<3;j++)normal[j]=(float)(normal[j]*reciprocal);
#endif
}

int rf_collision_sweep_face(const rf_collision_face *face,const float start[3],
    const float displacement[3],const float normal_displacement[3],float radius,
    float limit,rf_collision_sweep_hit *result,uint32_t *matched)
{
    rf_collision_sweep_hit value;float lo[3],hi[3],end[3],scratch[3];uint32_t hit,j,i;int status;
    if(!face || !start || !displacement || !normal_displacement || !result || !matched)return RF_RANGE;
    if(!isfinite(radius) || radius<0 || !isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    value.hits=0;value.edge=0;
    if(radius<.0001f) {
        status=rf_collision_thin_face(face,start,displacement,limit,&value.hit,&hit);if(status)return status;
        if(hit) {value.hits=1;*result=value;}*matched=hit;return RF_OK;
    }
    status=rf_collision_face_accept(&face->filter,&hit);if(status)return status;
    if(!hit)goto miss;
    if(face->filter.query_flags&0x180u)return RF_NOT_FOUND;
    for(j=0;j<3;j++) {
        if(!isfinite(start[j]) || !isfinite(displacement[j]) || !isfinite(normal_displacement[j]))return RF_FORMAT;
        end[j]=start[j]+displacement[j];lo[j]=face->minimum[j]-radius;hi[j]=face->maximum[j]+radius;
    }
    status=rf_collision_segment_box(lo,hi,start,end,scratch,&hit);if(status)return status;if(!hit)goto miss;
    status=rf_collision_sphere_plane(start,displacement,radius,face->plane,&value.hit.fraction,value.hit.point,&hit);if(status)return status;if(!hit)goto miss;
    if(value.hit.fraction>limit)goto miss;
    status=rf_collision_polygon_contains(face->plane,value.hit.point,face->vertices,face->count,&hit);if(status)return status;
    if(hit) {memcpy(value.hit.normal,face->plane,12);value.hits=1;goto accept;}
    for(j=0;j<3;j++) {
        lo[j]=(start[j]<end[j]?start[j]:end[j])-radius;
        hi[j]=(start[j]<end[j]?end[j]:start[j])+radius;
    }
    for(i=0;i<face->count;i++) {
        rf_collision_ray_hit candidate;
        const float *a=face->vertices[i],*b=face->vertices[(i+1)%face->count];
        status=rf_collision_segment_box(lo,hi,a,b,scratch,&hit);if(status)return status;if(!hit)continue;
        status=rf_collision_sphere_edge(start,displacement,radius,a,b,limit,&candidate.fraction,candidate.point,&hit);if(status)return status;if(!hit)continue;
        for(j=0;j<3;j++) {
            volatile float scaled=normal_displacement[j]*candidate.fraction;
            volatile float center=start[j]+scaled;candidate.normal[j]=center-candidate.point[j];
        }
        sweep_normalize(candidate.normal);
        for(j=0;j<3;j++)if(!isfinite(candidate.normal[j]))return RF_FORMAT;
        value.hit=candidate;value.edge=1;value.hits++;limit=candidate.fraction;
    }
    if(!value.hits)goto miss;
 accept:
    *result=value;*matched=1;return RF_OK;
 miss:
    *matched=0;return RF_OK;
}

int rf_collision_thin_tree(const rf_collision_node *nodes,uint32_t node_count,
    const rf_collision_face *faces,uint32_t face_count,uint32_t query_flags,
    const float start[3],const float displacement[3],float limit,
    uint32_t *stack,uint32_t capacity,rf_collision_tree_hit *result,uint32_t *matched)
{
    rf_collision_tree_hit value;float end[3],scratch[3];uint32_t i,j,used=0,visited=0,hit;int status;
    if(!start || !displacement || !result || !matched || (node_count && (!nodes || !stack || capacity<node_count)) || (face_count && !faces))return RF_RANGE;
    if(!isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    for(j=0;j<3;j++) {if(!isfinite(start[j]) || !isfinite(displacement[j]))return RF_FORMAT;end[j]=start[j]+displacement[j];if(!isfinite(end[j]))return RF_FORMAT;}
    for(i=0;i<node_count;i++) {
        const rf_collision_node *n=nodes+i;
        if(n->first_face>face_count || n->face_count>face_count-n->first_face ||
           (n->left!=UINT32_MAX && n->left>=node_count) || (n->right!=UINT32_MAX && n->right>=node_count))return RF_RANGE;
        for(j=0;j<3;j++)if(!isfinite(n->minimum[j]) || !isfinite(n->maximum[j]) || n->minimum[j]>n->maximum[j])return RF_FORMAT;
    }
    value.hits=0;if(node_count)stack[used++]=0;
    while(used) {
        const rf_collision_node *n=nodes+stack[--used];
        if(++visited>node_count)return RF_FORMAT;
        status=rf_collision_segment_box(n->minimum,n->maximum,start,end,scratch,&hit);if(status)return status;
        if(!hit)continue;
        for(i=0;i<n->face_count;i++) {
            uint32_t index=n->first_face+i;rf_collision_face face=faces[index];
            face.filter.query_flags=query_flags;
            status=rf_collision_thin_face(&face,start,displacement,limit,&value.hit,&hit);if(status)return status;
            if(hit) {
                value.face_index=index;value.hits++;limit=value.hit.fraction;
                if(query_flags&1u)goto done;
            }
        }
        if(n->left!=UINT32_MAX) {if(used==capacity)return RF_RANGE;stack[used++]=n->left;}
        if(n->right!=UINT32_MAX) {if(used==capacity)return RF_RANGE;stack[used++]=n->right;}
    }
 done:
    if(value.hits)*result=value;
    *matched=value.hits!=0;return RF_OK;
}

int rf_collision_sweep_tree(const rf_collision_node *nodes,uint32_t node_count,
    const rf_collision_face *faces,uint32_t face_count,uint32_t query_flags,
    const float start[3],const float displacement[3],const float normal_displacement[3],float radius,float limit,
    uint32_t *stack,uint32_t capacity,rf_collision_sweep_tree_hit *result,uint32_t *matched)
{
    rf_collision_sweep_tree_hit value;float end[3],scratch[3],lo[3],hi[3];uint32_t i,j,used=0,visited=0,hit;int status;
    if(!start || !displacement || !normal_displacement || !result || !matched || (node_count && (!nodes || !stack || capacity<node_count)) || (face_count && !faces))return RF_RANGE;
    if(!isfinite(radius) || radius<0 || !isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    for(j=0;j<3;j++) {if(!isfinite(start[j]) || !isfinite(displacement[j]) || !isfinite(normal_displacement[j]))return RF_FORMAT;end[j]=start[j]+displacement[j];if(!isfinite(end[j]))return RF_FORMAT;}
    for(i=0;i<node_count;i++) {
        const rf_collision_node *n=nodes+i;
        if(n->first_face>face_count || n->face_count>face_count-n->first_face ||
           (n->left!=UINT32_MAX && n->left>=node_count) || (n->right!=UINT32_MAX && n->right>=node_count))return RF_RANGE;
        for(j=0;j<3;j++)if(!isfinite(n->minimum[j]) || !isfinite(n->maximum[j]) || n->minimum[j]>n->maximum[j])return RF_FORMAT;
    }
    value.hits=0;if(node_count)stack[used++]=0;
    while(used) {
        const rf_collision_node *n=nodes+stack[--used];
        if(++visited>node_count)return RF_FORMAT;
        for(j=0;j<3;j++) {lo[j]=n->minimum[j]-radius;hi[j]=n->maximum[j]+radius;}
        status=rf_collision_segment_box(lo,hi,start,end,scratch,&hit);if(status)return status;
        if(!hit)continue;
        for(i=0;i<n->face_count;i++) {
            uint32_t index=n->first_face+i;rf_collision_face face=faces[index];rf_collision_sweep_hit candidate;
            face.filter.query_flags=query_flags;
            status=rf_collision_sweep_face(&face,start,displacement,normal_displacement,radius,limit,&candidate,&hit);if(status)return status;
            if(hit) {
                if(candidate.hits>UINT32_MAX-value.hits)return RF_RANGE;
                value.hit=candidate.hit;value.edge=candidate.edge;value.face_index=index;value.hits+=candidate.hits;limit=value.hit.fraction;
                if(query_flags&1u)goto done;
            }
        }
        if(n->left!=UINT32_MAX) {if(used==capacity)return RF_RANGE;stack[used++]=n->left;}
        if(n->right!=UINT32_MAX) {if(used==capacity)return RF_RANGE;stack[used++]=n->right;}
    }
 done:
    if(value.hits)*result=value;
    *matched=value.hits!=0;return RF_OK;
}

static uint32_t split_axis(const float *lo,const float *hi)
{
    uint32_t axis=0;float y;
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved,control;
    __asm { fnstcw saved }
    control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm {
        fldcw control
        mov ecx,lo
        mov edx,hi
        fld dword ptr [edx]
        fsub dword ptr [ecx]
        fld dword ptr [edx+4]
        fsub dword ptr [ecx+4]
        fst y
        fcomp st(1)
        fnstsw ax
        test ah,0x41
        jnz split_z
        fstp st(0)
        fld y
        mov axis,1
    split_z:
        fld dword ptr [edx+8]
        fsub dword ptr [ecx+8]
        fcomp st(1)
        fnstsw ax
        test ah,0x41
        fstp st(0)
        jnz split_done
        mov axis,2
    split_done:
        fldcw saved
    }
#elif defined(__i386__) || defined(__x86_64__)
    unsigned short saved,control;
    __asm__ volatile("fnstcw %0":"=m"(saved));control=(unsigned short)((saved&~0x0f00u)|0x0300u);
    __asm__ volatile("fldcw %[control]\n\tflds (%[hi])\n\tfsubs (%[lo])\n\tflds 4(%[hi])\n\tfsubs 4(%[lo])\n\tfsts %[y]\n\tfcomp %%st(1)\n\tfnstsw %%ax\n\ttestb $0x41,%%ah\n\tjnz 1f\n\tfstp %%st(0)\n\tflds %[y]\n\tmovl $1,%[axis]\n1:\n\tflds 8(%[hi])\n\tfsubs 8(%[lo])\n\tfcomp %%st(1)\n\tfnstsw %%ax\n\ttestb $0x41,%%ah\n\tfstp %%st(0)\n\tjnz 2f\n\tmovl $2,%[axis]\n2:\n\tfldcw %[saved]"
        :[axis]"+m"(axis),[y]"=m"(y):[hi]"r"(hi),[lo]"r"(lo),[control]"m"(control),[saved]"m"(saved):"ax","cc","st","st(1)","memory");
#else
    long double span=(long double)hi[0]-lo[0],ys=(long double)hi[1]-lo[1];y=(float)ys;
    if(ys>span) {axis=1;span=y;}if((long double)hi[2]-lo[2]>span)axis=2;
#endif
    return axis;
}
int rf_collision_partition(const rf_collision_node *node,const rf_collision_face *faces,
    uint32_t count,uint8_t *labels,uint32_t *axis,uint32_t counts[3])
{
    uint32_t i,j,a,totals[3]={0,0,0};float center;
    if(!node || !axis || !counts || (count && (!faces || !labels)))return RF_RANGE;
    for(j=0;j<3;j++)if(!isfinite(node->minimum[j]) || !isfinite(node->maximum[j]) || node->minimum[j]>node->maximum[j])return RF_FORMAT;
    for(i=0;i<count;i++)for(j=0;j<3;j++)if(!isfinite(faces[i].minimum[j]) || !isfinite(faces[i].maximum[j]) || faces[i].minimum[j]>faces[i].maximum[j] || faces[i].minimum[j]<node->minimum[j] || faces[i].maximum[j]>node->maximum[j])return RF_FORMAT;
    a=split_axis(node->minimum,node->maximum);
    center=(float)(((double)node->minimum[a]+node->maximum[a])*.5);
    for(i=0;i<count;i++) {uint8_t group=faces[i].minimum[a]>=center?1:faces[i].maximum[a]<=center?2:0;labels[i]=group;totals[group]++;}
    *axis=a;memcpy(counts,totals,sizeof(totals));return RF_OK;
}

void rf_collision_tree_close(rf_collision_tree *tree)
{
    if(tree) {free(tree->storage);memset(tree,0,sizeof(*tree));}
}

static int room_overlaps(const rf_collision_room_view *room,const float *lo,const float *hi)
{
    uint32_t j;for(j=0;j<3;j++)if(lo[j]>room->maximum[j] || hi[j]<room->minimum[j])return 0;
    return 1;
}
int rf_collision_thin_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],float limit,
    rf_collision_room_hit *result,uint32_t *matched)
{
    float lo[3],hi[3];uint32_t i,j,k,hits=0;rf_collision_room_hit value;int status;
    if(!start || !displacement || !result || !matched || (room_count && !rooms) ||
       (primary_count && !primary) || (child_count && !children))return RF_RANGE;
    if(query_flags&0x1180u)return RF_NOT_FOUND;
    if(!isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    for(j=0;j<3;j++) {
        float end=start[j]+displacement[j];
        if(!isfinite(start[j]) || !isfinite(displacement[j]) || !isfinite(end))return RF_FORMAT;
        lo[j]=start[j]<end?start[j]:end;hi[j]=start[j]>end?start[j]:end;
    }
    for(i=0;i<room_count;i++) {
        const rf_collision_room_view *room=rooms+i;
        if(!room->tree || room->skip>255 || room->first_child>child_count || room->child_count>child_count-room->first_child)return RF_RANGE;
        for(j=0;j<3;j++)if(!isfinite(room->minimum[j]) || !isfinite(room->maximum[j]) || room->minimum[j]>room->maximum[j])return RF_FORMAT;
    }
    for(i=0;i<primary_count;i++)if(primary[i]>=room_count)return RF_RANGE;
    for(i=0;i<child_count;i++)if(children[i]>=room_count)return RF_RANGE;
    if(displacement[0]==0 && displacement[1]==0 && displacement[2]==0) {*matched=0;return RF_OK;}
    for(i=0;i<primary_count;i++) {
        const rf_collision_room_view *parent=rooms+primary[i];
        if((parent->skip && !(query_flags&8u)) || !room_overlaps(parent,lo,hi))continue;
        for(k=0;;k++) {
            uint32_t index=k?children[parent->first_child+k-1]:primary[i],hit;
            const rf_collision_room_view *room=rooms+index;const rf_collision_tree *tree=room->tree;
            if(room_overlaps(room,lo,hi)) {
                status=rf_collision_thin_tree(tree->nodes,tree->node_count,tree->faces,tree->face_count,query_flags,
                    start,displacement,limit,tree->stack,tree->node_capacity,&value.tree,&hit);if(status)return status;
                if(hit) {
                    if(value.tree.hits>UINT32_MAX-hits)return RF_RANGE;
                    hits+=value.tree.hits;limit=value.tree.hit.fraction;value.room=index;
                    if(query_flags&1u)goto done;
                }
            }
            if(k==parent->child_count)break;
        }
    }
 done:
    if(hits) {value.tree.hits=hits;*result=value;}
    *matched=hits!=0;return RF_OK;
}
static int sweep_rooms_prepared(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],const float normal_displacement[3],uint32_t active,float radius,float limit,
    rf_collision_sweep_room_hit *result,uint32_t *matched)
{
    float lo[3],hi[3];uint32_t i,j,k,hits=0;rf_collision_sweep_room_hit value;int status;
    if(!start || !displacement || !normal_displacement || !result || !matched || (room_count && !rooms) ||
       (primary_count && !primary) || (child_count && !children))return RF_RANGE;
    if(query_flags&0x1180u)return RF_NOT_FOUND;
    if(!isfinite(radius) || radius<0 || !isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    for(j=0;j<3;j++) {
        float end=start[j]+displacement[j];
        if(!isfinite(start[j]) || !isfinite(displacement[j]) || !isfinite(end))return RF_FORMAT;
        lo[j]=(start[j]<end?start[j]:end)-radius;hi[j]=(start[j]<end?end:start[j])+radius;
        if(!isfinite(lo[j]) || !isfinite(hi[j]))return RF_FORMAT;
    }
    for(i=0;i<room_count;i++) {
        const rf_collision_room_view *room=rooms+i;
        if(!room->tree || room->skip>255 || room->first_child>child_count || room->child_count>child_count-room->first_child)return RF_RANGE;
        for(j=0;j<3;j++)if(!isfinite(room->minimum[j]) || !isfinite(room->maximum[j]) || room->minimum[j]>room->maximum[j])return RF_FORMAT;
    }
    for(i=0;i<primary_count;i++)if(primary[i]>=room_count)return RF_RANGE;
    for(i=0;i<child_count;i++)if(children[i]>=room_count)return RF_RANGE;
    if(!active) {*matched=0;return RF_OK;}
    for(i=0;i<primary_count;i++) {
        const rf_collision_room_view *parent=rooms+primary[i];
        if((parent->skip && !(query_flags&8u)) || !room_overlaps(parent,lo,hi))continue;
        for(k=0;;k++) {
            uint32_t index=k?children[parent->first_child+k-1]:primary[i],hit;
            const rf_collision_room_view *room=rooms+index;const rf_collision_tree *tree=room->tree;
            if(room_overlaps(room,lo,hi)) {
                status=rf_collision_sweep_tree(tree->nodes,tree->node_count,tree->faces,tree->face_count,query_flags,
                    start,displacement,normal_displacement,radius,limit,tree->stack,tree->node_capacity,&value.tree,&hit);if(status)return status;
                if(hit) {
                    if(value.tree.hits>UINT32_MAX-hits)return RF_RANGE;
                    hits+=value.tree.hits;limit=value.tree.hit.fraction;value.room=index;
                    if(query_flags&1u)goto done;
                }
            }
            if(k==parent->child_count)break;
        }
    }
 done:
    if(hits) {value.tree.hits=hits;*result=value;}
    *matched=hits!=0;return RF_OK;
}
int rf_collision_sweep_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],float radius,float limit,
    rf_collision_sweep_room_hit *result,uint32_t *matched)
{
    uint32_t active=displacement && (displacement[0]!=0 || displacement[1]!=0 || displacement[2]!=0);
    return sweep_rooms_prepared(rooms,room_count,primary,primary_count,children,child_count,
        query_flags,start,displacement,displacement,active,radius,limit,result,matched);
}
int rf_collision_transformed_rooms(const rf_collision_room_view *rooms,uint32_t room_count,
    const uint32_t *primary,uint32_t primary_count,const uint32_t *children,uint32_t child_count,
    uint32_t query_flags,const float start[3],const float displacement[3],const float origin[3],
    const float matrix[3][3],float radius,float limit,rf_collision_sweep_room_hit *result,uint32_t *matched)
{
    float local_start[3],local_delta[3];uint32_t active;int status;
    status=rf_collision_query_local(start,displacement,origin,matrix,query_flags,local_start,local_delta,&active);if(status)return status;
    if(!active) {memcpy(local_start,start,12);memcpy(local_delta,displacement,12);}
    return sweep_rooms_prepared(rooms,room_count,primary,primary_count,children,child_count,
        query_flags,local_start,local_delta,displacement,active,radius,limit,result,matched);
}
int rf_collision_tree_open(const rf_collision_face *faces,uint32_t count,uint32_t budget,rf_collision_tree *tree)
{
    rf_collision_tree value={0};uint64_t capacity,retained,scratch_bytes;unsigned char *scratch;
    rf_collision_face *temporary;uint32_t *ids,used=0,i,j;uint8_t *labels;int status=RF_OK;
    if(!tree || (count && !faces))return RF_RANGE;
    capacity=count?(uint64_t)count*2-1:0;
    retained=sizeof(value)+capacity*(sizeof(rf_collision_node)+sizeof(uint32_t))+(uint64_t)count*(sizeof(rf_collision_face)+sizeof(uint32_t));
    scratch_bytes=(uint64_t)count*(sizeof(rf_collision_face)+sizeof(uint32_t)+1);
    if(retained+scratch_bytes>budget || retained+scratch_bytes>UINT32_MAX)return RF_RANGE;
    for(i=0;i<count;i++)for(j=0;j<3;j++)if(!isfinite(faces[i].minimum[j]) || !isfinite(faces[i].maximum[j]) || faces[i].minimum[j]>faces[i].maximum[j])return RF_FORMAT;
    value.allocated_bytes=(uint32_t)retained;value.peak_bytes=(uint32_t)(retained+scratch_bytes);
    if(!count) {*tree=value;return RF_OK;}
    value.storage=malloc((size_t)(retained-sizeof(value)));scratch=(unsigned char*)malloc((size_t)scratch_bytes);
    if(!value.storage || !scratch) {free(value.storage);free(scratch);return RF_IO;}
    value.nodes=(rf_collision_node*)value.storage;value.faces=(rf_collision_face*)(value.nodes+capacity);
    value.source_indices=(uint32_t*)(value.faces+count);value.stack=value.source_indices+count;
    value.node_capacity=(uint32_t)capacity;value.face_count=count;value.node_count=1;
    temporary=(rf_collision_face*)scratch;ids=(uint32_t*)(temporary+count);labels=(uint8_t*)(ids+count);
    memcpy(value.faces,faces,(size_t)count*sizeof(*faces));for(i=0;i<count;i++)value.source_indices[i]=i;
    value.nodes[0].first_face=0;value.nodes[0].face_count=count;value.stack[used++]=0;
    while(used) {
        uint32_t index=value.stack[--used],axis,groups[3],positions[3];rf_collision_node *node=value.nodes+index;
        uint32_t first=node->first_face,n=node->face_count;
        node->left=node->right=UINT32_MAX;
        memcpy(node->minimum,value.faces[first].minimum,12);memcpy(node->maximum,value.faces[first].maximum,12);
        for(i=1;i<n;i++)for(j=0;j<3;j++) {
            if(value.faces[first+i].minimum[j]<node->minimum[j])node->minimum[j]=value.faces[first+i].minimum[j];
            if(value.faces[first+i].maximum[j]>node->maximum[j])node->maximum[j]=value.faces[first+i].maximum[j];
        }
        status=rf_collision_partition(node,value.faces+first,n,labels,&axis,groups);if(status)break;
        if(!groups[1] || !groups[2])continue;
        positions[0]=0;positions[1]=groups[0];positions[2]=groups[0]+groups[1];
        for(i=0;i<n;i++) {j=positions[labels[i]]++;temporary[j]=value.faces[first+i];ids[j]=value.source_indices[first+i];}
        memcpy(value.faces+first,temporary,(size_t)n*sizeof(*temporary));memcpy(value.source_indices+first,ids,(size_t)n*sizeof(*ids));
        if(value.node_count+2>value.node_capacity) {status=RF_RANGE;break;}
        node->face_count=groups[0];node->left=value.node_count++;node->right=value.node_count++;
        value.nodes[node->left].first_face=first+groups[0];value.nodes[node->left].face_count=groups[1];
        value.nodes[node->right].first_face=first+groups[0]+groups[1];value.nodes[node->right].face_count=groups[2];
        value.stack[used++]=node->right;value.stack[used++]=node->left;
    }
    free(scratch);if(status) {rf_collision_tree_close(&value);return status;}
    *tree=value;return RF_OK;
}

static int query_solid(const rf_collision_solid_view *solid,uint32_t flags,
    const float start[3],const float delta[3],float limit,rf_collision_sweep_room_hit *result,uint32_t *matched)
{
    rf_collision_sweep_tree_hit value;uint32_t hit;int status;
    if(solid->room_count)return rf_collision_transformed_rooms(solid->rooms,solid->room_count,
        solid->primary,solid->primary_count,solid->children,solid->child_count,flags,start,delta,
        solid->input_origin,solid->input_matrix,0,limit,result,matched);
    status=rf_collision_flat_faces(solid->flat_faces,solid->flat_count,flags,start,delta,
        solid->input_origin,solid->input_matrix,0,limit,&value,&hit);if(status)return status;
    if(hit) {result->tree=value;result->room=UINT32_MAX;}*matched=hit;return RF_OK;
}

int rf_collision_ray_solids(const rf_collision_solid_view *moving,uint32_t count,
    const rf_collision_solid_view *stationary,const float start[3],const float end[3],
    uint32_t flags,rf_collision_solid_hit *result,uint32_t *matched)
{
    rf_collision_solid_hit value={0};rf_collision_sweep_room_hit local;
    float current_end[3],delta[3],scratch[3],limit=1;uint32_t i,j,hit,found=0,q;int status;
    if((count && !moving) || !stationary || !start || !end || !matched)return RF_RANGE;
    for(j=0;j<3;j++) {
        if(!isfinite(start[j]) || !isfinite(end[j]))return RF_FORMAT;
        current_end[j]=end[j];delta[j]=end[j]-start[j];if(!isfinite(delta[j]))return RF_FORMAT;
    }
    q=(((((flags&0xc0u)<<1)|(flags&0x20u))<<1)|(flags&0x1eu))<<4 | (flags&1u);
    for(i=0;i<count;i++) {
        const rf_collision_solid_view *solid=moving+i;
        status=rf_collision_segment_box(solid->minimum,solid->maximum,start,current_end,scratch,&hit);if(status)return status;if(!hit)continue;
        status=query_solid(solid,q,start,delta,limit,&local,&hit);if(status)return status;if(!hit)continue;
        found=1;limit=local.tree.hit.fraction;
        if(result) {
            status=rf_collision_contact_world(&local.tree.hit,solid->output_origin,solid->output_matrix,&value.hit);if(status)return status;
            value.object_id=solid->object_id;value.solid_index=i;value.room=local.room;value.face_index=local.tree.face_index;
        }
        if(flags&1u)goto done;
        for(j=0;j<3;j++) {volatile float scaled=delta[j]*limit;current_end[j]=start[j]+scaled;delta[j]=current_end[j]-start[j];}
    }
    status=query_solid(stationary,q|4u,start,delta,limit,&local,&hit);if(status)return status;
    if(hit) {
        found=1;
        if(result) {value.hit=local.tree.hit;value.object_id=UINT32_MAX;value.solid_index=UINT32_MAX;value.room=local.room;value.face_index=local.tree.face_index;}
    }
 done:
    if(found && result)*result=value;*matched=found;return RF_OK;
}

int rf_collision_flat_faces(const rf_collision_face *faces,uint32_t count,uint32_t flags,
    const float start[3],const float delta[3],const float origin[3],const float matrix[3][3],
    float radius,float limit,rf_collision_sweep_tree_hit *result,uint32_t *matched)
{
    float local_start[3],local_delta[3];uint32_t active,i,found;int status;
    rf_collision_sweep_tree_hit value={0};rf_collision_sweep_hit candidate;
    if((count && !faces) || !result || !matched)return RF_RANGE;
    if(!isfinite(radius) || radius<0 || !isfinite(limit) || limit<0 || limit>1)return RF_FORMAT;
    status=rf_collision_query_local(start,delta,origin,matrix,flags,local_start,local_delta,&active);if(status)return status;
    if(!active) {*matched=0;return RF_OK;}
    for(i=0;i<count;i++) {
        rf_collision_face face=faces[i];face.filter.query_flags=flags;
        status=rf_collision_sweep_face(&face,local_start,local_delta,delta,radius,limit,&candidate,&found);if(status)return status;
        if(found) {
            if(candidate.hits>UINT32_MAX-value.hits)return RF_RANGE;
            value.hit=candidate.hit;value.face_index=i;value.edge=candidate.edge;value.hits+=candidate.hits;limit=candidate.hit.fraction;
        }
    }
    if(value.hits)*result=value;*matched=value.hits!=0;return RF_OK;
}
