#ifndef ROCKET_AUDIO_H
#define ROCKET_AUDIO_H
#include <stddef.h>
#include <stdint.h>
#include "../../codex/rocketleague/physics/rocket_physics.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { ROCKET_SOUND_JUMP=1, ROCKET_SOUND_FLIP=2, ROCKET_SOUND_DOUBLE_JUMP=4 };
/* Local car only. Render/network snapshots must never call update. */
void rocket_audio_load(const char *model_directory);
void rocket_audio_shutdown(void);
void rocket_audio_stop(void);
void rocket_audio_reset(void);
void rocket_audio_update(const RocketSnapshot *state, unsigned style, int active);
int rocket_audio_available(void);
void rocket_audio_mix(int16_t *stereo, size_t frames, float gain);
/* Main-thread native sound bridge; never called from the audio thread. */
void rocket_audio_mario(unsigned events, int boosting, int stop);
#ifdef __cplusplus
}
#endif
#endif
