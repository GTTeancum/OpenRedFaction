#ifndef RF_SCENE_TACTICAL_TARGET_CLEARANCE_INCLUDED
#define RF_SCENE_TACTICAL_TARGET_CLEARANCE_INCLUDED
/* 4025d0 as called by 40676e: (target, own-start, own-goal, closest-out).
 * This intentionally preserves the original XY-only projection numerator,
 * XYZ denominator and XYZ final squared distance. It is not the conventional
 * closest-point helper. Inputs/outputs do not alias; no scene writes occur.
 * Finite / nonzero segment rejection is the approved safety adaptation. */
static int scene_tactical_target_clearance(const float target[3],
    const float start[3],const float goal[3],float closest[3],double *score)
{
    float segment[3],relative[3],scaled[3],point[3],difference[3],numerator,fraction;
    double denominator,value;uint32_t i;
    if(!target||!start||!goal||!closest||!score)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(target[i])||!isfinite(start[i])||!isfinite(goal[i]))return RF_FORMAT;
        segment[i]=(float)((double)goal[i]-start[i]);
        relative[i]=(float)((double)target[i]-start[i]);
        if(!isfinite(segment[i])||!isfinite(relative[i]))return RF_FORMAT;
    }
    numerator=(float)((double)segment[0]*relative[0]+(double)segment[1]*relative[1]);
    denominator=((double)segment[0]*segment[0]+(double)segment[1]*segment[1])+
        (double)segment[2]*segment[2];
    if(!isfinite(numerator)||!isfinite(denominator)||denominator<=0)return RF_FORMAT;
    fraction=(float)((double)numerator/denominator);
    if(!isfinite(fraction))return RF_FORMAT;
    if(fraction<0)fraction=0;if(fraction>1)fraction=1;
    for(i=0;i<3;i++) {
        scaled[i]=(float)((double)fraction*segment[i]);
        point[i]=(float)((double)start[i]+scaled[i]);
        difference[i]=(float)((double)point[i]-target[i]);
        if(!isfinite(scaled[i])||!isfinite(point[i])||!isfinite(difference[i]))return RF_FORMAT;
    }
    value=((double)difference[0]*difference[0]+(double)difference[1]*difference[1])+
        (double)difference[2]*difference[2];
    if(!isfinite(value))return RF_FORMAT;
    for(i=0;i<3;i++)closest[i]=point[i];
    *score=value;return RF_OK;
}
#endif
