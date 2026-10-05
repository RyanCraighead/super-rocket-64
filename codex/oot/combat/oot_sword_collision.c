#include "oot_sword.h"
#include <math.h>

/* Intersection of a triangle and a vertical cylinder is equivalent to clipping
 * the triangle to the cylinder's Y slab and intersecting its XZ projection with
 * the cap disk. Clipping keeps convexity; this avoids fast-sword tunneling and
 * the false positives of a bounding sphere around Link. */
typedef struct { double x,y,z; } V;

static unsigned clip_y(const V *in,unsigned count,V *out,double y,bool above) {
    unsigned written=0;
    for(unsigned i=0;i<count;i++) {
        V a=in[i],b=in[(i+1)%count];
        bool ai=above?a.y>=y:a.y<=y, bi=above?b.y>=y:b.y<=y;
        if(ai) out[written++]=a;
        if(ai!=bi) {
            double t=(y-a.y)/(b.y-a.y);
            out[written++]=(V){a.x+t*(b.x-a.x),y,a.z+t*(b.z-a.z)};
        }
    }
    return written;
}
static double edge_dist2(V a,V b,double x,double z) {
    double dx=b.x-a.x,dz=b.z-a.z;
    double d=dx*dx+dz*dz;
    double t=d>0?((x-a.x)*dx+(z-a.z)*dz)/d:0;
    if(t<0)t=0;
    if(t>1)t=1;
    dx=a.x+t*dx-x;dz=a.z+t*dz-z;
    return dx*dx+dz*dz;
}
static bool triangle_hit(OotSwordVec3 a,OotSwordVec3 b,OotSwordVec3 c,
                         double x,double y,double z,double r,double h) {
    V p[8]={{a.x,a.y,a.z},{b.x,b.y,b.z},{c.x,c.y,c.z}},q[8];
    unsigned n=clip_y(p,3,q,y,true);
    if(!n)return false;
    n=clip_y(q,n,p,y+h,false);
    if(!n)return false;
    double area=0;
    bool positive=false,negative=false;
    for(unsigned i=0;i<n;i++) {
        V u=p[i],v=p[(i+1)%n];
        if(edge_dist2(u,v,x,z)<=r*r)return true;
        area+=u.x*v.z-v.x*u.z;
        double cross=(v.x-u.x)*(z-u.z)-(v.z-u.z)*(x-u.x);
        if(cross>1e-8)positive=true;
        if(cross<-1e-8)negative=true;
    }
    return fabs(area)>1e-8 && !(positive&&negative);
}
bool oot_sword_quad_hits_cylinder(const OotSwordQuad *q,float x,float y,float z,float r,float h) {
    if(!q||!q->valid||!isfinite(x)||!isfinite(y)||!isfinite(z)||!isfinite(r)||!isfinite(h)||
       fabsf(x)>1e7f||fabsf(y)>1e7f||fabsf(z)>1e7f||r<0||r>1e6f||h<0||h>1e6f)return false;
    for(int i=0;i<4;i++)if(!isfinite(q->v[i].x)||!isfinite(q->v[i].y)||!isfinite(q->v[i].z)||
       fabsf(q->v[i].x)>1e7f||fabsf(q->v[i].y)>1e7f||fabsf(q->v[i].z)>1e7f)return false;
    /* Collider_SetQuadVertices source order: new base/tip, old base/tip. */
    return triangle_hit(q->v[2],q->v[3],q->v[1],x,y,z,r,h)||
           triangle_hit(q->v[2],q->v[1],q->v[0],x,y,z,r,h);
}
