#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include "../../codex/rocketleague/physics/rocket_physics.h"

/* Host-rendered Standard-style flame, not UE3 particle/shader parity.
 * Attachment coordinates are the owned Octane BoostEmitterLeft/Right sockets.
 * Stateless simulation time keeps pause, remote poses and repeated draws stable. */
namespace rocket_boost_visual {
struct Vertex { float p[3],uv[4],color[4]; };
constexpr unsigned VERTICES=36;
inline void append(std::vector<Vertex>& vertices,const RocketSnapshot& pose) {
    if(!pose.boosting)return;
    for(float v:pose.position)if(!std::isfinite(v))return;
    for(float v:pose.basis)if(!std::isfinite(v)||std::fabs(v)>1.01f)return;
    const float phase=float(pose.ticks%240)/120.f;
    const float length=59.f+5.f*std::sin(phase*18.8495559f);
    const unsigned indices[6]={0,1,2,2,1,3};
    for(int nozzle=0;nozzle<2;++nozzle)for(int plane=0;plane<3;++plane) {
        const float angle=float(plane)*1.04719755f;
        for(unsigned index:indices) {
            const float along=index>=2?1.f:0.f,side=index%2?1.f:-1.f;
            const float p[3]={-48.f-along*length,(nozzle?9.f:-9.f)+std::cos(angle)*side*7.f,
                             9.f+std::sin(angle)*side*7.f};
            Vertex v={};
            for(int k=0;k<3;++k){v.p[k]=pose.position[k];for(int q=0;q<3;++q)v.p[k]+=pose.basis[q*3+k]*p[q]*ROCKET_HOST_SCALE;}
            v.uv[0]=along;v.uv[1]=side;v.uv[2]=phase;v.uv[3]=float(nozzle)*1.7f;
            // Actual MasterBoost_Standard_MIC CustomColor. Host glow is approximate.
            v.color[0]=1.f;v.color[1]=.20867f;v.color[2]=.0697254f;v.color[3]=1.f;
            vertices.push_back(v);
        }
    }
}
}
