/* Temporary Wing boost is ordinary Octane motion with a fuel allowance.
 * Compare actual fixed-step worlds across ground, air, flips and preference
 * changes; no rendered-feel or live-game assertion is made by this test. */
#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"wing physics line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using World=std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)>;
static const RocketTriangle floorMesh[]={
    {{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}},
    {{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}}
};
static World create(int air){
    World w(rocket_world_create(),rocket_world_destroy);CHECK(w);
    CHECK(rocket_world_mesh(w.get(),0,floorMesh,2));
    float p[]={0,air?10000.f:40.f,0},v[]={0,0,0};CHECK(rocket_world_reset(w.get(),p,v,0));return w;
}
static RocketSnapshot frame(World &w,unsigned f,RocketInput in={}){
    CHECK(rocket_world_frame(w.get(),f,&in,0,0)==4);RocketSnapshot s={};CHECK(rocket_world_snapshot(w.get(),&s));return s;
}
static void sameMotion(RocketSnapshot a,RocketSnapshot b){
    a.boost=b.boost=0;CHECK(!memcmp(&a,&b,sizeof a));
}
static void trajectories(){
    for(int scenario=0;scenario<6;scenario++){
        auto wing=create(scenario>=3),ordinary=create(scenario>=3);
        rocket_world_set_temporary_boost(wing.get(),1);
        CHECK(rocket_world_set_boost_mode(ordinary.get(),1));
        for(unsigned f=1;f<=180;f++){
            RocketInput in={};
            if(f>15){in.throttle=.8f;in.steer=.3f;in.pitch=-.3f;in.roll=.2f;in.yaw=.4f;}
            in.boost=scenario%3==1&&f>20;in.powerslide=scenario==0&&f>40;
            in.jump=scenario%3==2&&((f>=22&&f<=27)||f==34);
            if(scenario%3==2&&f==34)in.pitch=-1;
            auto a=frame(wing,f,in),b=frame(ordinary,f,in);sameMotion(a,b);
            CHECK(a.boost==100&&rocket_world_boost_mode(wing.get())==0);
        }
    }
}
static void allowance(){
    for(int preference=0;preference<2;preference++){
        auto w=create(1);frame(w,1);
        RocketInput boost={};boost.boost=1;
        RocketSnapshot s={};for(unsigned f=2;f<=71;f++)s=frame(w,f,boost);
        const float finite=s.boost;CHECK(finite>20&&finite<25);
        CHECK(rocket_world_set_boost_mode(w.get(),preference));
        rocket_world_set_temporary_boost(w.get(),1);
        for(unsigned f=72;f<=160;f++){
            s=frame(w,f,boost);CHECK(s.boost==finite&&s.boosting);
            RocketSnapshot paused=s;CHECK(!rocket_world_frame(w.get(),f,&boost,0,0));
            CHECK(rocket_world_snapshot(w.get(),&s));CHECK(!memcmp(&s,&paused,sizeof s));
        }
        CHECK(rocket_world_collect_coin(w.get()));
        CHECK(rocket_world_snapshot(w.get(),&s));float balance=finite+(preference?0:5);
        CHECK(std::fabs(s.boost-balance)<.001f);
        rocket_world_set_temporary_boost(w.get(),0);s=frame(w,161,boost);
        CHECK(rocket_world_boost_mode(w.get())==preference);
        CHECK(preference?s.boost==balance:std::fabs(s.boost-(balance-100.f/90))<.001f);
        /* A native/character handoff preserves finite balance, but a new world
         * pose requires a fresh lease and release before held boost can fire. */
        float remaining=s.boost,p[]={0,10000,0},v[]={0,0,0};
        rocket_world_set_temporary_boost(w.get(),1);CHECK(rocket_world_reset(w.get(),p,v,0));
        CHECK(rocket_world_set_boost_mode(w.get(),0));s=frame(w,1,boost);CHECK(s.boost==remaining&&!s.boosting);
        frame(w,2);s=frame(w,3,boost);CHECK(s.boost<remaining);
        /* Exhausted finite boost resumes unlimited for exactly the lease, then
         * expires with no refill even while the boost button remains held. */
        for(unsigned f=4;f<=80;f++)s=frame(w,f,boost);CHECK(s.boost==0);
        rocket_world_set_temporary_boost(w.get(),1);
        for(unsigned f=81;f<=120;f++){s=frame(w,f,boost);CHECK(s.boost==0&&s.boosting);}
        rocket_world_set_temporary_boost(w.get(),0);s=frame(w,121,boost);CHECK(s.boost==0&&!s.boosting);
    }
}
int main(){trajectories();allowance();printf("Wing timed boost/ordinary motion: %u checks passed\n",checks);}
