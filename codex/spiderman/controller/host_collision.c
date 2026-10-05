#include "host_collision.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>
/* Work in whole world units to avoid magnifying squared fixed12 magnitudes. */
typedef struct Sweep {
    double start[3], delta[3], x,z,low,high,radius;
} Sweep;
static double distance_squared(const Sweep *s,double t) {
    double x=s->start[0]+s->delta[0]*t-s->x;
    double z=s->start[2]+s->delta[2]*t-s->z;
    double radial=fmax(0.0,hypot(x,z)-s->radius);
    double y=s->start[1]+s->delta[1]*t;
    double vertical=y<s->low?s->low-y:y>s->high?y-s->high:0;
    return radial*radial+vertical*vertical;
}
int smn64_host_sweep_cylinder(const int32_t from[3],const int32_t to[3],int32_t radius,
    int32_t x,int32_t z,int32_t low,int32_t high,int32_t cylinder,int32_t contact[3]) {
    if(!from||!to||!contact||radius<0||cylinder<=0||low>high)return -1;
    Sweep s;for(int k=0;k<3;++k){s.start[k]=from[k]/4096.0;s.delta[k]=((double)to[k]-from[k])/4096.0;}
    s.x=x/4096.0;s.z=z/4096.0;s.low=low/4096.0;s.high=high/4096.0;s.radius=cylinder/4096.0;
    const double r=radius/4096.0,limit=r*r;
    double t=0;
    if(distance_squared(&s,0)>limit){
        /* Distance to a closed convex solid along a line is convex. Ternary
         * bracketing handles cylinder caps and their rounded rim, avoiding the
         * false corner hits of an expanded AABB or expanded-radius slab test. */
        double left=0,right=1;
        for(int i=0;i<96;++i){double a=left+(right-left)/3,b=right-(right-left)/3;if(distance_squared(&s,a)<distance_squared(&s,b))right=b;else left=a;}
        t=(left+right)*.5;
        double best=distance_squared(&s,t),end=distance_squared(&s,1);
        if(end<best){t=1;best=end;}
        /* Tiny absolute world-unit-squared tolerance stabilizes an exactly
         * tangent double calculation; it is below a fixed12 quantum squared. */
        if(best>limit+1e-13)return 0;
        left=0;right=t;
        for(int i=0;i<80;++i){double mid=(left+right)*.5;if(distance_squared(&s,mid)<=limit+1e-13)right=mid;else left=mid;}
        t=right;
    }
    int32_t result[3];for(int k=0;k<3;++k){double q=round((s.start[k]+s.delta[k]*t)*4096.0);if(q<INT32_MIN||q>INT32_MAX||!isfinite(q))return -1;result[k]=(int32_t)q;}
    for(int k=0;k<3;++k)contact[k]=result[k];
    return 1;
}
