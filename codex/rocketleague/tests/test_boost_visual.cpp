#include "src/pc/rocket_boost_visual.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <cstdlib>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";std::exit(1);}}while(0)
int main(){
    using namespace rocket_boost_visual;
    RocketSnapshot s={};s.basis[0]=1;s.basis[5]=1;s.basis[7]=1;
    std::vector<Vertex> v;
    s.boost=100;append(v,s);CHECK(v.empty()); // Fuel alone is not thrust.
    s.boosting=1;s.boost=0;append(v,s);CHECK(v.size()==VERTICES); // Actual final fuel tick.
    for(unsigned tick=0;tick<480;++tick){
        s.ticks=tick;v.clear();const auto before=s;append(v,s);
        CHECK(std::memcmp(&s,&before,sizeof s)==0);CHECK(v.size()==VERTICES);
        for(const auto &p:v){
            for(float x:p.p)CHECK(std::isfinite(x));
            CHECK(p.p[0]<=-48*ROCKET_HOST_SCALE);CHECK(p.p[0]>=-112.01f*ROCKET_HOST_SCALE);
            CHECK(p.p[1]>=2*ROCKET_HOST_SCALE&&p.p[1]<=16*ROCKET_HOST_SCALE);
            CHECK(std::fabs(p.p[2])>=2*ROCKET_HOST_SCALE&&std::fabs(p.p[2])<=16*ROCKET_HOST_SCALE);
        }
        auto again=std::vector<Vertex>{};append(again,s);
        CHECK(std::memcmp(v.data(),again.data(),v.size()*sizeof(Vertex))==0); // paused / repeated eye draw
    }
    const auto reference=s;v.clear();append(v,s);
    for(unsigned i=0;i<16;++i){
        s=reference;float a=float(i)*.3926990817f;
        s.basis[0]=std::cos(a);s.basis[2]=std::sin(a);s.basis[6]=0;s.basis[7]=1;
        s.basis[3]=-std::sin(a);s.basis[5]=std::cos(a);s.position[0]=500;s.position[1]=-220;s.position[2]=300;
        std::vector<Vertex> rotated;append(rotated,s);CHECK(rotated.size()==v.size());
        for(size_t j=0;j<v.size();++j){
            CHECK(std::fabs(rotated[j].p[0]-(500+v[j].p[0]*std::cos(a)-v[j].p[2]*std::sin(a)))<.001f);
            CHECK(std::fabs(rotated[j].p[1]-(-220+v[j].p[1]))<.001f);
            CHECK(std::fabs(rotated[j].p[2]-(300+v[j].p[0]*std::sin(a)+v[j].p[2]*std::cos(a)))<.001f);
        }
    }
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        s=reference;s.position[0]=invalid;v.clear();append(v,s);CHECK(v.empty());
        s=reference;s.basis[3]=invalid;append(v,s);CHECK(v.empty());
    }
    s=reference;s.ticks=UINT64_MAX;v.clear();append(v,s);CHECK(v.size()==VERTICES);
    s.boosting=0;v.clear();append(v,s);CHECK(v.empty()); // release/exhaustion has no lingering history
    s=reference;s.boosting=1;append(v,s);CHECK(v.size()==VERTICES); // fresh remote/player pose
    std::cout<<"Rear boost geometry: "<<checks<<" checks passed\n";
}
