/* Real suspension/chassis physics. No window, controller, saves or sockets. */
#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <algorithm>
static int checks;
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,rocket_world_error());std::exit(1);}}while(0)
constexpr float pi=3.14159265359f;
struct Fixture {
    std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> w{rocket_world_create(),rocket_world_destroy};
    uint64_t frame=0;RocketInput input{};
    Fixture(unsigned mode,float angle,int layer=0,unsigned material=0,bool reverse=false) {
        CHECK(w);rocket_world_set_surface_mode(w.get(),mode);
        float c=std::cos(angle),s=std::sin(angle);
        float p[]={0,5000+34*c,-34*s},v[]={0,0,0};CHECK(rocket_world_reset(w.get(),p,v,0));
        // A native authored front face rotated from a floor around host X.
        RocketTriangle mesh[]={{{{-20000,0,-20000},{20000,0,20000},{20000,0,-20000}},(uint8_t)material},
            {{{-20000,0,-20000},{-20000,0,20000},{20000,0,20000}},(uint8_t)material}};
        if(layer==2) {
            RocketPlatform p{};p.object_id=123;p.position[1]=5000;
            p.basis[0]=1;p.basis[4]=c;p.basis[5]=-s;p.basis[7]=s;p.basis[8]=c;
            p.triangles=mesh;p.count=2;CHECK(rocket_world_platforms(w.get(),&p,1));
        } else {
            for(auto &t:mesh)for(auto &v:t.v){float z=v[2];v[1]=5000+s*z;v[2]=c*z;}
            CHECK(rocket_world_mesh(w.get(),layer,mesh,2));
        }
        RocketSnapshot pose{};std::copy(p,p+3,pose.position);
        float sign=reverse?-1.f:1.f;
        pose.basis[1]=s*sign;pose.basis[2]=c*sign;pose.basis[3]=sign;pose.basis[7]=c;pose.basis[8]=-s;
        CHECK(rocket_world_recover(w.get(),&pose));
    }
    void step(int n=1){while(n--)CHECK(rocket_world_frame(w.get(),++frame,&input,0,0)==4);}
    RocketSnapshot state(){RocketSnapshot s{};CHECK(rocket_world_snapshot(w.get(),&s));
        for(float v:s.position)CHECK(std::isfinite(v));for(float v:s.velocity)CHECK(std::isfinite(v));return s;}
};
static void same(const RocketSnapshot&a,const RocketSnapshot&b){
    for(int i=0;i<3;i++){CHECK(a.position[i]==b.position[i]);CHECK(a.velocity[i]==b.velocity[i]);CHECK(a.angular_velocity[i]==b.angular_velocity[i]);}
    for(int i=0;i<9;i++)CHECK(a.basis[i]==b.basis[i]);
    for(int i=0;i<4;i++)CHECK(a.wheel_contacts[i]==b.wheel_contacts[i]);
    CHECK(a.boost==b.boost&&a.grounded==b.grounded&&a.jumped==b.jumped&&a.flipped==b.flipped);
}
int main(){
    // Native walls-on and Octane continue climbing. New walls-off tires cannot
    // propel or brake against a wall, including reverse and object-keyed faces.
    for(int layer:{0,1,2})for(bool reverse:{false,true}){
        Fixture off(2,pi/2,layer,0,reverse),on(1,pi/2,layer,0,reverse),car(0,pi/2,layer,0,reverse);
        for(Fixture*f:{&off,&on,&car}){f->step();f->input.throttle=reverse?-1:1;}
        int onSupport=0;
        for(int i=0;i<24;i++){
            off.step();on.step();car.step();auto a=off.state(),b=on.state();
            CHECK(!a.grounded);for(int contact:a.wheel_contacts)CHECK(!contact);
            onSupport+=b.grounded;same(b,car.state());
        }
        auto a=off.state(),b=on.state();
        std::printf("wall layer=%d reverse=%d offY=%.1f onY=%.1f offVy=%.1f support=%d\n",layer,reverse,a.position[1],b.position[1],a.velocity[1],onSupport);
        // Kinematic bodies use the backend's different world-contact/sticky
        // path, but the existing modes must still have actual tire support.
        CHECK(onSupport>12);CHECK(a.position[1]<4800&&a.velocity[1]<-500);CHECK(b.position[1]>5100);
        CHECK(a.boost==100&&!a.jumped&&!a.flipped);
        // Host changes the live rule; no reset, refill, boost or stale support.
        rocket_world_set_surface_mode(on.w.get(),2);float y=b.position[1];on.input={};on.step(75);
        CHECK(!on.state().grounded&&on.state().velocity[1]<-700&&on.state().position[1]<y);
    }
    // Ceiling adhesion is wall driving too. Turning it off resumes freefall.
    Fixture ceiling(2,pi),ceilingOn(1,pi);
    ceiling.input.throttle=ceilingOn.input.throttle=1;
    int ceilingSupport=0;
    for(int i=0;i<15;i++){ceiling.step();ceilingOn.step();CHECK(!ceiling.state().grounded);ceilingSupport+=ceilingOn.state().grounded;}
    CHECK(ceiling.state().velocity[1]<-500);CHECK(ceilingSupport>0);
    // Ordinary slopes and slippery floors have exactly the same trajectory in
    // both native modes; world-space support works on rotated platforms too.
    for(int layer:{0,1,2})for(float angle:{0.f,.3f,.7f})for(unsigned material:{0u,1u,2u,5u,6u,14u}){
        Fixture off(2,angle,layer,material),on(1,angle,layer,material);
        for(int i=0;i<80;i++){
            off.input.throttle=on.input.throttle=i>10?1.f:0;
            off.input.steer=on.input.steer=i>50?.4f:0;
            off.input.powerslide=on.input.powerslide=i>65;
            off.step();on.step();same(off.state(),on.state());
        }
    }
    // Collision still stops the chassis at a vertical face while floor tires
    // support the car. A wall-off switch never removes the collision mesh.
    Fixture floor(2,0);RocketTriangle wall[]={{{{-3000,5000,300},{-3000,9000,300},{3000,9000,300}},0},
        {{{-3000,5000,300},{3000,9000,300},{3000,5000,300}},0}};
    CHECK(rocket_world_mesh(floor.w.get(),1,wall,2));floor.step(20);floor.input.throttle=1;floor.step(120);
    CHECK(floor.state().position[2]<300&&floor.state().position[1]<5150);
    floor.input.throttle=-1;floor.step(45);CHECK(floor.state().position[2]<0); // reverse exit
    // Real airborne control is preserved after losing wall support. No mode
    // clamps upward momentum; rocket boost remains an airborne thruster.
    for(int axis=0;axis<3;axis++){
        Fixture air(2,pi/2);air.step();
        if(axis==0)air.input.pitch=1;if(axis==1)air.input.yaw=1;if(axis==2)air.input.roll=1;
        air.step(10);auto s=air.state();float n=0;for(float v:s.angular_velocity)n+=v*v;
        CHECK(n>.1f&&!s.grounded&&s.velocity[1]<0);
    }
    Fixture thrust(2,pi/2);thrust.step();thrust.input.boost=1;thrust.step(12);
    CHECK(thrust.state().boost<100&&thrust.state().boosting&&!thrust.state().grounded);
    std::printf("surface modes physics: %d checks passed\n",checks);
}
