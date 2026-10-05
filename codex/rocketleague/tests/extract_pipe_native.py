from pathlib import Path
import sys,re,json
root=Path(__file__).resolve().parents[3];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(root/'codex/rocketleague/tests'))
from extract_chimney_native import extract
from native_slice import function
extract(out/'chimney_native.inc')
clean=lambda s:re.sub(r'/\*.*?\*/|//[^\n]*','',s,flags=re.S)
cases=[]
for level in ('bitdw','bitfs','bits','thi'):
 s=clean((root/'levels'/level/'script.c').read_text())
 if level=='thi':
  parts=[(area,re.search(r'static const LevelScript script_func_local_'+str(area+6)+r'\[\]\s*=\s*\{(.*?)\};',s,re.S)[1]) for area in (1,2)]
 else:parts=[(1,re.search(r'\bAREA\(\s*1\s*,.*?END_AREA\(\)',s,re.S)[0])]
 for area,part in parts:
  for obj in re.findall(r'\bOBJECT\(([^)]*)\)',part):
   f=[v.strip() for v in obj.split(',')]
   if f[-1]!='bhvWarpPipe' and not(level=='bitfs' and f[-1]=='bhvWarp' and f[-2]=='0x140B0000'):continue
   params=int(f[-2],0);nodeid=(params>>16)&255
   nodes=[[v.strip() for v in n.split(',')] for n in re.findall(r'WARP_NODE\(([^)]*)\)',part)]
   node=[n for n in nodes if int(n[0],0)==nodeid];assert len(node)==1
   cases.append(dict(name=f'{level}/area{area}/node{nodeid:02x}',level='LEVEL_'+level.upper(),area=area,params=params,pipe=f[-1]=='bhvWarpPipe',position=[int(v,0) for v in f[1:4]],node=node[0][:4]))
assert len(cases)==9
base=(root/'codex/rocketleague/tests/test_chimney_native.c').read_text()
base=base.replace('int main(void){','int chimney_suite_main(void){')
base=base.replace('CHECK(sound==SOUND_MENU_ENTER_HOLE);','CHECK((u32)sound==(chimney.collisionData==warp_pipe_seg3_collision_03009AC8?SOUND_MENU_ENTER_PIPE:SOUND_MENU_ENTER_HOLE));')
base=base.replace('a==12&&b==80','a==(chimney.collisionData==warp_pipe_seg3_collision_03009AC8?15:12)&&b==80')
extra='''
#include "game/rocket_incoming.h"
#include "game/rocket_adapter.h"
#include "pc/rocket_runtime.h"
#include "behavior_data.h"
const BehaviorScript bhvGoomba[]={11},bhvBobomb[]={12};
int rocket_adapter_body_snapshot(struct Object *o,RocketSnapshot *s){(void)o;(void)s;abort();}
'''
extra+=(root/'src/game/rocket_incoming.c').read_text().replace('#include "rocket_incoming.h"','#include "game/rocket_incoming.h"').replace('#include "rocket_adapter.h"','#include "game/rocket_adapter.h"').replace('"../../codex/rocketleague/physics/body_contact.h"','"codex/rocketleague/physics/body_contact.h"')
extra+=function((root/'src/game/object_collision.c').read_text(),'detect_object_hitbox_overlap')
extra+='\nstruct PipeCase {const char *name;int level,area;u32 params;int pipe;float pos[3];struct WarpNode node;};\nstatic const struct PipeCase cases[]={\n'
for c in cases:extra+='{"'+c['name']+'",'+c['level']+','+str(c['area'])+','+hex(c['params'])+','+str(int(c['pipe']))+',{'+','.join(map(str,c['position']))+'},{'+','.join(c['node'])+'}},\n'
extra+='};\n'
extra+=r'''
static struct MarioState *pipe_setup(const struct PipeCase *c,int role,u32 action,float y){
    struct MarioState *m=fresh(role,action);area.index=c->area;chimneyNode.node=c->node;
    chimney.oBehParams=c->params;chimney.collisionData=c->pipe?segmented_to_virtual(warp_pipe_seg3_collision_03009AC8):NULL;
    memcpy(&chimney.oPosX,c->pos,sizeof(Vec3f));gCurrentObject=&chimney;bhv_warp_loop();
    CHECK(chimney.hitboxRadius==(c->pipe?50.f:200.f)&&chimney.hitboxHeight==50);
    memcpy(m->pos,c->pos,sizeof(Vec3f));m->pos[1]+=y;m->floorHeight=c->pos[1];
    vec3f_copy(&players[0].oPosX,m->pos);players[0].hitboxRadius=37;players[0].hitboxHeight=160;players[0].oInteractType=INTERACT_PLAYER;
    m->numStars=70;return m;
}
static int overlap(struct MarioState *m){
    players[0].numCollidedObjs=chimney.numCollidedObjs=0;players[0].collidedObjInteractTypes=0;
    int hit=detect_object_hitbox_overlap(&players[0],&chimney);m->collidedObjInteractTypes=players[0].collidedObjInteractTypes;return hit;
}
static void consume(struct MarioState *m,const struct PipeCase *c){
    CHECK(m->action==ACT_DISAPPEARED&&actionCalls==1&&!warpCalls&&m->usedObj==&chimney);
    CHECK(!memcmp(&chimneyNode.node,&c->node,sizeof c->node));CHECK(m->health==0x880&&m->numStars==70&&m->numCoins==29);
    CHECK(!act_disappeared(m)&&!warpCalls);CHECK(!act_disappeared(m)&&warpCalls==1&&requestedWarp==WARP_OP_WARP_OBJECT);
    CHECK(requestedNode==&chimneyNode);
}
int main(void){
 const u32 actions[]={ACT_IDLE,ACT_WALKING,ACT_FREEFALL};
 for(unsigned c=0;c<sizeof cases/sizeof*cases;c++){
  const struct PipeCase *pipe=&cases[c];
  for(int role=NT_NONE;role<=NT_CLIENT;role++)for(unsigned action=0;action<3;action++){
   struct MarioState*m=pipe_setup(pipe,role,actions[action],34);CHECK(overlap(m));mario_process_interactions(m);consume(m,pipe);
   m=pipe_setup(pipe,role,actions[action],219);CHECK(!overlap(m));mario_process_interactions(m);CHECK(!actionCalls&&!warpCalls);
  }
  for(int role=NT_NONE;role<=NT_CLIENT;role++)for(int visit=0;visit<4;visit++){
   struct MarioState*m=pipe_setup(pipe,role,ACT_IDLE,34);m->skipWarpInteractionsTimer=30;
   for(int frame=0;frame<29;frame++){CHECK(overlap(m));mario_process_interactions(m);CHECK(!actionCalls&&!warpCalls);}
   CHECK(overlap(m));mario_process_interactions(m);consume(m,pipe);
  }
  struct MarioState*m=pipe_setup(pipe,NT_CLIENT,ACT_EMERGE_FROM_PIPE,34);CHECK(overlap(m));mario_process_interactions(m);CHECK(!actionCalls&&!warpCalls);
  m=pipe_setup(pipe,NT_SERVER,ACT_IDLE,34);m=&gMarioStates[1];collided(m);mario_process_interactions(m);CHECK(!actionCalls&&!warpCalls);
  printf("native route passed: %s -> level=%u area=%u node=%u\n",pipe->name,pipe->node.destLevel,pipe->node.destArea,pipe->node.destNode);
 }
 printf("PASS %u current native pipe/hole collision, dispatch, route, cooldown/re-entry and local host/client assertions\n",checks);
}
'''
(out/'native-audit.c').write_text(base+extra,newline='\n');(out/'route-matrix.json').write_text(json.dumps(cases,indent=2)+'\n')
print('Generated actual native collision/interaction fixture for all nine pipe/hole routes.')
