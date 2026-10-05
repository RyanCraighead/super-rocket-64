#include "spiderman_web_attack_host.h"
#include <math.h>
#include "sm64.h"
#include "surface_terrains.h"
#include "engine/surface_load.h"
static double dot(const double a[3],const double b[3]){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void cross(double o[3],const double a[3],const double b[3]){o[0]=a[1]*b[2]-a[2]*b[1];o[1]=a[2]*b[0]-a[0]*b[2];o[2]=a[0]*b[1]-a[1]*b[0];}
static int intersects(const struct Surface *s,const double origin[3],const double dir[3]){
    double e1[3],e2[3],h[3],q[3],v[3];
    for(int i=0;i<3;i++){e1[i]=(double)s->vertex2[i]-s->vertex1[i];e2[i]=(double)s->vertex3[i]-s->vertex1[i];v[i]=origin[i]-s->vertex1[i];}
    cross(h,dir,e2);double d=dot(e1,h);if(fabs(d)<1e-12)return 0;
    double a=dot(v,h)/d;if(a<0||a>1)return 0;
    cross(q,v,e1);double b=dot(dir,q)/d;if(b<0||a+b>1)return 0;
    double t=dot(e2,q)/d;return t>0&&t<=1;
}
int spiderman_web_attack_dynamic_only(const int32_t from[3],const int32_t to[3]){
    double origin[3],dir[3];const double sign[3]={1,-1,-1};size_t visited=0;
    if(!from||!to)return -1;
    for(int i=0;i<3;i++){origin[i]=sign[i]*from[i]/4096.0;dir[i]=sign[i]*((double)to[i]-from[i])/4096.0;}
    if(dot(dir,dir)==0)return 0;
    double lx=fmin(origin[0],origin[0]+dir[0]),hx=fmax(origin[0],origin[0]+dir[0]);
    double lz=fmin(origin[2],origin[2]+dir[2]),hz=fmax(origin[2],origin[2]+dir[2]);
    if(hx< -LEVEL_BOUNDARY_MAX||lx>=LEVEL_BOUNDARY_MAX||hz< -LEVEL_BOUNDARY_MAX||lz>=LEVEL_BOUNDARY_MAX)return 0;
    int x0=(int)floor((lx+LEVEL_BOUNDARY_MAX)/CELL_SIZE),x1=(int)floor((hx+LEVEL_BOUNDARY_MAX)/CELL_SIZE);
    int z0=(int)floor((lz+LEVEL_BOUNDARY_MAX)/CELL_SIZE),z1=(int)floor((hz+LEVEL_BOUNDARY_MAX)/CELL_SIZE);
    if(x0<0)x0=0;
    if(z0<0)z0=0;
    if(x1>=NUM_CELLS)x1=NUM_CELLS-1;
    if(z1>=NUM_CELLS)z1=NUM_CELLS-1;
    for(int z=z0;z<=z1;z++)for(int x=x0;x<=x1;x++)for(int p=0;p<3;p++)for(int dynamic=0;dynamic<2;dynamic++){
        /* Also inspect object-owned triangles filed in the static partition. */
        const struct SurfaceNode *node=(dynamic?gDynamicSurfacePartition[z][x][p]:gStaticSurfacePartition[z][x][p]).next;
        for(;node;node=node->next){
            if(++visited>1000000||!node->surface)return -3;
            const struct Surface *s=node->surface;
            if(!dynamic&&!s->object&&!(s->flags&SURFACE_FLAG_DYNAMIC)&&s->poolType!=SURFACE_POOL_DYNAMIC)continue;
            if(s->type==SURFACE_INTANGIBLE||(s->flags&SURFACE_FLAG_INTANGIBLE))continue;
            if(s->normal.x*dir[0]+s->normal.y*dir[1]+s->normal.z*dir[2]>0)continue;
            if(intersects(s,origin,dir))return -4;
        }
    }
    return 0;
}
