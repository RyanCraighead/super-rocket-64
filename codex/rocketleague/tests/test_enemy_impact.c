#include "../physics/enemy_impact.h"
#include <assert.h>
#include <stdio.h>
static RocketSnapshot pose(float x,float y,float z,uint64_t ticks) {
    RocketSnapshot car={0};car.position[0]=x;car.position[1]=y;car.position[2]=z;
    car.basis[0]=car.basis[5]=car.basis[7]=1;car.basis[5]=-1;
    car.velocity[0]=ROCKET_ENEMY_SUPERSONIC_SPEED;car.ticks=ticks;return car;
}
static RocketEnemyTarget enemy={{0,0,0},40,0,80};
static int drive(float speed,float height,float side,uint32_t epoch,uint64_t delta) {
    RocketEnemyContact track={0};
    RocketSnapshot a=pose(-400,height,side,100),b=pose(-100,height,side,100+delta);
    a.velocity[0]=b.velocity[0]=speed;
    assert(!rocket_enemy_contact(&track,&a,1,&enemy));
    int hit=rocket_enemy_contact(&track,&b,epoch,&enemy);
    assert(!rocket_enemy_contact(&track,&b,epoch,&enemy));return hit;
}
int main(void) {
    assert(ROCKET_ENEMY_SUPERSONIC_SPEED==4400.f);
    assert(!drive(4399.99f,0,0,1,8));assert(drive(4400,0,0,1,8));
    assert(drive(4600,0,0,1,8));assert(!drive(4200,0,0,1,8));
    assert(!drive(4400,200,0,1,8));assert(!drive(4400,0,300,1,8));
    assert(!drive(4400,0,0,2,8));assert(!drive(4400,0,0,1,25));
    assert(!drive(4400,0,0,1,1)); // impossible displacement in one source tick
    RocketEnemyContact t={0};RocketSnapshot a=pose(-500,0,0,100),b=pose(350,0,0,124);
    assert(!rocket_enemy_contact(&t,&a,1,&enemy));assert(rocket_enemy_contact(&t,&b,1,&enemy)); // crossed target
    memset(&t,0,sizeof t);a=pose(400,0,0,100);b=pose(100,0,0,108);
    a.velocity[0]=b.velocity[0]=-4400; // rear-first impacts use the real velocity
    assert(!rocket_enemy_contact(&t,&a,1,&enemy));assert(rocket_enemy_contact(&t,&b,1,&enemy));
    memset(&t,0,sizeof t);a=pose(0,0,-350,100);b=pose(0,0,-50,108);
    a.velocity[0]=b.velocity[0]=0;a.velocity[2]=b.velocity[2]=4400;
    assert(!rocket_enemy_contact(&t,&a,1,&enemy));assert(rocket_enemy_contact(&t,&b,1,&enemy)); // side slide
    memset(&t,0,sizeof t);a=pose(0,350,0,100);b=pose(0,50,0,108);
    a.velocity[0]=b.velocity[0]=0;a.velocity[1]=b.velocity[1]=-4400;
    assert(!rocket_enemy_contact(&t,&a,1,&enemy));assert(rocket_enemy_contact(&t,&b,1,&enemy)); // vertical impact
    memset(&t,0,sizeof t);a=pose(-400,0,0,100);b=pose(-100,0,0,108);
    assert(!rocket_enemy_contact(&t,&a,1,&enemy));b.velocity[0]=NAN;
    assert(!rocket_enemy_contact(&t,&b,1,&enemy)&&!t.valid);
    b=pose(-100,0,0,108);assert(!rocket_enemy_contact(&t,&a,1,&enemy));b.position[0]=INFINITY;
    assert(!rocket_enemy_contact(&t,&b,1,&enemy)&&!t.valid);
    assert(!rocket_enemy_contact(&t,&a,1,&enemy));b=pose(-100,0,0,99);
    assert(!rocket_enemy_contact(&t,&b,1,&enemy)); // tick rollback warms a new segment
    memset(&t,0,sizeof t);a=pose(-100,0,0,100);b=a;b.ticks+=8;
    assert(!rocket_enemy_contact(&t,&a,1,&enemy));assert(!rocket_enemy_contact(&t,&b,1,&enemy)); // standing overlap
    assert(!rocket_enemy_contact(&t,NULL,1,&enemy)&&!t.valid);
    puts("enemy contact: threshold, front/rear/side/vertical sweeps, separation, replay/reset/stale/nonfinite passed");
}
