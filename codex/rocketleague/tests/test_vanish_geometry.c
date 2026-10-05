#include "../physics/vanish_collision.h"
#include <assert.h>
#include <stdio.h>
static RocketSnapshot car(void){RocketSnapshot s={0};s.basis[2]=s.basis[3]=s.basis[7]=1;return s;}
int main(void){
    RocketSnapshot s=car();
    float wall[3][3]={{-500,-500,147},{500,-500,147},{0,500,147}};
    assert(rocket_car_triangle_overlap(&s,wall,0)); // Front offset extends beyond the center/radius proxy.
    for(int i=0;i<3;i++)wall[i][2]=150;
    assert(!rocket_car_triangle_overlap(&s,wall,0));
    assert(rocket_car_triangle_overlap(&s,wall,3));
    for(int i=0;i<3;i++)wall[i][2]=-95;
    assert(!rocket_car_triangle_overlap(&s,wall,0)); // Rear is shorter because of the forward offset.
    for(int i=0;i<3;i++)wall[i][2]=-92;
    assert(rocket_car_triangle_overlap(&s,wall,0));
    float roof[3][3]={{-500,80,-500},{0,80,500},{500,80,-500}};
    assert(rocket_car_triangle_overlap(&s,roof,0));
    for(int i=0;i<3;i++)roof[i][1]=82;
    assert(!rocket_car_triangle_overlap(&s,roof,0));
    float side[3][3]={{86,-500,-500},{86,-500,500},{86,500,0}};
    assert(rocket_car_triangle_overlap(&s,side,0));
    for(int i=0;i<3;i++)side[i][0]=88;
    assert(!rocket_car_triangle_overlap(&s,side,0));
    // Rotate both geometry and chassis: the answer must not depend on the host axes.
    for(int t=0;t<12;t++){
        float angle=t*.37f,c=cosf(angle),sn=sinf(angle);
        RocketSnapshot r=s;float triangle[3][3];
        for(int a=0;a<3;a++){r.basis[a*3]=c*s.basis[a*3]-sn*s.basis[a*3+1];r.basis[a*3+1]=sn*s.basis[a*3]+c*s.basis[a*3+1];}
        for(int i=0;i<3;i++){triangle[i][0]=c*wall[i][0]-sn*wall[i][1];triangle[i][1]=sn*wall[i][0]+c*wall[i][1];triangle[i][2]=wall[i][2];}
        assert(rocket_car_triangle_overlap(&r,triangle,0));
        r.position[2]=-500;assert(!rocket_car_triangle_overlap(&r,triangle,0));
    }
    // A triangle's bounding box can intersect while the triangle itself misses.
    float corner[3][3]={{80,75,300},{80,300,140},{300,75,140}};
    assert(!rocket_car_triangle_overlap(&s,corner,0));
    float degenerate[3][3]={{0,40,0},{0,40,0},{0,40,0}};
    assert(!rocket_car_triangle_overlap(&s,degenerate,0));
    puts("Vanish full-chassis geometry: offset, roof, width, rear, clearance, rotated and triangle-corner cases passed");
}
