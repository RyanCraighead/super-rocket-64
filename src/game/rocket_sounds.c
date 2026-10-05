/* Mario-style fallback for local Octane movement; owns its own sound source. */
#include "sm64.h"
#include "audio/external.h"
#include "game/mario.h"
#include "game/level_update.h"
#include "pc/rocket_audio.h"
static Vec3f carSoundPosition;
static int ownSounds;
void rocket_audio_mario(unsigned events,int boosting,int stop) {
    if(stop) { if(ownSounds)stop_sounds_from_source(carSoundPosition); ownSounds=0; return; }
    struct MarioState *m=&gMarioStates[0];
    if(!m->marioObj)return;
    for(int i=0;i<3;i++)carSoundPosition[i]=m->marioObj->header.gfx.cameraToObject[i];
    ownSounds=1;
    if(events&ROCKET_SOUND_JUMP)play_sound(SOUND_MARIO_YAH_WAH_HOO,carSoundPosition);
    if(events&ROCKET_SOUND_DOUBLE_JUMP)play_sound(SOUND_MARIO_YAHOO,carSoundPosition);
    if(events&ROCKET_SOUND_FLIP)play_sound(SOUND_ACTION_SPIN,carSoundPosition);
    if(boosting)play_sound(SOUND_MOVING_FLYING,carSoundPosition);
    else stop_sound(SOUND_MOVING_FLYING,carSoundPosition);
}
