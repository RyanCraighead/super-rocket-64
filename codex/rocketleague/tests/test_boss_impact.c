/* Authored contact policy tests, not original-game parity or native play proof. */
#include "../physics/boss_impact.h"
#include <assert.h>
#include <stdio.h>
static RocketBossContact track;
static RocketSnapshot car;
static RocketBossTarget boss;
static void fresh(void) {
    memset(&track,0,sizeof(track));memset(&car,0,sizeof(car));memset(&boss,0,sizeof(boss));
    car.position[1]=40;car.position[2]=-400;car.velocity[2]=2200;
    car.basis[2]=car.basis[3]=car.basis[7]=1;car.ticks=4;
    boss.radius=100;boss.height=100;boss.eligible=1;boss.forward[1]=1;
}
static int sample(float z,uint64_t ticks,uint32_t epoch){car.position[2]=z;car.ticks=ticks;return rocket_boss_contact(&track,&car,epoch,&boss);}
int main(void) {
    fresh();assert(!sample(-400,4,1));assert(sample(-330,8,1)); // Fast front entry.
    assert(!sample(-330,8,1));assert(!sample(-310,12,1)); // Duplicate and overlap.
    assert(!sample(-400,16,1));assert(sample(-330,20,1)); // Separate and re-enter.
    fresh();car.velocity[2]=1799;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    car.velocity[2]=2200;assert(!sample(-300,12,1)); // Accelerating while overlapping does not re-hit.
    fresh();car.velocity[2]=-2200;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    fresh();assert(!sample(-330,4,1));assert(!sample(-320,8,1)); // Spawn in contact.
    fresh();assert(!sample(-400,4,1));assert(!sample(-330,8,2)); // Reset/warp epoch.
    fresh();assert(!sample(-400,4,1));assert(!sample(-330,40,1)); // Long interruption.
    fresh();assert(!sample(-400,4,1));assert(!sample(-330,3,1)); // Older ticks.
    fresh();assert(!sample(-400,4,1));rocket_boss_contact(&track,NULL,0,NULL);assert(!sample(-330,8,1));
    fresh();boss.eligible=0;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    boss.eligible=1;assert(!sample(-310,12,1)); // Immunity entry consumes contact.
    fresh();car.position[1]=200;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    fresh();car.position[0]=300;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    fresh();car.basis[7]=-1;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    fresh();car.basis[0]=NAN;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    fresh();assert(!sample(-400,4,1));assert(!sample(-395,8,1)); // Velocity alone cannot create contact.
    fresh();boss.rear_only=1;assert(!sample(-400,4,1));assert(sample(-330,8,1));
    fresh();boss.rear_only=1;boss.forward[1]=-1;assert(!sample(-400,4,1));assert(!sample(-330,8,1));
    fresh();boss.rear_only=1;boss.forward[0]=1;boss.forward[1]=0;
    assert(!sample(-400,4,1));assert(!sample(-330,8,1)); // Side of Bowser.
    fresh();assert(!sample(-1800,4,1));assert(!sample(-330,8,1)); // Discontinuous translation.
    fresh();car.position[1]=120;car.velocity[2]=3000;
    assert(!sample(-220-ROCKET_BOSS_FRONT,4,1));car.position[1]=155;
    assert(sample(-120-ROCKET_BOSS_FRONT,8,1)); // Rising grazing contact before horizontal closest approach.
    fresh();car.position[1]=190;car.velocity[2]=4400;
    assert(!sample(-220-ROCKET_BOSS_FRONT,4,1));car.position[1]=120;
    assert(sample(220-ROCKET_BOSS_FRONT,16,1)); // Falling contact after closest approach.
    puts("boss contact: speed, entry, rear, immunity, repeat, pause/reset guards passed");
    return 0;
}
