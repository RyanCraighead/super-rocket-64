#include "spiderman_world.h"
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "engine/surface_load.h"

static double dot(const double a[3],const double b[3]) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void cross(double out[3],const double a[3],const double b[3]) {
    out[0]=a[1]*b[2]-a[2]*b[1];out[1]=a[2]*b[0]-a[0]*b[2];out[2]=a[0]*b[1]-a[1]*b[0];
}
/* Moller-Trumbore segment parameter; double intermediates are a declared host
 * geometry choice. Tests include tiny cell-border rays and front/back rejection.
 * Do not substitute this algorithm for claims about original ROM geometry. */
static int intersect(const struct Surface *s,const double origin[3],const double dir[3],double *t) {
    double e1[3],e2[3],h[3],q[3],v[3];
    for(int k=0;k<3;++k){e1[k]=(double)s->vertex2[k]-s->vertex1[k];e2[k]=(double)s->vertex3[k]-s->vertex1[k];v[k]=origin[k]-s->vertex1[k];}
    cross(h,dir,e2);double det=dot(e1,h);
    if(fabs(det)<1e-12)return 0;
    double inv=1.0/det,u=dot(v,h)*inv;
    if(u<0 || u>1)return 0;
    cross(q,v,e1);double b=dot(dir,q)*inv;
    if(b<0 || u+b>1)return 0;
    double distance=dot(e2,q)*inv;
    if(distance<=0 || distance>1)return 0;
    *t=distance;return 1;
}
static uint16_t source_flags(const struct Surface *s) {
    /* Host policy: ordinary solid static geometry can support adhesion/webs.
     * Hazards, quicksand and level-transport surfaces cannot. Source flag4 is
     * the recovered no-adhesion/web-anchor bit; other source flags remain zero. */
    return s->type==SURFACE_BURNING || s->type==SURFACE_DEATH_PLANE ||
        s->type==SURFACE_VERTICAL_WIND || s->type==SURFACE_WARP ||
        s->type==SURFACE_LOOK_UP_WARP ||
        (s->type>=SURFACE_INSTANT_WARP_1B && s->type<=SURFACE_INSTANT_WARP_1E) ||
        SURFACE_IS_QUICKSAND(s->type) ? 4 : 0;
}
int spiderman_world_trace_ex(const int32_t from[3],const int32_t to[3],int include_objects,SpidermanWorldHit *out) {
    if(!from || !to || !out || (include_objects!=0 && include_objects!=1))return -1;
    SpidermanWorldHit result={0};double origin[3],dir[3];
    const double sign[3]={1,-1,-1};
    for(int k=0;k<3;++k){origin[k]=sign[k]*(double)from[k]/4096.0;dir[k]=sign[k]*((double)to[k]-from[k])/4096.0;}
    double length=sqrt(dot(dir,dir));
    if(length==0){*out=result;return 1;}
    double endx=origin[0]+dir[0],endz=origin[2]+dir[2];
    double lo_x=fmin(origin[0],endx),hi_x=fmax(origin[0],endx),lo_z=fmin(origin[2],endz),hi_z=fmax(origin[2],endz);
    if(hi_x< -LEVEL_BOUNDARY_MAX || lo_x>=LEVEL_BOUNDARY_MAX || hi_z< -LEVEL_BOUNDARY_MAX || lo_z>=LEVEL_BOUNDARY_MAX){*out=result;return 1;}
    /* Enumerate intersected bounding-box cells rather than the host camera ray's
     * sampled DDA, which can miss a very short ray crossing a cell boundary. */
    int minx=(int)floor((lo_x+LEVEL_BOUNDARY_MAX)/CELL_SIZE),maxx=(int)floor((hi_x+LEVEL_BOUNDARY_MAX)/CELL_SIZE);
    int minz=(int)floor((lo_z+LEVEL_BOUNDARY_MAX)/CELL_SIZE),maxz=(int)floor((hi_z+LEVEL_BOUNDARY_MAX)/CELL_SIZE);
    if(minx<0)minx=0;
    if(minz<0)minz=0;
    if(maxx>=NUM_CELLS)maxx=NUM_CELLS-1;
    if(maxz>=NUM_CELLS)maxz=NUM_CELLS-1;
    const struct Surface *best=NULL;int best_dynamic=0;double nearest=2;size_t visited=0;
    for(int z=minz;z<=maxz;++z)for(int x=minx;x<=maxx;++x)for(int partition=0;partition<3;++partition)for(int dynamic=0;dynamic<=include_objects;++dynamic) {
        const struct SurfaceNode *node=(dynamic?gDynamicSurfacePartition[z][x][partition]:gStaticSurfacePartition[z][x][partition]).next;
        for(;node;node=node->next) {
            if(++visited>1000000)return -3;
            const struct Surface *surface=node->surface;if(!surface)return -3;
            if(!include_objects && (surface->object || (surface->flags&SURFACE_FLAG_DYNAMIC) || surface->poolType==SURFACE_POOL_DYNAMIC))continue;
            if(surface->type==SURFACE_INTANGIBLE || (surface->flags&SURFACE_FLAG_INTANGIBLE))continue;
            /* Original static query rejects normal dot ray > 0 (800555C8).
             * Host normal follows the same winding as its triangle vertices. */
            if(surface->normal.x*dir[0]+surface->normal.y*dir[1]+surface->normal.z*dir[2]>0)continue;
            double t;if(!intersect(surface,origin,dir,&t) || t>=nearest)continue;
            nearest=t;best=surface;best_dynamic=dynamic;
        }
    }
    if(!best){*out=result;return 1;}
    if(best_dynamic || best->object || (best->flags&SURFACE_FLAG_DYNAMIC) || best->poolType==SURFACE_POOL_DYNAMIC)return -4;
    double n[3]={best->normal.x,-best->normal.y,-best->normal.z};
    double norm=dot(n,n);if(!isfinite(norm) || fabs(norm-1)>0.01)return -3;
    for(int k=0;k<3;++k){
        volatile float native_hit=(float)(sign[k]*(origin[k]+dir[k]*nearest));
        volatile float native_fixed=native_hit*4096.0f;
        double fixed=trunc((double)native_fixed);if(!isfinite(fixed) || fixed<INT32_MIN || fixed>INT32_MAX)return -3;
        result.position[k]=(int32_t)fixed;
        volatile float normal_fixed=(float)n[k]*4096.0f;
        result.normal[k]=(int16_t)normal_fixed;
    }
    /* Source line40 boundary: binary32 fraction times binary32 whole-unit
     * length, truncated toward zero, rather than distance from rounded hit XYZ. */
    volatile float fraction=(float)nearest, source_length=(float)length;
    volatile float source_distance=fraction*source_length;
    result.present=1;result.distance=(int32_t)source_distance;result.surface=best;result.source_flags=source_flags(best);
    *out=result;return 1;
}

int spiderman_world_trace(const int32_t from[3],const int32_t to[3],SpidermanWorldHit *out) {return spiderman_world_trace_ex(from,to,1,out);}
