#ifndef CHARACTER_NET_CODEC_H
#define CHARACTER_NET_CODEC_H
#include <stddef.h>
#include <stdint.h>
#include "../../codex/rocketleague/physics/rocket_physics.h"
/* Explicit wire identifiers. Reserved source characters remain offline until
 * their own state/effects/asset contracts are implemented and verified. */
enum CharacterNetKind { CNET_MARIO=0, CNET_LINK=1, CNET_BOMBERMAN=2,
    CNET_BANJO=3, CNET_SPIDERMAN=4, CNET_TONY_HAWK=5, CNET_OCTANE=6 };
#define CNET_WIRE_SIZE 208
#define CNET_VERSION_SUFFIX "-cnet3-octane2-boost-mode1-enemy1-coin1-boss2-whomp2-switch1-vanish1-jet1-env1-metal1-caps1-platform1-wheel1-bump1"
/* Presentation has a valid mesh pose but MUST NOT participate in car contacts. */
enum CharacterNetActivity { CNET_INACTIVE=0, CNET_DRIVING=1, CNET_PRESENTATION=2 };
typedef struct CharacterNetState {
    uint32_t sequence, epoch;
    uint16_t area_sequence;
    uint8_t kind, active, interaction;
    RocketSnapshot car;
} CharacterNetState;
typedef struct CharacterNetTrack {
    CharacterNetState previous, latest;
    double received, duration;
    int valid;
} CharacterNetTrack;
int character_net_encode(uint8_t *out,size_t size,const CharacterNetState *state);
int character_net_decode(CharacterNetState *out,const uint8_t *wire,size_t size);
int character_net_track_push(CharacterNetTrack *track,const CharacterNetState *state,double now);
int character_net_track_sample(const CharacterNetTrack *track,double now,CharacterNetState *out);
/* Raw, fresh source state only. Rendering interpolation never authorizes hits. */
int character_net_track_support(const CharacterNetTrack *track,double now,CharacterNetState *out);
int character_net_track_contact(const CharacterNetTrack *track,double now,CharacterNetState *out);
#endif
