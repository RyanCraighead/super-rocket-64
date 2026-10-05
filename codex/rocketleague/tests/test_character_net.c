#include "../../../src/pc/character_net_codec.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static CharacterNetState pose(void){
    CharacterNetState s={0};s.speed_percent=100;s.kind=CNET_OCTANE;s.active=1;s.sequence=100;s.epoch=2;s.area_sequence=0x1234;
    s.car.basis[0]=s.car.basis[4]=s.car.basis[8]=1;s.car.boost=100;s.car.grounded=1;
    for(int i=0;i<4;i++){s.car.wheel_radius[i]=32;s.car.wheel_position[i][1]=40;s.car.wheel_position[i][2]=-30;}
    s.car.ticks=UINT64_C(0x100000002);return s;
}
int main(void){
    uint8_t wire[CNET_WIRE_SIZE],copy[CNET_WIRE_SIZE];CharacterNetState a=pose(),b;
    CHECK(character_net_encode(wire,sizeof wire,&a));CHECK(character_net_decode(&b,wire,sizeof wire));
    CHECK(b.area_sequence==0x1234);
    CHECK(wire[4]==4);
    a.car.boosting=1;CHECK(character_net_encode(copy,sizeof copy,&a));
    CHECK(character_net_decode(&b,copy,sizeof copy)&&b.car.boosting);
    copy[4]=2;CHECK(!character_net_decode(&b,copy,sizeof copy));
    a.car.boosting=0;
    CHECK(b.sequence==100&&b.epoch==2&&b.car.ticks==a.car.ticks&&b.car.boost==100&&b.car.grounded);
    CHECK(wire[8]==100&&wire[9]==0&&wire[188]==2&&wire[192]==1); /* little endian golden offsets */
    for(size_t n=0;n<sizeof wire;n++)CHECK(!character_net_decode(&b,wire,n));
    const int corrupt[]={0,4,5,6,7,16,198,205};
    for(unsigned i=0;i<sizeof corrupt/sizeof *corrupt;i++){memcpy(copy,wire,sizeof wire);copy[corrupt[i]]=255;CHECK(!character_net_decode(&b,copy,sizeof copy));}
    /* Nonfinite / reflected / non-orthonormal / wildly displaced inputs rejected atomically. */
    CharacterNetState unchanged={0};unchanged.speed_percent=100;unchanged.sequence=42;b=unchanged;
    memcpy(copy,wire,sizeof wire);copy[22]=0x80;copy[23]=0x7f;
    CHECK(!character_net_decode(&b,copy,sizeof copy));CHECK(b.sequence==42);
    b=a;b.car.basis[0]=-1;CHECK(!character_net_encode(copy,sizeof copy,&b));
    b=a;b.car.basis[1]=.5f;CHECK(!character_net_encode(copy,sizeof copy,&b));
    b=a;b.car.wheel_position[0][0]=1001;CHECK(!character_net_encode(copy,sizeof copy,&b));
    b=a;b.car.velocity[0]=INFINITY;CHECK(!character_net_encode(copy,sizeof copy,&b));
    b=a;b.car.boost=NAN;CHECK(!character_net_encode(copy,sizeof copy,&b));
    CharacterNetTrack t={0};CHECK(character_net_track_push(&t,&a,1));
    CHECK(!character_net_track_push(&t,&a,1.1));b=a;b.sequence=99;CHECK(!character_net_track_push(&t,&b,1.1));
    b=a;b.sequence++;b.car.position[0]=100;for(int i=0;i<4;i++)b.car.wheel_position[i][0]=100;
    /* 180 degree aerial roll: halfway must still be an orthonormal rotation. */
    b.car.basis[4]=-1;b.car.basis[8]=-1;
    for(int i=0;i<4;i++){b.car.wheel_position[i][1]=-40;b.car.wheel_position[i][2]=30;}
    CHECK(character_net_track_push(&t,&b,1.1));CharacterNetState interpolated;
    CHECK(character_net_track_sample(&t,1.15,&interpolated));
    CHECK(fabsf(interpolated.car.position[0]-50)<.001f);
    /* Source interaction permission differs from drawable activity. Contacts
     * use the received pose, never the 50-unit interpolated render pose. */
    CharacterNetState contact;
    CHECK(!character_net_track_contact(&t,1.15,&contact));
    b.interaction=1;b.sequence++;CHECK(character_net_track_push(&t,&b,1.15));
    CHECK(character_net_track_contact(&t,1.2,&contact));CHECK(contact.car.position[0]==100);
    CHECK(!character_net_track_contact(&t,1.351,&contact));
    CHECK(!character_net_track_contact(&t,1.0,&contact));
    CHECK(!character_net_track_contact(&t,NAN,&contact));
    CHECK(character_net_encode(copy,sizeof copy,&b));CHECK(copy[4]==4&&copy[5]==1);
    CHECK(character_net_decode(&contact,copy,sizeof copy)&&contact.interaction);
    b.interaction=0;b.sequence++;CHECK(character_net_track_push(&t,&b,1.16));
    CHECK(!character_net_track_contact(&t,1.17,&contact));
    CHECK(character_net_track_support(&t,1.17,&contact));
    CHECK(!character_net_track_support(&t,1.361,&contact));
    CHECK(!character_net_track_support(&t,NAN,&contact));
    CHECK(fabsf(interpolated.car.basis[4])<.001f&&fabsf(fabsf(interpolated.car.basis[5])-1)<.001f);
    CHECK(character_net_encode(copy,sizeof copy,&interpolated));
    CHECK(fabsf(hypotf(interpolated.car.wheel_position[0][1],interpolated.car.wheel_position[0][2])-50)<.001f);
    CHECK(!character_net_track_sample(&t,2.201,&interpolated));
    CHECK(!character_net_track_sample(&t,1.0,&interpolated));
    CHECK(!character_net_track_sample(&t,NAN,&interpolated));
    /* Reset/warp snaps instead of flying across an unrelated level. */
    b.sequence++;b.epoch++;b.car.position[0]=300;
    CHECK(character_net_track_push(&t,&b,1.2));CHECK(character_net_track_sample(&t,1.2,&interpolated));CHECK(interpolated.car.position[0]==300);
    b.sequence++;b.car.position[0]=6000;for(int i=0;i<4;i++)b.car.wheel_position[i][0]=6000;
    CHECK(character_net_track_push(&t,&b,1.3));CHECK(character_net_track_sample(&t,1.3,&interpolated));CHECK(interpolated.car.position[0]==6000);
    /* Native door/cutscene/Mario ownership is explicit, not a stale car pose. */
    b.sequence++;b.active=0;CHECK(character_net_track_push(&t,&b,1.4));CHECK(character_net_track_sample(&t,1.4,&interpolated));CHECK(!interpolated.active);
    CHECK(character_net_encode(copy,sizeof copy,&b));CHECK(character_net_decode(&interpolated,copy,sizeof copy));CHECK(!interpolated.active);
    b=a;b.sequence=200;b.active=CNET_PRESENTATION;b.car.position[0]=250;
    b.interaction=1;CHECK(!character_net_encode(copy,sizeof copy,&b));
    b.interaction=0;
    CHECK(character_net_encode(copy,sizeof copy,&b));CHECK(copy[4]==4);
    CHECK(character_net_decode(&interpolated,copy,sizeof copy)&&interpolated.active==CNET_PRESENTATION);
    copy[4]=1;CHECK(!character_net_decode(&interpolated,copy,sizeof copy));
    CHECK(character_net_track_push(&t,&b,1.5));
    CHECK(!character_net_track_contact(&t,1.5,&contact));
    CHECK(!character_net_track_support(&t,1.5,&contact));
    b.sequence++;b.active=CNET_DRIVING;b.car.position[0]=300;
    CHECK(character_net_track_push(&t,&b,1.6));CHECK(character_net_track_sample(&t,1.6,&interpolated));
    CHECK(interpolated.car.position[0]==300); /* no blend across native handoff */
    /* Per-player tracks do not touch each other or source physics. Disconnect resets only the slot. */
    CharacterNetTrack other={0};CHECK(character_net_track_push(&other,&a,1));memset(&t,0,sizeof t);
    CHECK(!character_net_track_sample(&t,1.5,&b));CHECK(character_net_track_sample(&other,1.5,&b));CHECK(b.sequence==100);
    a.sequence=UINT32_MAX;CHECK(character_net_track_push(&t,&a,2));a.sequence=0;CHECK(character_net_track_push(&t,&a,2.1));
    a.sequence=UINT32_MAX;CHECK(!character_net_track_push(&t,&a,2.2));
    /* Every valid mutation across hundreds of arbitrary full orientations roundtrips. */
    for(int i=0;i<360;i++){
        a=pose();float angle=i*.01745329252f,c=cosf(angle),s=sinf(angle);
        a.car.basis[0]=c;a.car.basis[1]=s;a.car.basis[3]=-s;a.car.basis[4]=c;
        CHECK(character_net_encode(copy,sizeof copy,&a));CHECK(character_net_decode(&b,copy,sizeof copy));
    }
    printf("character network: %u checks passed\n",checks);return 0;
}
