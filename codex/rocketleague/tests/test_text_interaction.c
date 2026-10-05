/* Real car adapter and native text entry functions; no ROM assets or UI. */
#define TEST_NATIVE_MATH
#define main adapter_suite_main
#include "test_adapter.c"
#undef main
void push_mario_out_of_object(struct MarioState *m,struct Object *o,f32 padding){(void)m;(void)o;(void)padding;}
#include "native_text_functions.inc.c"

static struct Object textObject;
static unsigned checks;
static void text_setup(int npc,int network) {
    fresh();step();
    object.hitboxRadius=37;object.hitboxHeight=160;
    memset(&textObject,0,sizeof textObject);
    textObject.activeFlags=ACTIVE_FLAG_ACTIVE;textObject.oInteractType=INTERACT_TEXT;
    textObject.oInteractionSubtype=npc?INT_SUBTYPE_NPC:INT_SUBTYPE_SIGN;
    textObject.oPosY=mario.pos[1]-40;textObject.oPosZ=mario.pos[2]+100;
    textObject.hitboxRadius=150;textObject.hitboxHeight=100;textObject.oMoveAngleYaw=(s16)0x8000;
    object.collidedObjs[0]=&textObject;object.numCollidedObjs=1;
    object.collidedObjInteractTypes=mario.collidedObjInteractTypes=INTERACT_TEXT;
    gNetworkType=network;gNetworkPlayerLocal=&gNetworkPlayers[0];gNetworkAreaLoaded=true;
    rocket_adapter_prepare_interactions(&mario); // neutral arms the edge
}
static int text_press(int down) {
    fixtureGamepad.connected=fixtureGamepad.isolated=1;fixtureGamepad.jump=down;
    controller.buttonDown=0;
    ++gGlobalTimer;previousFrame=gGlobalTimer;
    rocket_adapter_prepare_interactions(&mario);
    u16 input=mario.input;int result=interact_text(&mario,INTERACT_TEXT,&textObject);
    assert(mario.input==input);checks++;
    return result;
}
int main(void) {
    for(int npc=0;npc<2;npc++)for(int net=NT_NONE;net<=NT_CLIENT;net++) {
        text_setup(npc,net);
        assert(text_press(1));assert(mario.usedObj==&textObject);
        assert(mario.action==(npc?ACT_WAITING_FOR_DIALOG:ACT_READING_SIGN));checks++;
        /* Native dialog takes over; a held initiating/closing input is not
         * a new text request after car reacquisition. */
        assert(!rocket_adapter_update(&mario));
        mario.action=ACT_IDLE;textObject.oInteractStatus=0;step();
        for(int i=0;i<8;i++)assert(!text_press(1));
        assert(!text_press(0));assert(text_press(1));
        for(int repeat=0;repeat<3;repeat++) {
            assert(!rocket_adapter_update(&mario));mario.action=ACT_IDLE;textObject.oInteractStatus=0;step();
            assert(!text_press(0));assert(text_press(1));
        }
    }
    /* Rejected contextual interaction leaves car jump held for physics. */
    for(int mode=0;mode<18;mode++) {
        text_setup(mode&1,NT_NONE);
        switch(mode) {
            case 0:object.numCollidedObjs=0;break;
            case 1:mario.faceAngle[1]=(s16)0x8000;break;
            case 2:textObject.oMoveAngleYaw=0;break;
            case 3:pose.grounded=0;mario.action=ACT_FREEFALL;break;
            case 4:pose.basis[7]=.2f;break;
            case 5:pose.flipping=1;break;
            case 6:textObject.oIntangibleTimer=-1;break;
            case 7:textObject.activeFlags=0;break;
            case 8:textObject.header.gfx.activeAreaIndex=2;break;
            case 9:textObject.oInteractStatus=INT_STATUS_INTERACTED;break;
            case 10:mario.freeze=1;break;
            case 11:uiBlocked=1;break;
            case 12:sCurrPlayMode=PLAY_MODE_PAUSED;break;
            case 13:gNetworkType=NT_CLIENT;gNetworkAreaSyncing=true;break;
            case 14:gNetworkType=NT_SERVER;gNetworkAreaLoaded=false;break;
            case 15:mario.playerIndex=1;break;
            case 16:door_wall(2,0);textObject.oPosZ=150;break;
            case 17:mario.pos[2]+=501;break;
        }
        assert(!text_press(1));assert(mario.usedObj==NULL);checks++;
        if(mode==0||mode==1||mode==2) {step();assert(observed.jump);checks++;}
    }
    text_setup(0,NT_NONE);fixtureGamepad.connected=fixtureGamepad.isolated=fixtureGamepad.jump=1;++gGlobalTimer;previousFrame=gGlobalTimer;
    rocket_adapter_prepare_interactions(&mario);rocket_adapter_prepare_interactions(&mario);
    assert(interact_text(&mario,INTERACT_TEXT,&textObject));checks++;
    /* Native Mario still uses native A or B, without a car bridge. */
    for(int b=0;b<2;b++) {
        text_setup(1,NT_NONE);rocket_adapter_set_selected(0);mario.input=b?INPUT_B_PRESSED:INPUT_A_PRESSED;
        assert(interact_text(&mario,INTERACT_TEXT,&textObject));checks++;
    }
    printf("PASS contextual native text: %u checks; signs/NPCs, fresh/held/repeated input, native facing/range, jump fallback, dialog return and local offline/host/client gates\n",checks);
    return 0;
}
