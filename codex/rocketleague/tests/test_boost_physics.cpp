#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
static int checks;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
using World=std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)>;
int main(){
    World car(rocket_world_create(),rocket_world_destroy);CHECK(car);
    const RocketTriangle floor[]={
        {{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}},
        {{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}}
    };
    CHECK(rocket_world_mesh(car.get(),0,floor,2));
    float p[]={0,40,0},v[]={0,0,0};CHECK(rocket_world_reset(car.get(),p,v,0));
    RocketInput input={};RocketSnapshot state;
    CHECK(rocket_world_frame(car.get(),1,&input,0,0)==4); // release input inhibition
    input.boost=1;
    for(unsigned frame=2;frame<30;frame++)CHECK(rocket_world_frame(car.get(),frame,&input,0,0)==4);
    CHECK(rocket_world_snapshot(car.get(),&state));float balance=state.boost;
    CHECK(balance>0&&balance<100);
    CHECK(!rocket_world_set_boost_mode(car.get(),2));CHECK(!rocket_world_set_boost_mode(nullptr,1));
    CHECK(rocket_world_set_boost_mode(car.get(),ROCKET_BOOST_INFINITE));
    for(unsigned frame=30;frame<240;frame++)CHECK(rocket_world_frame(car.get(),frame,&input,0,0)==4);
    CHECK(rocket_world_snapshot(car.get(),&state));CHECK(state.boost==balance);CHECK(state.velocity[2]>4000);
    auto before=state;CHECK(!rocket_world_frame(car.get(),240,&input,1,0));
    CHECK(rocket_world_snapshot(car.get(),&state));CHECK(state.ticks==before.ticks&&state.boost==balance);
    CHECK(rocket_world_set_boost_mode(car.get(),ROCKET_BOOST_COIN_ONLY));
    input.boost=0;CHECK(rocket_world_frame(car.get(),241,&input,0,0)==4);input.boost=1;
    for(unsigned frame=242;frame<380;frame++)CHECK(rocket_world_frame(car.get(),frame,&input,0,0)==4);
    CHECK(rocket_world_snapshot(car.get(),&state));CHECK(state.boost==0);
    CHECK(rocket_world_set_boost_mode(car.get(),ROCKET_BOOST_INFINITE));
    CHECK(rocket_world_reset(car.get(),p,v,0)); // reset does not change selected rule
    input.boost=0;CHECK(rocket_world_frame(car.get(),1,&input,0,0)==4);input.boost=1;
    for(unsigned frame=2;frame<180;frame++)CHECK(rocket_world_frame(car.get(),frame,&input,0,0)==4);
    CHECK(rocket_world_snapshot(car.get(),&state));CHECK(state.boost==0&&state.velocity[2]>4000);
    CHECK(rocket_world_set_boost_mode(car.get(),0));
    for(unsigned frame=180;frame<300;frame++)CHECK(rocket_world_frame(car.get(),frame,&input,0,0)==4);
    CHECK(rocket_world_snapshot(car.get(),&state));CHECK(state.boost==0);
    // Infinite still accelerates from an empty finite tank, then restores zero.
    CHECK(rocket_world_set_boost_mode(car.get(),1));input.throttle=-1;input.boost=0;
    for(unsigned frame=300;frame<350;frame++)CHECK(rocket_world_frame(car.get(),frame,&input,0,0)==4);
    CHECK(rocket_world_snapshot(car.get(),&state));float speed=state.velocity[2];input.boost=1;
    for(unsigned frame=350;frame<400;frame++)CHECK(rocket_world_frame(car.get(),frame,&input,0,0)==4);
    CHECK(rocket_world_snapshot(car.get(),&state));CHECK(state.boost==0);CHECK(state.velocity[2]>speed+100);
    CHECK(rocket_world_set_boost_mode(car.get(),0));CHECK(rocket_world_snapshot(car.get(),&state));CHECK(state.boost==0);
    std::printf("boost physics: %d checks passed\n",checks);
}
