#include "../physics/whomp_impact.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Whomp physics line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)

/* Actual pinned RocketSim and Bullet, a generated test rectangle, and ordinary
 * controls. No fabricated snapshots, ROM data or native game process. */
static void trajectory(int flip,int boost,int expected) {
    RocketWorld *world=rocket_world_create();CHECK(world);
    const RocketTriangle mesh[]={
        {{{-150,100,50},{150,100,430},{150,100,50}}},
        {{{-150,100,50},{-150,100,430},{150,100,430}}}
    };
    CHECK(rocket_world_mesh(world,0,mesh,2));
    float p[]={0,flip?400.f:900.f,flip?80.f:220.f},v[]={0,-800,0};
    CHECK(rocket_world_reset(world,p,v,0));
    CHECK(rocket_world_set_boost_mode(world,ROCKET_BOOST_INFINITE));
    RocketWhompBack back={{0,0,0},{1,0},{0,1},{-150,50},{150,430},100,1};
    RocketWhompContact contact={};int hits=0;
    for(unsigned frame=1;frame<65;frame++){
        RocketInput input={};
        input.pitch=flip?(frame>=3?-1.f:0.f):(frame<=9?-1.f:0.f);
        input.jump=flip&&frame==3;input.boost=!flip&&boost&&frame>9;
        CHECK(rocket_world_frame(world,frame,&input,0,0)==4);
        RocketSnapshot car;CHECK(rocket_world_snapshot(world,&car));
        float point[3];int kind=rocket_whomp_contact(&contact,&car,1,&back,point);
        if(kind){CHECK(kind==expected);hits++;}
    }
    CHECK(hits==(expected?1:0));rocket_world_destroy(world);
}
int main(){
    trajectory(1,0,1);trajectory(0,1,2);trajectory(0,0,0);
    std::puts("Whomp physics: real flip and boosted dive each enter once; unboosted dive never attacks");
}
