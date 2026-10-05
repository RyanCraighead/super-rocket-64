#include "../../../src/game/rocket_sounds.c"
#include <stdio.h>
#include <stdlib.h>
struct MarioState gMarioStates[MAX_PLAYERS];
static int plays,stops,allStops,checks;
static u32 lastSound;
static f32 *lastPosition;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void play_sound(s32 sound,f32 *pos){++plays;lastSound=sound;lastPosition=pos;}
void stop_sound(u32 sound,f32 *pos){++stops;CHECK(sound==SOUND_MOVING_FLYING);CHECK(pos==carSoundPosition);}
void stop_sounds_from_source(f32 *pos){++allStops;CHECK(pos==carSoundPosition);}
int main(void){
    struct Object local={0},remote={0};gMarioStates[0].marioObj=&local;gMarioStates[1].marioObj=&remote;
    local.header.gfx.cameraToObject[0]=123;remote.header.gfx.cameraToObject[0]=-999;
    rocket_audio_mario(0,0,1);CHECK(allStops==0);
    rocket_audio_mario(ROCKET_SOUND_JUMP,0,0);CHECK(plays==1&&lastSound==SOUND_MARIO_YAH_WAH_HOO&&lastPosition[0]==123);
    rocket_audio_mario(ROCKET_SOUND_FLIP,0,0);CHECK(plays==2&&lastSound==SOUND_ACTION_SPIN);
    rocket_audio_mario(ROCKET_SOUND_DOUBLE_JUMP,0,0);CHECK(plays==3&&lastSound==SOUND_MARIO_YAHOO);
    for(int i=0;i<10;i++)rocket_audio_mario(0,1,0);
    CHECK(plays==13&&lastSound==SOUND_MOVING_FLYING&&!(lastSound&SOUND_DISCRETE));
    rocket_audio_mario(0,0,0);CHECK(plays==13&&stops==4);
    rocket_audio_mario(0,0,1);CHECK(allStops==1);
    rocket_audio_mario(0,0,1);CHECK(allStops==1);
    gMarioStates[0].marioObj=NULL;rocket_audio_mario(7,1,0);CHECK(plays==13);
    printf("native local car sound bridge: %d checks passed\n",checks);return 0;
}
