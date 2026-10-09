#ifndef ROCKET_STAIR_SUPPORT_H
#define ROCKET_STAIR_SUPPORT_H
/* Detect fully covered, connected stair treads. Other walls, obstacles and
 * ledges remain untouched; the caller chooses when to use these support planes. */
#include "rocket_physics.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <vector>
namespace rocket_stairs {
using Point=std::array<float,3>;
struct Patch {Point a,b,c,d;unsigned material;}; // lower edge a/b, upper c/d
struct Riser {Point a,b;float bottom;std::array<float,2> normal;};
struct P2 {double x,y;};
using Polygon=std::vector<P2>;
inline double area(const Polygon &p){
    double sum=0;for(size_t i=0;i<p.size();i++)sum+=p[i].x*p[(i+1)%p.size()].y-p[i].y*p[(i+1)%p.size()].x;
    return std::fabs(sum)*.5;
}
inline Polygon clip(const Polygon &p,P2 a,P2 b,bool inside){
    Polygon result;if(p.empty())return result;
    auto side=[&](P2 q){double s=(b.x-a.x)*(q.y-a.y)-(b.y-a.y)*(q.x-a.x);return inside?s:-s;};
    P2 previous=p.back();double before=side(previous);
    for(auto current:p){
        double now=side(current);
        if((before>=0)!=(now>=0)){
            double f=before/(before-now);
            result.push_back({previous.x+(current.x-previous.x)*f,previous.y+(current.y-previous.y)*f});
        }
        if(now>=0)result.push_back(current);
        previous=current;before=now;
    }
    return result;
}
using Floors=std::map<float,std::vector<const RocketTriangle*>>;
/* Subtract actual horizontal triangles from the required tread rectangle.
 * A tiny gap stays uncovered; overlapping triangles cannot compensate for it.
 * Bound polygon fragmentation and fail closed on pathological geometry. */
inline bool covered(const Floors &floors,const Patch &p,float y,unsigned material){
    auto group=floors.find(y);if(group==floors.end())return false;
    Polygon rectangle={{p.a[0],p.a[2]},{p.b[0],p.b[2]},{p.d[0],p.d[2]},{p.c[0],p.c[2]}};
    const double total=area(rectangle);if(total<1)return false;
    double xmin=p.a[0],xmax=xmin,zmin=p.a[2],zmax=zmin;
    for(auto q:rectangle){xmin=std::min(xmin,q.x);xmax=std::max(xmax,q.x);zmin=std::min(zmin,q.y);zmax=std::max(zmax,q.y);}
    std::vector<Polygon> remaining={rectangle};
    for(const auto *t:group->second){
        if(t->material!=material)continue;
        if(std::max({t->v[0][0],t->v[1][0],t->v[2][0]})<xmin||std::min({t->v[0][0],t->v[1][0],t->v[2][0]})>xmax||
           std::max({t->v[0][2],t->v[1][2],t->v[2][2]})<zmin||std::min({t->v[0][2],t->v[1][2],t->v[2][2]})>zmax)continue;
        // Horizontal upward host triangles are clockwise in X/Z. Reverse them.
        P2 triangle[]={{t->v[0][0],t->v[0][2]},{t->v[2][0],t->v[2][2]},{t->v[1][0],t->v[1][2]}};
        std::vector<Polygon> next;
        for(auto subject:remaining){
            for(int k=0;k<3&&!subject.empty();k++){
                auto outside=clip(subject,triangle[k],triangle[(k+1)%3],false);
                if(area(outside)>1e-7)next.push_back(std::move(outside));
                subject=clip(subject,triangle[k],triangle[(k+1)%3],true);
            }
            if(next.size()>64)return false;
        }
        remaining=std::move(next);
        if(remaining.empty())return true;
    }
    double missing=0;for(const auto &q:remaining)missing+=area(q);
    return missing<1e-7;
}
inline bool tread(const Floors &floors,Patch &p,float y){
    auto group=floors.find(y);if(group==floors.end())return false;
    unsigned tried=0;
    for(auto *t:group->second){
        if(t->material>15||(tried&(1u<<t->material)))continue;
        tried|=1u<<t->material;
        if(covered(floors,p,y,t->material)){p.material=t->material;return true;}
    }
    return false;
}
// The smoothing wedge must be empty above its real tread. A low obstacle or
// overhang inside it cannot be hidden just because the floor is continuous.
inline bool clearWedge(const RocketTriangle *triangles,size_t count,const Patch &p){
    using V=std::array<double,3>;
    double ux=p.b[0]-p.a[0],uz=p.b[2]-p.a[2],vx=p.c[0]-p.a[0],vz=p.c[2]-p.a[2];
    double width=std::hypot(ux,uz),run=std::hypot(vx,vz),rise=p.c[1]-p.a[1];
    ux/=width;uz/=width;vx/=run;vz/=run;
    constexpr double epsilon=.0001;
    const std::array<double,4> planes[]={
        {1,0,0,-epsilon},{-1,0,0,width-epsilon},
        {0,1,0,-epsilon},{0,-1,0,run-epsilon},
        {0,0,1,-epsilon},{0,rise/run,-1,-epsilon}};
    for(size_t i=0;i<count;i++){
        const auto &t=triangles[i];
        if(std::max({t.v[0][1],t.v[1][1],t.v[2][1]})<=p.a[1]+epsilon||
           std::min({t.v[0][1],t.v[1][1],t.v[2][1]})>=p.c[1])continue;
        std::vector<V> polygon;
        for(const auto &q:t.v){double x=q[0]-p.a[0],z=q[2]-p.a[2];polygon.push_back({x*ux+z*uz,x*vx+z*vz,q[1]-p.a[1]});}
        for(const auto &plane:planes){
            if(polygon.empty())break;
            auto side=[&](const V &q){return q[0]*plane[0]+q[1]*plane[1]+q[2]*plane[2]+plane[3];};
            std::vector<V> next;V before=polygon.back();double d0=side(before);
            for(const auto &q:polygon){
                double d1=side(q);
                if((d0>=0)!=(d1>=0)){double f=d0/(d0-d1);V cut;for(int k=0;k<3;k++)cut[k]=before[k]+f*(q[k]-before[k]);next.push_back(cut);}
                if(d1>=0)next.push_back(q);
                before=q;d0=d1;
            }
            polygon=std::move(next);
        }
        if(!polygon.empty())return false;
    }
    return true;
}
inline std::vector<Patch> detect(const RocketTriangle *triangles,size_t count){
    Floors floors;std::vector<Riser> risers;
    for(size_t i=0;i<count;i++){
        const auto &t=triangles[i];
        const float *a=t.v[0],*b=t.v[1],*c=t.v[2];
        float nx=(b[1]-a[1])*(c[2]-a[2])-(b[2]-a[2])*(c[1]-a[1]);
        float ny=(b[2]-a[2])*(c[0]-a[0])-(b[0]-a[0])*(c[2]-a[2]);
        float nz=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
        if(a[1]==b[1]&&b[1]==c[1]&&ny>0){floors[a[1]].push_back(&t);continue;}
        float n=std::hypot(nx,nz);if(n==0||std::fabs(ny)>n*.0001f)continue;
        float top=std::max({a[1],b[1],c[1]}),bottom=std::min({a[1],b[1],c[1]});
        if(top-bottom<4||top-bottom>60)continue;
        const float *first=nullptr,*second=nullptr;
        for(const auto &v:t.v)if(v[1]==top){if(first)second=v;else first=v;}
        if(!second)continue; // the companion triangle has only one top vertex
        Riser r{{first[0],top,first[2]},{second[0],top,second[2]},bottom,{nx/n,nz/n}};
        if(r.b<r.a)std::swap(r.a,r.b);
        if(std::hypot(r.a[0]-r.b[0],r.a[2]-r.b[2])<16)continue;
        risers.push_back(r);
    }
    // A bounded fallback keeps unusually complex custom levels on old collision.
    if(risers.size()>4096)return {};
    std::sort(risers.begin(),risers.end(),[](const Riser &a,const Riser &b){return a.a[1]<b.a[1];});
    struct Link {size_t lower,upper;Patch patch;};std::vector<Link> links;
    std::vector<unsigned> incoming(risers.size()),outgoing(risers.size());
    for(size_t i=0;i<risers.size();i++)for(size_t j=i+1;j<risers.size();j++){
        const auto &a=risers[i],&b=risers[j];float rise=b.a[1]-a.a[1];
        if(rise>60)break;
        if(rise<4||b.bottom!=a.a[1]||a.normal[0]*b.normal[0]+a.normal[1]*b.normal[1]<.9999f)continue;
        float dx=b.a[0]-a.a[0],dz=b.a[2]-a.a[2],run=std::hypot(dx,dz);
        if(run<8||run>240||rise>run*1.1f||dx*a.normal[0]+dz*a.normal[1]>-run*.9999f)continue;
        if(std::fabs((b.b[0]-a.b[0])-dx)>.001f||std::fabs((b.b[2]-a.b[2])-dz)>.001f)continue;
        Patch patch{a.a,a.b,b.a,b.b,0};
        if(!tread(floors,patch,a.a[1]))continue;
        links.push_back({i,j,patch});outgoing[i]++;incoming[j]++;
    }
    std::vector<Patch> result;
    for(const auto &link:links){
        // At least three connected risers. A lone ledge/box is never a ramp.
        if(!incoming[link.lower]&&!outgoing[link.upper])continue;
        result.push_back(link.patch);
        if(!incoming[link.lower]){
            const auto &r=risers[link.lower];const auto &p=link.patch;
            Patch start{r.a,r.b,r.a,r.b,p.material};
            start.a[0]-=p.c[0]-p.a[0];start.a[2]-=p.c[2]-p.a[2];start.a[1]=r.bottom;
            start.b[0]-=p.d[0]-p.b[0];start.b[2]-=p.d[2]-p.b[2];start.b[1]=r.bottom;
            // The bottom approach also needs a fully covered real landing.
            if(tread(floors,start,r.bottom))result.push_back(std::move(start));
        }
    }
    // Bound the final obstacle proof too. Complex custom meshes fail closed.
    if(result.size()&&count>2000000/result.size())return {};
    result.erase(std::remove_if(result.begin(),result.end(),[&](const Patch &p){return !clearWedge(triangles,count,p);}),result.end());
    return result;
}
}
#endif
