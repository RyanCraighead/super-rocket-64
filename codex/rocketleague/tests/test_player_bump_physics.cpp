#include "../physics/player_bump_contact.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){std::fprintf(stderr,"physics bump line %d: %s (%s)\n",__LINE__,#x,rocket_world_error());std::exit(1);}}while(0)
struct Fixture {
 std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> world{rocket_world_create(),rocket_world_destroy};
 uint64_t frame=0;RocketInput input={};
 Fixture(float z=0){CHECK(world);RocketTriangle floor[]={{{{-5000,0,-5000},{5000,0,5000},{5000,0,-5000}},0},{{{-5000,0,-5000},{-5000,0,5000},{5000,0,5000}},0}};
 CHECK(rocket_world_mesh(world.get(),0,floor,2));float p[]={0,40,z},v[]={0,0,0};CHECK(rocket_world_reset(world.get(),p,v,0));}
 RocketSnapshot state(){RocketSnapshot s={};CHECK(rocket_world_snapshot(world.get(),&s));return s;}
 void step(unsigned n=1){while(n--)CHECK(rocket_world_frame(world.get(),++frame,&input,0,0)==4);}
};
static void preserved(const RocketSnapshot &a,const RocketSnapshot &b){
 CHECK(!std::memcmp(a.position,b.position,sizeof a.position));CHECK(!std::memcmp(a.basis,b.basis,sizeof a.basis));
 CHECK(a.ticks==b.ticks&&a.boost==b.boost&&a.jump_time==b.jump_time&&a.flip_time==b.flip_time&&a.air_time==b.air_time);
 CHECK(a.jumped==b.jumped&&a.double_jumped==b.double_jumped&&a.flipped==b.flipped&&a.flipping==b.flipping);
 CHECK(a.grounded==b.grounded&&!std::memcmp(a.wheel_contacts,b.wheel_contacts,sizeof a.wheel_contacts));
}
int main(){
 Fixture a,b(230);a.step(60);b.step(60);float push[]={0,0,1200};CHECK(rocket_world_bump(a.world.get(),push));
 RocketSnapshot before=a.state(),other=b.state();RocketBumpBody first,second;
 CHECK(rocket_bump_body(&first,&before,1)&&rocket_bump_body(&second,&other,1));float delta[3];CHECK(rocket_bump_impulse(&first,&second,delta));
 std::printf("Settled contact delta %.5f %.5f %.5f; body up %.5f %.5f %.5f\n",delta[0],delta[1],delta[2],before.basis[6],before.basis[7],before.basis[8]);
 CHECK(delta[2]>600&&std::fabs(delta[0])<2.f&&std::fabs(delta[1])<20.f);
 float opposite[]={-delta[0],-delta[1],-delta[2]};CHECK(rocket_world_bump(a.world.get(),opposite));CHECK(rocket_world_bump(b.world.get(),delta));
 auto after=a.state(),afterOther=b.state();preserved(before,after);preserved(other,afterOther);
 CHECK(std::fabs(after.velocity[2]+afterOther.velocity[2]-1200)<.01f);
 for(int i=0;i<45;i++){a.step();b.step();CHECK(a.state().position[1]>0&&b.state().position[1]>0);}
 CHECK(b.state().position[2]>other.position[2]+50);
 /* In-flight jump/flip expenditure and finite fuel survive external contact. */
 a.input.jump=1;a.step();a.input.jump=0;a.step();a.input.jump=1;a.input.pitch=1;a.input.boost=1;a.step(4);
 before=a.state();float side[]={300,200,-100};CHECK(rocket_world_bump(a.world.get(),side));after=a.state();preserved(before,after);
 for(int k=0;k<3;k++)CHECK(std::fabs(after.velocity[k]-before.velocity[k]-side[k])<.01f);
 for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),2401.f}){
  float bad[]={invalid,0,0};before=a.state();CHECK(!rocket_world_bump(a.world.get(),bad));after=a.state();preserved(before,after);CHECK(!std::memcmp(before.velocity,after.velocity,sizeof before.velocity));
 }
 CHECK(!rocket_world_bump(nullptr,side));
 /* Exact body geometry: side gaps, vertical gaps, separating motion, bad poses. */
 before=b.state();before.position[0]=before.position[2]=0;before.position[1]=40;before.velocity[0]=before.velocity[1]=before.velocity[2]=0;
 for(int heading=0;heading<16;heading++){
  float angle=heading*6.28318530718f/16;before.basis[0]=std::sin(angle);before.basis[1]=0;before.basis[2]=std::cos(angle);
  before.basis[3]=std::cos(angle);before.basis[4]=0;before.basis[5]=-std::sin(angle);before.basis[6]=0;before.basis[7]=1;before.basis[8]=0;
  other=before;for(int k=0;k<3;k++){other.position[k]+=before.basis[k]*235;other.velocity[k]=0;before.velocity[k]=before.basis[k]*1000;}
  CHECK(rocket_bump_body(&first,&before,1)&&rocket_bump_body(&second,&other,1));CHECK(rocket_bump_impulse(&first,&second,delta));
  float projected=rocket_bump_dot(delta,before.basis);CHECK(projected>500&&projected<650);
  for(int k=0;k<3;k++)CHECK(std::fabs(delta[k]-before.basis[k]*projected)<.1f);
  other.position[1]+=200;CHECK(rocket_bump_body(&second,&other,1));CHECK(!rocket_bump_impulse(&first,&second,delta));
 }
 std::printf("PASS %u actual RocketSim player-bump checks: momentum, motion, floor collision, fuel/jump/flip preservation and input validation\n",checks);
}
