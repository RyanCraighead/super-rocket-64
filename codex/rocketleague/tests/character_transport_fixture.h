/* Shared inert host fixture. Includes real packet writer/ingress/apply code.
 * No sockets, SDL calls, windows, controllers or game processes are used. */
#define DISABLE_MODULE_LOG 1
#define character_net_accept observed_accept
#include "../../../src/pc/network/packets/packet_player.c"
#undef character_net_accept
#include "../../../src/pc/network/packets/packet.c"
#include <stdlib.h>
#include "game/character_switch.h"
static int checks,forwarded,accepted;
static int switchEnabled;
static enum CharacterSwitchId selectedCharacter=CHARACTER_MARIO;
int character_switch_enabled(void){return switchEnabled;}
enum CharacterSwitchId character_switch_active(void){return selectedCharacter;}
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
struct CLIOptions gCLIOpts;
enum NetworkType gNetworkType;
struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS],*gNetworkPlayerLocal,*gNetworkPlayerServer;
struct NetworkSystem *gNetworkSystem;
struct ServerSettings gServerSettings;
bool gDjuiInMainMenu;
bool network_player_any_connected(void){return true;}
#ifndef ROCKET_PLATFORM_REAL_TEST
int rocket_platform_car(unsigned index){return index?character_net_is_car(index):gCLIOpts.rocketCar;}
#endif
static struct Packet *capturedPacket;
static const RocketSnapshot *sourceSnapshot;
static const RocketSnapshot *presentationSnapshot;
static uint32_t sourceEpoch;
static RocketSnapshot drawnSnapshot;
static unsigned drawCalls;
static uint32_t drawnCaps;
static double fixtureNow=1;
void network_send(struct Packet *p){if(capturedPacket)*capturedPacket=*p;}
uint32_t rocket_runtime_epoch(void){return sourceEpoch;}
int rocket_runtime_snapshot(RocketSnapshot *s){if(!sourceSnapshot)return 0;*s=*sourceSnapshot;return 1;}
int character_presentation_car_snapshot(RocketSnapshot *s){if(!presentationSnapshot)return 0;*s=*presentationSnapshot;return 1;}
int rocket_runtime_draw_snapshot(const RocketSnapshot *s,const float *v,const float *p,const int *w){(void)v;(void)p;(void)w;drawnSnapshot=*s;drawCalls++;return 1;}
int rocket_runtime_draw_snapshot_caps(const RocketSnapshot *s,uint32_t caps,const float *v,const float *p,const int *w){drawnCaps=caps;return rocket_runtime_draw_snapshot(s,v,p,w);}
void *gNetworkServerAddr;
u32 gNetworkStartupTimer;
s16 gCurrCourseNum,gCurrActStarNum,gCurrLevelNum,gCurrAreaIndex;
bool gNetworkAreaLoaded;
struct MarioState gMarioStates[MAX_PLAYERS];
struct PlayerCameraState gPlayerCameraState[MAX_PLAYERS];
struct LevelValues gLevelValues;
static struct Object objects[MAX_PLAYERS];
static struct Controller controllers[MAX_PLAYERS];

static char *id(u8 i){(void)i;return "test";}
bool ban_list_contains(char *s){(void)s;return false;}
bool network_allow_unknown_local_index(enum PacketType t){(void)t;return false;}
void network_send_ack(struct Packet *p){(void)p;}
u8 network_player_disconnected(u8 i){return i;}
void network_send_kick(u8 i,enum KickReasonType r){(void)i;(void)r;}
void network_send_to(u8 i,struct Packet *p){(void)i;(void)p;forwarded++;}
void packet_ordered_add(struct Packet *p){packet_process(p);}
struct NetworkPlayer *network_player_from_global_index(u8 i){for(int n=0;n<MAX_PLAYERS;n++)if(gNetworkPlayers[n].connected&&gNetworkPlayers[n].globalIndex==i)return &gNetworkPlayers[n];return NULL;}
s32 get_dialog_id(void){return -1;}
#if !defined(ROCKET_WING_REAL_TEST) && !defined(ROCKET_PLATFORM_REAL_TEST)
struct SyncObject *sync_object_get(u32 i){(void)i;return NULL;}
#endif
void construct_player_popup(struct NetworkPlayer *np,char *msg,const char *level){(void)np;(void)msg;(void)level;}
void mario_drop_held_object(struct MarioState *m){(void)m;}
s32 force_idle_state(struct MarioState *m){(void)m;return 0;}
u32 set_mario_action(struct MarioState *m,u32 action,u32 arg){(void)m;(void)action;(void)arg;return 0;}
f32 find_floor_height(f32 x,f32 y,f32 z){(void)x;(void)y;(void)z;return -11000;}
f32 find_ceil_height(f32 x,f32 y,f32 z){(void)x;(void)y;(void)z;return 20000;}
struct Object *gCheckingSurfaceCollisionsForObject;
void play_character_sound(struct MarioState *m,enum CharacterSound s){(void)m;(void)s;}
int character_net_accept(unsigned index,const CharacterNetState *s);
int observed_accept(unsigned index,const CharacterNetState *s){int ok=character_net_accept(index,s);accepted+=ok;return ok;}
f64 clock_elapsed_f64(void){return fixtureNow;}
char *djui_language_get(const char *a,const char *b){(void)a;(void)b;return "test";}
void network_receive_ack(struct Packet *p){(void)p;}
#ifndef ROCKET_PLATFORM_REAL_TEST
void network_receive_object(struct Packet *p){(void)p;}
#endif
void network_receive_spawn_objects(struct Packet *p){(void)p;}
void network_receive_spawn_star(struct Packet *p){(void)p;}
void network_receive_spawn_star_nle(struct Packet *p){(void)p;}
void network_receive_collect_star(struct Packet *p){(void)p;}
void network_receive_collect_coin(struct Packet *p){(void)p;}
bool network_character_coin_valid(struct Packet *p){(void)p;return true;}
static unsigned coinClearCalls[MAX_PLAYERS];
void network_coin_boost_clear(unsigned index){if(index<MAX_PLAYERS)coinClearCalls[index]++;}
void network_receive_collect_item(struct Packet *p){(void)p;}
void network_receive_global_popup(struct Packet *p){(void)p;}
void network_receive_debug_sync(struct Packet *p){(void)p;}
void network_receive_join_request(struct Packet *p){(void)p;}
void network_receive_join(struct Packet *p){(void)p;}
void network_receive_chat(struct Packet *p){(void)p;}
void network_receive_kick(struct Packet *p){(void)p;}
void network_receive_chat_command(struct Packet *p){(void)p;}
void network_receive_moderator(struct Packet *p){(void)p;}
void network_receive_keep_alive(struct Packet *p){(void)p;}
void network_receive_leaving(struct Packet *p){(void)p;}
void network_receive_save_file(struct Packet *p){(void)p;}
void network_receive_save_set_flag(struct Packet *p){(void)p;}
void network_receive_save_remove_flag(struct Packet *p){(void)p;}
void network_receive_network_players(struct Packet *p){(void)p;}
void network_receive_death(struct Packet *p){(void)p;}
void network_receive_ping(struct Packet *p){(void)p;}
void network_receive_pong(struct Packet *p){(void)p;}
void network_receive_change_level(struct Packet *p){(void)p;}
void network_receive_change_area(struct Packet *p){(void)p;}
void network_receive_level_area_request(struct Packet *p){(void)p;}
void network_receive_level_request(struct Packet *p){(void)p;}
void network_receive_level(struct Packet *p){(void)p;}
void network_receive_area_request(struct Packet *p){(void)p;}
void network_receive_area(struct Packet *p){(void)p;}
void network_receive_sync_valid(struct Packet *p){(void)p;}
void network_receive_level_spawn_info(struct Packet *p){(void)p;}
void network_receive_level_macro(struct Packet *p){(void)p;}
void network_receive_level_area_inform(struct Packet *p){(void)p;}
void network_receive_level_respawn_info(struct Packet *p){(void)p;}
void network_receive_change_water_level(struct Packet *p){(void)p;}
void network_receive_player_settings(struct Packet *p){(void)p;}
void network_receive_mod_list_request(struct Packet *p){(void)p;}
void network_receive_mod_list(struct Packet *p){(void)p;}
void network_receive_download_request(struct Packet *p){(void)p;}
void network_receive_download(struct Packet *p){(void)p;}
void network_receive_mod_list_entry(struct Packet *p){(void)p;}
void network_receive_mod_list_file(struct Packet *p){(void)p;}
void network_receive_mod_list_done(struct Packet *p){(void)p;}
void network_receive_lua_sync_table_request(struct Packet *p){(void)p;}
void network_receive_lua_sync_table(struct Packet *p){(void)p;}
void network_receive_network_players_request(struct Packet *p){(void)p;}
void network_receive_request_failed(struct Packet *p){(void)p;}
void network_receive_lua_custom(struct Packet *p){(void)p;}
void network_receive_lua_custom_bytestring(struct Packet *p){(void)p;}
void network_receive_custom(struct Packet *p){(void)p;}
#ifndef ROCKET_WING_REAL_TEST
/* Ingress tests explicitly provide an authority-issued lease. Incoming Mario
 * flags alone cannot create one; the real lease lifecycle is tested by Wing. */
static u32 verifiedCapFlags[MAX_PLAYERS];
u32 rocket_caps_active_flags(unsigned i){return i<MAX_PLAYERS?verifiedCapFlags[i]&MARIO_SPECIAL_CAPS:0;}
u32 rocket_caps_visual_flags(unsigned i){return i<MAX_PLAYERS?
    (gMarioStates[i].flags&~MARIO_SPECIAL_CAPS)|rocket_caps_active_flags(i):0;}
void rocket_caps_clear(unsigned i){
    if(i>=MAX_PLAYERS)return;
    if(verifiedCapFlags[i]){gMarioStates[i].flags&=~MARIO_SPECIAL_CAPS;gMarioStates[i].capTimer=0;}
    verifiedCapFlags[i]=0;
}
void rocket_caps_clear_all(void){for(unsigned i=0;i<MAX_PLAYERS;i++)rocket_caps_clear(i);}
void rocket_caps_apply(struct MarioState *m){
    if(!m||m->playerIndex>=MAX_PLAYERS)return;
    u32 flags=rocket_caps_active_flags(m->playerIndex);
    m->flags=(m->flags&~MARIO_SPECIAL_CAPS)|flags;
    if(flags){m->flags|=MARIO_CAP_ON_HEAD;m->flags&=~MARIO_CAP_IN_HAND;}
}
int rocket_caps_packet_allowed(const struct Packet *p){(void)p;return 0;}
int rocket_caps_cancel_allowed(const struct Packet *p){(void)p;return 0;}
void rocket_caps_receive_cancel(struct Packet *p){(void)p;}
int rocket_caps_item_allowed(const struct Packet *p){(void)p;return 1;}
#ifndef ROCKET_PLATFORM_REAL_TEST
int rocket_caps_object_allowed(struct Packet *p){(void)p;return 1;}
#endif
void rocket_caps_receive(struct Packet *p){(void)p;}
#endif
#ifndef ROCKET_WING_REAL_TEST
bool network_caps_spawn_allowed(const struct Packet *p){(void)p;return true;}
#endif
#ifndef ROCKET_BOOST_REAL_TEST
int rocket_boost_packet_allowed(const struct Packet *p){(void)p;return 0;}
void rocket_boost_receive_rule(struct Packet *p){(void)p;}
#endif

/* Boss authority is exercised by its separate integration fixture. */
#ifndef ROCKET_PLATFORM_REAL_TEST
void boss_net_receive(struct Packet *p){(void)p;}
#endif
#ifndef ROCKET_PLATFORM_REAL_TEST
int boss_net_legacy_packet_valid(struct Packet *p){(void)p;return 1;}
#endif
int boss_net_player_holds(struct Object *o,unsigned global){(void)o;(void)global;return 1;}
int rocket_adapter_interaction_snapshot(RocketSnapshot *state){if(!sourceSnapshot)return 0;*state=*sourceSnapshot;return 1;}

#ifndef ROCKET_PLATFORM_REAL_TEST
int rocket_platform_packet_allowed(const struct Packet *p){(void)p;return 1;}
#endif
