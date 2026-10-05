/* Execute the real native push/pull action and door-spawn transform. Animation
 * services are explicit deterministic mocks; no ROM animation data is used. */
#define main presentation_suite_main
#include "test_presentation.c"
#undef main
#include "level_table.h"
#include "game/hardcoded.h"
static int animationEnded,animationId,warpRequests;
struct MarioState gMarioStates[MAX_PLAYERS];
bool gNeverEnteredCastle;
struct BehaviorValues gBehaviorValues;
static s16 travelYaw;
s16 set_character_animation(struct MarioState *m,enum CharacterAnimID animID){(void)m;animationId=animID;return 0;}
void update_mario_pos_for_anim(struct MarioState *m) {
    /* Simulate native root translation through the door, along travel rather
     * than the authored root heading of the push animation. */
    assert(animationId==CHAR_ANIM_PULL_DOOR_WALK_IN||animationId==CHAR_ANIM_PUSH_DOOR_WALK_IN);
    m->pos[0]+=10*sins(travelYaw);m->pos[2]+=10*coss(travelYaw);
}
void stop_and_set_height_to_floor(struct MarioState *m) {
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);
    vec3s_set(m->marioObj->header.gfx.angle,0,m->faceAngle[1],0);
}
s32 is_anim_at_end(struct MarioState *m){(void)m;return animationEnded;}
s16 level_trigger_warp(struct MarioState *m,s32 op){assert(m->playerIndex==0&&op==WARP_OP_WARP_DOOR);warpRequests++;return 20;}
u32 set_mario_action(struct MarioState *m,u32 action,u32 arg){m->prevAction=m->action;m->action=action;m->actionArg=arg;m->actionTimer=0;return 1;}
/* Generated verbatim from the current native source by test_door_handoff.sh. */
#include "native_door_actions.inc.c"
int main(void) {
    unsigned checks=0;
    for(int yaw=0;yaw<65536;yaw+=8192)for(unsigned side=1;side<=2;side++)for(unsigned warp=0;warp<2;warp++) {
        fresh(CHARACTER_OCTANE);struct Object door={0};door.oMoveAngleYaw=yaw;
        mario.usedObj=mario.interactObj=&door;mario.action=side==1?ACT_PULLING_DOOR:ACT_PUSHING_DOOR;mario.actionArg=side|(warp?4:0);
        travelYaw=(s16)(yaw+(side==2?0x8000:0));animationEnded=0;warpRequests=0;
        for(int tick=0;tick<18;tick++) {
            character_presentation_begin(&mario);animationEnded=!warp&&tick==17;
            act_going_through_door(&mario);
            struct MarioState native=mario;character_presentation_finish(&mario,0);
            door_assert_yaw(travelYaw);assert(!memcmp(&native,&mario,sizeof mario));checks++;
        }
        assert(warp?warpRequests==1:mario.action==ACT_IDLE&&mario.faceAngle[1]==travelYaw);checks++;
        if(warp) {
            struct SpawnInfo spawn={0};spawn.startAngle[1]=(s16)(yaw+0x4000);
            init_door_warp(&spawn,side|4);assert(spawn.startAngle[1]==(s16)(travelYaw+0x4000));
            gCurrLevelNum++;mario.action=ACT_WARP_DOOR_SPAWN;mario.faceAngle[1]=spawn.startAngle[1];
            character_presentation_begin(&mario);stop_and_set_height_to_floor(&mario);character_presentation_finish(&mario,0);
            door_assert_yaw(spawn.startAngle[1]);checks++;
        }
    }
    printf("PASS native door execution: %u checks; actual push/pull actions, authored sides, last-frame reset and destination spawn orientation\n",checks);
}
