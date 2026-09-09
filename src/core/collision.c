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
