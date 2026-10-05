#include "../physics/enemy_impact.h"
#include "../physics/boss_impact.h"
#include "../physics/switch_contact.h"
#include "../physics/player_bump_contact.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"speed impact line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static RocketSnapshot car(float x,float velocity,uint64_t ticks){
    RocketSnapshot c={0};c.position[0]=x;c.velocity[0]=velocity;c.ticks=ticks;
    c.basis[0]=c.basis[7]=1;c.basis[5]=-1;return c;
}
static RocketSnapshot dive(float gap,uint64_t ticks,float speed){
    RocketSnapshot c={0};c.basis[1]=-1;c.basis[3]=1;c.basis[8]=1;
    c.position[2]=220;c.velocity[1]=-speed;c.boosting=1;c.air_time=.5f;c.ticks=ticks;
    c.position[1]=100+ROCKET_ENEMY_OFFSET+ROCKET_ENEMY_HALF_LENGTH+gap;return c;
}
int main(void){
    CHECK(!rocket_speed_valid(0)&&!rocket_speed_valid(49)&&!rocket_speed_valid(101));
    CHECK(rocket_speed_preference(0)==75&&rocket_speed_preference(100)==100);
    for(unsigned percent=50;percent<=100;percent+=25){
        float scale=rocket_speed_multiplier(percent);
        for(int boundary=-1;boundary<=1;boundary++){
            float velocity=4400.f*scale+boundary*.25f;
            RocketSnapshot a=car(-400,velocity,4),b=car(-100,velocity,12);
            CHECK(rocket_enemy_supersonic_at_speed(&b,scale)==(boundary>=0));
            RocketEnemyTarget enemy={{0,0,0},40,0,80};RocketEnemyContact track={0};
            CHECK(!rocket_enemy_contact_at_speed(&track,&a,1,&enemy,scale));
            CHECK(rocket_enemy_contact_at_speed(&track,&b,1,&enemy,scale)==(boundary>=0));
            // King Bob-omb/Bowser and the ordinary Bob-omb share this exact policy.
            for(unsigned small=0;small<2;small++){
                float limit=(small?360.f:1800.f)*scale;
                a=car(-400,limit+boundary*.25f,4);b=car(-330,limit+boundary*.25f,8);
                a.position[1]=b.position[1]=40;
                RocketBossTarget boss={{0,0,0},100,0,100,{1,0},1,1};RocketBossContact bt={0};
                CHECK(!rocket_bumper_contact(&bt,&a,1,&boss,limit));
                CHECK(rocket_bumper_contact(&bt,&b,1,&boss,limit)==(boundary>=0));
            }
            RocketWhompBack back={{0,0,0},{1,0},{0,1},{-200,50},{150,430},100,1};
            a=dive(45,100,1200.f*scale+boundary*.25f);b=dive(1,104,0);b.boosting=1;
            RocketWhompContact wt={0};float point[3];
            CHECK(!rocket_whomp_contact_at_speed(&wt,&a,1,&back,point,scale));
            CHECK(rocket_whomp_contact_at_speed(&wt,&b,1,&back,point,scale)==(boundary>=0?2:0));
            wt=(RocketWhompContact){0};
            CHECK(!rocket_switch_contact_at_speed(&wt,&a,1,&back,point,scale));
            CHECK(rocket_switch_contact_at_speed(&wt,&b,1,&back,point,scale)==(boundary>=0?2:0));
            // Angle/rotation and a genuine downward physical crossing are still required.
            wt=(RocketWhompContact){0};a.boosting=0;
            CHECK(!rocket_whomp_contact_at_speed(&wt,&a,1,&back,point,scale));
            CHECK(!rocket_whomp_contact_at_speed(&wt,&b,1,&back,point,scale));
        }
        for(int boundary=-1;boundary<=1;boundary++){
            RocketWhompBack back={{0,0,0},{1,0},{0,1},{-200,50},{150,430},100,1};
            RocketSnapshot high=dive(45,96,0),a=dive(9,100,0),b=dive(1,104,0);
            high.boosting=a.boosting=b.boosting=0;
            a.flipped=a.flipping=1;a.flip_time=.2f;a.angular_velocity[0]=4;
            b.basis[1]=-cosf(.1f);b.basis[2]=sinf(.1f);b.basis[7]=sinf(.1f);b.basis[8]=cosf(.1f);
            float point[3];rocket_whomp_lowest(&b,point);
            b.position[1]+=109.f-(120.f*scale/30.f+boundary*.01f)-point[1];
            RocketWhompContact t={0};
            CHECK(!rocket_whomp_contact_at_speed(&t,&high,1,&back,point,scale));
            CHECK(!rocket_whomp_contact_at_speed(&t,&a,1,&back,point,scale));
            CHECK(rocket_whomp_contact_at_speed(&t,&b,1,&back,point,scale)==(boundary>=0?1:0));
            t=(RocketWhompContact){0};
            CHECK(!rocket_switch_contact_at_speed(&t,&high,1,&back,point,scale));
            CHECK(!rocket_switch_contact_at_speed(&t,&a,1,&back,point,scale));
            CHECK(rocket_switch_contact_at_speed(&t,&b,1,&back,point,scale)==(boundary>=0?1:0));
        }
        RocketSnapshot a=car(0,6000,4),b=car(230,0,4);RocketBumpBody ba,bb;float delta[3];
        CHECK(rocket_bump_body(&ba,&a,1)&&rocket_bump_body(&bb,&b,1));
        CHECK(rocket_bump_impulse_at_speed(&ba,&bb,delta,scale));
        CHECK(fabsf(delta[0]-2400.f*scale)<.001f);
        ba.velocity[0]=-30.f*scale-.01f;CHECK(!rocket_bump_impulse_at_speed(&ba,&bb,delta,scale));
        ba.velocity[0]=-30.f*scale;CHECK(rocket_bump_impulse_at_speed(&ba,&bb,delta,scale));
        CHECK(!rocket_bump_impulse_at_speed(&ba,&bb,delta,NAN));
    }
    printf("speed impact boundaries: %u checks passed\n",checks);
}
