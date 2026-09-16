/* Render-only triangle metadata. Debris collides as its recovered moving
 * sphere, not as this rotated visual mesh. Do not use static CSG face validation
 * here: rounded float plane offsets cannot satisfy a fixed absolute residual
 * at arbitrary world coordinates. Preview consumes count and plane only.
 */
static int scene_debris_render_plane(const rf_geomod_vertex vertices[3],rf_collision_face *out)
{
    rf_collision_face value={0};double a[3],b[3],normal[3],length=0,offset=0;uint32_t i,j;
    if(!vertices || !out)return RF_RANGE;
    for(i=0;i<3;i++)for(j=0;j<3;j++)if(!isfinite(vertices[i].position[j]))return RF_FORMAT;
    for(j=0;j<3;j++){
        a[j]=(double)vertices[1].position[j]-vertices[0].position[j];
        b[j]=(double)vertices[2].position[j]-vertices[0].position[j];
    }
    for(j=0;j<3;j++){
        normal[j]=a[(j+1)%3]*b[(j+2)%3]-a[(j+2)%3]*b[(j+1)%3];
        length+=normal[j]*normal[j];
    }
    if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
    length=sqrt(length);
    for(j=0;j<3;j++){
        value.plane[j]=(float)(normal[j]/length);
        offset-=(double)value.plane[j]*vertices[0].position[j];
    }
    value.plane[3]=(float)offset;if(!isfinite(value.plane[3]))return RF_FORMAT;
    value.count=3;value.triangle_surface=1;
    *out=value;return RF_OK;
}
