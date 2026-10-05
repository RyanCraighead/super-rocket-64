#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
static int checks;
#define CHECK(x) do{checks++;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Car {
    std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> world{rocket_world_create(),rocket_world_destroy};
    RocketInput input={};RocketSnapshot state={};uint64_t frame=0;
    Car(){CHECK(world);reset();}
    void reset(float height=1000){float p[]={0,height,0},v[]={0,0,0};CHECK(rocket_world_reset(world.get(),p,v,0));frame=0;input={};step();}
    void step(int frames=1){while(frames--){CHECK(rocket_world_frame(world.get(),++frame,&input,0,0)==4);CHECK(rocket_world_snapshot(world.get(),&state));}}
    void water(int metal=0,float level=10000){rocket_world_set_water(world.get(),1,level,metal);}
    void dry(){rocket_world_set_water(world.get(),0,0,0);}
};
static int boundedWater(float x,float z,float *level){(void)x;*level=10000;return z<50;}
int main(){
    CHECK(rocket_water_classify(0,1,200,99,0)==ROCKET_WATER_JET);
    CHECK(rocket_water_classify(0,1,200,100,0)==ROCKET_WATER_DRY);
    CHECK(rocket_water_classify(1,1,200,119,0)==ROCKET_WATER_JET);
    CHECK(rocket_water_classify(1,1,200,120,0)==ROCKET_WATER_DRY);
    CHECK(rocket_water_classify(1,1,200,90,1)==ROCKET_WATER_METAL);
    CHECK(rocket_water_classify(2,1,200,90,0)==ROCKET_WATER_JET);
    CHECK(!rocket_water_classify(1,0,200,0,0));
    CHECK(!rocket_water_classify(1,1,NAN,0,0));
    CHECK(!rocket_water_classify(1,1,200,INFINITY,0));

    Car car;car.input.boost=1;car.step(35);float tank=car.state.boost;
    CHECK(tank>0&&tank<100);car.reset();CHECK(car.state.boost==tank);
    car.water();car.input.boost=1;car.step(180);
    CHECK(car.state.water_mode==ROCKET_WATER_JET&&car.state.boost==tank);
    CHECK(car.state.velocity[2]>1000&&car.state.position[1]>1000);
    car.input.pitch=.7f;car.input.yaw=.6f;car.input.roll=.5f;car.step(30);
    CHECK(std::fabs(car.state.basis[7]-1)>.1f);CHECK(car.state.boost==tank);
    auto paused=car.state;
    CHECK(!rocket_world_frame(car.world.get(),++car.frame,&car.input,1,0));
    CHECK(rocket_world_snapshot(car.world.get(),&car.state));
    CHECK(car.state.ticks==paused.ticks&&car.state.position[1]==paused.position[1]&&car.state.boost==tank);
    CHECK(!rocket_world_frame(car.world.get(),car.frame,&car.input,0,0));
    car.input={};car.step();car.dry();car.input.boost=1;car.step(120);
    CHECK(car.state.water_mode==ROCKET_WATER_DRY&&car.state.boost==0);
    car.reset();car.water();car.input.boost=1;car.step(60);
    CHECK(car.state.boost==0&&car.state.velocity[2]>1000); // empty tank still jets
    for(int i=0;i<20;i++){car.dry();car.water();car.step();CHECK(car.state.boost==0);}
    CHECK(rocket_world_collect_coin(car.world.get()));car.step();CHECK(car.state.boost==5);
    car.dry();CHECK(rocket_world_set_boost_mode(car.world.get(),ROCKET_BOOST_INFINITE));car.step(40);CHECK(car.state.boost==5);
    car.water();car.step(40);CHECK(car.state.boost==5);
    car.dry();car.step(40);CHECK(car.state.boost==5);
    CHECK(rocket_world_set_boost_mode(car.world.get(),ROCKET_BOOST_COIN_ONLY));car.step(40);CHECK(car.state.boost==0);

    // Native current is drag-relative flow; heavy Metal does not inherit it.
    Car swim,metal,heavyControl;const float flow[]={600,0,0};
    swim.water();metal.water(1);
    rocket_world_set_water_current(swim.world.get(),flow);rocket_world_set_water_current(metal.world.get(),flow);
    swim.step(60);metal.step(60);heavyControl.step(60);
    CHECK(swim.state.velocity[0]>300);CHECK(std::fabs(metal.state.velocity[0])<.01f);
    CHECK(metal.state.position[1]>heavyControl.state.position[1]);
    CHECK(std::fabs(metal.state.velocity[1]+540.f)<.1f); // native Metal terminal fall, 18 units/frameCHECK(swim.state.position[1]>1000);
    metal.input.jump=1;metal.step(1); // ordinary metal car jump, never held swim lift
    CHECK(metal.state.water_mode==ROCKET_WATER_METAL);
    metal.step(30);CHECK(metal.state.velocity[1]<0); // held jump cannot sustain jet lift
    metal.water(0);metal.input={};metal.step(180);CHECK(metal.state.water_mode==ROCKET_WATER_JET);
    CHECK(metal.state.velocity[1]>0); // cap expiry resumes buoyancy at current depth

    // Only jet swimming borrows boost. Metal obeys the finite tank unless
    // the host or Wing explicitly grants unlimited boost.
    Car metalFuel;metalFuel.water(1);metalFuel.input.boost=1;metalFuel.step(30);
    CHECK(metalFuel.state.water_mode==ROCKET_WATER_METAL);
    CHECK(metalFuel.state.boost>0&&metalFuel.state.boost<90);
    float metalTank=metalFuel.state.boost;
    metalFuel.water(0);metalFuel.step(30);
    CHECK(metalFuel.state.water_mode==ROCKET_WATER_JET&&metalFuel.state.boost==metalTank);
    metalFuel.water(1);metalFuel.step(150);CHECK(metalFuel.state.boost==0);
    metalFuel.reset();metalFuel.water(1);metalFuel.input.boost=1;metalFuel.step(45);
    CHECK(metalFuel.state.boost==0&&std::fabs(metalFuel.state.velocity[2])<.01f);
    CHECK(rocket_world_set_boost_mode(metalFuel.world.get(),ROCKET_BOOST_INFINITE));
    metalFuel.step(30);CHECK(metalFuel.state.boost==0&&metalFuel.state.velocity[2]>500);
    CHECK(metalFuel.state.water_mode==ROCKET_WATER_METAL&&metalFuel.state.velocity[1]<0);
    CHECK(rocket_world_set_boost_mode(metalFuel.world.get(),ROCKET_BOOST_COIN_ONLY));
    metalFuel.water(0);metalFuel.step(30);CHECK(metalFuel.state.boost==0);
    // Wing is a temporary allowance, independent of the coin-only preference.
    // Coins gained during Wing still fill the hidden tank; expiry restores it.
    metalFuel.reset();metalFuel.water(1);
    rocket_world_set_temporary_boost(metalFuel.world.get(),1);
    CHECK(rocket_world_boost_mode(metalFuel.world.get())==ROCKET_BOOST_COIN_ONLY);
    metalFuel.input.boost=1;metalFuel.step(30);
    CHECK(metalFuel.state.boost==0&&metalFuel.state.velocity[2]>500);
    CHECK(rocket_world_collect_coin(metalFuel.world.get()));metalFuel.step(30);
    CHECK(metalFuel.state.boost==5&&metalFuel.state.water_mode==ROCKET_WATER_METAL);
    rocket_world_set_temporary_boost(metalFuel.world.get(),0);metalFuel.step(10);
    CHECK(metalFuel.state.boost==0);

    // Sideways exit switches native Metal sinking back to dry gravity during
    // the same host frame, with no stale zero-gravity arena or tank refill.
    Car metalEdge;metalEdge.water(1);rocket_world_set_water_query(metalEdge.world.get(),boundedWater);
    metalEdge.input.boost=1;int metalEdgeSteps=0;
    while(metalEdge.state.position[2]<50&&metalEdgeSteps++<60)metalEdge.step();
    CHECK(metalEdgeSteps<60&&metalEdge.state.water_mode==ROCKET_WATER_DRY);
    metalEdge.input={};float edgeVelocity=metalEdge.state.velocity[1],edgeFuel=metalEdge.state.boost;
    metalEdge.step(3);CHECK(metalEdge.state.velocity[1]<edgeVelocity-100);
    CHECK(metalEdge.state.boost==edgeFuel);

    Car pitchUp,pitchDown;
    pitchUp.water();pitchDown.water();pitchUp.input.pitch=-1;pitchDown.input.pitch=1;
    pitchUp.step(15);pitchDown.step(15);
    pitchUp.input.pitch=pitchDown.input.pitch=0;pitchUp.input.boost=pitchDown.input.boost=1;
    pitchUp.step(45);pitchDown.step(45);
    CHECK(pitchUp.state.velocity[1]*pitchDown.state.velocity[1]<0); // steer thrust to climb AND dive
    const RocketTriangle floor[]={
        {{{-10000,0,-10000},{10000,0,10000},{10000,0,-10000}}},
        {{{-10000,0,-10000},{-10000,0,10000},{10000,0,10000}}}
    };
    Car bottom;CHECK(rocket_world_mesh(bottom.world.get(),0,floor,2));bottom.reset(40);bottom.water();bottom.step(30);
    CHECK(bottom.state.grounded);bottom.input.jump=1;bottom.step(30);
    CHECK(!bottom.state.grounded&&bottom.state.position[1]>200); // lift overcomes wheel adhesion

    // Lift crosses the real surface threshold; dry boost starts consuming again.
    Car surface;surface.reset(0);surface.water(0,200);surface.input.jump=surface.input.boost=1;
    bool exited=false;
    for(int i=0;i<120&&!exited;i++){surface.step();exited=surface.state.water_mode==ROCKET_WATER_DRY;}
    CHECK(exited);float beforeExit=surface.state.boost;surface.step(3);CHECK(surface.state.boost<beforeExit);
    CHECK(surface.state.position[1]>120);
    // Scene clear and reset cannot leave submerged mode/flow latched.
    surface.reset();CHECK(surface.state.water_mode==ROCKET_WATER_DRY);
    CHECK(surface.state.boost<100);
    Car edge;edge.water();rocket_world_set_water_query(edge.world.get(),boundedWater);
    edge.input.boost=1;int edgeSteps=0;
    while(edge.state.position[2]<50&&edgeSteps++<60)edge.step();
    CHECK(edgeSteps<60&&edge.state.water_mode==ROCKET_WATER_DRY);
    float edgeTank=edge.state.boost;edge.step(3);CHECK(edge.state.boost<edgeTank);
    edge.reset();edge.water();edge.step();CHECK(edge.state.water_mode==ROCKET_WATER_JET); // reset cleared old query
    std::printf("water physics: %d checks passed\n",checks);
}
