#ifndef SM64_CHARACTER_SWITCH_H
#define SM64_CHARACTER_SWITCH_H
#ifdef __cplusplus
extern "C" {
#endif

/* Stable UI/diagnostic identities, not network character IDs. */
enum CharacterSwitchId {
    CHARACTER_MARIO = 0,
    CHARACTER_LINK = 1,
    CHARACTER_BOMBERMAN = 2,
    CHARACTER_BANJO = 3,
    CHARACTER_SPIDERMAN = 4,
    CHARACTER_TONY = 5,
    CHARACTER_OCTANE = 6,
    CHARACTER_COUNT = 7
};

/* Called once after the local asset loaders have completed. No hot loading. */
void character_switch_init(void);
int character_switch_enabled(void);
/* Configured direct online wheel mode; readiness is checked separately. */
int character_switch_online(void);
int character_switch_available(enum CharacterSwitchId id);
const char *character_switch_reason(enum CharacterSwitchId id);
const char *character_switch_name(enum CharacterSwitchId id);
enum CharacterSwitchId character_switch_active(void);
int character_switch_accepts(enum CharacterSwitchId id);
/* Effective local model only; never rewrites the user's persisted model choice. */
int character_switch_native_model(int player_index, int configured_model);
/* NULL means allowed. Both opening and committing independently recheck. */
const char *character_switch_can_open(void);
int character_switch_prepare_open(void);
int character_switch_commit(enum CharacterSwitchId id);
/* SDL only queues intent. Apply at the next normal simulation boundary before
 * actor updates, so the selected source acquires this same render frame. */
int character_switch_request(enum CharacterSwitchId id);
/* -1 rejected, 0 no request, 1 successfully applied (including reselect). */
int character_switch_apply_pending(void);
int character_switch_pending(void);
void character_switch_cancel_pending(void);

typedef struct CharacterSwitchSnapshot {
    unsigned commits;
    int previous, active;
    float before_position[3], after_position[3];
    int before_health, after_health;
} CharacterSwitchSnapshot;
/* Read-only last successful real commit, captured before the next game tick. */
int character_switch_snapshot(CharacterSwitchSnapshot *out);

#ifdef __cplusplus
}
#endif
#endif
