#include "../physics/rocket_physics.h"
#include "../physics/pole_pose.h"
#include "../physics/vanish_collision.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <vector>
static int checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"pole physics line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Fixture{
 std::unique_ptr<RocketWorld,decltype(&rocket_world_destroy)> w{rocket_world_create(),rocket_world_destroy};
 uint64_t frame=0;RocketInput input{};
 Fixture(){CHECK(w);float p[]={0,5000,0},v[]={0,0,0};CHECK(rocket_world_reset(w.get(),p,v,0));}
 RocketSnapshot state(){RocketSnapshot s{};CHECK(rocket_world_snapshot(w.get(),&s));return s;}
 void step(int n=1){while(n--)CHECK(rocket_world_frame(w.get(),++frame,&input,0,0)==4);}
 void launch(float x,float y,float z,float angle=0){auto s=state();float feet[]={x,y,z};rocket_pole_pose(&s,feet,std::sin(angle),std::cos(angle));
  s.velocity[0]=-720*std::sin(angle);s.velocity[1]=1860;s.velocity[2]=-720*std::cos(angle);
  CHECK(rocket_world_pole_release(w.get(),&s,1));frame=0;}
};
static bool clear(const RocketSnapshot&s,const std::vector<RocketTriangle>&mesh,float margin=0){
 for(auto&t:mesh)if(rocket_car_triangle_overlap(&s,t.v,margin))return false;return true;
}
static void basic(){
 Fixture f;f.step();f.input.boost=1;f.step(15);float fuel=f.state().boost;CHECK(fuel<100);
 f.launch(0,5000,0);auto s=f.state();CHECK(s.boost==fuel&&s.jumped&&!s.double_jumped&&!s.flipped&&!s.grounded);
 CHECK(s.basis[1]==1&&s.velocity[1]==1860&&s.velocity[2]==-720);
 f.input.jump=f.input.boost=1;f.step(5);s=f.state();CHECK(!s.double_jumped&&!s.flipped&&s.boost==fuel);
 f.input={};f.step();f.input.jump=1;f.input.pitch=-1;f.step();CHECK(f.state().flipped);
 f.input.jump=0;f.input.boost=1;f.step(5);CHECK(f.state().boost<fuel);
 for(int axis=0;axis<3;axis++){Fixture a;a.launch(0,10000,0);a.step();if(axis==0)a.input.pitch=1;if(axis==1)a.input.yaw=1;if(axis==2)a.input.roll=1;a.step(15);auto p=a.state();CHECK(std::fabs(p.basis[1]-1)>.01f||std::fabs(p.basis[3]-1)>.01f);}
 auto original=f.state();auto bad=original;bad.basis[0]=NAN;CHECK(!rocket_world_pole_release(f.w.get(),&bad,1));
 auto after=f.state();CHECK(!std::memcmp(&original,&after,sizeof after));
 bad=original;bad.velocity[1]=INFINITY;CHECK(!rocket_world_pole_release(f.w.get(),&bad,1));
 CHECK(!rocket_world_pole_release(f.w.get(),&original,2));
 // A fall/detach supplies no grounded flag or artificial launch impulse.
 original.velocity[0]=original.velocity[1]=original.velocity[2]=0;
 CHECK(rocket_world_pole_release(f.w.get(),&original,0));s=f.state();CHECK(!s.jumped&&!s.grounded&&s.velocity[1]==0);
}
static void geometry(const char*path){
 std::ifstream file(path);size_t count;file>>count;CHECK(file.good()&&count<100000);
 std::vector<RocketTriangle> mesh(count);for(auto&t:mesh)for(auto&v:t.v)for(float&f:v)file>>f;CHECK(file.good());
 // The authored 203x205 shaft is too short for a horizontal Octane. The same
 // unscaled box fits vertically at every yaw, including its forward/up offset.
 Fixture shape;auto s=shape.state();s.position[0]=0;s.position[1]=3750;s.position[2]=1331;CHECK(!clear(s,mesh));
 for(int angle=0;angle<360;angle+=5){auto p=s;float feet[]={0,3750,1331};float a=angle*3.14159265f/180;
  rocket_pole_pose(&p,feet,std::sin(a),std::cos(a));CHECK(clear(p,mesh,2));}
 const float locations[2][3]={{2867,1310,2867},{0,4020,1331}};
 for(unsigned mode=0;mode<3;mode++)for(unsigned speed:{50u,75u,100u})for(int index=0;index<2;index++){
  Fixture f;CHECK(rocket_world_mesh(f.w.get(),0,mesh.data(),mesh.size()));rocket_world_set_surface_mode(f.w.get(),mode);CHECK(rocket_world_set_speed(f.w.get(),speed));
  const float*p=locations[index];f.launch(p[0],p[1],p[2]);CHECK(clear(f.state(),mesh));float peak=p[1];
  for(int i=0;i<24;i++){f.step();s=f.state();peak=std::fmax(peak,s.position[1]);CHECK(rocket_body_pose_valid(&s));}
  std::printf("pole %d mode %u speed %u exit %.1f %.1f %.1f peak %.1f\n",index,mode,speed,s.position[0],s.position[1],s.position[2],peak);
  CHECK(peak>p[1]+300);CHECK(std::fabs(s.position[2]-p[2])>150);CHECK(!s.double_jumped&&!s.flipped&&s.boost==100);
 }
 // Release partway through the shaft: Bullet retains the full body and wall
 // collision until the launch rises clear of the lip, without a teleport.
 Fixture mid;CHECK(rocket_world_mesh(mid.w.get(),0,mesh.data(),mesh.size()));rocket_world_set_surface_mode(mid.w.get(),2);
 mid.launch(0,3750,1331);float previousY=mid.state().position[1];
 for(int i=0;i<24;i++){mid.step();s=mid.state();CHECK(std::fabs(s.position[1]-previousY)<80);previousY=s.position[1];}
 std::printf("mid-shaft release %.1f %.1f %.1f\n",s.position[0],s.position[1],s.position[2]);CHECK(s.position[1]>4100&&s.position[2]<1150);
}
int main(int argc,char**argv){basic();if(argc==2)geometry(argv[1]);std::printf("PASS pole release physics: %d checks\n",checks);}
