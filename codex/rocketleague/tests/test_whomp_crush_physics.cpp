#include "../physics/crush_contact.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <initializer_list>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){std::fprintf(stderr,"crush physics %d: %s; %s\n",__LINE__,#x,rocket_world_error());std::exit(1);}}while(0)
/* Actual pinned backend, generated floor and moving underside/top triangles.
 * The guarded run tests the same geometric handoff as the host. The companion
 * native fixture executes the actual damage/release action after that handoff. */
static const RocketTriangle floorMesh[]={{{{-3000,0,-3000},{3000,0,3000},{3000,0,-3000}},0},{{{-3000,0,-3000},{-3000,0,3000},{3000,0,3000}},0}};
static const RocketTriangle slab[]={{{{-180,0,-300},{180,0,-300},{180,0,300}},0},{{{-180,0,-300},{180,0,300},{-180,0,300}},0},
 {{{-180,100,-300},{180,100,300},{180,100,-300}},0},{{{-180,100,-300},{-180,100,300},{180,100,300}},0}};
static RocketPlatform platform(){RocketPlatform p{};p.object_id=1;p.basis[0]=p.basis[4]=p.basis[8]=1;p.triangles=slab;p.count=4;return p;}
static void crush(float x,bool guard){
    auto*w=rocket_world_create();CHECK(w);CHECK(rocket_world_mesh(w,0,floorMesh,2));float p[]={x,40,0},v[]={0,0,0};CHECK(rocket_world_reset(w,p,v,0));
    auto ceiling=platform();ceiling.position[1]=350;RocketInput in{};float low=10000;bool handedOff=false;
    for(unsigned f=1;f<=80;f++){
        if(f>35&&f<=45)ceiling.position[1]-=35;
        RocketSnapshot car{};CHECK(rocket_world_snapshot(w,&car));
        if(guard)for(int t=0;t<2;t++){
            float tri[3][3];for(int i=0;i<3;i++)for(int k=0;k<3;k++)tri[i][k]=slab[t].v[i][k]+ceiling.position[k];
            float h,bottom;
            if(rocket_crush_ceiling(&car,tri,&h,&bottom)&&h<=150&&h>=-80&&bottom>=-8&&bottom<=80)handedOff=true;
        }
        if(handedOff)break;
        CHECK(rocket_world_platforms(w,&ceiling,1));CHECK(rocket_world_frame(w,f,&in,0,0)==4);
        CHECK(rocket_world_snapshot(w,&car));low=fminf(low,car.position[1]);
    }
    if(guard){CHECK(handedOff&&low>0);}else{CHECK(!handedOff&&low<0);}
    std::printf("%s x=%.0f minimum origin Y=%.3f\n",guard?"handoff":"baseline",x,low);
    rocket_world_destroy(w);
}
static void standup_launch(){
    auto*w=rocket_world_create();CHECK(w);CHECK(rocket_world_mesh(w,0,floorMesh,2));float p[]={0,140,180},v[]={0,0,0};CHECK(rocket_world_reset(w,p,v,0));
    auto top=platform();RocketInput input{};RocketSnapshot car{};float high=0,fast=0;
    for(unsigned f=1;f<=100;f++){
        if(f>40){float angle=-fminf(1.5707963f,(f-40)*.049087385f),c=cosf(angle),s=sinf(angle);top.basis[4]=c;top.basis[5]=s;top.basis[7]=-s;top.basis[8]=c;}
        CHECK(rocket_world_platforms(w,&top,1));CHECK(rocket_world_frame(w,f,&input,0,0)==4);CHECK(rocket_world_snapshot(w,&car));
        if(f==40)CHECK(car.grounded&&car.position[1]>125);
        if(f>40){high=fmaxf(high,car.position[1]);fast=fmaxf(fast,car.velocity[1]);}
    }
    CHECK(high>220&&fast>100);std::printf("back stand-up preserved: maximum Y %.2f, upward speed %.2f\n",high,fast);rocket_world_destroy(w);
}
int main(){for(float x:{0.f,170.f,250.f}){crush(x,false);crush(x,true);}standup_launch();std::printf("PASS %u actual physics crush/back-launch assertions\n",checks);}
