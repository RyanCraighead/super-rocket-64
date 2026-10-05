#include "../physics/whomp_impact.h"
#include <assert.h>
#include <stdio.h>
static RocketWhompBack back(void){
    RocketWhompBack b={{0,0,0},{1,0},{0,1},{-200,50},{150,430},100,1};return b;
}
static RocketSnapshot dive(float gap,uint64_t ticks){
    RocketSnapshot c={0};c.basis[1]=-1;c.basis[3]=1;c.basis[8]=1;
    c.position[2]=220;c.velocity[1]=-1400;c.boosting=1;c.air_time=.5f;c.ticks=ticks;
    c.position[1]=100+ROCKET_ENEMY_OFFSET+ROCKET_ENEMY_HALF_LENGTH+gap;
    return c;
}
static int contact(RocketSnapshot a,RocketSnapshot b,RocketWhompBack target){
    RocketWhompContact t={0};float p[3];
    assert(!rocket_whomp_contact(&t,&a,1,&target,p));
    return rocket_whomp_contact(&t,&b,1,&target,p);
}
int main(void){
    RocketWhompBack target=back();RocketSnapshot a=dive(45,100),b=dive(1,104);float p[3];
    assert(contact(a,b,target)==2);
    RocketWhompContact t={0};assert(!rocket_whomp_contact(&t,&a,1,&target,p));
    assert(rocket_whomp_contact(&t,&b,1,&target,p)==2);
    assert(!rocket_whomp_contact(&t,&b,1,&target,p));b.ticks+=4;
    assert(!rocket_whomp_contact(&t,&b,1,&target,p));
    b=dive(1,104);a.boosting=0;assert(!contact(a,b,target));a=dive(45,100);
    b.boosting=0;assert(!contact(a,b,target));b=dive(1,104);
    a.velocity[1]=-1199;assert(!contact(a,b,target));a=dive(45,100);
    a.grounded=1;assert(!contact(a,b,target));a.grounded=0;
    a.wheel_contacts[0]=1;assert(!contact(a,b,target));a.wheel_contacts[0]=0;
    a.air_time=0;assert(!contact(a,b,target));a.air_time=.5f;
    target.eligible=0;assert(!contact(a,b,target));target=back();
    a.position[0]=b.position[0]=145;assert(!contact(a,b,target));a=dive(45,100);b=dive(1,104);
    b.position[1]-=20;assert(!contact(a,b,target));b=dive(1,104);
    b.ticks=113;assert(!contact(a,b,target));b=dive(1,104);
    t=(RocketWhompContact){0};rocket_whomp_contact(&t,&a,1,&target,p);
    assert(!rocket_whomp_contact(&t,&b,2,&target,p));
    t=(RocketWhompContact){0};rocket_whomp_contact(&t,&a,1,&target,p);target.height+=44;
    assert(!rocket_whomp_contact(&t,&a,1,&target,p));a.ticks+=4;
    assert(!rocket_whomp_contact(&t,&a,1,&target,p));target=back();a=dive(45,100);
    // A failed slow entry is not redeemed by boosting while still overlapping.
    t=(RocketWhompContact){0};a.boosting=0;rocket_whomp_contact(&t,&a,1,&target,p);
    assert(!rocket_whomp_contact(&t,&b,1,&target,p));b.ticks+=4;
    assert(!rocket_whomp_contact(&t,&b,1,&target,p));
    // Rotating airborne flip, with no boost. The second sample rotates .1 rad.
    a=dive(45,100);b=dive(1,104);a.boosting=b.boosting=0;
    a.flipped=a.flipping=1;a.flip_time=.2f;a.angular_velocity[0]=4;a.velocity[1]=-600;
    b.basis[1]=-cosf(.1f);b.basis[2]=sinf(.1f);b.basis[7]=sinf(.1f);b.basis[8]=cosf(.1f);
    rocket_whomp_lowest(&b,p);b.position[1]+=101-p[1];
    assert(contact(a,b,target)==1);
    a.flipping=0;assert(!contact(a,b,target));a.flipping=1;
    a.angular_velocity[0]=2.99f;assert(!contact(a,b,target));a.angular_velocity[0]=4;
    // Active rotation reaches the back even with little/no center descent.
    a.velocity[1]=0;assert(contact(a,b,target)==1);
    a.wheel_contacts[0]=a.wheel_contacts[1]=1;assert(contact(a,b,target)==1);
    a.grounded=1;assert(!contact(a,b,target));a.grounded=0;
    a.wheel_contacts[0]=a.wheel_contacts[1]=0;
    a.flipped=0;assert(!contact(a,b,target));a.flipped=1;
    a.angular_velocity[0]=INFINITY;assert(!contact(a,b,target));a.angular_velocity[0]=4;
    a.flip_time=.7f;assert(!contact(a,b,target));
    // Rotation alone drives the chassis down while the center rises slightly.
    a=dive(45,100);a.boosting=0;a.velocity[1]=0;
    memset(a.basis,0,sizeof a.basis);a.basis[2]=a.basis[3]=a.basis[7]=1;
    a.position[1]=145-ROCKET_ENEMY_OFFSET_UP+ROCKET_ENEMY_HALF_HEIGHT;
    a.flipped=a.flipping=1;a.flip_time=.1f;a.angular_velocity[0]=4;
    b=a;b.ticks=104;b.position[1]+=2;
    b.basis[1]=-sinf(.3f);b.basis[2]=cosf(.3f);b.basis[7]=cosf(.3f);b.basis[8]=sinf(.3f);
    assert(contact(a,b,target)==1);
    a.flipping=0;assert(!contact(a,b,target));
    a=dive(45,100);a.position[0]=NAN;assert(!contact(a,b,target));
    puts("PASS Whomp policy: actual downward entry, flip/dive thresholds, edge/ground/slow/overlap/reset/time/target-motion rejection");
}
