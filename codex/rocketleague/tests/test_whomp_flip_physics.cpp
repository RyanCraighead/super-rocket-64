#include "../physics/whomp_impact.h"
extern "C" {
#include "../../../src/pc/character_net_codec.h"
}
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"Whomp flip physics line %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)

/* Genuine pinned RocketSim/Bullet trajectories, empty fuel, and ordinary input.
 * Sample the same trajectory as a local frame and as 10/15/30 Hz remote poses.
 * Codec round trips ensure this works with the actual serialized flip flags. */
static void trajectory(float y,float vy,int side,int cadence,int phase,int flip,int expected,int button=0) {
    RocketWorld *world=rocket_world_create();CHECK(world);
    float width=button?100.f:side?360.f:180.f;
    float low=button?-100.f:100.f,high=button?100.f:450.f;
    const RocketTriangle mesh[]={
        {{{-width,100,low},{width,100,high},{width,100,low}}},
        {{{-width,100,low},{-width,100,high},{width,100,high}}}
    };
    CHECK(rocket_world_mesh(world,0,mesh,2));
    float p[]={0,y,button?-210.f:side?250.f:120.f},v[]={0,vy,0};CHECK(rocket_world_reset(world,p,v,0));
    CHECK(rocket_world_set_boost_mode(world,ROCKET_BOOST_COIN_ONLY));
    RocketInput drain={};
    CHECK(rocket_world_frame(world,1,&drain,0,0)==4);drain.boost=1;
    for(unsigned f=2;f<150;f++)CHECK(rocket_world_frame(world,f,&drain,0,0)==4);
    CHECK(rocket_world_reset(world,p,v,0)); // Preserve the actually exhausted tank.

    RocketWhompBack back={{0,0,0},{1,0},{0,1},{-width,low},{width,high},100,1};
    RocketWhompContact local={},remote={};int hits=0,wireHits=0;
    for(unsigned frame=1;frame<32;frame++){
        RocketInput input={};input.pitch=!side&&frame>=3?-1.f:0;
        input.roll=side&&frame>=3?1.f:0;input.jump=flip&&frame==3;
        CHECK(rocket_world_frame(world,frame,&input,0,0)==4);
        RocketSnapshot car;CHECK(rocket_world_snapshot(world,&car));
        CHECK(car.boost==0&&!car.boosting);
        float point[3];int kind=rocket_whomp_contact(&local,&car,1,&back,point);
        if(kind){CHECK(kind==1);hits++;}
        if(frame%cadence!=(unsigned)phase)continue;
        CharacterNetState sent={},received={};uint8_t wire[CNET_WIRE_SIZE];
        sent.kind=CNET_OCTANE;sent.active=CNET_DRIVING;sent.interaction=1;
        sent.sequence=frame;sent.epoch=1;sent.car=car;
        CHECK(character_net_encode(wire,sizeof wire,&sent));
        CHECK(character_net_decode(&received,wire,sizeof wire));
        kind=rocket_whomp_contact(&remote,&received.car,received.epoch,&back,point);
        if(kind){CHECK(kind==1);wireHits++;}
        CHECK(!rocket_whomp_contact(&remote,&received.car,received.epoch,&back,point));
    }
    if(hits!=(flip?1:0)||wireHits!=expected)std::fprintf(stderr,"y=%.0f vy=%.0f side=%d cadence=%d phase=%d flip=%d hits=%d remote=%d expected=%d\n",y,vy,side,cadence,phase,flip,hits,wireHits,expected);
    CHECK(hits==(flip?1:0)&&wireHits==expected);rocket_world_destroy(world);
}
int main(){
    const float cases[][3]={{240,0,0},{280,0,0},{280,-400,0},{320,-200,0},{280,-200,1},{320,-400,1}};
    for(const auto &c:cases)for(int cadence=1;cadence<=3;cadence++)for(int phase=0;phase<cadence;phase++)
        trajectory(c[0],c[1],(int)c[2],cadence,phase,1,
            !(c[2]==1&&c[0]==320&&cadence==3&&phase==2)); // Entire brief entry lost between poses.
    // A glancing side contact can begin and end between 10 Hz poses. The
    // local 30 Hz path must still accept it; missing contact evidence is not
    // invented on the remote peer. Switches use local input + reliable events.
    trajectory(240,0,1,1,0,1,1);
    trajectory(240,0,1,3,0,1,0); // local hit is checked separately below
    trajectory(240,0,0,1,0,1,1,1);
    trajectory(240,0,0,1,0,0,0);trajectory(280,-200,1,1,0,0,0);
    std::puts("PASS zero-fuel flips: real forward/side rotation, suspension contact, slow center descent; native frames and CNET 10/15/30 Hz including duplicates; passive air-roll rejected");
}
