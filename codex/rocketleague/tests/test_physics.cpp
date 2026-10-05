#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>
#include "../../../src/pc/utils/rocket_sha256.h"

static int checks;
#ifdef ROCKET_TEST_HOST_MATH
extern "C" int rocket_poison_atan2f_calls;
#endif
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s (%s)\n",__LINE__,#x,rocket_world_error());std::exit(1);}}while(0)
using World=std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)>;
static const RocketTriangle floorMesh[]={
    {{{-20000,0,-20000},{20000,0,20000},{20000,0,-20000}}},
    {{{-20000,0,-20000},{-20000,0,20000},{20000,0,20000}}}
};
struct Fixture {
    World world{rocket_world_create(),rocket_world_destroy};uint64_t frame=0;
    RocketInput input={};
    Fixture(){CHECK(world);CHECK(rocket_world_mesh(world.get(),0,floorMesh,2));reset();}
    // Existing trajectory cases explicitly start full; production resets retain fuel.
    void reset(float height=40){for(int i=0;i<20;i++)rocket_world_collect_coin(world.get());float p[]={0,height,0},v[]={0,0,0};CHECK(rocket_world_reset(world.get(),p,v,0));frame=0;input={};}
    RocketSnapshot state(){RocketSnapshot s;CHECK(rocket_world_snapshot(world.get(),&s));return s;}
    void step(int count=1){for(int i=0;i<count;++i)CHECK(rocket_world_frame(world.get(),++frame,&input,0,0)==4);}
    void settle(){step(30);CHECK(state().grounded);}
};
static float speed(const RocketSnapshot &s){return std::sqrt(s.velocity[0]*s.velocity[0]+s.velocity[1]*s.velocity[1]+s.velocity[2]*s.velocity[2]);}
static bool same(const RocketSnapshot &a,const RocketSnapshot &b){
    // Compare fields, not unspecified C struct padding.
    for(int i=0;i<3;++i)if(a.position[i]!=b.position[i]||a.velocity[i]!=b.velocity[i]||a.angular_velocity[i]!=b.angular_velocity[i])return false;
    for(int i=0;i<9;++i)if(a.basis[i]!=b.basis[i])return false;
    for(int i=0;i<4;++i){if(a.wheel_contacts[i]!=b.wheel_contacts[i]||a.wheel_steer[i]!=b.wheel_steer[i]||a.wheel_radius[i]!=b.wheel_radius[i])return false;for(int j=0;j<3;++j)if(a.wheel_position[i][j]!=b.wheel_position[i][j])return false;}
    return a.boost==b.boost&&a.jump_time==b.jump_time&&a.flip_time==b.flip_time&&a.air_time==b.air_time&&a.ticks==b.ticks&&a.grounded==b.grounded&&a.jumped==b.jumped&&a.double_jumped==b.double_jumped&&a.flipped==b.flipped&&a.flipping==b.flipping;
}
static void test_coin_boost(){
    Fixture f;f.settle();f.input.boost=1;f.step(30);
    auto before=f.state();CHECK(before.boost>60&&before.boost<70);CHECK(before.boosting);
    CHECK(rocket_world_collect_coin(f.world.get()));auto after=f.state();
    CHECK(after.boost==before.boost+5);after.boost=before.boost;CHECK(same(before,after));
    f.input={};f.step(600); // Twenty seconds without pickups never recharge.
    CHECK(f.state().boost==before.boost+5);
    float p[]={0,40,0},v[]={0,0,0};
    CHECK(rocket_world_reset(f.world.get(),p,v,0));CHECK(f.state().boost==before.boost+5);
    float tank=f.state().boost;
    CHECK(rocket_world_set_boost_mode(f.world.get(),ROCKET_BOOST_INFINITE));CHECK(f.state().boost==tank);
    CHECK(rocket_world_boost_mode(f.world.get())==ROCKET_BOOST_INFINITE);
    f.step();f.input.boost=1;f.step(180);CHECK(f.state().boost==tank&&speed(f.state())>4400);
    CHECK(rocket_world_collect_coin(f.world.get()));
    CHECK(rocket_world_reset(f.world.get(),p,v,0));CHECK(f.state().boost==tank);
    CHECK(rocket_world_set_boost_mode(f.world.get(),ROCKET_BOOST_COIN_ONLY));CHECK(f.state().boost==tank);
    CHECK(!rocket_world_set_boost_mode(f.world.get(),99));
    f.input={};f.step();f.input.boost=1;f.step(150);CHECK(f.state().boost==0);CHECK(!f.state().boosting);
    f.input={};f.step(600);CHECK(f.state().boost==0);
    CHECK(rocket_world_collect_coin(f.world.get()));CHECK(f.state().boost==5);
    for(int i=0;i<25;i++)CHECK(rocket_world_collect_coin(f.world.get()));CHECK(f.state().boost==100);
    CHECK(!rocket_world_collect_coin(nullptr));CHECK(!rocket_world_set_boost_mode(nullptr,ROCKET_BOOST_COIN_ONLY));
}
int main(){
    test_coin_boost();
    {
        Fixture metal,dry;
        metal.reset(10000);dry.reset(10000);
        rocket_world_set_metal_water(metal.world.get(),1);
        for(int frame=1;frame<=30;frame++) {
            metal.step();dry.step();
            CHECK(std::fabs(metal.state().velocity[1]-std::fmax(-540.f,-30.f*frame))<.05f);
        }
        CHECK(dry.state().velocity[1]<-1000&&metal.state().position[1]>dry.state().position[1]);
        auto frozen=metal.state();
        CHECK(!rocket_world_frame(metal.world.get(),++metal.frame,&metal.input,1,0));CHECK(same(frozen,metal.state()));
        rocket_world_set_metal_water(metal.world.get(),0);metal.step();CHECK(metal.state().velocity[1]<-570);
        // Reset/ownership changes clear the water override and restore dry
        // gravity without changing any other controller parameter.
        metal.reset();dry.reset();
        for(int frame=0;frame<30;frame++){metal.step();dry.step();CHECK(same(metal.state(),dry.state()));}
        rocket_world_set_metal_water(metal.world.get(),1);metal.input.throttle=1;metal.step(45);
        CHECK(metal.state().grounded&&metal.state().velocity[2]>500);
        metal.input.jump=1;metal.step(3);CHECK(metal.state().jumped&&metal.state().position[1]>60);
        puts("PASS native metal sinking target, ground driving/jump, pause, expiry gravity and dry reset equivalence");
    }
    CHECK(rocket_assets::sha256({})=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(rocket_assets::sha256({'a','b','c'})=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(rocket_assets::sha256(std::vector<unsigned char>(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // Coordinate round trip and handedness: forward cross right equals up.
    float r[]={10,20,30},h[3],back[3];rocket_to_host(r,h,2);CHECK(h[0]==40&&h[1]==60&&h[2]==20);
    rocket_from_host(h,back,2);CHECK(std::memcmp(r,back,sizeof r)==0);
    // A reset snapshot is valid before the first tick, even when paused. It
    // must not expose uninitialized backend suspension fields to the network.
    Fixture f,initialPeer;auto initial=f.state();CHECK(same(initial,initialPeer.state()));
    CHECK(initial.ticks==0&&!initial.grounded);
    for(int w=0;w<4;w++){
        CHECK(!initial.wheel_contacts[w]&&initial.wheel_steer[w]==0);
        for(float coordinate:initial.wheel_position[w])CHECK(std::isfinite(coordinate)&&std::fabs(coordinate)<200);
    }
    f.input.jump=f.input.boost=1;
    CHECK(!rocket_world_frame(f.world.get(),++f.frame,&f.input,1,0));CHECK(same(initial,f.state()));
    f.input={};f.settle();auto s=f.state();CHECK(s.wheel_contacts[0]&&s.wheel_contacts[1]&&s.wheel_contacts[2]&&s.wheel_contacts[3]);
    f.reset();CHECK(same(initial,f.state()));f.settle();
    CHECK(s.position[1]>20&&s.position[1]<60);
    f.input.throttle=1;f.step(180);s=f.state();CHECK(s.velocity[2]>2700&&s.velocity[2]<2900);
    // Brake then reverse, distinct from an instantaneous sign swap.
    f.input.throttle=-1;f.step();CHECK(f.state().velocity[2]>0);f.step(120);CHECK(f.state().velocity[2]<-1500);
    f.reset();f.settle();f.input.throttle=1;f.input.boost=1;f.step(90);s=f.state();CHECK(speed(s)>4400&&speed(s)<=4601);CHECK(s.boost<1);
    // Held jump cannot issue a second jump. A fresh release/press can.
    f.reset();f.settle();f.input.jump=1;f.step(6);s=f.state();CHECK(s.jumped&&!s.double_jumped&&!s.flipped&&s.position[1]>90);
    f.step(2);CHECK(!f.state().double_jumped);f.input.jump=0;f.step();f.input.jump=1;f.step();CHECK(f.state().double_jumped);
    f.input.jump=0;f.step(100);CHECK(f.state().grounded&&!f.state().jumped);
    // Directional flip, flip cancellation and aerial axes are the pinned backend's behavior.
    f.reset();f.settle();f.input.jump=1;f.step(3);f.input.jump=0;f.step();f.input.jump=1;f.input.pitch=-1;f.step();
    s=f.state();CHECK(s.flipped&&s.flipping&&!s.double_jumped);CHECK(s.velocity[2]>600);
    f.input.jump=0;f.input.pitch=1;f.step(7);s=f.state();CHECK(std::isfinite(s.basis[0])&&s.flip_time>0);
    f.reset(3000);f.step(50);f.input.jump=1;f.input.pitch=-1;f.step();
    // Falling without jumping grants a flip reset; this differs from a timed jump.
    CHECK(f.state().flipped);
    // Determinism in a fixed build: two independent worlds, same inputs/mesh.
    Fixture a,b;a.settle();b.settle();
    for(int i=0;i<300;++i){
        a.input.throttle=b.input.throttle=(i%71<50?1.f:-1.f);
        a.input.steer=b.input.steer=(i%43<20?.65f:-.4f);
        a.input.jump=b.input.jump=(i==40||i==45||i==200);
        a.input.pitch=b.input.pitch=(i>42&&i<65?-1.f:0.f);
        a.input.boost=b.input.boost=(i%100<30);
        a.input.powerslide=b.input.powerslide=(i%60>45);
        a.step();b.step();auto x=a.state(),y=b.state();CHECK(same(x,y));
    }
    f.reset();f.settle();s=f.state();f.input.jump=1;f.input.boost=1;
    for(int i=0;i<100;++i)CHECK(rocket_world_frame(f.world.get(),++f.frame,&f.input,1,0)==0);
    auto paused=f.state();CHECK(same(s,paused));
    f.step();CHECK(!f.state().jumped&&f.state().boost==100);
    f.input={};f.step();f.input.jump=1;f.step();CHECK(f.state().jumped);
    auto before=f.state();CHECK(rocket_world_frame(f.world.get(),f.frame,&f.input,0,0)==0);
    CHECK(rocket_world_frame(f.world.get(),f.frame-1,&f.input,0,0)==0);s=f.state();CHECK(same(before,s));
    // Focus blocks throttle/boost, while airborne gravity continues; no delayed jump on return.
    f.reset(3000);f.step();f.input.throttle=1;f.input.boost=1;f.input.jump=1;
    CHECK(rocket_world_frame(f.world.get(),++f.frame,&f.input,0,1)==4);CHECK(f.state().velocity[1]<0&&f.state().boost==100);
    f.step();CHECK(!f.state().flipped&&!f.state().double_jumped&&f.state().boost==100);
    // Invalid replacement leaves old terrain intact; input nonfinites are neutral.
    RocketTriangle bad=floorMesh[0];bad.v[0][0]=std::numeric_limits<float>::quiet_NaN();
    CHECK(!rocket_world_mesh(f.world.get(),0,&bad,1));f.reset();f.input.throttle=bad.v[0][0];f.settle();CHECK(speed(f.state())<10);
    // Fixture replenishes boost explicitly; reset erases flip/input/frame history.
    f.reset();f.settle();CHECK(f.state().boost==100&&!f.state().flipped&&!f.state().double_jumped);
    // Used-world steering/suspension history must not leak across reset.
    f.input.throttle=1;f.input.steer=.8f;f.input.powerslide=1;f.step(60);f.reset();Fixture fresh;
    for(int i=0;i<60;++i){f.step();fresh.step();CHECK(same(f.state(),fresh.state()));}
    // 120 Hz freefall: pinned backend gravity is -650 RL units/s^2.
    f.reset(5000);f.step(30);s=f.state();CHECK(std::fabs(s.velocity[1]+1300)<1);CHECK(s.ticks==120);
    // Jump release starts the 1.25 s second-jump expiry. Remove the floor to
    // avoid landing resetting eligibility, then try a fresh late press.
    f.reset();f.settle();f.input.jump=1;f.step(6);f.input.jump=0;CHECK(rocket_world_mesh(f.world.get(),0,nullptr,0));f.step(45);f.input.jump=1;f.input.pitch=-1;f.step();CHECK(!f.state().flipped&&!f.state().double_jumped);
    CHECK(rocket_world_mesh(f.world.get(),0,floorMesh,2));
    // Cancellation must measurably reduce flip rotation against a control trace.
    Fixture cancelled,uncancelled;
    for(Fixture *q:{&cancelled,&uncancelled}){q->settle();q->input.jump=1;q->step(3);q->input.jump=0;q->step();q->input.jump=1;q->input.pitch=-1;q->step();q->input.jump=0;}
    cancelled.input.pitch=1;uncancelled.input.pitch=-1;cancelled.step(6);uncancelled.step(6);
    CHECK(std::fabs(cancelled.state().angular_velocity[0])<std::fabs(uncancelled.state().angular_velocity[0]));
    // All three air axes create rotation with no contact; neutral controls damp.
    for(int axis=0;axis<3;++axis){f.reset(5000);f.step();if(axis==0)f.input.pitch=1;if(axis==1)f.input.yaw=1;if(axis==2)f.input.roll=1;f.step(12);s=f.state();float length=0;for(float v:s.angular_velocity)length+=v*v;CHECK(length>.1f&&length<31.f);}
    // Steering and powerslide must produce distinct trajectories, not cosmetic wheels.
    a.reset();b.reset();a.settle();b.settle();a.input.throttle=b.input.throttle=1;a.step(35);b.step(35);
    a.input.steer=b.input.steer=.8f;b.input.powerslide=1;a.step(20);b.step(20);
    CHECK(std::fabs(a.state().position[0]-b.state().position[0])>10);
    // Side/back/diagonal dodge directions use separate impulses.
    for(int direction=0;direction<3;++direction){f.reset();f.settle();f.input.jump=1;f.step(3);f.input.jump=0;f.step();f.input.jump=1;f.input.pitch=direction==0?1.f:direction==2?-1.f:0.f;f.input.yaw=direction>0?1.f:0.f;f.step();s=f.state();CHECK(s.flipped);if(direction==0)CHECK(s.velocity[2]<-600);else CHECK(s.velocity[0]>500);}
    // Continuous curved host triangles: drive floor -> wall -> ceiling. This
    // exercises static-body contact classification and triangle seam correction.
    std::vector<RocketTriangle> track(floorMesh,floorMesh+2);
    auto strip=[&](float y0,float z0,float y1,float z1){
        track.push_back({{{-2500,y0,z0},{-2500,y1,z1},{2500,y1,z1}}});
        track.push_back({{{-2500,y0,z0},{2500,y1,z1},{2500,y0,z0}}});
    };
    for(int i=0;i<48;++i){float t0=3.14159265359f*i/48.f,t1=3.14159265359f*(i+1)/48.f;strip(600*(1-std::cos(t0)),600*std::sin(t0),600*(1-std::cos(t1)),600*std::sin(t1));}
    strip(1200,0,1200,-6000);
    Fixture wall;CHECK(rocket_world_mesh(wall.world.get(),0,track.data(),track.size()));
    float start[]={0,40,-1400},velocity[]={0,0,0};CHECK(rocket_world_reset(wall.world.get(),start,velocity,0));wall.settle();wall.input.throttle=1;wall.input.boost=1;
    bool wallContact=false,ceilingContact=false;float lowestUp=1;
    for(int i=0;i<105;++i){wall.step();s=wall.state();lowestUp=std::min(lowestUp,s.basis[7]);if(s.grounded&&std::fabs(s.basis[7])<.4f)wallContact=true;if(s.grounded&&s.basis[7]<-.7f)ceilingContact=true;}
    std::printf("curved track: wall=%d ceiling=%d minimum_up=%f\n",wallContact,ceilingContact,lowestUp);
    CHECK(wallContact&&ceilingContact);
    // Authored host faces meet at sharp concave edges (unlike RL's curved
    // arena). Two-sided triangle contacts used to push the car under this
    // ramp. Synthetic vertices only; no castle collision data is embedded.
    const RocketTriangle ramp[]={
        {{{-4000,0,-4000},{-4000,0,0},{4000,0,0}}},
        {{{-4000,0,-4000},{4000,0,0},{4000,0,-4000}}},
        {{{-4000,0,0},{-4000,1200,1000},{4000,1200,1000}}},
        {{{-4000,0,0},{4000,1200,1000},{4000,0,0}}},
        {{{-4000,1200,1000},{-4000,1200,4000},{4000,1200,4000}}},
        {{{-4000,1200,1000},{4000,1200,4000},{4000,1200,1000}}}
    };
    for(int direction=0;direction<18;direction++){
        int reverse=direction%2;
        float angle=(direction/6)*.4f,c=std::cos(angle),sn=std::sin(angle);
        std::vector<RocketTriangle> rotated(ramp,ramp+6);
        for(auto &triangle:rotated)for(auto &vertex:triangle.v){
            // Keep the finite test track's outer boundaries well away.
            for(int k:{0,2})if(std::fabs(vertex[k])==4000)vertex[k]*=5;
            float x=vertex[0],z=vertex[2];vertex[0]=c*x+sn*z;vertex[2]=-sn*x+c*z;
        }
        Fixture sharp;CHECK(rocket_world_mesh(sharp.world.get(),0,rotated.data(),rotated.size()));
        float p[]={0,reverse?1240.f:40.f,reverse?1600.f:-600.f},v[]={0,0,0};
        p[0]=sn*p[2];p[2]*=c;
        CHECK(rocket_world_reset(sharp.world.get(),p,v,(reverse?3.14159265359f:0)+(direction%6/2)*.35f+angle));
        sharp.settle();sharp.input.throttle=.4f;float high=0,low=2000;
        for(int i=0;i<160;i++){
            sharp.step();auto pose=sharp.state();
            float floor=std::fmin(std::fmax((sn*pose.position[0]+c*pose.position[2])*1.2f,0.f),1200.f);
            if(pose.position[1]<=floor-30)std::fprintf(stderr,"ramp direction=%d frame=%d position=(%f,%f,%f) floor=%f\n",direction,i,pose.position[0],pose.position[1],pose.position[2],floor);
            CHECK(pose.position[1]>floor-30);high=std::fmax(high,pose.position[1]);low=std::fmin(low,pose.position[1]);
        }
        CHECK(reverse?low<100:high>1000);
        // Replacement followed by reset must not leave contact markers/history.
        CHECK(rocket_world_mesh(sharp.world.get(),0,floorMesh,2));sharp.reset();sharp.settle();
    }
    // A wholly back-facing chassis below an authored floor must not be
    // pulled through it by an inverted contact or contaminate another world.
    Fixture underside,empty,other;
    underside.reset(-80);empty.reset(-80);CHECK(rocket_world_mesh(empty.world.get(),0,nullptr,0));other.settle();
    for(int i=0;i<20;i++){underside.step();empty.step();other.step();CHECK(same(underside.state(),empty.state()));CHECK(other.state().grounded);}
    // Replacing/removing a dynamic floor changes contact, without stale bodies.
    f.reset();CHECK(rocket_world_mesh(f.world.get(),0,nullptr,0));CHECK(rocket_world_mesh(f.world.get(),1,floorMesh,2));f.settle();CHECK(rocket_world_mesh(f.world.get(),1,nullptr,0));f.step(5);CHECK(!f.state().grounded&&f.state().velocity[1]<0);
    // Expiry correction is not a respawn/refill: spent fuel and air abilities
    // survive while the full clear chassis pose replaces penetrating contacts.
    Fixture phase;phase.settle();auto clear=phase.state();
    phase.input.throttle=1;phase.input.boost=1;phase.input.jump=1;phase.step(8);
    auto spent=phase.state();CHECK(spent.boost<99&&spent.jumped);
    CHECK(rocket_world_recover(phase.world.get(),&clear));auto recovered=phase.state();
    CHECK(recovered.boost==spent.boost&&recovered.ticks==spent.ticks&&recovered.jumped==spent.jumped);
    CHECK(recovered.jump_time==spent.jump_time&&recovered.flip_time==spent.flip_time&&recovered.air_time==spent.air_time);
    for(int k=0;k<3;k++)CHECK(recovered.position[k]==clear.position[k]&&recovered.velocity[k]==0&&recovered.angular_velocity[k]==0);
    for(int k=0;k<9;k++)CHECK(recovered.basis[k]==clear.basis[k]);
    phase.step();CHECK(phase.state().boost==spent.boost); // Held boost inhibited until release.
    CHECK(!rocket_world_recover(nullptr,&clear));CHECK(!rocket_world_recover(phase.world.get(),nullptr));
    auto invalid=clear;invalid.position[1]=std::numeric_limits<float>::quiet_NaN();
    CHECK(!rocket_world_recover(phase.world.get(),&invalid));invalid=clear;invalid.basis[0]=9;
    CHECK(!rocket_world_recover(phase.world.get(),&invalid));invalid=clear;
    for(int k=0;k<3;k++)invalid.basis[k]=-invalid.basis[k];
    CHECK(!rocket_world_recover(phase.world.get(),&invalid)); // Reflected basis rejected.
#ifdef ROCKET_TEST_HOST_MATH
    CHECK(rocket_poison_atan2f_calls==0);
#endif
    std::printf("PASS %d checks: native RocketSim component/integration contracts; NOT original Rocket League parity or SM64 gameplay\n",checks);
}
