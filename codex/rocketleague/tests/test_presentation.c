/* Actual presentation bridge; source render services are explicit inert mocks. */
#include <assert.h>
#include <stdio.h>
#ifndef CHARACTER_PRESENTATION_SOURCE
#define CHARACTER_PRESENTATION_SOURCE "../../../src/game/character_presentation.c"
#endif
#include CHARACTER_PRESENTATION_SOURCE
struct CLIOptions gCLIOpts;
s16 gCurrLevelNum;
static int available=1,snapshotReady,submits,draws,linkReady;
static enum CharacterSwitchId choice=CHARACTER_OCTANE;
static RocketSnapshot car;
static BkRenderSnapshot banjo;
static Bm64RenderSnapshot bomber;
static SpidermanRenderSnapshot spider;
static ThpsRenderSnapshot tony;
int character_switch_enabled(void){return 1;}
enum CharacterSwitchId character_switch_active(void){return choice;}
int rocket_runtime_enabled(void){return available;}
int rocket_runtime_snapshot(RocketSnapshot *s){if(!snapshotReady)return 0;*s=car;return 1;}
int rocket_runtime_draw_snapshot(const RocketSnapshot *s,const float v[16],const float p[16],const int w[4]){(void)s;(void)v;(void)p;(void)w;draws++;return 1;}
int bk_runtime_enabled(void){return available;}
int bm64_runtime_enabled(void){return available;}
int bk_runtime_snapshot(BkRenderSnapshot *s){if(!snapshotReady)return 0;*s=banjo;return 1;}
void bk_runtime_submit(const BkRenderSnapshot *s){banjo=*s;submits++;snapshotReady=available;}
int bm64_runtime_snapshot(Bm64RenderSnapshot *s){if(!snapshotReady)return 0;*s=bomber;return 1;}
void bm64_runtime_submit(const Bm64RenderSnapshot *s){bomber=*s;submits++;snapshotReady=available;}
int spiderman_runtime_snapshot(SpidermanRenderSnapshot *s){if(!snapshotReady)return 0;*s=spider;return 1;}
int spiderman_runtime_submit(const SpidermanRenderSnapshot *s){spider=*s;submits++;return available;}
int thps_runtime_snapshot(ThpsRenderSnapshot *s){if(!snapshotReady)return 0;*s=tony;return 1;}
int thps_runtime_submit(const ThpsRenderSnapshot *s){tony=*s;submits++;return available;}
int oot_link_runtime_snapshot(OotLinkSnapshot *s){memset(s,0,sizeof *s);s->drawable=snapshotReady;return 1;}
int oot_link_runtime_present(const float p[3],int16_t y){(void)p;(void)y;linkReady++;return available;}
int spiderman_runtime_draw(const float v[16],const float p[16],const int w[4]){(void)v;(void)p;(void)w;draws++;return 1;}
int thps_runtime_draw(const float v[16],const float p[16],const int w[4]){(void)v;(void)p;(void)w;draws++;return 1;}
static struct MarioState mario;
static struct Object playerObject;
static struct Area testArea;
static struct Surface floorSurface;
static void fresh(enum CharacterSwitchId id){
 owner=NULL;valid=presenting=0;choice=id;submits=draws=linkReady=0;available=1;
 memset(&mario,0,sizeof mario);memset(&playerObject,0,sizeof playerObject);memset(&car,0,sizeof car);
 memset(&gCLIOpts,0,sizeof gCLIOpts);gCLIOpts.characterWheel=1;
 mario.marioObj=&playerObject;mario.area=&testArea;mario.floor=&floorSurface;mario.action=ACT_BACKWARD_GROUND_KB;
 mario.health=0x580;mario.invincTimer=30;mario.hurtCounter=4;mario.vel[0]=-15;
 playerObject.header.gfx.node.flags=GRAPH_RENDER_ACTIVE;
 playerObject.header.gfx.pos[0]=123;playerObject.header.gfx.pos[1]=80;playerObject.header.gfx.pos[2]=456;
 playerObject.header.gfx.angle[1]=0x4000;
 car.basis[2]=car.basis[3]=car.basis[7]=1;car.ticks=88;
 for(int i=0;i<4;i++){car.wheel_radius[i]=32;car.wheel_position[i][0]=i*10;car.wheel_position[i][1]=-20;}
 snapshotReady=1;character_presentation_begin(&mario);character_presentation_begin(&mario);
 snapshotReady=0; /* real adapter would suspend before native action execution */
}
#include "test_door_presentation.inc.c"
int main(void){
 test_door_presentation();
 for(int id=CHARACTER_LINK;id<CHARACTER_COUNT;id++){
  fresh(id);struct MarioState before=mario;character_presentation_finish(&mario,0);
  assert(presenting&&(playerObject.header.gfx.node.flags&GRAPH_RENDER_INVISIBLE));
  assert(!memcmp(&mario,&before,sizeof mario)); /* damage, immunity, action, motion unchanged */
  if(id==CHARACTER_OCTANE){RocketSnapshot s;assert(character_presentation_car_snapshot(&s));
   assert(s.position[0]==123&&s.position[1]==120&&s.position[2]==456&&s.ticks==88);
   assert(fabsf(s.basis[0]-1)<.0001f&&s.velocity[0]==0);
   assert(fabsf(s.wheel_position[1][2]-(456-10))<.001f);
  }
  character_presentation_draw(NULL,NULL,NULL);
  if(id==CHARACTER_OCTANE||id==CHARACTER_TONY||id==CHARACTER_SPIDERMAN)assert(draws==1);
 }
 fresh(CHARACTER_OCTANE);character_presentation_finish(&mario,1);assert(!presenting);
 fresh(CHARACTER_OCTANE);available=0;character_presentation_finish(&mario,0);assert(!presenting);
 fresh(CHARACTER_OCTANE);mario.action=ACT_DISAPPEARED;character_presentation_finish(&mario,0);assert(!presenting);
 fresh(CHARACTER_OCTANE);playerObject.header.gfx.pos[0]=NAN;character_presentation_finish(&mario,0);assert(!presenting);
 fresh(CHARACTER_OCTANE);gCurrLevelNum++;character_presentation_begin(&mario);assert(!presenting);
 playerObject.header.gfx.pos[0]=5000;character_presentation_finish(&mario,0);
 RocketSnapshot warped;assert(character_presentation_car_snapshot(&warped)&&warped.position[0]==5000);
 fresh(CHARACTER_OCTANE);choice=CHARACTER_MARIO;character_presentation_begin(&mario);character_presentation_finish(&mario,0);assert(!valid&&!presenting);
 fresh(CHARACTER_OCTANE);mario.playerIndex=1;character_presentation_finish(&mario,0);assert(!presenting);
 fresh(CHARACTER_OCTANE);mario.action=ACT_READING_AUTOMATIC_DIALOG;character_presentation_finish(&mario,0);assert(presenting);
 character_presentation_begin(&mario);playerObject.header.gfx.pos[2]+=25;character_presentation_finish(&mario,0);
 RocketSnapshot s;assert(character_presentation_car_snapshot(&s)&&s.position[2]==481&&s.ticks==88);
 puts("PASS presentation: six selected meshes, native injury state unchanged, host transform, remote snapshot, cutscene continuation, source/warp/switch/failure gates");
 return 0;
}
