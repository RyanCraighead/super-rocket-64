#ifndef ROCKET_BODY_CONTACT_H
#define ROCKET_BODY_CONTACT_H
/* Read-only crossover geometry, not a damage/attack policy. These dimensions
 * match CAR_CONFIG_OCTANE in pinned RocketSim c2baacb8, including its offsets. */
#include "rocket_physics.h"
#include <math.h>

#define ROCKET_BODY_HALF_LENGTH (120.507f * .5f * ROCKET_HOST_SCALE)
#define ROCKET_BODY_HALF_WIDTH  (86.6994f * .5f * ROCKET_HOST_SCALE)
#define ROCKET_BODY_HALF_HEIGHT (38.6591f * .5f * ROCKET_HOST_SCALE)
#define ROCKET_BODY_FORWARD_OFFSET (13.8757f * ROCKET_HOST_SCALE)
#define ROCKET_BODY_UP_OFFSET (20.755f * ROCKET_HOST_SCALE)

typedef struct RocketBodyPoint { float x, z; } RocketBodyPoint;
static inline float rocket_body_cross(RocketBodyPoint a, RocketBodyPoint b, RocketBodyPoint c) {
    return (b.x-a.x)*(c.z-a.z)-(b.z-a.z)*(c.x-a.x);
}
static inline int rocket_body_pose_valid(const RocketSnapshot *car) {
    if (!car) return 0;
    for (int i=0;i<3;i++) if (!isfinite(car->position[i])) return 0;
    for (int i=0;i<9;i++) if (!isfinite(car->basis[i])) return 0;
    for (int i=0;i<3;i++) for (int j=i;j<3;j++) {
        float dot=0;
        for (int k=0;k<3;k++) dot+=car->basis[3*i+k]*car->basis[3*j+k];
        if (fabsf(dot-(i==j?1.f:0.f))>.01f) return 0;
    }
    return 1;
}

/* Intersect the oriented box with the cylinder's vertical slab, then project
 * that convex intersection to XZ. The cylinder intersects iff its axis is in
 * the projected hull or within radius of a hull edge. Unlike a bounding sphere
 * or projected rectangle this also handles tilted/rolled roof and corner gaps.
 * No allocations, actor writes, temporal history, or collision-list insertion.
 * position is the native cylinder's bottom center, after hitboxDownOffset. */
static inline int rocket_body_overlaps_cylinder(const RocketSnapshot *car,
        const float position[3], float radius, float height) {
    if (!rocket_body_pose_valid(car)||!position||!isfinite(radius)||radius<=0||
        !isfinite(height)||height<0) return 0;
    for (int i=0;i<3;i++) if (!isfinite(position[i])) return 0;
    const float half[3]={ROCKET_BODY_HALF_LENGTH,ROCKET_BODY_HALF_WIDTH,ROCKET_BODY_HALF_HEIGHT};
    float center[3],extent[3]={0},vertices[8][3];
    for (int k=0;k<3;k++) {
        center[k]=car->position[k]+car->basis[k]*ROCKET_BODY_FORWARD_OFFSET+
            car->basis[6+k]*ROCKET_BODY_UP_OFFSET-position[k];
        for (int axis=0;axis<3;axis++) extent[k]+=fabsf(car->basis[3*axis+k])*half[axis];
        if (!isfinite(center[k])) return 0;
    }
    if (fabsf(center[0])>extent[0]+radius||fabsf(center[2])>extent[2]+radius||
        center[1]+extent[1]<0||center[1]-extent[1]>height) return 0;
    RocketBodyPoint points[32],hull[64];
    int count=0;
    for (int v=0;v<8;v++) {
        for (int k=0;k<3;k++) {
            vertices[v][k]=center[k];
            for (int axis=0;axis<3;axis++)
                vertices[v][k]+=car->basis[3*axis+k]*half[axis]*((v&(1<<axis))?1.f:-1.f);
        }
        if (vertices[v][1]>=0&&vertices[v][1]<=height)
            points[count++]=(RocketBodyPoint){vertices[v][0],vertices[v][2]};
    }
    for (int v=0;v<8;v++) for (int axis=0;axis<3;axis++) {
        if (v&(1<<axis)) continue;
        const float *a=vertices[v],*b=vertices[v|(1<<axis)];
        float dy=b[1]-a[1];
        if (dy==0) continue;
        for (int plane=0;plane<2;plane++) {
            float t=((plane?height:0)-a[1])/dy;
            if (t>=0&&t<=1) points[count++]=(RocketBodyPoint){a[0]+t*(b[0]-a[0]),a[2]+t*(b[2]-a[2])};
        }
    }
    if (!count) return 0;
    /* Bounded insertion sort, followed by Andrew's convex hull. */
    for (int i=1;i<count;i++) {
        RocketBodyPoint point=points[i];int j=i;
        while (j>0&&(points[j-1].x>point.x||(points[j-1].x==point.x&&points[j-1].z>point.z))) {
            points[j]=points[j-1];j--;
        }
        points[j]=point;
    }
    int n=0;
    for (int i=0;i<count;i++) {
        while (n>=2&&rocket_body_cross(hull[n-2],hull[n-1],points[i])<=0) n--;
        hull[n++]=points[i];
    }
    int lower=n;
    for (int i=count-2;i>=0;i--) {
        while (n>lower&&rocket_body_cross(hull[n-2],hull[n-1],points[i])<=0) n--;
        hull[n++]=points[i];
    }
    if (n>1) n--;
    int inside=n>=3;
    RocketBodyPoint origin={0,0};
    for (int i=0;i<n;i++) {
        RocketBodyPoint a=hull[i],b=hull[(i+1)%n];
        if (rocket_body_cross(a,b,origin)<0) inside=0;
        float dx=b.x-a.x,dz=b.z-a.z,denom=dx*dx+dz*dz;
        float t=denom>0?fminf(1.f,fmaxf(0.f,-(a.x*dx+a.z*dz)/denom)):0;
        float x=a.x+t*dx,z=a.z+t*dz;
        if (x*x+z*z<radius*radius) return 1;
    }
    return inside;
}
#endif
