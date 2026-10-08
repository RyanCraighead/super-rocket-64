#include "../physics/rocket_physics.h"
#include "../physics/quicksand_visual.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
static int checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"sand physics line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Fixture {
    std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> w{rocket_world_create(),rocket_world_destroy};
    uint64_t frame=0;RocketInput input{};
    Fixture(){CHECK(w);RocketTriangle floor[]={{{{-20000,0,-20000},{20000,0,20000},{20000,0,-20000}},0},
        {{{-20000,0,-20000},{-20000,0,20000},{20000,0,20000}},0}};
        CHECK(rocket_world_mesh(w.get(),0,floor,2));reset();step(30);}
    void reset(float height=40){float p[]={0,height,0},v[]={0,0,0};CHECK(rocket_world_reset(w.get(),p,v,0));frame=0;input={};}
    void depth(float n){CHECK(rocket_world_set_quicksand_depth(w.get(),n));}
    void step(int n=1){while(n--)CHECK(rocket_world_frame(w.get(),++frame,&input,0,0)==4);}
    RocketSnapshot state(){RocketSnapshot s{};CHECK(rocket_world_snapshot(w.get(),&s));return s;}
};
static void same(const RocketSnapshot&a,const RocketSnapshot&b){
    for(int k=0;k<3;k++){CHECK(a.position[k]==b.position[k]);CHECK(a.velocity[k]==b.velocity[k]);}
    for(int k=0;k<9;k++)CHECK(a.basis[k]==b.basis[k]);CHECK(a.boost==b.boost&&a.grounded==b.grounded);
}
int main(){
    Fixture dry,sunk;sunk.depth(60);dry.input.throttle=sunk.input.throttle=1;
    for(int i=0;i<150;i++){dry.step();sunk.step();}
    auto a=dry.state(),b=sunk.state();
    std::printf("normal drive dry speed=%.1f buried=%.1f\n",a.velocity[2],b.velocity[2]);
    CHECK(b.velocity[2]>0&&b.velocity[2]<a.velocity[2]*.3f);CHECK(b.grounded&&std::fabs(b.position[1]-a.position[1])<1);
    CHECK(b.boost==a.boost&&b.quicksand_depth==60); // collision body is not lowered
    RocketSnapshot picture=b;rocket_quicksand_visual_pose(&picture);
    CHECK(picture.position[1]==b.position[1]-60);
    for(int i=0;i<4;i++)CHECK(picture.wheel_position[i][1]==b.wheel_position[i][1]-60);
    CHECK(!std::memcmp(picture.basis,b.basis,sizeof b.basis));
    sunk.depth(0);sunk.step(150);CHECK(sunk.state().velocity[2]>a.velocity[2]*.9f);
    // Only native ground launch is weakened; airborne motion/boost do not use
    // the wheel-mobility factor or a downward velocity/speed clamp.
    Fixture jump,soft;soft.depth(5);jump.input.jump=soft.input.jump=1;jump.step();soft.step();
    CHECK(soft.state().jumped&&soft.state().velocity[1]>jump.state().velocity[1]*.4f&&soft.state().velocity[1]<jump.state().velocity[1]*.7f);
    Fixture air,airSand;air.reset(10000);airSand.reset(10000);airSand.depth(60);
    for(int i=0;i<40;i++){air.input.throttle=airSand.input.throttle=1;air.input.boost=airSand.input.boost=i>5;air.step();airSand.step();same(air.state(),airSand.state());}
    airSand.reset();CHECK(airSand.state().quicksand_depth==0);
    for(float bad:{-1.f,201.f,INFINITY,NAN}){CHECK(!rocket_world_set_quicksand_depth(airSand.w.get(),bad));CHECK(airSand.state().quicksand_depth==0);}
    Fixture old,zero;zero.depth(0);for(int i=0;i<90;i++){old.input.throttle=zero.input.throttle=1;old.step();zero.step();same(old.state(),zero.state());}
    std::printf("quicksand physics/visual separation: %d checks passed\n",checks);
}
