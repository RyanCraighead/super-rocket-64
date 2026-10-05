#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
static int checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,rocket_world_error());std::exit(1);}}while(0)
struct Fixture {
    std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> w{rocket_world_create(),rocket_world_destroy};
    uint64_t frame=0;RocketInput input={};
    Fixture(){CHECK(w);reset();}
    void reset(float height=40){float p[]={0,height,0},v[]={0,0,0};CHECK(rocket_world_reset(w.get(),p,v,0));frame=0;input={};}
    void mesh(unsigned material,float grade=0,int layer=0){
        RocketTriangle floor[]={{{{-20000,-20000*grade,-20000},{20000,20000*grade,20000},{20000,-20000*grade,-20000}},(uint8_t)material},
            {{{-20000,-20000*grade,-20000},{-20000,20000*grade,20000},{20000,20000*grade,20000}},(uint8_t)material}};
        CHECK(rocket_world_mesh(w.get(),layer,floor,2));
    }
    void mode(unsigned mode){rocket_world_set_surface_mode(w.get(),mode);}
    void env(const RocketEnvironment &e){CHECK(rocket_world_set_environment(w.get(),&e));}
    void step(int n=1){while(n--)CHECK(rocket_world_frame(w.get(),++frame,&input,0,0)==4);}
    RocketSnapshot state(){RocketSnapshot s;CHECK(rocket_world_snapshot(w.get(),&s));return s;}
};
static void same(const RocketSnapshot &a,const RocketSnapshot &b){
    CHECK(a.ticks==b.ticks);for(int i=0;i<3;++i){CHECK(a.position[i]==b.position[i]);CHECK(a.velocity[i]==b.velocity[i]);}
    for(int i=0;i<9;++i)CHECK(a.basis[i]==b.basis[i]);
    CHECK(a.boost==b.boost&&a.grounded==b.grounded);
}
int main(){
    // Exact compatibility: material tags do not alter the default trajectory.
    Fixture normal,tagged,nativeNormal;
    normal.mesh(0);tagged.mesh(ROCKET_MATERIAL_VERY_SLIPPERY|ROCKET_MATERIAL_SLIDING);nativeNormal.mesh(0);nativeNormal.mode(1);
    for(int i=0;i<90;++i){
        normal.input.throttle=tagged.input.throttle=nativeNormal.input.throttle=i>30?1.f:0;
        normal.step();tagged.step();nativeNormal.step();same(normal.state(),tagged.state());same(normal.state(),nativeNormal.state());
    }
    // Native material changes tire propulsion; chassis and ordinary floor stay intact.
    Fixture car,ice,slippery,slide;
    car.mesh(0);ice.mesh(2);slippery.mesh(1);slide.mesh(4);
    ice.mode(1);slippery.mode(1);slide.mode(1);
    for(Fixture *f:{&car,&ice,&slippery,&slide}){f->step(30);f->input.throttle=1;f->step(30);}
    CHECK(car.state().position[2]>slippery.state().position[2]);
    CHECK(slippery.state().position[2]>ice.state().position[2]);
    CHECK(std::fabs(slide.state().position[2])<1);
    // Two tires on each material: classify each actual wheel triangle, never
    // apply the chassis-center material as a blanket friction multiplier.
    Fixture mixed;RocketTriangle halves[]={
        {{{-20000,0,-20000},{0,0,20000},{0,0,-20000}},0},
        {{{-20000,0,-20000},{-20000,0,20000},{0,0,20000}},0},
        {{{0,0,-20000},{20000,0,20000},{20000,0,-20000}},4},
        {{{0,0,-20000},{0,0,20000},{20000,0,20000}},4}};
    CHECK(rocket_world_mesh(mixed.w.get(),0,halves,4));mixed.mode(1);mixed.step(30);mixed.input.throttle=1;mixed.step(15);
    CHECK(mixed.state().position[2]>1&&mixed.state().position[2]<car.state().position[2]);
    halves[0].material=255;CHECK(!rocket_world_mesh(mixed.w.get(),0,halves,4));
    for(unsigned tag:{8u,9u,10u,11u,12u,13u,15u,16u}){
        halves[0].material=(uint8_t)tag;
        CHECK(!rocket_world_mesh(mixed.w.get(),0,halves,4));
        RocketPlatform platform={};platform.object_id=123;
        platform.basis[0]=platform.basis[4]=platform.basis[8]=1;
        platform.triangles=halves;platform.count=4;
        CHECK(!rocket_world_platforms(mixed.w.get(),&platform,1));
    }
    mixed.step();CHECK(std::isfinite(mixed.state().position[2]));
    // Real uphill grade: threshold-tagged native floor prevents ordinary tire climbing.
    Fixture rampCar,rampSlide;rampCar.mesh(6,.3f);rampSlide.mesh(6,.3f);rampSlide.mode(1);
    for(Fixture *f:{&rampCar,&rampSlide}){f->step(30);f->input.throttle=1;f->step(60);}
    CHECK(rampCar.state().position[2]>rampSlide.state().position[2]+100.f);
    // The scoped ice race retains real steering and braking on a downhill
    // grade. Compare identical states/inputs against zero grip and Car grip.
    Fixture raceCoast,raceBrake,raceSteer,zeroBrake,zeroSteer,fullBrake;
    for(Fixture *f:{&raceCoast,&raceBrake,&raceSteer,&zeroBrake,&zeroSteer,&fullBrake}){
        f->mesh(f==&zeroBrake||f==&zeroSteer?6:14,-.3f);
        f->mode(f==&fullBrake?0:1);
        float p[]={0,80,0},v[]={0,0,1400};
        CHECK(rocket_world_reset(f->w.get(),p,v,0));f->step(10);
    }
    raceBrake.input.throttle=zeroBrake.input.throttle=fullBrake.input.throttle=-1;
    raceSteer.input.steer=zeroSteer.input.steer=1;
    for(Fixture *f:{&raceCoast,&raceBrake,&raceSteer,&zeroBrake,&zeroSteer,&fullBrake})f->step(30);
    auto coast=raceCoast.state(),brake=raceBrake.state(),steer=raceSteer.state();
    auto zero=zeroBrake.state(),full=fullBrake.state();
    std::printf("race speeds coast=%.1f brake=%.1f zero=%.1f full=%.1f; turn x=%.1f zero=%.1f\n",
        coast.velocity[2],brake.velocity[2],zero.velocity[2],full.velocity[2],steer.position[0],zeroSteer.state().position[0]);
    CHECK(brake.velocity[2]<coast.velocity[2]-100);
    CHECK(brake.velocity[2]<zero.velocity[2]-100);
    CHECK(brake.velocity[2]>full.velocity[2]+100); // still substantially slippery
    CHECK(std::fabs(steer.position[0])>std::fabs(zeroSteer.state().position[0])+20);
    // The tag never changes explicit Car grip; leaving/replacing race geometry
    // removes the behavior immediately without a global persistent override.
    Fixture scopedCar,plainCar;
    scopedCar.mesh(14,-.3f);plainCar.mesh(6,-.3f);
    for(int i=0;i<60;++i){scopedCar.input.throttle=plainCar.input.throttle=1;
        scopedCar.step();plainCar.step();same(scopedCar.state(),plainCar.state());}
    raceCoast.mesh(6,-.3f);raceCoast.reset();zeroBrake.reset();
    raceCoast.step(30);zeroBrake.step(30);same(raceCoast.state(),zeroBrake.state());
    raceCoast.mesh(14,-.3f);raceCoast.reset();raceBrake.reset();
    raceCoast.step(30);raceBrake.step(30);same(raceCoast.state(),raceBrake.state());
    // Live mode and material transitions, including a replaced dynamic layer.
    slide.mode(0);slide.step(30);CHECK(slide.state().position[2]>100);
    slide.mode(1);slide.mesh(0);slide.step(30);CHECK(slide.state().velocity[2]>100);
    Fixture dynamic;dynamic.mesh(4,0,1);dynamic.mode(1);dynamic.step(30);dynamic.input.throttle=1;dynamic.step(20);
    CHECK(std::fabs(dynamic.state().position[2])<1);dynamic.mesh(0,0,1);dynamic.step(20);CHECK(dynamic.state().position[2]>50);
    // Currents advect, never become cumulative acceleration or survive exit/reset.
    Fixture still,flow;still.reset(10000);flow.reset(10000);
    RocketEnvironment current={{120,0,-60},{0,0,0}};flow.env(current);
    for(int i=0;i<30;++i){still.step();flow.step();}
    auto a=still.state(),b=flow.state();CHECK(std::fabs(b.position[0]-a.position[0]-120)<.01f);
    CHECK(std::fabs(b.position[2]-a.position[2]+60)<.01f);CHECK(std::fabs(b.velocity[0])<.001f);
    flow.env({});flow.step();CHECK(std::fabs(flow.state().position[0]-b.position[0])<.001f);
    // Pause, duplicate frames, blocked controls, invalid samples, and reset.
    flow.env(current);auto paused=flow.state();CHECK(!rocket_world_frame(flow.w.get(),++flow.frame,&flow.input,1,0));same(paused,flow.state());
    CHECK(!rocket_world_frame(flow.w.get(),flow.frame,&flow.input,0,0));same(paused,flow.state());
    CHECK(rocket_world_frame(flow.w.get(),++flow.frame,&flow.input,0,1)==4);CHECK(flow.state().position[0]>paused.position[0]);
    RocketEnvironment bad=current;bad.drift[0]=std::numeric_limits<float>::infinity();CHECK(!rocket_world_set_environment(flow.w.get(),&bad));
    float x=flow.state().position[0];flow.step();CHECK(std::fabs(flow.state().position[0]-x)<.001f);
    flow.env(current);flow.reset(10000);still.reset(10000);flow.step();still.step();same(flow.state(),still.state());
    // Updrafts are real velocity acceleration, integrated at all four substeps.
    RocketEnvironment wind={{0,0,0},{0,4500,0}};flow.env(wind);flow.step();still.step();
    CHECK(std::fabs(flow.state().velocity[1]-still.state().velocity[1]-150.f)<.01f);
    CHECK(flow.state().position[1]>still.state().position[1]);
    std::printf("environment physics: %d checks passed\n",checks);
}
