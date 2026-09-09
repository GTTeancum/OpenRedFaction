#include "rf/collision.h"
#include <math.h>
#include <string.h>

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
