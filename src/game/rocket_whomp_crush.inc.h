/* Whomp underside contacts borrow native squish locomotion before the rigid
 * chassis can be trapped between two solid surfaces. Back riding remains in
 * RocketSim. No enemy-health writes, remote-player damage or collision removal. */
#include "../../codex/rocketleague/physics/crush_contact.h"
static struct {
    struct MarioState *m;
    struct Object *object;
    struct Area *area;
    const Collision *collision;
    u32 sync;
    s16 level;
    RocketSnapshot footprint;
    float anchor[3];
} whompCrush;
static int crush_object(const struct Object *o,const struct MarioState *m,int entering) {
    if(!o||(o->behavior!=bhvSmallWhomp&&o->behavior!=bhvWhompKingBoss)||
       !(o->activeFlags&ACTIVE_FLAG_ACTIVE)||(o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))||
       o->header.gfx.activeAreaIndex!=m->area->index||o->oSyncDeath||o->oIntangibleTimer||
       (o->header.gfx.node.flags&GRAPH_RENDER_INVISIBLE))return 0;
    if(entering)return o->oAction==4||o->oAction==5||(o->oAction==6&&o->oSubAction!=10);
    return o->oAction>=4&&o->oAction<=6;
}
static struct Surface *crush_ceiling(struct MarioState *m,const RocketSnapshot *car,
        struct Object *only,int entering,float *height) {
    struct Surface *best=NULL;*height=INFINITY;
    for(int z=0;z<NUM_CELLS;z++)for(int x=0;x<NUM_CELLS;x++)
        for(struct SurfaceNode *node=gDynamicSurfacePartition[z][x][SPATIAL_PARTITION_CEILS].next;node;node=node->next) {
            struct Surface *s=node->surface;
            if(!solid_surface(s)||s->normal.y>=-.01f||(only&&s->object!=only)||
               !crush_object(s->object,m,entering)||m->floor->object==s->object)continue;
            float triangle[3][3],h,bottom;const s16 *v[]={s->vertex1,s->vertex2,s->vertex3};
            for(int i=0;i<3;i++)for(int k=0;k<3;k++)triangle[i][k]=v[i][k];
            if(!rocket_crush_ceiling(car,triangle,&h,&bottom))continue;
            /* Native squish clearance is 150 units. Require a nearby supporting
             * floor and an underside below the body, never a car atop the back. */
            if(entering&&(bottom<m->floorHeight-8.f||bottom>m->floorHeight+80.f||
               h>m->floorHeight+150.f||h<m->floorHeight-80.f))continue;
            if(h<*height){*height=h;best=s;}
        }
    return best;
}
static int whomp_crush_prepare(struct MarioState *m) {
    int continuing=m&&whompCrush.m==m&&(m->action==ACT_SQUISHED||(m->squishTimer>0&&m->squishTimer<255));
    if(!selected||!m||m->playerIndex||!m->marioObj||!m->area||!m->floor||
       !rocket_runtime_enabled()||m->heldObj||m->heldByObj||m->riddenObj||
       whompCrush.area!=m->area||whompCrush.level!=gCurrLevelNum) {
        memset(&whompCrush,0,sizeof whompCrush);continuing=0;
    }
    if(!selected||!m||m->playerIndex||!m->marioObj||!m->area||!m->floor||
       !rocket_runtime_enabled()||m->heldObj||m->heldByObj||m->riddenObj)return 0;
    if(m->freeze||sCurrPlayMode!=PLAY_MODE_NORMAL||(gTimeStopState&TIME_STOP_ACTIVE))return continuing;
    if(gNetworkType!=NT_NONE&&(!gNetworkAreaLoaded||gNetworkAreaSyncing||!gNetworkPlayerLocal))return 0;
    RocketSnapshot footprint;
    struct Surface *ceiling;float height;
    if(continuing) {
        if(!whompCrush.object||whompCrush.object->oSyncID!=whompCrush.sync||
           whompCrush.object->collisionData!=whompCrush.collision||!crush_object(whompCrush.object,m,0)) {
            memset(&whompCrush,0,sizeof whompCrush);return 0;
        }
        /* Finish native recovery while the full rigid chassis cannot yet fit.
         * Native motion/health/timer continue; no additional crush is invented. */
        if(m->action!=ACT_SQUISHED)return 1;
        footprint=whompCrush.footprint;
        for(int k=0;k<3;k++)footprint.position[k]+=m->pos[k]-whompCrush.anchor[k];
        ceiling=crush_ceiling(m,&footprint,whompCrush.object,0,&height);
        if(!ceiling)return 1; // Native action owns release/death when the obstacle clears.
    } else {
        memset(&whompCrush,0,sizeof whompCrush);
        if(m->health<0x100||m->squishTimer||!rocket_adapter_body_snapshot(m->marioObj,&footprint))return 0;
        ceiling=crush_ceiling(m,&footprint,NULL,1,&height);if(!ceiling)return 0;
        whompCrush.m=m;whompCrush.object=ceiling->object;whompCrush.area=m->area;whompCrush.level=gCurrLevelNum;
        whompCrush.sync=ceiling->object->oSyncID;whompCrush.collision=ceiling->object->collisionData;
        whompCrush.footprint=footprint;
        /* Native positions are feet; the RocketSim origin is above them. This
         * bounded floor handoff changes no X/Z and never revives an OOB player. */
        m->pos[1]=m->floorHeight;vec3f_copy(whompCrush.anchor,m->pos);
        vec3f_set(m->vel,0,0,0);m->forwardVel=m->slideVelX=m->slideVelZ=0;
        set_mario_action(m,ACT_SQUISHED,0);
    }
    m->ceil=ceiling;m->ceilHeight=fmaxf(m->floorHeight,height);m->input|=INPUT_SQUISHED;
    return 1;
}
