#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <bit>
#include <fstream>
#include <algorithm>
#include <memory>
#include <vector>

static int checks;
static unsigned surfaceMode=ROCKET_SURFACES_CAR;
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s (%s)\n",__LINE__,#x,rocket_world_error());std::exit(1);}}while(0)
using World=std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)>;
static void tread(std::vector<RocketTriangle>& mesh,float y,float begin,float end) {
    mesh.push_back({{{-2000,y,begin},{2000,y,end},{2000,y,begin}},0});
    mesh.push_back({{{-2000,y,begin},{-2000,y,end},{2000,y,end}},0});
}
static std::vector<RocketTriangle> stairs(float rise,float run,int count) {
    std::vector<RocketTriangle> mesh;tread(mesh,0,-20000,0);
    for(int i=0;i<count;++i) {
        float y=i*rise,z=i*run;
        mesh.push_back({{{-2000,y,z},{-2000,y+rise,z},{2000,y+rise,z}},0});
        mesh.push_back({{{-2000,y,z},{2000,y+rise,z},{2000,y,z}},0});
        tread(mesh,y+rise,z,i==count-1?20000:z+run);
    }
    return mesh;
}
struct Fixture {
    World w{rocket_world_create(),rocket_world_destroy};uint64_t frame=0;RocketInput input{};
    Fixture(const std::vector<RocketTriangle>& mesh,float z=-500,float y=40,float yaw=0,int layer=0) {
        CHECK(w);
        rocket_world_set_surface_mode(w.get(),surfaceMode);
        float p[]={0,y,z},v[]={0,0,0};CHECK(rocket_world_reset(w.get(),p,v,yaw));
        if(layer<2)CHECK(rocket_world_mesh(w.get(),layer,mesh.data(),mesh.size()));
        else {RocketPlatform p{};p.object_id=7;p.basis[0]=p.basis[4]=p.basis[8]=1;
            p.triangles=mesh.data();p.count=mesh.size();CHECK(rocket_world_platforms(w.get(),&p,1));}
        step(30);
    }
    void step(int n=1) {for(int i=0;i<n;++i)CHECK(rocket_world_frame(w.get(),++frame,&input,0,0)==4);}
    RocketSnapshot state() {RocketSnapshot s{};CHECK(rocket_world_snapshot(w.get(),&s));return s;}
};
static void climb(float rise,float run,unsigned speed,int layer=0,float yaw=0,float throttle=1,bool follow=false) {
    Fixture f(stairs(rise,run,8),-500,40,yaw,layer);CHECK(rocket_world_set_speed(f.w.get(),speed));
    f.input.throttle=throttle;
    float minUp=1,maxRise=0;int frames=0;auto previous=f.state();
    while(previous.position[2]<run*8+150&&frames++<360) {
        // Driver feedback for the exhaustive speed sweep: steering is still
        // physical, and incidental yaw on a long flight must be correctable.
        // Keep the fixed-input cases above separate from this route follower.
        if(follow)f.input.steer=std::clamp(-2.f*std::atan2(previous.basis[0],previous.basis[2])-
            previous.position[0]/500.f,-1.f,1.f);
        f.step();auto s=f.state();minUp=std::fmin(minUp,s.basis[7]);
        maxRise=std::fmax(maxRise,s.position[1]-previous.position[1]);
        CHECK(s.boost==previous.boost);CHECK(!s.jumped&&!s.double_jumped&&!s.flipped);
        CHECK(s.ticks==previous.ticks+4);previous=s;
    }
    std::printf("climb rise=%.0f run=%.0f speed=%u layer=%d yaw=%.2f throttle=%.1f follow=%d frames=%d z=%.1f y=%.1f minUp=%.3f maxFrameRise=%.2f\n",
        rise,run,speed,layer,yaw,throttle,follow,frames,previous.position[2],previous.position[1],minUp,maxRise);
    std::printf("  support %d%d%d%d ground=%d vel=%.1f %.1f %.1f up=%.3f\n",previous.wheel_contacts[0],previous.wheel_contacts[1],previous.wheel_contacts[2],previous.wheel_contacts[3],previous.grounded,previous.velocity[0],previous.velocity[1],previous.velocity[2],previous.basis[7]);
    CHECK(previous.position[2]>=run*8+150);CHECK(minUp>.5f);
    // This is a 30 Hz observation (four substeps), including unmodified
    // suspension velocity. Bound visible ascent by the native floor buffer.
    CHECK(maxRise<=78);
    if(surfaceMode==ROCKET_SURFACES_NATIVE_NO_WALLS){
        // Riser tires no longer brake an airborne axle against a vertical
        // face. Test the actual landing as well as the first goal crossing;
        // preserve momentum instead of adding a mode-specific downward clamp.
        f.input={};f.step(30);previous=f.state();CHECK(previous.grounded);
        CHECK(!previous.jumped&&!previous.flipped);
        CHECK(std::fabs(previous.position[1]-rise*8-34)<5);
    }
    CHECK(std::fabs(previous.position[1]-rise*8-34)<65);
}
static uint64_t digest(uint64_t h,const RocketSnapshot &s) {
    auto mix=[&](float f){h^=std::bit_cast<uint32_t>(f);h*=1099511628211ull;};
    for(float f:s.position)mix(f);for(float f:s.velocity)mix(f);for(float f:s.basis)mix(f);
    for(float f:s.angular_velocity)mix(f);mix(s.boost);mix(s.jump_time);mix(s.flip_time);mix(s.air_time);
    mix(float(s.grounded));mix(float(s.jumped));mix(float(s.double_jumped));mix(float(s.flipping));
    for(int c:s.wheel_contacts)mix(float(c));return h;
}
// Same source can be linked to the prior adapter for an exact trajectory
// comparison. Hash explicit fields only, never struct padding or addresses.
static void invariants() {
    for(int scenario=0;scenario<10;++scenario) {
        auto mesh=stairs(scenario==1?80:scenario==2?102:51,102,scenario==3?1:8);
        if(scenario==0) {mesh.clear();tread(mesh,0,-20000,20000);}
        if(scenario==3) { // Riser with no upper support.
            mesh.resize(4);
        }
        if(scenario==4) { // Insufficient space for the whole car above the step.
            mesh.push_back({{{-2000,120,-2000},{2000,120,-2000},{2000,120,2000}},0});
            mesh.push_back({{{-2000,120,-2000},{2000,120,2000},{-2000,120,2000}},0});
        }
        if(scenario==5) {mesh.clear();tread(mesh,0,-20000,20000);
            for(auto &t:mesh)for(auto &v:t.v)v[1]=v[2]*.3f;}
        Fixture f(mesh,scenario==9?1100:-500,scenario==9?448:40,scenario==9?3.14159265f:0);
        f.input.throttle=scenario==6?0:1;
        if(scenario==7) {float p[]={0,1000,-100},v[]={0,0,400};CHECK(rocket_world_reset(f.w.get(),p,v,0));}
        if(scenario==8) {f.input.jump=1;f.step(5);f.input.jump=0;f.step(2);f.input.jump=1;f.input.pitch=-1;f.step();}
        uint64_t h=1469598103934665603ull;
        for(int i=0;i<(scenario==7||scenario==8?12:180);++i) {
            if(scenario==6) {RocketInput blocked{};blocked.throttle=1;CHECK(rocket_world_frame(f.w.get(),++f.frame,&blocked,0,1)==4);}
            else f.step();
            h=digest(h,f.state());
        }
        auto s=f.state();std::printf("invariant %d %016llx pos %.2f %.2f %.2f\n",scenario,(unsigned long long)h,s.position[0],s.position[1],s.position[2]);
    }
}
static void nativeGeometry(int argc,char **argv) {
    CHECK(argc==10||argc==11);std::ifstream file(argv[2]);CHECK(file.good());size_t count;file>>count;
    std::vector<RocketTriangle> mesh(count);for(auto &t:mesh)for(auto &v:t.v)for(float &f:v)file>>f;CHECK(file.good());
    Fixture f(mesh);float p[]={std::stof(argv[3]),std::stof(argv[4]),std::stof(argv[5])},v[]={0,0,0};
    CHECK(rocket_world_reset(f.w.get(),p,v,std::stof(argv[6])));f.step(30);
    f.input.throttle=std::stof(argv[8]);CHECK(rocket_world_set_speed(f.w.get(),std::stoul(argv[9])));
    float minUp=1,maxRise=0;auto prev=f.state();
    for(int i=0;i<std::stoi(argv[7]);++i) {f.step();auto s=f.state();minUp=std::fmin(minUp,s.basis[7]);
        maxRise=std::fmax(maxRise,s.position[1]-prev.position[1]);prev=s;
        if(argc==11&&s.position[2]>=std::stof(argv[10]))break;}
    std::printf("native geometry final %.2f %.2f %.2f minUp %.3f maxFrameRise %.2f\n",prev.position[0],prev.position[1],prev.position[2],minUp,maxRise);
    if(argc==11)CHECK(prev.position[2]>=std::stof(argv[10]));
}
int main(int argc,char **argv) {
    // Optional trailing rule exercises the same owned-geometry and synthetic
    // fixtures in each mode, without changing their established CLI.
    if(argc>2&&std::strcmp(argv[argc-2],"--surface")==0){surfaceMode=std::stoul(argv[argc-1]);CHECK(rocket_surface_valid(surfaceMode));argc-=2;}
    if(argc>1&&std::strcmp(argv[1],"--speed-sweep")==0) {
        for(unsigned speed=50;speed<=100;++speed)climb(51,102,speed,0,0,1,true);
        std::printf("stair speed sweep: %d checks passed\n",checks);return 0;
    }
    if(argc>1&&std::strcmp(argv[1],"--invariants")==0) {invariants();return 0;}
    if(argc>1&&std::strcmp(argv[1],"--geometry")==0) {nativeGeometry(argc,argv);return 0;}
    for(float rise:{25.f,26.f,51.f})for(unsigned speed:{50u,75u,100u})climb(rise,102,speed);
    for(int layer:{1,2})climb(51,102,100,layer);
    climb(51,205,100);climb(25,26,100);climb(51,102,100,0,.35f);
    climb(51,102,100,0,3.14159265f,-1);
    for(unsigned speed=50;speed<=100;++speed)climb(51,102,speed,0,0,1,true);
    invariants();
    std::printf("stair physics: %d checks passed\n",checks);
    return 0;
}
