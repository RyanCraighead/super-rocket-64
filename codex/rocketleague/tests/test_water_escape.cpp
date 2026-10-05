#include "../physics/rocket_physics.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <memory>
static int checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static const RocketTriangle walls[]={
 {{{0,-10000,-10000},{0,10000,-10000},{0,10000,10000}}},
 {{{0,-10000,-10000},{0,10000,10000},{0,-10000,10000}}},
 {{{-10000,-10000,0},{10000,10000,0},{-10000,10000,0}}},
 {{{-10000,-10000,0},{10000,-10000,0},{10000,10000,0}}}
};
static const RocketTriangle ceiling[]={
 {{{-10000,2000,-10000},{10000,2000,-10000},{10000,2000,10000}}},
 {{{-10000,2000,-10000},{10000,2000,10000},{-10000,2000,10000}}}
};
struct Car {
 std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> world{rocket_world_create(),rocket_world_destroy};
 RocketInput input={};RocketSnapshot state={};uint64_t frame=0;
 void step(int frames=1,int paused=0,int blocked=0){while(frames--){CHECK(rocket_world_frame(world.get(),++frame,&input,paused,blocked)==(paused?0:4));CHECK(rocket_world_snapshot(world.get(),&state));for(float x:state.position)CHECK(std::isfinite(x));for(float x:state.velocity)CHECK(std::isfinite(x));}}
 void wall(unsigned speed=100,int corner=0,int metal=0){
  CHECK(rocket_world_mesh(world.get(),0,walls,corner?4:2));float p[]={35,1000,corner?140.f:0.f},v[]={0,0,0};CHECK(rocket_world_reset(world.get(),p,v,0));
  CHECK(rocket_world_set_speed(world.get(),speed));CHECK(rocket_world_snapshot(world.get(),&state));
  float basis[]={0,0,-1,0,1,0,1,0,0};for(int i=0;i<9;i++)state.basis[i]=basis[i];CHECK(rocket_world_recover(world.get(),&state));
  rocket_world_set_water(world.get(),1,10000,metal);input={};step(60);
 }
};
static int edgeWater(float x,float,float *level){*level=10000;return x<70;}
int main(){
 for(unsigned speed:{50,75,100})for(unsigned jump:{30,50,100}){
  Car car;car.wall(speed);CHECK(rocket_world_set_jump_height(car.world.get(),jump));
  CHECK(car.state.grounded);float start=car.state.position[0],fuel=car.state.boost;
  car.input.jump=1;car.step(20);CHECK(!car.state.grounded);CHECK(car.state.position[0]>start+80);CHECK(car.state.position[1]>1200);CHECK(car.state.boost==fuel);
  // Holding in open water adds no wall impulse, nor any jump/flip resource.
  float outward=car.state.velocity[0];car.step(50);CHECK(car.state.velocity[0]<outward);CHECK(!car.state.jumped&&!car.state.double_jumped&&!car.state.flipped);
  for(int repeat=0;repeat<3;repeat++){
   car.wall(speed);car.input.jump=1;car.step();car.input.jump=0;car.step(10);CHECK(car.state.position[0]>start+30);
  }
  // Holding through a pause/block must still require a release before escape.
  car.wall(speed);car.input.jump=1;car.step(1,1);car.step(8);CHECK(std::fabs(car.state.position[0]-start)<3);
  car.input.jump=0;car.step();car.input.jump=1;car.step(12);CHECK(car.state.position[0]>start+50);
  car.wall(speed);car.input.jump=1;car.step(1,0,1);car.step(8);CHECK(std::fabs(car.state.position[0]-start)<3);
  car.input.jump=0;car.step();car.input.jump=1;car.step(12);CHECK(car.state.position[0]>start+50);
  // Fresh environmental samples shut separation off at the water-box edge.
  car.wall(speed);rocket_world_set_water_query(car.world.get(),edgeWater);car.input.jump=1;car.step(20);CHECK(car.state.water_mode==ROCKET_WATER_DRY);
 }
 for(unsigned speed:{50,75,100})for(int boost:{0,1}){
  Car corner;corner.wall(speed,1);corner.input.throttle=1;corner.input.boost=boost;corner.step(30);float start=corner.state.position[0];
  corner.input.jump=1;for(int i=0;i<20;i++){corner.step();CHECK(corner.state.position[0]>15);CHECK(corner.state.position[2]>20);}
  CHECK(corner.state.position[0]>start+70); // leaves the wall even while driving into the corner
 }
 for(unsigned speed:{50,75,100}){
  Car roof;CHECK(rocket_world_mesh(roof.world.get(),0,ceiling,2));float p[]={0,1965,0},v[]={0,0,0};CHECK(rocket_world_reset(roof.world.get(),p,v,0));CHECK(rocket_world_set_speed(roof.world.get(),speed));
  CHECK(rocket_world_snapshot(roof.world.get(),&roof.state));float basis[]={0,0,1,-1,0,0,0,-1,0};for(int i=0;i<9;i++)roof.state.basis[i]=basis[i];CHECK(rocket_world_recover(roof.world.get(),&roof.state));
  rocket_world_set_water(roof.world.get(),1,10000,0);roof.step(60);CHECK(roof.state.grounded);float start=roof.state.position[1],lowest=start;
  roof.input.jump=1;for(int i=0;i<60;i++){roof.step();lowest=std::fmin(lowest,roof.state.position[1]);CHECK(roof.state.position[1]<1985);}
  CHECK(lowest<start-15);
 }
 // No jump means normal intentional wall driving remains attached.
 Car drive;drive.wall();drive.input.throttle=1;drive.step(30);CHECK(drive.state.grounded);CHECK(drive.state.position[0]<45);
 // Metal remains on its distinct heavy physics/jump path; this separation is jet-only.
 Car metal;metal.wall(100,0,1);CHECK(metal.state.water_mode==ROCKET_WATER_METAL);float before=metal.state.boost;metal.input.boost=1;metal.step(4);CHECK(metal.state.boost<before);
 std::printf("underwater escape: %d checks passed\n",checks);
}
