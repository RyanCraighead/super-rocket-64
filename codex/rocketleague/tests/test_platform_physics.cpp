#include "../physics/rocket_physics.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

static int checks;
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s (%s)\n",__LINE__,#x,rocket_world_error());std::exit(1);}}while(0)
using World=std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)>;
static constexpr float identity[9]={1,0,0,0,1,0,0,0,1};

static std::vector<RocketTriangle> floorTriangles(float half=700.f) {
    return {
        {{{-half,0,-half},{half,0,half},{half,0,-half}}},
        {{{-half,0,-half},{-half,0,half},{half,0,half}}}
    };
}
static RocketPlatform platform(uint64_t id,const std::vector<RocketTriangle> &triangles,
                               float x=0,float y=0,float z=0,const float *basis=identity) {
    RocketPlatform p{};p.object_id=id;p.position[0]=x;p.position[1]=y;p.position[2]=z;
    for(int i=0;i<9;++i)p.basis[i]=basis[i];
    p.triangles=triangles.data();p.count=triangles.size();return p;
}
static float speed(const RocketSnapshot &s) {
    return std::sqrt(s.velocity[0]*s.velocity[0]+s.velocity[1]*s.velocity[1]+s.velocity[2]*s.velocity[2]);
}
struct Fixture {
    World world{rocket_world_create(),rocket_world_destroy};
    uint64_t frame=0;
    RocketInput input{};
    Fixture(){CHECK(world);reset();}
    void reset(float x=0,float y=40,float z=0) {
        float p[3]={x,y,z},v[3]={0,0,0};CHECK(rocket_world_reset(world.get(),p,v,0));frame=0;input={};
    }
    RocketSnapshot state() {
        RocketSnapshot s{};CHECK(rocket_world_snapshot(world.get(),&s));return s;
    }
    void step(const RocketPlatform *platforms,size_t count) {
        CHECK(rocket_world_platforms(world.get(),platforms,count));
        CHECK(rocket_world_frame(world.get(),++frame,&input,0,0)==ROCKET_SUBSTEPS);
    }
    void settle(const RocketPlatform &p,int frames=40) {
        for(int i=0;i<frames;++i)step(&p,1);
        CHECK(state().grounded);
    }
};

static void staticBaselineAndElevator() {
    Fixture fixed,carrier;
    auto floor=floorTriangles(1600);
    CHECK(rocket_world_mesh(fixed.world.get(),0,floor.data(),floor.size()));
    RocketPlatform p=platform(1,floor);
    for(int i=0;i<45;++i){carrier.step(&p,1);CHECK(rocket_world_frame(fixed.world.get(),++fixed.frame,&fixed.input,0,0)==4);}
    auto a=fixed.state(),b=carrier.state();
    CHECK(a.grounded&&b.grounded);
    CHECK(std::fabs(a.position[1]-b.position[1])<3.f);
    CHECK(speed(a)<20&&speed(b)<20);

    Fixture lateral;
    RocketPlatform moving=platform(20,floor);
    lateral.settle(moving);
    float startX=lateral.state().position[0];
    for(int i=1;i<=60;++i){moving.position[0]=1.5f*float(i);lateral.step(&moving,1);}
    auto carried=lateral.state();
    CHECK(carried.position[0]>startX+15.f);
    CHECK(std::isfinite(carried.velocity[0])&&std::fabs(carried.velocity[0])<250.f);

    // A rising support moves the raycast contact and contributes its support
    // velocity to suspension damping, so the car rides the elevator.
    float start=b.position[1];
    for(int i=1;i<=60;++i){p.position[1]=float(i);carrier.step(&p,1);}
    b=carrier.state();
    CHECK(b.position[1]>start+25.f);
    CHECK(std::isfinite(b.velocity[1])&&std::fabs(b.velocity[1])<250.f);
}

static void seesawAndPendulumStrike() {
    Fixture seesaw;
    auto plank=floorTriangles(1200);
    RocketPlatform p=platform(2,plank);
    seesaw.settle(p);
    float angle=0;
    for(int i=1;i<=48;++i){
        angle=(12.f*3.14159265359f/180.f)*float(i)/48.f;
        float c=std::cos(angle),s=std::sin(angle);
        const float basis[9]={1,0,0,0,c,s,0,-s,c};
        p=platform(2,plank,0,0,0,basis);seesaw.step(&p,1);
    }
    auto tilt=seesaw.state();
    CHECK(tilt.grounded&&std::fabs(tilt.basis[8])>.08f);

    // Rotating vertical panel approximates a pendulum arm striking the car.
    Fixture strike;
    auto ground=floorTriangles(3000);
    CHECK(rocket_world_mesh(strike.world.get(),0,ground.data(),ground.size()));
    std::vector<RocketTriangle> wall={
        {{{0,-100,-500},{0,300,500},{0,300,-500}}},
        {{{0,-100,-500},{0,-100,500},{0,300,500}}}
    };
    RocketPlatform arm=platform(3,wall,140,110,0);
    strike.settle(platform(4,ground),35);
    for(int i=1;i<=48;++i){
        float theta=(42.f*3.14159265359f/180.f)*float(i)/48.f,c=std::cos(theta),s=std::sin(theta);
        const float basis[9]={c,0,-s,0,1,0,s,0,c};
        arm=platform(3,wall,140,110,0,basis);strike.step(&arm,1);
    }
    auto hit=strike.state();
    CHECK(speed(hit)>50.f);
    CHECK(std::fabs(hit.position[0])+std::fabs(hit.position[2])>20.f);
}

static void distantReplacementUnloadResetAndDuplicate() {
    Fixture f;
    auto ground=floorTriangles(3000);
    CHECK(rocket_world_mesh(f.world.get(),0,ground.data(),ground.size()));
    auto farMesh=floorTriangles(300);
    RocketPlatform distant=platform(9,farMesh,10000,0,10000);
    for(int i=0;i<50;++i){
        distant.position[0]+=35.f;
        f.step(&distant,1);
    }
    auto still=f.state();CHECK(still.grounded&&speed(still)<20.f);

    // A same-ID geometry replacement preserves contact; small inverse-transform
    // noise is below tolerance and does not rebuild the triangle shape.
    RocketPlatform local=platform(10,ground);
    for(int i=0;i<35;++i)f.step(&local,1);
    CHECK(f.state().grounded);
    auto replacement=floorTriangles(3200);
    local=platform(10,replacement);
    f.step(&local,1);
    auto replaced=f.state();CHECK(replaced.grounded&&speed(replaced)<100.f);
    CHECK(rocket_world_platforms(f.world.get(),nullptr,0));
    CHECK(rocket_world_frame(f.world.get(),++f.frame,&f.input,0,0)==4);
    CHECK(f.state().grounded); // static floor still owns support after unload

    // Large pose discontinuity rebases instead of assigning fictitious velocity.
    local=platform(10,replacement,0,4000,0);
    f.step(&local,1);
    CHECK(speed(f.state())<150.f);

    // Reset removes old platform bodies. Re-register before the first new-epoch
    // frame, then verify duplicate frame IDs neither tick nor replay motion.
    f.reset();local=platform(11,replacement);
    CHECK(rocket_world_platforms(f.world.get(),&local,1));
    CHECK(rocket_world_frame(f.world.get(),1,&f.input,0,0)==4);
    auto before=f.state();
    local.position[1]=20.f;CHECK(rocket_world_platforms(f.world.get(),&local,1));
    CHECK(rocket_world_frame(f.world.get(),1,&f.input,0,0)==0);
    auto duplicate=f.state();
    CHECK(before.ticks==duplicate.ticks&&before.position[1]==duplicate.position[1]);
    CHECK(speed(duplicate)<150.f);
}

static void dynamicOnlyLifecycleAndMotionBounds() {
    Fixture f;
    auto original=floorTriangles(700);
    RocketPlatform support=platform(30,original);
    f.settle(support);
    auto enlarged=floorTriangles(900);
    support=platform(30,enlarged);
    for(int i=0;i<3;++i)f.step(&support,1);
    auto replaced=f.state();CHECK(replaced.grounded&&speed(replaced)<100.f);

    CHECK(rocket_world_platforms(f.world.get(),nullptr,0));
    CHECK(rocket_world_frame(f.world.get(),++f.frame,&f.input,0,0)==ROCKET_SUBSTEPS);
    auto unloaded=f.state();CHECK(!unloaded.grounded&&unloaded.velocity[1]<0&&speed(unloaded)<100.f);

    // Reset clears the previous object's body and motion history. The first
    // frame is unsupported until the host submits a fresh object snapshot.
    support=platform(31,original);
    for(int i=0;i<40;++i)f.step(&support,1);
    CHECK(f.state().grounded);
    f.reset();
    CHECK(rocket_world_frame(f.world.get(),++f.frame,&f.input,0,0)==ROCKET_SUBSTEPS);
    auto afterReset=f.state();CHECK(!afterReset.grounded&&afterReset.velocity[1]<0&&speed(afterReset)<100.f);
    support=platform(31,original);
    f.step(&support,1);
    auto beforeDuplicate=f.state();
    support.position[1]=20.f;CHECK(rocket_world_platforms(f.world.get(),&support,1));
    CHECK(rocket_world_frame(f.world.get(),f.frame,&f.input,0,0)==0);
    auto afterDuplicate=f.state();
    CHECK(afterDuplicate.ticks==beforeDuplicate.ticks&&afterDuplicate.position[1]==beforeDuplicate.position[1]);
    CHECK(speed(afterDuplicate)<150.f);

    // A just-under-threshold translation still yields finite, bounded support
    // velocity (120 host units over one 30 Hz frame).
    Fixture bounded;
    support=platform(32,original);
    bounded.settle(support);
    support.position[0]=120.f;bounded.step(&support,1);
    auto nearCutoff=bounded.state();
    CHECK(std::isfinite(speed(nearCutoff))&&speed(nearCutoff)<7000.f);
}

static void combinedMaterialWaterAndEnvironment() {
    auto triangles=floorTriangles(10000);for(auto &t:triangles)t.material=ROCKET_MATERIAL_SLIDING;
    Fixture native,compat;auto p=platform(91,triangles);
    rocket_world_set_surface_mode(native.world.get(),ROCKET_SURFACES_NATIVE);
    native.settle(p);compat.settle(p);native.input.throttle=compat.input.throttle=1;
    for(int i=0;i<30;i++){native.step(&p,1);compat.step(&p,1);}
    CHECK(std::fabs(native.state().position[2])<1&&compat.state().position[2]>100);
    for(auto &t:triangles)t.material=ROCKET_MATERIAL_NORMAL;
    for(int i=0;i<20;i++)native.step(&p,1);
    CHECK(native.state().position[2]>50); // Same ID and vertices; material-only change invalidates cache.
    triangles[0].material=7;CHECK(!rocket_world_platforms(native.world.get(),&p,1));
    triangles[0].material=6;CHECK(rocket_world_platforms(native.world.get(),&p,1));

    Fixture still,flow;still.reset(0,10000,0);flow.reset(0,10000,0);
    RocketEnvironment current={{120,0,-60},{0,0,0}};
    CHECK(rocket_world_set_environment(flow.world.get(),&current));
    for(int i=0;i<30;i++){p.position[1]=float(i);still.step(&p,1);flow.step(&p,1);}
    auto a=still.state(),b=flow.state();CHECK(a.ticks==120&&b.ticks==120);
    CHECK(std::fabs(b.position[0]-a.position[0]-120)<.01f);
    CHECK(std::fabs(b.position[2]-a.position[2]+60)<.01f);
    CHECK(std::fabs(b.velocity[0])<.001f); // Native drift was applied once, then removed.

    Fixture metal;for(auto &t:triangles)t.material=0;p=platform(92,triangles);
    rocket_world_set_water(metal.world.get(),1,10000,1);metal.settle(p);
    CHECK(metal.state().water_mode==ROCKET_WATER_METAL);float before=metal.state().position[1];
    rocket_world_set_temporary_boost(metal.world.get(),1);metal.input.boost=1;
    for(int i=0;i<30;i++){p.position[1]+=1;metal.step(&p,1);}
    CHECK(metal.state().grounded&&metal.state().position[1]>before+10);
    CHECK(metal.state().boost==100); // Wing allowance never changes the underlying finite tank.
    rocket_world_set_temporary_boost(metal.world.get(),0);rocket_world_set_water(metal.world.get(),1,10000,0);
    metal.input={};metal.input.jump=1;
    for(int i=0;i<30;i++)metal.step(&p,1);
    CHECK(metal.state().water_mode==ROCKET_WATER_JET&&!metal.state().grounded);
    CHECK(metal.state().position[1]>p.position[1]+200&&metal.state().boost==100);
}

int main() {
    combinedMaterialWaterAndEnvironment();
    staticBaselineAndElevator();
    seesawAndPendulumStrike();
    distantReplacementUnloadResetAndDuplicate();
    dynamicOnlyLifecycleAndMotionBounds();
    std::printf("PASS %d checks: object-keyed kinematic platform physics\n",checks);
}
