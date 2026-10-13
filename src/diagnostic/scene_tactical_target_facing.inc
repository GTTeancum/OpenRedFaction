#ifndef RF_SCENE_TACTICAL_TARGET_FACING_INCLUDED
#define RF_SCENE_TACTICAL_TARGET_FACING_INCLUDED
/* Original4020c0: target-player3c -> shooter3c, normalized by4faaf0,
 * dotted with the target's eye forward7f8, compared inclusively to its authored
 * cosine840. This is not a world-obstruction query or camera-FOV test.
 * The caller must supply the actual target class cosine; no guessed90-degree
 * fallback is permitted. Coincident/nonfinite geometry is rejected safely.
 * Finite arithmetic follows the existing original-derived audio normalizer. */
static int scene_tactical_target_facing(const float target_position[3],
    const float target_forward[3],float cosine,const float shooter_position[3],uint32_t *visible)
{
    float direction[3];double length2,reciprocal,dot;
    if(!target_position||!target_forward||!shooter_position||!visible)return RF_RANGE;
    if(!isfinite(cosine)||cosine < -1.0f||cosine > 1.0f)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(target_position[i])||!isfinite(target_forward[i])||!isfinite(shooter_position[i]))return RF_FORMAT;
        direction[i]=(float)((double)shooter_position[i]-target_position[i]);
        if(!isfinite(direction[i]))return RF_FORMAT;
    }
    length2=(double)direction[0]*direction[0]+(double)direction[1]*direction[1]+(double)direction[2]*direction[2];
    if(!isfinite(length2)||length2<=0)return RF_FORMAT;
    reciprocal=1.0/sqrt(length2);
    for(uint32_t i=0;i<3;i++)direction[i]=(float)(reciprocal*direction[i]);
    dot=(double)target_forward[2]*direction[2]+(double)target_forward[1]*direction[1]+(double)target_forward[0]*direction[0];
    if(!isfinite(dot))return RF_FORMAT;
    *visible=dot>=(double)cosine;return RF_OK;
}
#endif
