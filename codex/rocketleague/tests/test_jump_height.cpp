#include "../physics/rocket_physics.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"jump height line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using World=std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)>;
static World create(unsigned height,unsigned speed=75,bool ground=true,float slope=0){
    World w(rocket_world_create(),rocket_world_destroy);CHECK(w);
    CHECK(rocket_world_set_speed(w.get(),speed));CHECK(rocket_world_set_jump_height(w.get(),height));
    if(ground){
        RocketTriangle mesh[]={
            {{{-10000,-10000*slope,-10000},{10000,10000*slope,10000},{10000,-10000*slope,-10000}}},
            {{{-10000,-10000*slope,-10000},{-10000,10000*slope,10000},{10000,10000*slope,10000}}}
        };CHECK(rocket_world_mesh(w.get(),0,mesh,2));
    }
    float p[]={0,ground?40.f:3000.f,0},v[]={0,0,0};CHECK(rocket_world_reset(w.get(),p,v,0));return w;
}
static RocketSnapshot step(World &w,unsigned f,RocketInput in={}){
    CHECK(rocket_world_frame(w.get(),f,&in,0,0)==4);RocketSnapshot s={};
    CHECK(rocket_world_snapshot(w.get(),&s));
    CHECK(std::isfinite(s.position[1])&&std::isfinite(s.velocity[1]));return s;
}
struct Result {float apex=0;bool jumped=false,doubled=false,flipped=false;float flipVelocity=0,flipTime=0;};
static Result measure(unsigned height,unsigned speed,unsigned hold,unsigned second=0,bool flip=false,float slope=0){
    auto w=create(height,speed,true,slope);Result r;float start=0;
    for(unsigned f=1;f<=100;f++){
        RocketInput in={};in.jump=(f>=30&&f<30+hold)||f==second;
        if(f==second&&flip)in.pitch=-1;
        auto s=step(w,f,in);float above=s.position[1]-slope*s.position[2];
        if(f==29){CHECK(s.grounded);start=above;}
        if(f>=30){r.apex=std::max(r.apex,above-start);r.jumped|=s.jumped;r.doubled|=s.double_jumped;r.flipped|=s.flipped;}
        if(f==second&&flip){r.flipVelocity=s.velocity[2];r.flipTime=s.flip_time;}
    }
    CHECK(r.jumped&&r.apex>0);return r;
}
static void apex_tests(){
    float largestError=0,minimumHalf=1,maximumHalf=0;
    for(unsigned speed:{75u,100u})for(unsigned hold=1;hold<=7;hold++){
        auto original=measure(100,speed,hold);float previous=0;
        for(unsigned height=50;height<=100;height++){
            auto r=measure(height,speed,hold);float fraction=r.apex/original.apex;
            float error=std::fabs(fraction-height/100.f);largestError=std::max(largestError,error);
            if(error>.015f)fprintf(stderr,"height=%u speed=%u hold=%u actual=%f reference=%f fraction=%f\n",height,speed,hold,r.apex,original.apex,fraction);
            CHECK(error<.015f);CHECK(r.apex>=previous);previous=r.apex;
            CHECK(!r.doubled&&!r.flipped);
            if(height==50){minimumHalf=std::min(minimumHalf,fraction);maximumHalf=std::max(maximumHalf,fraction);}
            if((height==50||height==100)&&(hold==1||hold==6))printf("apex: height=%u speed=%u hold_frames=%u rise=%.4f host units\n",height,speed,hold,r.apex);
        }
    }
    printf("all integer settings/hold lengths: maximum height-ratio error %.6f; 50%% ratios %.6f..%.6f\n",largestError,minimumHalf,maximumHalf);
    // Speed tuning cannot change jump height: compare actual 50/75/100 speed runs.
    for(unsigned height:{50u,75u,100u})for(unsigned hold:{1u,3u,6u}){
        auto a=measure(height,50,hold),b=measure(height,75,hold),c=measure(height,100,hold);
        CHECK(a.apex==b.apex&&b.apex==c.apex);
    }
}
static void secondary_tests(){
    for(unsigned speed:{75u,100u})for(unsigned hold:{1u,6u}){
        unsigned second=hold==1?35:40;
        auto full=measure(100,speed,hold,second),half=measure(50,speed,hold,second);
        CHECK(full.doubled&&half.doubled&&!half.flipped);CHECK(half.apex<full.apex&&half.apex>measure(50,speed,hold).apex);
        CHECK(half.apex/full.apex>.35f&&half.apex/full.apex<.65f);
        printf("double jump: speed=%u hold_frames=%u rises=%.4f/%.4f ratio=%.6f\n",speed,hold,half.apex,full.apex,half.apex/full.apex);
        auto flipFull=measure(100,speed,hold,second,true),flipHalf=measure(50,speed,hold,second,true);
        CHECK(flipFull.flipped&&flipHalf.flipped&&!flipHalf.doubled);
        printf("flip: speed=%u hold_frames=%u forward=%.4f/%.4f timer=%.6f/%.6f\n",speed,hold,flipHalf.flipVelocity,flipFull.flipVelocity,flipHalf.flipTime,flipFull.flipTime);
        CHECK(std::fabs(flipFull.flipVelocity-flipHalf.flipVelocity)<10.f&&flipFull.flipTime==flipHalf.flipTime);
    }
    for(float slope:{-.2f,.2f})for(unsigned hold:{1u,6u}){
        auto full=measure(100,75,hold,0,false,slope),half=measure(50,75,hold,0,false,slope);
        CHECK(half.apex<full.apex&&half.apex/full.apex>.35f&&half.apex/full.apex<.65f);
        printf("slope: rise/run=%.2f hold_frames=%u rises=%.4f/%.4f\n",slope,hold,half.apex,full.apex);
    }
}
static void unchanged_paths(){
    for(unsigned speed:{75u,100u})for(unsigned scenario=0;scenario<6;scenario++){
        auto full=create(100,speed,false),half=create(50,speed,false);
        if(scenario>=2&&scenario<=4){rocket_world_set_water(full.get(),1,30000,0);rocket_world_set_water(half.get(),1,30000,0);}
        if(scenario==3){rocket_world_set_metal_water(full.get(),1);rocket_world_set_metal_water(half.get(),1);}
        if(scenario==1||scenario==4){rocket_world_set_temporary_boost(full.get(),1);rocket_world_set_temporary_boost(half.get(),1);}
        for(unsigned f=1;f<=80;f++){
            RocketInput in={};if(f>1&&scenario){in.throttle=.5f;in.pitch=.25f;in.boost=1;in.jump=scenario==2||scenario==4;}
            if(scenario==5){in={};in.jump=f==5;in.pitch=f==5?-1.f:0;}
            auto a=step(full,f,in),b=step(half,f,in);
            if(memcmp(&a,&b,sizeof a))fprintf(stderr,"unexpected unchanged-path difference scenario=%u frame=%u\n",scenario,f);
            CHECK(!memcmp(&a,&b,sizeof a));
            if(scenario==0&&f==1)CHECK(std::fabs(a.velocity[1]+1300.f/30)<.1f);
            if(scenario==1||scenario==4)CHECK(a.boost==100);
        }
    }
}
static void changes_and_limits(){
    CHECK(rocket_jump_preference(0)==50&&rocket_jump_preference(49)==50&&rocket_jump_preference(101)==50);
    CHECK(rocket_jump_impulse_scale(100)==1.f&&rocket_jump_hold_scale(100)==1.f);
    CHECK(!rocket_world_set_jump_height(nullptr,50));
    auto w=create(100);CHECK(rocket_world_jump_height(w.get())==100);
    CHECK(!rocket_world_set_jump_height(w.get(),0)&&!rocket_world_set_jump_height(w.get(),49)&&!rocket_world_set_jump_height(w.get(),101));
    CHECK(rocket_world_jump_height(w.get())==100);
    for(unsigned f=1;f<=45;f++){
        RocketInput in={};in.jump=f>=30&&f<=35;in.boost=f>32;auto before=step(w,f,in);
        CHECK(rocket_world_set_jump_height(w.get(),f%2?50:100));RocketSnapshot after={};CHECK(rocket_world_snapshot(w.get(),&after));
        CHECK(!memcmp(&before,&after,sizeof before));CHECK(rocket_world_speed(w.get())==75);
    }
}
int main(){apex_tests();secondary_tests();unchanged_paths();changes_and_limits();printf("jump height physics: %u checks passed\n",checks);}
