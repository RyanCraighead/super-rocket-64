#include "pc/character_net_codec.h"
/* Model the native action's authored root yaw and its corrected final face yaw.
 * Actual door-action execution is additionally covered by the native fixture. */
static void door_assert_yaw(s16 yaw) {
    RocketSnapshot snapshot;assert(character_presentation_car_snapshot(&snapshot));
    assert(fabsf(snapshot.basis[0]-sins(yaw))<.0001f&&fabsf(snapshot.basis[2]-coss(yaw))<.0001f);
    CharacterNetState sent={0},received={0};sent.speed_percent=100;uint8_t wire[CNET_WIRE_SIZE];
    sent.kind=CNET_OCTANE;sent.active=CNET_PRESENTATION;sent.car=snapshot;
    assert(character_net_encode(wire,sizeof wire,&sent)&&character_net_decode(&received,wire,sizeof wire));
    assert(received.active==CNET_PRESENTATION&&!received.interaction);
    assert(!memcmp(received.car.basis,snapshot.basis,sizeof snapshot.basis));
}
static void test_door_presentation(void) {
    unsigned checks=0;
    for(int yaw=0;yaw<65536;yaw+=8192)for(unsigned side=1;side<=2;side++)for(unsigned warp=0;warp<2;warp++) {
        fresh(CHARACTER_OCTANE);struct Object door={0};door.oMoveAngleYaw=yaw;
        mario.usedObj=&door;mario.action=side==1?ACT_PULLING_DOOR:ACT_PUSHING_DOOR;mario.actionArg=side|(warp?4:0);
        s16 travel=(s16)(yaw+(side==2?0x8000:0));
        for(int frame=0;frame<4;frame++) {
            character_presentation_begin(&mario);
            mario.faceAngle[1]=playerObject.header.gfx.angle[1]=yaw; // native root animation orientation
            struct MarioState before=mario;
            character_presentation_finish(&mario,0);door_assert_yaw(travel);checks++;
            assert(!memcmp(&before,&mario,sizeof mario));
        }
        character_presentation_begin(&mario);mario.action=ACT_IDLE;mario.actionArg=0;mario.faceAngle[1]=travel;
        character_presentation_finish(&mario,0);door_assert_yaw(travel);checks++; // graphics angle is still old
        if(warp) {
            gCurrLevelNum++;mario.action=ACT_WARP_DOOR_SPAWN;mario.faceAngle[1]=(s16)(travel+0x4000);
            character_presentation_begin(&mario);character_presentation_finish(&mario,0);door_assert_yaw(mario.faceAngle[1]);checks++;
        }
        choice=CHARACTER_MARIO;character_presentation_begin(&mario);character_presentation_finish(&mario,0);assert(!presenting);checks++;
    }
    printf("PASS door presentation: %u checks; push/pull, both sides, eight headings, local/warp final frames and character changes\n",checks);
}
