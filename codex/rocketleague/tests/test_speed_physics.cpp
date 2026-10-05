#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"speed physics line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
using World=std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)>;
static const RocketTriangle floorMesh[]={
    {{{-100000,0,-100000},{100000,0,100000},{100000,0,-100000}}},
    {{{-100000,0,-100000},{-100000,0,100000},{100000,0,100000}}}
};
static World create(unsigned percent,bool floor=true,float y=40,float speed=0){
    World w(rocket_world_create(),rocket_world_destroy);CHECK(w);
#ifndef SPEED_BASELINE
    CHECK(rocket_world_set_speed(w.get(),percent));
#else
    CHECK(percent==100);
#endif
    if(floor)CHECK(rocket_world_mesh(w.get(),0,floorMesh,2));
    float p[]={0,y,0},v[]={0,0,speed};CHECK(rocket_world_reset(w.get(),p,v,0));
    return w;
}
static RocketSnapshot step(World &w,unsigned frame,RocketInput in={}){
    CHECK(rocket_world_frame(w.get(),frame,&in,0,0)==4);
    RocketSnapshot s={};CHECK(rocket_world_snapshot(w.get(),&s));return s;
}
static float speed(const RocketSnapshot &s){return std::sqrt(s.velocity[0]*s.velocity[0]+s.velocity[1]*s.velocity[1]+s.velocity[2]*s.velocity[2]);}
static void dump(const RocketSnapshot &s){
    for(float x:s.position)printf("%a ",x);for(float x:s.velocity)printf("%a ",x);
    for(float x:s.basis)printf("%a ",x);for(float x:s.angular_velocity)printf("%a ",x);
    for(auto &w:s.wheel_position)for(float x:w)printf("%a ",x);
    for(float x:s.wheel_steer)printf("%a ",x);for(float x:s.wheel_radius)printf("%a ",x);
    printf("%a %a %a %a %llu %d %d %d %d %d %d\n",s.boost,s.jump_time,s.flip_time,s.air_time,
        (unsigned long long)s.ticks,s.grounded,s.jumped,s.double_jumped,s.flipped,s.flipping,s.boosting);
}
static void trace100(){
    for(int scenario=0;scenario<8;scenario++){
        auto w=create(100,scenario<5,scenario<5?40.f:3000.f);
        if(scenario==6)rocket_world_set_water(w.get(),1,5000,0);
        if(scenario==7)rocket_world_set_metal_water(w.get(),1);
        for(unsigned f=1;f<=240;f++){
            RocketInput in={};
            if(f>20){in.throttle=f<140?1.f:-1.f;in.boost=(scenario==1||scenario>=5)&&f<140;}
            if(scenario==2||scenario==3){in.steer=.5f;in.powerslide=scenario==3;}
            if(scenario==4){in.jump=(f>=30&&f<=35)||f==40;in.pitch=f==40?-1.f:0;}
            if(scenario==6)in.jump=f>60&&f<100;
            dump(step(w,f,in));
        }
    }
}
#ifndef SPEED_BASELINE
static void scaled(unsigned percent){
    float scale=percent/100.f;
    auto w=create(percent);CHECK(rocket_world_speed(w.get())==percent);
    CHECK(!rocket_world_set_speed(w.get(),0));CHECK(!rocket_world_set_speed(w.get(),49));
    CHECK(!rocket_world_set_speed(w.get(),101));CHECK(!rocket_world_set_speed(nullptr,75));
    CHECK(rocket_world_speed(w.get())==percent);
    RocketSnapshot s;for(unsigned f=1;f<=300;f++){RocketInput in={};in.throttle=f>20;s=step(w,f,in);}
    CHECK(std::fabs(s.velocity[2]/scale-2820.f)<100.f);
    float road=s.velocity[2];RocketInput boost={};boost.throttle=boost.boost=1;
    CHECK(rocket_world_set_boost_mode(w.get(),1));
    for(unsigned f=301;f<=460;f++)s=step(w,f,boost);
    CHECK(std::fabs(speed(s)-4600.f*scale)<1.f);CHECK(s.velocity[2]>road);
    float fuel=s.boost;auto before=s;
    CHECK(rocket_world_set_speed(w.get(),percent==100?75:100));
    CHECK(rocket_world_snapshot(w.get(),&s));CHECK(!memcmp(&s,&before,sizeof s));
    CHECK(rocket_world_set_speed(w.get(),percent));
    RocketInput brake={};brake.throttle=-1;for(unsigned f=461;f<=473;f++)s=step(w,f,brake);
    CHECK(s.velocity[2]<before.velocity[2]-.5f*scale*3000.f);CHECK(s.boost==fuel);
    // Fuel rate, no refills on rule changes, temporary/underwater allowance.
    w=create(percent);step(w,1);for(unsigned f=2;f<32;f++)s=step(w,f,boost);
    CHECK(std::fabs(s.boost-(100.f-100.f/3.f))<.1f);
    fuel=s.boost;rocket_world_set_temporary_boost(w.get(),1);
    for(unsigned f=32;f<80;f++)s=step(w,f,boost);CHECK(s.boost==fuel);
    CHECK(rocket_world_set_speed(w.get(),75));CHECK(rocket_world_collect_coin(w.get()));
    CHECK(rocket_world_snapshot(w.get(),&s));CHECK(std::fabs(s.boost-fuel-5)<.001f);
    // World gravity is unchanged; only the existing terminal cap is scaled.
    w=create(percent,false,50000);s=step(w,1);CHECK(std::fabs(s.velocity[1]+1300.f/30)<.1f);
    for(unsigned f=2;f<=180;f++)s=step(w,f);CHECK(std::fabs(speed(s)-4600.f*scale)<1.f);
}
static void compare_motion(){
    for(unsigned percent:{50u,75u})for(int scenario=0;scenario<7;scenario++){
        float scale=percent/100.f;
        bool ground=scenario<4;float y=ground?40:3000;
        auto a=create(100,ground,y,scenario==1?2000.f:0),b=create(percent,ground,y,scenario==1?2000.f*scale:0);
        if(scenario==6){rocket_world_set_water(a.get(),1,5000,0);rocket_world_set_water(b.get(),1,5000,0);}
        RocketSnapshot x={},z={};
        for(unsigned f=1;f<=60;f++){
            RocketInput in={};
            if(scenario==0||scenario==1)in.throttle=f>20?1.f:0;
            if(scenario==1){in.steer=.6f;in.powerslide=f>40;}
            if(scenario==2||scenario==3){in.jump=(f>=25&&f<=29)||f==35;in.pitch=scenario==3&&f==35?-1.f:0;}
            if(scenario>=4&&f>1){in.boost=scenario==4;in.throttle=scenario==5||scenario==6;in.jump=scenario==6&&f>20;}
            x=step(a,f,in);z=step(b,f,in);
            CHECK(x.ticks==z.ticks&&x.boost==z.boost);
            if(scenario==2){CHECK(x.jumped==z.jumped&&x.double_jumped==z.double_jumped);CHECK(std::fabs(x.position[1]-z.position[1])<.01f);}
            if(scenario==3){CHECK(x.flipped==z.flipped&&x.flipping==z.flipping);CHECK(x.flip_time==z.flip_time);}
            if((scenario==4||scenario==5)&&f<=40){CHECK(std::fabs(z.velocity[2]-x.velocity[2]*scale)<.1f);CHECK(std::fabs(z.velocity[1]-x.velocity[1])<.1f);}
        }
        if(scenario==0)CHECK(std::fabs(z.velocity[2]/x.velocity[2]-scale)<.035f);
        if(scenario==1)CHECK(std::fabs(z.wheel_steer[0]-x.wheel_steer[0])<.09f);
        if(scenario==3)CHECK(std::fabs(z.velocity[2]/x.velocity[2]-scale)<.03f);
        if(scenario==6){CHECK(z.water_mode==ROCKET_WATER_JET);CHECK(z.velocity[2]<x.velocity[2]&&z.velocity[2]>0);CHECK(z.velocity[1]<x.velocity[1]);}
    }
}
#endif
int main(int argc,char **argv){
    if(argc==2&&!strcmp(argv[1],"--trace100")){trace100();return 0;}
#ifndef SPEED_BASELINE
    for(unsigned p:{50u,75u,100u})scaled(p);
    compare_motion();printf("speed physics: %u checks passed\n",checks);
#endif
}
