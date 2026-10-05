#include "bm64_pose.h"
#include "utils/oot_asset_path.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace bm64 {
namespace {
using Json=nlohmann::json;
void need(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
int integer(const Json &j,int low,int high){
    need(j.is_number_integer()&&!j.is_boolean(),"invalid BM64 integer");
    if(j.is_number_unsigned())need(j.get<uint64_t>()<=uint64_t(high),"BM64 integer overflow");
    int64_t value=j.get<int64_t>();need(value>=low&&value<=high,"BM64 integer out of range");return int(value);
}
float scalar(const Json &j){need(j.is_number()&&!j.is_boolean(),"invalid BM64 scalar");float value=j.get<float>();need(std::isfinite(value)&&std::fabs(value)<=100000,"unbounded BM64 scalar");return value;}
const Json &array(const Json &j,size_t min,size_t max){need(j.is_array()&&j.size()>=min&&j.size()<=max,"invalid BM64 array");return j;}
Transform transform(const Json &j){Transform t;int index=0;for(const char *key:{"translation","rotation","scale"})for(const Json &v:array(j.at(key),3,3))t[index++]=scalar(v);return t;}
Matrix identity(){Matrix m={};m[0]=m[5]=m[10]=m[15]=1;return m;}
Matrix multiply(const Matrix &a,const Matrix &b){Matrix r={};for(int c=0;c<4;++c)for(int i=0;i<4;++i)for(int k=0;k<4;++k)r[c*4+i]+=a[k*4+i]*b[c*4+k];return r;}
Matrix local(const Transform &t){
    const float radians=0.01745329238474369f;
    float x=t[3]*radians,y=t[4]*radians,z=t[5]*radians;
    Matrix rx=identity(),ry=identity(),rz=identity(),tr=identity();
    rx[5]=rx[10]=std::cos(x);rx[6]=std::sin(x);rx[9]=-rx[6];
    ry[0]=ry[10]=std::cos(y);ry[8]=std::sin(y);ry[2]=-ry[8];
    rz[0]=rz[5]=std::cos(z);rz[1]=std::sin(z);rz[4]=-rz[1];
    tr[12]=t[0];tr[13]=t[1];tr[14]=t[2];return multiply(tr,multiply(rz,multiply(rx,ry)));
}
float lerp(float a,float b,float w){return a+(b-a)*w;}
Transform interpolate(Transform a,Transform b,float w){
    for(int i=3;i<6;++i){if(a[i]>180)a[i]-=360;if(b[i]>180)b[i]-=360;}
    for(int i=0;i<9;++i)a[i]=lerp(a[i],b[i],w);
    return a;
}
}
Rig Rig::load(const std::string &path){
    auto bytes=oot_asset_path::readFile(oot_asset_path::canonical(path),16*1024*1024);
    Json j=Json::parse(bytes.begin(),bytes.end(),[](int depth,Json::parse_event_t,Json &){need(depth<=32,"BM64 JSON nesting limit");return true;});
    Rig r;const Json &s=j.at("skeleton"),&ls=array(s.at("limbs"),1,64);int count=int(ls.size());
    need(s.at("rotation_units")=="degrees","unsupported BM64 rotation units");
    r.root=integer(s.at("root"),0,count-1);
    auto link=[&](const Json &v){return v.is_null()?-1:integer(v,0,count-1);};
    for(int i=0;i<count;++i){const Json &l=ls[i];integer(l.at("index"),i,i);r.limbs.push_back({link(l.at("parent")),link(l.at("child")),link(l.at("sibling")),transform(l)});}
    need(r.limbs[r.root].parent==-1&&r.limbs[r.root].sibling==-1,"invalid BM64 root");
    std::vector<bool> seen(count,false);
    std::function<void(int,int)> visit=[&](int i,int parent){if(i<0)return;need(!seen[i],"cyclic BM64 skeleton");need(r.limbs[i].parent==parent,"inconsistent BM64 parent");seen[i]=true;r.order.push_back(i);visit(r.limbs[i].child,i);visit(r.limbs[i].sibling,parent);};
    visit(r.root,-1);need(r.order.size()==ls.size(),"disconnected BM64 skeleton");
    for(const Json &p:array(j.at("pose_sets"),0,512)){
        std::vector<Override> overrides;std::vector<bool> used(count,false);
        for(const Json &o:array(p.at("overrides"),0,64)){int index=integer(o.at("limb"),0,count-1);need(!used[index],"duplicate BM64 pose limb");used[index]=true;overrides.push_back({index,transform(o)});}
        r.poses.push_back(overrides);
    }
    for(const Json &a:array(j.at("animations"),0,64)){
        Animation animation;animation.id=integer(a.at("id"),0,100000);animation.duration=integer(a.at("duration"),1,100000);
        for(const Json &t:array(a.at("tracks"),0,256)){
            Track track;track.opcode=integer(t.at("opcode"),47,48);track.start=integer(t.at("start"),0,animation.duration);track.end=integer(t.at("end"),track.start,animation.duration);track.pose=-1;
            if(track.opcode==47){
                const Json &ps=array(t.at("poses"),2,512),&ds=array(t.at("durations"),ps.size(),ps.size());int end=track.start;
                for(size_t i=0;i<ps.size();++i){track.poses.push_back(integer(ps[i],0,int(r.poses.size())-1));int d=integer(ds[i],i+1<ps.size()?1:0,i+1<ps.size()?100000:0);track.durations.push_back(d);end+=d;}
                need(end==track.end,"inconsistent BM64 pose track end");
            }else{
                track.pose=integer(t.at("pose"),0,int(r.poses.size())-1);int last=-1;
                for(const Json &key:array(t.at("keys"),2,512)){array(key,2,2);int time=integer(key[0],0,track.end-track.start);need(time>last,"unordered BM64 weight keys");last=time;track.keys.push_back({time,scalar(key[1])});}
                need(track.keys.front().first==0&&track.keys.back().first==track.end-track.start,"incomplete BM64 weight track");
            }
            animation.tracks.push_back(track);
        }
        r.animations.push_back(animation);
    }
    return r;
}
std::vector<Transform> Rig::sample(int animation,float frame) const {
    if(animation==-1){need(frame==0,"bind pose has no time");return sample_layers({});}
    return sample_layers({{animation,frame}});
}
std::vector<Transform> Rig::sample_layers(const std::vector<std::pair<int,float>> &channels) const {
    need(channels.size()<=8,"too many BM64 animation channels");
    std::vector<Transform> out;for(const Limb &l:limbs)out.push_back(l.bind);
    for(auto channel:channels){
    int animation=channel.first;float frame=channel.second;
    need(std::isfinite(frame),"nonfinite BM64 frame");
    need(animation>=0&&size_t(animation)<animations.size(),"unknown original BM64 animation");const Animation &a=animations[animation];need(frame>=0&&frame<=a.duration,"BM64 frame outside track");
    for(const Track &t:a.tracks){
        if(frame<t.start||frame>t.end)continue;
        float time=frame-t.start;
        if(t.opcode==47){
            int upper=1,leftTime=0,rightTime=t.durations[0];
            while(upper<int(t.poses.size())-1&&int(time)>=rightTime){leftTime=rightTime;rightTime+=t.durations[upper];++upper;}
            float weight=(time-leftTime)/float(rightTime-leftTime);
            std::vector<Transform> left,right;std::vector<bool> changed(limbs.size(),false);for(const Limb &l:limbs){left.push_back(l.bind);right.push_back(l.bind);}
            for(const Override &o:poses[t.poses[upper-1]]){left[o.limb]=o.value;changed[o.limb]=true;}
            for(const Override &o:poses[t.poses[upper]]){right[o.limb]=o.value;changed[o.limb]=true;}
            for(size_t i=0;i<limbs.size();++i)if(changed[i])out[i]=interpolate(left[i],right[i],weight);
        }else{
            size_t upper=0;while(upper<t.keys.size()&&t.keys[upper].first<int(time))++upper;need(upper<t.keys.size(),"BM64 weight key not found");float weight;
            if(t.keys[upper].first==int(time))weight=t.keys[upper].second;
            else{need(upper>0,"BM64 weight interval invalid");auto l=t.keys[upper-1],r=t.keys[upper];float slope=(r.second-l.second)/float(r.first-l.first);weight=l.second+(time-l.first)*slope;}
            for(const Override &o:poses[t.pose])for(int i=0;i<9;++i)out[o.limb][i]+=(o.value[i]-limbs[o.limb].bind[i])*weight;
        }
    }
    }
    return out;
}
bool Rig::palettes(const std::vector<Transform> &pose,const float view[16],const float world[3],float yaw,float hostScale,float objectScale,std::vector<Matrix> &joints,std::vector<Matrix> &billboards) const {
    if(pose.size()!=limbs.size()||!view||!world||!std::isfinite(yaw)||!std::isfinite(hostScale)||!std::isfinite(objectScale)||hostScale<=0||hostScale>100||objectScale<=0||objectScale>100)return false;
    for(int i=0;i<16;++i)if(!std::isfinite(view[i]))return false;
    for(int i=0;i<3;++i)if(!std::isfinite(world[i]))return false;
    for(const Transform &t:pose)for(float v:t)if(!std::isfinite(v)||std::fabs(v)>100000)return false;
    float a=view[0],b=view[4],c=view[8],d=view[1],e=view[5],f=view[9],g=view[2],h=view[6],i=view[10];
    float det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::fabs(det)<1e-8f)return false;
    Matrix inverse=identity();inverse[0]=(e*i-f*h)/det;inverse[4]=(c*h-b*i)/det;inverse[8]=(b*f-c*e)/det;inverse[1]=(f*g-d*i)/det;inverse[5]=(a*i-c*g)/det;inverse[9]=(c*d-a*f)/det;inverse[2]=(d*h-e*g)/det;inverse[6]=(b*g-a*h)/det;inverse[10]=(a*e-b*d)/det;
    Transform actor={{world[0],world[1],world[2],0,yaw,0,1,1,1}};Matrix global=local(actor);float scale=hostScale*objectScale;for(int col=0;col<3;++col)for(int row=0;row<3;++row)global[col*4+row]*=scale;
    joints.resize(limbs.size());billboards.resize(limbs.size());
    for(int index:order){joints[index]=multiply(limbs[index].parent<0?global:joints[limbs[index].parent],local(pose[index]));billboards[index]=inverse;for(int col=0;col<3;++col)for(int row=0;row<3;++row)billboards[index][col*4+row]*=scale;for(int k=0;k<3;++k)billboards[index][12+k]=joints[index][12+k];}
    return true;
}
}
