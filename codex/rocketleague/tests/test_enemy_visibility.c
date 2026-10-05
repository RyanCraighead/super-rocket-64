/* Actual production surface queries; no renderer, actor simulation or ROM needed. */
#include "../../../src/game/rocket_adapter.c"
#include "../physics/enemy_impact.h"
#include <assert.h>
#include <stdio.h>
SpatialPartitionCell gStaticSurfacePartition[NUM_CELLS][NUM_CELLS],gDynamicSurfacePartition[NUM_CELLS][NUM_CELLS];
struct LevelValues gLevelValues;
static struct Object target,other;
static struct Surface surfaces[4];
static struct SurfaceNode nodes[4];
static unsigned checks;
#define CHECK(expression) do { checks++;assert(expression); } while(0)
static void fresh(void){
    memset(&target,0,sizeof target);memset(&other,0,sizeof other);
    memset(surfaces,0,sizeof surfaces);memset(nodes,0,sizeof nodes);
    memset(gStaticSurfacePartition,0,sizeof gStaticSurfacePartition);
    memset(gDynamicSurfacePartition,0,sizeof gDynamicSurfacePartition);
    memset(&gLevelValues,0,sizeof gLevelValues);phaseActive=0;
    target.oPosX=100;target.hitboxHeight=100;
}
static void triangle(unsigned i,int dynamic,const s16 vertices[3][3],float normalY){
    struct Surface *s=&surfaces[i];
    memcpy(s->vertex1,vertices[0],sizeof s->vertex1);
    memcpy(s->vertex2,vertices[1],sizeof s->vertex2);
    memcpy(s->vertex3,vertices[2],sizeof s->vertex3);s->normal.y=normalY;
    nodes[i].surface=s;
    SpatialPartitionCell (*partition)[NUM_CELLS]=dynamic?gDynamicSurfacePartition:gStaticSurfacePartition;
    /* Deliberately use different cells/partitions: all geometry must be scanned. */
    partition[NUM_CELLS-1-i][i][i%3].next=&nodes[i];
}
int main(void){
    const float origin[3]={0,0,0};
    const s16 ramp[3][3]={{-921,256,5888},{768,768,5376},{-921,256,5375}};
    /* Native QA8 host r1: previous tick1000, current1004, Goomba sync420.
     * The old horizontal ray intersects this actual BOB floor 97.115 units
     * ahead. The car and target are above the slope along their 3D sightline. */
    const float previous[3]={-621.7029f,376.1206f,5535.8794f};
    const float current[3]={-479.8910f,420.8781f,5549.0283f};
    fresh();target.oPosX=-378.8224f;target.oPosY=470.9977f;target.oPosZ=5549.6230f;
    target.hitboxHeight=75;triangle(0,0,ramp,.9569958f);
    CHECK(!rocket_adapter_object_visible(previous,&target));
    CHECK(rocket_adapter_enemy_visible(previous,&target,0));
    CHECK(rocket_adapter_enemy_visible(current,&target,0));
    /* Exact client r4 endpoints reproduce the same independent veto. */
    const float remotePrevious[3]={-621.8190f,376.0248f,5537.5342f};
    const float remoteCurrent[3]={-480.0286f,420.8094f,5550.8555f};
    target.oPosX=-380.4263f;target.oPosY=470.3914f;target.oPosZ=5550.8130f;
    CHECK(!rocket_adapter_object_visible(remotePrevious,&target));
    CHECK(rocket_adapter_enemy_visible(remotePrevious,&target,0));
    CHECK(rocket_adapter_enemy_visible(remoteCurrent,&target,0));
    /* Full bases are recorded for sent CNET sequences104/105, ticks984/1000.
     * Replaying those exact raw poses with the corresponding client-observed
     * target history proves the sweep/rotation policy can already find a hit
     * before the unsent1004 pose. This is not a claim about host target timing. */
    RocketSnapshot before={.position={-1210.528f,330.853f,5487.291f},
        .velocity={4350.976f,132.3231f,365.2376f},.ticks=984,
        .basis={.9951985f,.002906844f,.09783375f,.09772784f,-.08465418f,-.9916063f,.005399588f,.9964061f,-.08453179f}};
    RocketSnapshot after={.position={-621.819f,376.0248f,5537.534f},
        .velocity={4267.024f,1184.316f,397.9327f},.ticks=1000,
        .basis={.9592087f,.269646f,.08491004f,.08711022f,.003818735f,-.9961914f,-.2689433f,.962952f,-.01982598f}};
    RocketEnemyTarget oldTarget={{-383.6339f,418.4819f,5553.1929f},108,418.4819f,75};
    RocketEnemyTarget newTarget={{-380.4263f,457.3914f,5550.813f},108,457.3914f,75};
    RocketEnemyContact track={0};
    CHECK(!rocket_enemy_contact(&track,&before,2,&oldTarget));
    CHECK(rocket_enemy_contact(&track,&after,2,&newTarget));
    target.oPosY=newTarget.bottom;
    CHECK(!rocket_adapter_object_visible(before.position,&target));
    CHECK(!rocket_adapter_object_visible(after.position,&target));
    CHECK(rocket_adapter_enemy_visible(before.position,&target,0));
    CHECK(rocket_adapter_enemy_visible(after.position,&target,0));
    const s16 wall[3][3]={{50,-100,-100},{50,200,-100},{50,50,200}};
    fresh();CHECK(rocket_adapter_enemy_visible(origin,&target,0));
    triangle(0,0,wall,0);CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    /* Native phase permission follows this actor's verified lease, not the
     * local renderer/adapter state, and only native permitted surfaces pass. */
    surfaces[0].type=SURFACE_VANISH_CAP_WALLS;phaseActive=1;
    CHECK(!rocket_adapter_enemy_visible(origin,&target,0));phaseActive=0;
    CHECK(rocket_adapter_enemy_visible(origin,&target,MARIO_VANISH_CAP));
    CHECK(!rocket_adapter_enemy_visible(origin,&target,MARIO_METAL_CAP|MARIO_WING_CAP));
    surfaces[0].normal.y=.5f;
    CHECK(!rocket_adapter_enemy_visible(origin,&target,MARIO_VANISH_CAP));
    gLevelValues.fixVanishFloors=1;
    CHECK(rocket_adapter_enemy_visible(origin,&target,MARIO_VANISH_CAP));
    triangle(1,1,wall,0); // A coincident ordinary wall must still block.
    CHECK(!rocket_adapter_enemy_visible(origin,&target,MARIO_VANISH_CAP));
    fresh();triangle(0,1,wall,0);
    CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    surfaces[0].object=&target;CHECK(rocket_adapter_enemy_visible(origin,&target,0));
    surfaces[0].object=&other;CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    surfaces[0].object=&target;triangle(1,0,wall,0);surfaces[1].object=&other;
    CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    fresh();triangle(0,0,wall,0);surfaces[0].flags=SURFACE_FLAG_INTANGIBLE;
    CHECK(rocket_adapter_enemy_visible(origin,&target,0));surfaces[0].flags=0;
    const int ignored[]={SURFACE_INTANGIBLE,SURFACE_CAMERA_BOUNDARY,SURFACE_RAYCAST};
    for(unsigned i=0;i<sizeof ignored/sizeof *ignored;i++){
        surfaces[0].type=ignored[i];CHECK(rocket_adapter_enemy_visible(origin,&target,0));
    }
    /* 3D rays retain real floor and ceiling obstruction, including a rising
     * floor steeper than the ray. Skipping floor partitions would fail here. */
    const s16 floor[3][3]={{0,-10,-100},{100,90,-100},{0,-10,100}};
    fresh();triangle(0,0,floor,.7071068f);CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    const s16 ceiling[3][3]={{0,40,-100},{100,40,-100},{100,40,100}};
    fresh();triangle(0,1,ceiling,-1);CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    const s16 beyond[3][3]={{150,-100,-100},{150,200,-100},{150,50,200}};
    fresh();triangle(0,0,beyond,0);CHECK(rocket_adapter_enemy_visible(origin,&target,0));
    fresh();target.oPosX=0;target.oPosY=-50;
    CHECK(rocket_adapter_enemy_visible(origin,&target,0)); // Coincident center.
    target.hitboxHeight=0;CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    target.hitboxHeight=100;target.hitboxDownOffset=NAN;CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    target.hitboxDownOffset=0;target.oPosZ=INFINITY;CHECK(!rocket_adapter_enemy_visible(origin,&target,0));
    CHECK(!rocket_adapter_enemy_visible(NULL,&target,0));CHECK(!rocket_adapter_enemy_visible(origin,NULL,0));
    printf("enemy visibility: %u production ramp/wall/ceiling/phase/object/input checks passed\n",checks);
    return 0;
}
