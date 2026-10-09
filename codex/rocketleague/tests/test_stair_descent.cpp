#define main previous_stair_tests
#include "test_stairs_physics.cpp"
#undef main
#include "../physics/stair_support.h"
struct MotionMeasure {
    float maxAcceleration=0,maxAngular=0,minUp=1,minY=1e9f,maxY=-1e9f;
    int air=0,frames=0;uint64_t hash=1469598103934665603ull;
    void sample(const RocketSnapshot &s,const RocketSnapshot &previous){
        maxAcceleration=std::max(maxAcceleration,std::fabs(s.velocity[1]-previous.velocity[1])*30);
        float angular=0;for(float a:s.angular_velocity)angular+=a*a;
        maxAngular=std::max(maxAngular,std::sqrt(angular));minUp=std::min(minUp,s.basis[7]);
        minY=std::min(minY,s.position[1]);maxY=std::max(maxY,s.position[1]);
        air+=!s.grounded;frames++;hash=digest(hash,s);
    }
    void print(const char *name,const RocketSnapshot &s){
        printf("%s: frames=%d air=%d maxAY=%.2f maxAngular=%.3f minUp=%.3f final=(%.2f,%.2f,%.2f) hash=%016llx\n",
            name,frames,air,maxAcceleration,maxAngular,minUp,s.position[0],s.position[1],s.position[2],(unsigned long long)hash);
    }
};
static void detector(){
    auto mesh=stairs(26,26,8);CHECK(rocket_stairs::detect(mesh.data(),mesh.size()).size()==8);
    for(float rise:{0.f,80.f,102.f}){auto bad=stairs(rise,26,8);CHECK(rocket_stairs::detect(bad.data(),bad.size()).empty());}
    for(int n:{1,2}){auto bad=stairs(26,26,n);CHECK(rocket_stairs::detect(bad.data(),bad.size()).empty());}
    // Removing every tread keeps the visual risers, but cannot create support.
    auto risers=mesh;for(size_t i=2;i<risers.size();i+=4){risers[i+2]=risers[i];risers[i+3]=risers[i+1];}
    CHECK(rocket_stairs::detect(risers.data(),risers.size()).empty());
    // A narrow genuine gap, even with duplicated overlapping geometry elsewhere,
    // must break continuity instead of contributing phantom area coverage.
    auto gap=stairs(26,26,3);
    for(int index:{4,5})for(auto &v:gap[index].v)if(v[2]==26)v[2]-=.25f;
    gap.push_back(gap[4]);gap.push_back(gap[5]);CHECK(rocket_stairs::detect(gap.data(),gap.size()).empty());
    // No support may cover an actual obstacle within the empty stair wedge.
    auto obstacle=stairs(26,26,3);
    obstacle.push_back({{{-20,30,8},{20,30,20},{20,30,8}},0});
    obstacle.push_back({{{-20,30,8},{-20,30,20},{20,30,20}},0});
    CHECK(rocket_stairs::detect(obstacle.data(),obstacle.size()).size()==2);
    // A side wall on the tread boundary does not erase a verified flight.
    auto side=mesh;
    side.push_back({{{2000,0,0},{2000,600,0},{2000,600,208}},0});
    CHECK(rocket_stairs::detect(side.data(),side.size()).size()==8);
    // A material boundary cannot be replaced by one uniform synthetic tread.
    auto mixed=stairs(26,26,3);mixed[4].material=ROCKET_MATERIAL_VERY_SLIPPERY;
    CHECK(rocket_stairs::detect(mixed.data(),mixed.size()).empty());
}
struct Run {MotionMeasure measure;RocketSnapshot final;bool complete;};
static Run synthetic(float throttle,float yawOffset,bool descend,bool continuous=false,
                     float rise=26,float run=26,int count=24,unsigned speed=100,bool reverse=false){
    auto mesh=stairs(rise,run,count);
    if(continuous){
        mesh.clear();tread(mesh,0,-20000,-run);tread(mesh,rise*count,(count-1)*run,20000);
        mesh.push_back({{{-2000,0,-run},{2000,rise*count,(count-1)*run},{2000,0,-run}},0});
        mesh.push_back({{{-2000,0,-run},{-2000,rise*count,(count-1)*run},{2000,rise*count,(count-1)*run}},0});
    }
    Fixture f(mesh,descend?run*count+220:-300,descend?rise*count+40:40,
              (descend?3.14159265f:0)+yawOffset+(reverse?3.14159265f:0));
    CHECK(rocket_world_set_speed(f.w.get(),speed));
    f.input.throttle=reverse?-throttle:throttle;auto before=f.state();MotionMeasure measure;
    for(int frame=0;frame<(descend?360:150);frame++){
        f.step();auto s=f.state();
        CHECK(s.boost==before.boost);CHECK(s.ticks==before.ticks+4);
        CHECK(!s.jumped&&!s.double_jumped&&!s.flipped);
        if(s.position[2]>=0&&s.position[2]<=run*count)measure.sample(s,before);
        if((descend&&s.position[2]<-160)||(!descend&&s.position[2]>run*count+160)){before=s;break;}
        before=s;
    }
    char label[100];snprintf(label,sizeof label,"%s %s throttle=%.2f yaw=%.2f",continuous?"true ramp":"synthetic",descend?"descent":"ascent",throttle,yawOffset);
    if(run==26&&count==24&&speed==100&&!reverse)measure.print(label,before);
    return {measure,before,descend?before.position[2]<-160:before.position[2]>run*count+160};
}
static void lifecycle(){
    surfaceMode=ROCKET_SURFACES_CAR;auto mesh=stairs(26,51,24);
    Fixture normal(mesh,1444,664,3.14159265f),jitter(mesh,1444,664,3.14159265f);
    normal.input.throttle=jitter.input.throttle=.3f;
    for(int frame=0;frame<130;frame++){
        normal.step();jitter.frame+=frame%5;jitter.step();
        auto a=normal.state(),b=jitter.state();CHECK(digest(0,a)==digest(0,b));
        for(int repeat=0;repeat<frame%4;repeat++){
            CHECK(rocket_world_frame(jitter.w.get(),jitter.frame,&jitter.input,0,0)==0);
            CHECK(digest(0,jitter.state())==digest(0,b));
        }
        if(frame==45){
            CHECK(rocket_world_frame(jitter.w.get(),++jitter.frame,&jitter.input,1,0)==0);
            CHECK(digest(0,jitter.state())==digest(0,b));
        }
        if(frame==50)normal.input.throttle=jitter.input.throttle=0;
        if(frame==65)normal.input.throttle=jitter.input.throttle=-.3f;
        if(frame==80)normal.input.throttle=jitter.input.throttle=.3f;
    }
    // Teleport/reset discards the supplementary body and every support cache.
    float position[]={0,664,1444},zero[]={0,0,0};
    CHECK(rocket_world_reset(normal.w.get(),position,zero,3.14159265f));normal.input={};normal.step(30);
    Fixture fresh(mesh,1444,664,3.14159265f);
    normal.input.throttle=fresh.input.throttle=.3f;
    bool jumped=false;
    for(int i=0;i<150;i++){
        auto before=normal.state();
        if(!jumped&&before.grounded&&before.position[2]>400&&before.position[2]<950){normal.input.jump=fresh.input.jump=1;jumped=true;}
        normal.step();fresh.step();CHECK(digest(0,normal.state())==digest(0,fresh.state()));
        if(normal.input.jump){CHECK(normal.state().jumped);CHECK(normal.state().velocity[1]>before.velocity[1]);normal.input.jump=fresh.input.jump=0;break;}
    }
    CHECK(jumped);
    auto jumpedState=normal.state();CHECK(rocket_world_recover(normal.w.get(),&jumpedState));
    normal.input={};normal.step(10);CHECK(normal.state().boost==jumpedState.boost);
    CHECK(rocket_world_mesh(normal.w.get(),0,nullptr,0));normal.step(15);CHECK(!normal.state().grounded);
    // A full-height wall across a verified flight remains an obstacle.
    auto blocked=stairs(26,51,24);float z=612,y=338;
    blocked.push_back({{{-2000,y,z},{2000,y,z},{2000,y+400,z}},0});
    blocked.push_back({{{-2000,y,z},{2000,y+400,z},{-2000,y+400,z}},0});
    Fixture wall(blocked,1444,664,3.14159265f);wall.input.throttle=.3f;
    for(int i=0;i<180;i++){wall.step();CHECK(wall.state().position[2]>z-5);}
    printf("stair lifecycle: cadence, pause, stop/reverse, jump, reset/recover, mesh removal and wall checks passed\n");
}
static std::vector<RocketTriangle> readGeometry(const char *path){
    std::ifstream input(path);CHECK(input.good());size_t count;input>>count;CHECK(count<100000);
    std::vector<RocketTriangle> mesh(count);for(auto &t:mesh)for(auto &v:t.v)for(float &x:v)input>>x;CHECK(input.good());return mesh;
}
static void routes(const char *path){
    auto mesh=readGeometry(path);
    auto patches=rocket_stairs::detect(mesh.data(),mesh.size());printf("owned basement: %zu verified support patches\n",patches.size());CHECK(patches.size()==28);
    const float route[][7]={
        {-1381,-1034,1400,0,2,1,1900},
        {3480,-1034,-1700,3.14159265f,2,-1,-2180},
        {3480,-1034,-3600,0,2,1,-3140},
        {3400,-1239,-2660,-1.570796327f,0,-1,3060}};
    for(int r=0;r<4;r++)for(unsigned mode:{0u,1u,2u})for(unsigned speed:{50u,75u,100u}){
        surfaceMode=mode;Fixture f(mesh);float zero[3]={};
        CHECK(rocket_world_reset(f.w.get(),route[r],zero,route[r][3]));f.step(30);
        CHECK(rocket_world_set_speed(f.w.get(),speed));f.input.throttle=.3f;
        auto before=f.state();MotionMeasure measure;int axis=int(route[r][4]);float direction=route[r][5];
        for(int i=0;i<180&&direction*(before.position[axis]-route[r][6])<0;i++){
            f.step();auto s=f.state();measure.sample(s,before);CHECK(s.boost==before.boost);before=s;
        }
        char label[100];snprintf(label,sizeof label,"owned basement route=%d mode=%u speed=%u",r,mode,speed);measure.print(label,before);
        CHECK(direction*(before.position[axis]-route[r][6])>=0);CHECK(measure.minUp>0);
    }
}
static void owned(int argc,char **argv){
    CHECK(argc==10);std::ifstream input(argv[2]);CHECK(input.good());size_t count;input>>count;
    std::vector<RocketTriangle> mesh(count);for(auto &t:mesh)for(auto &v:t.v)for(float &x:v)input>>x;CHECK(input.good());
    Fixture f(mesh);float p[]={std::stof(argv[3]),std::stof(argv[4]),std::stof(argv[5])},v[]={0,0,0};
    CHECK(rocket_world_reset(f.w.get(),p,v,std::stof(argv[6])));f.step(30);
    f.input.throttle=std::stof(argv[8]);CHECK(rocket_world_set_speed(f.w.get(),std::stoul(argv[9])));
    auto before=f.state();MotionMeasure measure;
    for(int frame=0;frame<std::stoi(argv[7]);frame++){f.step();auto s=f.state();measure.sample(s,before);before=s;}
    measure.print("owned",before);
}
int main(int argc,char **argv){
    if(argc>1&&!std::strcmp(argv[1],"--geometry")){owned(argc,argv);return 0;}
    if(argc==3&&!std::strcmp(argv[1],"--routes")){routes(argv[2]);return 0;}
    if(argc>1&&!std::strcmp(argv[1],"--one")){synthetic(.6f,0,true);return 0;}
    detector();
    for(float throttle:{.3f,.6f,1.f})for(float yaw:{0.f,.2f})synthetic(throttle,yaw,false);
    // Compare against a genuine continuous ramp with the same rise/run and
    // flat landings. Fast crest takeoff remains ordinary physics. The stairs
    // must traverse without repeated tread stalls, extra flips or large shocks.
    for(unsigned mode:{unsigned(ROCKET_SURFACES_CAR),unsigned(ROCKET_SURFACES_NATIVE),unsigned(ROCKET_SURFACES_NATIVE_NO_WALLS)}){
        surfaceMode=mode;
        for(float run:{26.f,51.f,102.f})for(float throttle:{.3f,.6f,1.f})for(float yaw:{0.f,.2f}){
            auto step=synthetic(throttle,yaw,true,false,26,run);
            auto ramp=synthetic(throttle,yaw,true,true,26,run);
            CHECK(step.complete&&ramp.complete);
            CHECK(step.measure.minUp>-.05f);
            CHECK(step.measure.frames<=ramp.measure.frames*1.3f+5);
            CHECK(step.measure.air<=ramp.measure.air+8);
            CHECK(step.measure.maxAcceleration<=std::max(5000.f,ramp.measure.maxAcceleration)*1.3f+1000);
        }
    }
    for(unsigned speed:{50u,75u,100u})for(bool reverse:{false,true})for(float yaw:{0.f,.2f}){
        auto step=synthetic(.6f,yaw,true,false,26,51,24,speed,reverse);
        auto ramp=synthetic(.6f,yaw,true,true,26,51,24,speed,reverse);
        CHECK(step.complete&&ramp.complete);CHECK(step.measure.minUp>0);
        CHECK(step.measure.frames<=ramp.measure.frames*1.3f+5);
    }
    lifecycle();
    printf("stair support: %d checks passed\n",checks);
}
