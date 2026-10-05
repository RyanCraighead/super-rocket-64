#include <assert.h>
#include <stdio.h>
#include "../physics/body_contact.h"

static RocketSnapshot pose(void) {
    RocketSnapshot car={0};car.basis[2]=car.basis[3]=car.basis[7]=1;return car;
}
static int hit(RocketSnapshot *car,float x,float y,float z,float radius,float height) {
    float p[3]={x,y,z};return rocket_body_overlaps_cylinder(car,p,radius,height);
}
int main(void) {
    RocketSnapshot car=pose();
    float front=ROCKET_BODY_FORWARD_OFFSET+ROCKET_BODY_HALF_LENGTH;
    float back=ROCKET_BODY_FORWARD_OFFSET-ROCKET_BODY_HALF_LENGTH;
    float roof=ROCKET_BODY_UP_OFFSET+ROCKET_BODY_HALF_HEIGHT;
    float bottom=ROCKET_BODY_UP_OFFSET-ROCKET_BODY_HALF_HEIGHT;
    assert(hit(&car,0,20,front+4,5,30));
    assert(!hit(&car,0,20,front+6,5,30));
    assert(hit(&car,0,20,back-4,5,30));
    assert(!hit(&car,0,20,back-6,5,30));
    assert(hit(&car,ROCKET_BODY_HALF_WIDTH+4,20,0,5,30));
    assert(hit(&car,-ROCKET_BODY_HALF_WIDTH-4,20,0,5,30));
    assert(!hit(&car,ROCKET_BODY_HALF_WIDTH+4,20,front+4,5,30)); // Empty corner, not a sphere.
    assert(hit(&car,0,roof,0,5,1));
    assert(!hit(&car,0,roof+1,0,5,10));
    assert(!hit(&car,0,bottom-2,0,5,1));
    assert(hit(&car,0,bottom-1,0,5,1));

    // Independent upright rectangle/circle oracle, across yaw and translations.
    for(int turn=0;turn<24;turn++) {
        float angle=turn*.2617993878f,s=sinf(angle),c=cosf(angle);
        car=pose();car.position[0]=713;car.position[1]=-320;car.position[2]=-199;
        car.basis[0]=s;car.basis[2]=c;car.basis[3]=c;car.basis[5]=-s;
        for(int f=-180;f<=220;f+=13) for(int r=-140;r<=140;r+=17) {
            float x=car.position[0]+s*f+c*r,z=car.position[2]+c*f-s*r;
            float dx=fmaxf(0,fabsf(f-ROCKET_BODY_FORWARD_OFFSET)-ROCKET_BODY_HALF_LENGTH);
            float dz=fmaxf(0,fabsf((float)r)-ROCKET_BODY_HALF_WIDTH);
            assert(hit(&car,x,-300,z,12,20)==(dx*dx+dz*dz<144));
        }
    }
    // Nose straight up: height and offset rotate with the box.
    car=pose();car.basis[2]=car.basis[7]=0;car.basis[1]=1;car.basis[8]=-1;
    assert(hit(&car,0,140,-40,5,5));
    assert(!hit(&car,0,155,-40,5,5));
    assert(!hit(&car,0,0,5,5,5));
    // Upside down: no standing-Mario phantom volume above the roof.
    car=pose();car.basis[3]=-1;car.basis[7]=-1;
    assert(hit(&car,0,-60,0,5,20));assert(!hit(&car,0,0,0,5,40));
    // At 45-degree pitch, the thin upper slice excludes an AABB corner.
    car=pose();car.basis[1]=car.basis[2]=car.basis[7]=.7071067812f;car.basis[8]=-.7071067812f;
    assert(hit(&car,0,140,55,3,2));assert(!hit(&car,0,140,-70,3,2));

    car=pose();car.basis[0]=NAN;assert(!hit(&car,0,20,0,5,30));
    car=pose();car.basis[2]=2;assert(!hit(&car,0,20,0,5,30));
    car=pose();car.position[0]=INFINITY;assert(!hit(&car,0,20,0,5,30));
    car=pose();assert(!hit(&car,0,20,0,NAN,30));assert(!hit(&car,0,NAN,0,5,30));
    assert(!hit(&car,0,20,0,-1,30));assert(!hit(&car,0,20,0,5,-1));
    puts("PASS offset Octane box/cylinder: body faces, rounded corners, yaw oracle, pitch/roll, vertical gaps and invalid poses");
}
