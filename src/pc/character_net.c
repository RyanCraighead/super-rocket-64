#include "game/rocket_caps.h"
#include "character_net.h"
#include "player_bump.h"
#include "rocket_runtime.h"
#include "rocket_boost.h"
#include "network/network.h"
#include "utils/misc.h"
#include "game/area.h"
#include "sm64.h"
#include "game/character_presentation.h"
#include "game/character_switch.h"
#include "game/rocket_adapter.h"
#include "object_fields.h"
#include "object_constants.h"
#include "engine/math_util.h"
#include <string.h>
#include <math.h>
static CharacterNetTrack tracks[MAX_PLAYERS];
static uint32_t generations[MAX_PLAYERS];
static uint32_t sequence;
#ifdef ROCKET_CAR_QA
static unsigned draws[MAX_PLAYERS];
static void record_motion(const char *stage,unsigned index,const CharacterNetState *s);
#endif
void character_net_clear(unsigned index){if(index<MAX_PLAYERS){player_bump_clear(index);rocket_caps_clear(index);if(gMarioStates[index].marioObj)gMarioStates[index].marioObj->platform=NULL;memset(&tracks[index],0,sizeof tracks[index]);generations[index]++;network_coin_boost_clear(index);}}
void character_net_clear_all(void){for(unsigned i=0;i<MAX_PLAYERS;i++)character_net_clear(i);}
unsigned character_net_local_kind(void){
    if(character_switch_enabled())return character_switch_active()==CHARACTER_OCTANE?CNET_OCTANE:CNET_MARIO;
    return gCLIOpts.rocketCar?CNET_OCTANE:CNET_MARIO;
}
int character_net_write(struct Packet *p){
    CharacterNetState s={0};s.sequence=++sequence;s.epoch=rocket_runtime_epoch();
    s.area_sequence=gNetworkPlayerLocal?gNetworkPlayerLocal->currLevelAreaSeqId:0;
    s.kind=character_net_local_kind();
    s.speed_percent=rocket_speed_percent();s.rule_revision=rocket_rule_revision();
    s.active=s.kind==CNET_OCTANE&&rocket_runtime_rule_ready()&&rocket_runtime_snapshot(&s.car);
    if(s.kind==CNET_OCTANE&&!s.active&&character_presentation_car_snapshot(&s.car))s.active=CNET_PRESENTATION;
    RocketSnapshot contact;
    s.interaction=s.active==CNET_DRIVING&&rocket_runtime_rule_ready()&&rocket_adapter_interaction_snapshot(&contact);
    uint8_t wire[CNET_WIRE_SIZE];
    if(!character_net_encode(wire,sizeof wire,&s)||p->cursor+sizeof wire+4>=PACKET_LENGTH)return 0;
#ifdef ROCKET_CAR_QA
    record_motion("sent",0,&s);
#endif
    packet_write(p,wire,sizeof wire);
    if(!p->error&&!p->writeError)player_bump_observe(0,&s,gMarioStates[0].pos,gMarioStates[0].vel);
    return !p->error&&!p->writeError;
}
int character_net_read(struct Packet *p,unsigned index,CharacterNetState *state){
    if(!state||index==0||index>=MAX_PLAYERS||p->error||p->cursor+CNET_WIRE_SIZE!=p->dataLength)return 0;
    if(!character_net_decode(state,p->buffer+p->cursor,CNET_WIRE_SIZE))return 0;
    if(state->area_sequence!=gNetworkPlayers[index].currLevelAreaSeqId||
       state->speed_percent!=rocket_speed_percent()||state->rule_revision!=rocket_rule_revision())return 0;
    p->cursor+=CNET_WIRE_SIZE;
    if(tracks[index].valid){uint32_t delta=state->sequence-tracks[index].latest.sequence;if(!delta||delta>=0x80000000u)return 0;}
    return 1;
}
int character_net_accept(unsigned index,const CharacterNetState *state){
    if(!state||index==0||index>=MAX_PLAYERS||state->speed_percent!=rocket_speed_percent()||
       state->rule_revision!=rocket_rule_revision())return 0;
    const CharacterNetTrack *track=&tracks[index];
    int transition=track->valid&&(state->kind!=track->latest.kind||state->epoch!=track->latest.epoch||
        state->area_sequence!=track->latest.area_sequence||state->speed_percent!=track->latest.speed_percent||
        state->rule_revision!=track->latest.rule_revision);
    int ok=character_net_track_push(&tracks[index],state,clock_elapsed_f64());
    /* A complete round trip may arrive between two native contact boundaries.
     * Invalidate only pose continuity, including accepted same-epoch kind
     * changes. Keep sequence rejection, cap leases and coin ledgers intact. */
    if(ok&&transition)generations[index]++;
#ifdef ROCKET_CAR_QA
    if(ok)record_motion("accepted",index,state);
#endif
    return ok;
}
static int same_area(unsigned index){
    if(!gCLIOpts.characterNet||index==0||index>=MAX_PLAYERS||!gNetworkPlayerLocal||!gNetworkAreaLoaded)return 0;
    const struct NetworkPlayer *np=&gNetworkPlayers[index],*local=gNetworkPlayerLocal;
    return np->connected&&np->currPositionValid&&np->currLevelSyncValid&&np->currAreaSyncValid&&
        np->currCourseNum==local->currCourseNum&&np->currActNum==local->currActNum&&
        np->currLevelNum==local->currLevelNum&&np->currAreaIndex==local->currAreaIndex;
}
int character_net_is_car(unsigned index){
    return gCLIOpts.characterNet&&index>0&&index<MAX_PLAYERS&&
        tracks[index].valid&&tracks[index].latest.kind==CNET_OCTANE;
}
static int physical_state(unsigned index,CharacterNetState *out,uint32_t *generation,int interaction){
    if(!out||!same_area(index))return 0;
    const struct MarioState *m=&gMarioStates[index];
    if(!gNetworkPlayerLocal->currLevelSyncValid||!gNetworkPlayerLocal->currAreaSyncValid||
       !m->marioObj||m->health<0x100||m->freeze||m->heldObj||m->heldByObj||m->riddenObj)return 0;
    const CharacterNetTrack *track=&tracks[index];
    double now=clock_elapsed_f64();
    if(!(interaction?character_net_track_contact(track,now,out):character_net_track_support(track,now,out))||
       track->latest.area_sequence!=gNetworkPlayers[index].currLevelAreaSeqId||
       track->latest.speed_percent!=rocket_speed_percent()||track->latest.rule_revision!=rocket_rule_revision())return 0;
    *out=track->latest;if(generation)*generation=generations[index];return 1;
}
int character_net_interaction_state(unsigned index,CharacterNetState *out,uint32_t *generation){
    return physical_state(index,out,generation,1);
}
int character_net_support_state(unsigned index,CharacterNetState *out,uint32_t *generation){
    return physical_state(index,out,generation,0);
}
int character_net_interaction_snapshot(unsigned index,CharacterNetState *out){
    return character_net_interaction_state(index,out,NULL);
}
int character_net_contact(unsigned index,CharacterNetState *state,unsigned *generation){
    uint32_t identity;
    int ok=character_net_interaction_state(index,state,&identity);
    if(ok&&generation)*generation=identity;
    return ok;
}
int character_net_player_fresh(unsigned index){
    CharacterNetState s;
    return same_area(index)&&character_net_track_sample(&tracks[index],clock_elapsed_f64(),&s);
}
int character_net_snapshot(unsigned index,RocketSnapshot *out){
    CharacterNetState s;
    if(!out||!same_area(index)||!character_net_track_sample(&tracks[index],clock_elapsed_f64(),&s)||
       !s.active||s.kind!=CNET_OCTANE)return 0;
    *out=s.car;return 1;
}
int character_net_pickup_pose(unsigned index,RocketSnapshot *out){
    if(!out||!character_net_player_fresh(index))return -1;
    const CharacterNetState *s=&tracks[index].latest;
    if(s->kind==CNET_MARIO)return 0;
    CharacterNetState contact;
    if(!character_net_interaction_state(index,&contact,NULL))return -1;
    *out=contact.car;return 1;
}
int character_net_remote_update(struct MarioState *m){
    if(!m||!m->marioObj||!gCLIOpts.characterNet||m->playerIndex==0||m->playerIndex>=MAX_PLAYERS)return 0;
    CharacterNetTrack *track=&tracks[m->playerIndex];
    if(!track->valid||!track->latest.active)return 0;
    /* Never predict an original controller using remote host-Mario inputs.
     * A stale active car stays hidden/intangible until a fresh pose arrives. */
    m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE;
    m->marioObj->oIntangibleTimer=-1;
    CharacterNetState s;
    if(same_area(m->playerIndex)&&character_net_track_sample(track,clock_elapsed_f64(),&s)){
        vec3f_copy(m->pos,s.car.position);vec3f_copy(m->marioObj->header.gfx.pos,m->pos);
        for(int k=0;k<3;k++)m->vel[k]=s.car.velocity[k]/30.f;
    }
    return 1;
}
int character_net_water_mode(unsigned index){
    CharacterNetState state;
    return same_area(index)&&character_net_track_sample(&tracks[index],clock_elapsed_f64(),&state)&&
        state.kind==CNET_OCTANE&&state.active==1?state.car.water_mode:ROCKET_WATER_DRY;
}
void character_net_draw(const float view[16],const float projection[16],const int viewport[4]){
    if(!gCLIOpts.characterNet)return;
    double now=clock_elapsed_f64();
    for(unsigned i=1;i<MAX_PLAYERS;i++){
        CharacterNetState s;
        if(same_area(i)&&character_net_track_sample(&tracks[i],now,&s)&&s.active&&s.kind==CNET_OCTANE){
            // Materials follow the same verified shared lease and native flicker
            // as local cap rendering, independent of owner-supplied flags.
            int drawn=rocket_runtime_draw_snapshot_player(&s.car,i,rocket_caps_visual_flags(i),view,projection,viewport);
#ifdef ROCKET_CAR_QA
            draws[i]+=!!drawn;
            if(drawn)record_motion("drawn",i,&s);
#else
            (void)drawn;
#endif
        }
    }
}
#ifdef ROCKET_CAR_QA
/* Passive observer: hidden OpenGL windows, no SDL/OS input injection, no
 * controller selection and no gameplay writes. Only explicit loopback QA. */
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include "level_table.h"
#include "lua/utils/smlua_level_utils.h"
#include "behavior_data.h"
#include "game/object_list_processor.h"
#include "game/mario.h"
#include "rocket_boost.h"
static void record_motion(const char *stage,unsigned index,const CharacterNetState *s){
    if(!gCLIOpts.characterNet||!gCLIOpts.loopbackOnly||!getenv("SM64_CHARACTER_NET_MOTION"))return;
    const RocketSnapshot *c=&s->car;
    fprintf(stderr,"CNET_MOTION {\"stage\":\"%s\",\"ms\":%u,\"index\":%u,\"sequence\":%u,\"epoch\":%u,\"area_sequence\":%u,\"level\":%d,\"active\":%u,\"ticks\":%llu,\"pos\":[%.7g,%.7g,%.7g],\"vel\":[%.7g,%.7g,%.7g],\"basis\":[%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g],\"boost\":%.7g,\"jump\":%d,\"double\":%d,\"flip\":%d,\"flip_time\":%.7g,\"ground\":%d}\n",
        stage,SDL_GetTicks(),index,s->sequence,s->epoch,s->area_sequence,gCurrLevelNum,s->active,(unsigned long long)c->ticks,
        c->position[0],c->position[1],c->position[2],c->velocity[0],c->velocity[1],c->velocity[2],
        c->basis[0],c->basis[1],c->basis[2],c->basis[3],c->basis[4],c->basis[5],c->basis[6],c->basis[7],c->basis[8],
        c->boost,c->jumped,c->double_jumped,c->flipped,c->flip_time,c->grounded);
}
int character_net_qa_hidden(void){
    return gCLIOpts.characterNet&&gCLIOpts.loopbackOnly&&getenv("SM64_CHARACTER_NET_OBSERVE")!=NULL&&
        !(getenv("SM64_CHARACTER_NET_MOTION")&&getenv("SM64_ROCKET_QA_OUTPUT"));
}
#include "rocket_coin_observe_qa.inc.h"
void character_net_qa_observe(void *window){
    static unsigned start,last,captured;
    static unsigned motionCaptures;
    if(!gCLIOpts.characterNet||!gCLIOpts.loopbackOnly||!getenv("SM64_CHARACTER_NET_OBSERVE"))return;
    coin_qa_observe();
    unsigned now=SDL_GetTicks();if(!start)start=now;
    const char *seconds=getenv("SM64_CHARACTER_NET_SECONDS");
    unsigned lifetime=seconds?(unsigned)atoi(seconds):25;
    if(lifetime<1||lifetime>((getenv("SM64_BOSS_NET_OBSERVE")||getenv("SM64_COMBINED_NET_OBSERVE"))?240:60))lifetime=25;
    const char *bossScenario=getenv("SM64_BOSS_NET_OBSERVE");
    const char *combinedScenario=getenv("SM64_COMBINED_NET_OBSERVE");
    if(bossScenario||combinedScenario){
        static int staged;
        char stopPath[4096];snprintf(stopPath,sizeof stopPath,"%s/stop",getenv("SM64_CHARACTER_NET_OBSERVE"));
        FILE *stop=fopen(stopPath,"rb");if(stop){fclose(stop);SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return;}
        const char *scenario=combinedScenario?combinedScenario:bossScenario;
        int target=combinedScenario?(!strcmp(scenario,"metal")?LEVEL_COTMC:
            !strcmp(scenario,"wing")?LEVEL_TOTWC:!strcmp(scenario,"vanish")?LEVEL_VCUTM:
            !strcmp(scenario,"jrb-water")?LEVEL_JRB:!strcmp(scenario,"sl-ice")?LEVEL_SL:LEVEL_BOB):
            !strcmp(scenario,"bowser")?LEVEL_BOWSER_1:LEVEL_BOB;
        if(!staged&&strcmp(scenario,"outside")&&now-start>1000){
            for(unsigned i=1;i<MAX_PLAYERS;i++)if(gNetworkPlayers[i].connected&&gNetworkPlayers[i].currLevelNum==target){
                // A passive observer below Bob's summit would attract native
                // nearest-player AI and invalidate the driver's runup route.
                // Stage at the normal base spawn; active Mario QA drives up.
                if(!warp_to_level(target,1,1))exit(92);
                staged=1;break;
            }
        }
    }
    if(now-start>=lifetime*1000){SDL_Event quit={0};quit.type=SDL_QUIT;SDL_PushEvent(&quit);return;}
    if(now-last>=500){
        last=now;RocketSnapshot local={0};int active=rocket_runtime_snapshot(&local);
        fprintf(stderr,"CNET_LOCAL ms=%u global=%u active=%d ticks=%llu pos=%.2f,%.2f,%.2f focus=%d\n",now-start,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:255,active,(unsigned long long)local.ticks,local.position[0],local.position[1],local.position[2],SDL_GetKeyboardFocus()!=NULL);
        if(combinedScenario)fprintf(stderr,"COMBINED_NET_LOCAL {\"ms\":%u,\"global\":%u,\"level\":%d,\"active\":%d,\"caps\":%u,\"visual_caps\":%u,\"cap_timer\":%u,\"native_flags\":%u,\"boost_mode\":%d,\"surface_mode\":%d,\"fuel\":%.5f,\"water\":%d,\"health\":%d,\"coins\":%d}\n",
            now-start,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:255,gCurrLevelNum,active,
            rocket_caps_active_flags(0),rocket_caps_visual_flags(0),gMarioStates[0].capTimer,gMarioStates[0].flags,
            rocket_boost_mode(),rocket_surface_mode(),local.boost,local.water_mode,gMarioStates[0].health,gMarioStates[0].numCoins);
        for(unsigned i=1;i<MAX_PLAYERS;i++)if(gNetworkPlayers[i].connected){
            CharacterNetState s={0};int sampled=character_net_track_sample(&tracks[i],clock_elapsed_f64(),&s);
            if(getenv("SM64_CHARACTER_NET_MOTION"))fprintf(stderr,"CNET_VISIBILITY {\"ms\":%u,\"index\":%u,\"connected\":1,\"area\":%d,\"peer_level\":%d,\"draws\":%u}\n",now,i,same_area(i),gNetworkPlayers[i].currLevelNum,draws[i]);
            fprintf(stderr,"CNET_REMOTE ms=%u local=%u global=%u valid=%d area=%d kind=%u active=%u sequence=%u ticks=%llu draws=%u pos=%.2f,%.2f,%.2f\n",now-start,i,gNetworkPlayers[i].globalIndex,sampled,same_area(i),s.kind,s.active,s.sequence,(unsigned long long)s.car.ticks,draws[i],s.car.position[0],s.car.position[1],s.car.position[2]);
            if(combinedScenario)fprintf(stderr,"COMBINED_NET_REMOTE {\"ms\":%u,\"global\":%u,\"peer\":%u,\"area\":%d,\"level\":%d,\"valid\":%d,\"active\":%u,\"caps\":%u,\"visual_caps\":%u,\"cap_timer\":%u,\"draws\":%u,\"fuel\":%.5f,\"water\":%d,\"ticks\":%llu}\n",
                now-start,gNetworkPlayerLocal?gNetworkPlayerLocal->globalIndex:255,gNetworkPlayers[i].globalIndex,
                same_area(i),gCurrLevelNum,sampled,s.active,rocket_caps_active_flags(i),rocket_caps_visual_flags(i),
                gMarioStates[i].capTimer,draws[i],s.car.boost,s.car.water_mode,(unsigned long long)s.car.ticks);
        }
        fflush(stderr);
    }
    const char *captureName=NULL;
    if(combinedScenario&&!getenv("SM64_ROCKET_QA_OUTPUT")){
        static unsigned capCaptures;
        unsigned remoteCaps=0;
        for(unsigned i=1;i<MAX_PLAYERS;i++)if(same_area(i))remoteCaps|=rocket_caps_visual_flags(i)&MARIO_SPECIAL_CAPS;
        if(remoteCaps&&!(capCaptures&1)){captureName="frame-remote-cap-active.ppm";capCaptures|=1;}
        else if(!remoteCaps&&(capCaptures&1)&&!(capCaptures&2)){captureName="frame-remote-cap-cleared.ppm";capCaptures|=2;}
    }
    if(bossScenario&&!getenv("SM64_ROCKET_QA_OUTPUT")&&gObjectLists){
        static int lastHealth=-1,rewardCaptured;static unsigned rewardSince;
        static char healthName[64];
        for(int list=0;list<NUM_OBJ_LISTS;list++){
            struct ObjectNode *head=&gObjectLists[list];
            for(struct ObjectNode *node=head->next;node&&node!=head;node=node->next){
                struct Object *obj=(struct Object*)node;
                if(!(obj->activeFlags&ACTIVE_FLAG_ACTIVE))continue;
                if((obj->behavior==bhvKingBobomb||obj->behavior==bhvBowser)&&obj->oHealth!=lastHealth){
                    if(lastHealth>=0){snprintf(healthName,sizeof healthName,"frame-boss-health-%d.ppm",obj->oHealth);captureName=healthName;}
                    lastHealth=obj->oHealth;
                }
                if(obj->behavior==bhvBowserKey||obj->behavior==bhvStarSpawnCoordinates){
                    if(!rewardSince)rewardSince=now;
                    if(!rewardCaptured&&now-rewardSince>1000){captureName="frame-boss-reward.ppm";rewardCaptured=1;}
                }
            }
        }
    }
    if(!captured&&now-start>=14000){captureName="frame.ppm";captured=1;}
    if(getenv("SM64_CHARACTER_NET_MOTION"))for(unsigned i=1;i<MAX_PLAYERS;i++){
        CharacterNetState s={0};
        if(!same_area(i)||!character_net_track_sample(&tracks[i],clock_elapsed_f64(),&s)||!s.active)continue;
        if(!(motionCaptures&1)&&s.car.boost<99&&hypotf(s.car.velocity[0],s.car.velocity[2])>500){captureName="frame-drive.ppm";motionCaptures|=1;}
        else if(!(motionCaptures&2)&&s.car.flipped&&s.car.flip_time>.1f){captureName="frame-flip.ppm";motionCaptures|=2;}
        else if((motionCaptures&2)&&!(motionCaptures&4)&&s.car.grounded){captureName="frame-landing.ppm";motionCaptures|=4;}
    }
    if(captureName){
        int w,h;SDL_GL_GetDrawableSize((SDL_Window*)window,&w,&h);
        if(w>0&&h>0&&w<=4096&&h<=4096){
            unsigned char *pixels=malloc((size_t)w*h*3);
            if(pixels){
                GLint align;glGetIntegerv(GL_PACK_ALIGNMENT,&align);glPixelStorei(GL_PACK_ALIGNMENT,1);
                glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels);glPixelStorei(GL_PACK_ALIGNMENT,align);
                char path[4096];int n=snprintf(path,sizeof path,"%s/%s",getenv("SM64_CHARACTER_NET_OBSERVE"),captureName);
                FILE *file=n>0&&n<(int)sizeof path?fopen(path,"wb"):NULL;
                if(file){fprintf(file,"P6\n%d %d\n255\n",w,h);for(int y=h-1;y>=0;y--)fwrite(pixels+(size_t)y*w*3,1,(size_t)w*3,file);fclose(file);}
                free(pixels);
            }
        }
    }
}
#endif
