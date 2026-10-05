#include <math.h>
#include <string.h>
#include "sm64.h"
#include "area.h"
#include "behavior_data.h"
#include "camera.h"
#include "bettercamera.h"
#include "first_person_cam.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "level_update.h"
#include "interaction.h"
#include "mario.h"
#include "mario_step.h"
#include "object_fields.h"
#include "object_list_processor.h"
#include "bm64_adapter.h"
#include "pc/bm64_runtime.h"
#include "../../codex/bm64/movement/bm64_movement.h"
#include "../../codex/bm64/movement/bm64_math.h"
#include "../../codex/bm64/movement/bm64_throw_input.h"
#include "../../codex/bm64/mechanics/bm64_bomb_kernel.h"

extern u32 attack_object(struct MarioState *m, struct Object *o, s32 interaction);

/* Source timing is nominal NTSC VI (60 updates/s). SM64 gameplay runs at 30 Hz,
 * so each native update executes two source ticks, with edges on the first.
 * This is an explicit fixed-rate host schedule, not the original scheduler. */
#define BM64_SOURCE_TICKS 2

typedef struct HostBomb {
    Bm64Bomb source;
    Bm64HoldTween hold;
    int active, left_owner;
    float lift, fall_velocity, speed;
    uint64_t roll_order;
} HostBomb;
typedef struct HostExplosion {
    Bm64Explosion source;
    Vec3f position;
    u8 hit[OBJECT_POOL_CAPACITY];
    int self_hit, model;
} HostExplosion;
/* Legacy launchers select their sole controller by default. The opt-in wheel
 * explicitly assigns one owner after all available assets are preloaded. */
static int sSelected = 1, sWheelMode;
static struct MarioState *sPlayer;
static struct Area *sArea;
static s16 sLevel;
static Vec3f sLastPosition;
static int sHavePosition, sOwnHide, sHeld = -1, sThrowing, sThrowReleased;
static uint64_t sTicks, sRollOrder;
static int sKicking, sKickReleased, sKickAnimation, sKickMode, sKickCountdown, sKickPending;
static float sKickProbeX, sKickProbeZ;
static Bm64Facing sFacing;
static Bm64Vertical sVertical;
static HostBomb sBombs[BM64_HOST_BOMBS];
static HostExplosion sExplosions[BM64_HOST_BOMBS];
static int sAnimation[2] = { -1, -1 };
static float sFrame[2];
static Bm64RenderSnapshot sRender;
static const int sDurations[27] = {
    180,48,48,40,40,32,32,18,1,1,24,30,18,24,24,24,24,144,24,150,168,24,72,221,60,18,221
};

static void restore_visibility(void) {
    if (sOwnHide && sPlayer && sPlayer->marioObj)
        sPlayer->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
    sOwnHide = 0;
}
void bm64_adapter_set_selected(int selected) {
    sWheelMode = 1;
    if (!selected) bm64_adapter_suspend();
    sSelected = !!selected;
}
void bm64_adapter_suspend(void) {
    restore_visibility();
    bm64_runtime_suspend();
    sPlayer = NULL; sArea = NULL; sHavePosition = 0; sHeld = -1; sThrowing = 0; sThrowReleased = 0;
    sTicks = sRollOrder = 0;
    sKicking=sKickReleased=sKickCountdown=sKickPending=0;
    sKickProbeX=sKickProbeZ=0;
    sAnimation[0] = sAnimation[1] = -1;
    memset(sFrame, 0, sizeof(sFrame));
    memset(sBombs, 0, sizeof(sBombs));
    memset(sExplosions, 0, sizeof(sExplosions));
    memset(&sRender, 0, sizeof(sRender));
}
static int supported(u32 action) {
    switch (action) {
        case ACT_IDLE: case ACT_WALKING: case ACT_DECELERATING:
        case ACT_BRAKING: case ACT_BRAKING_STOP: case ACT_TURNING_AROUND:
        case ACT_FINISH_TURNING_AROUND: case ACT_FREEFALL: case ACT_FREEFALL_LAND:
            return 1;
        default: return 0;
    }
}
static float wrap_degrees(float angle) {
    angle = fmodf(angle, 360.0f);
    return angle < 0.0f ? angle + 360.0f : angle;
}
static int finite_position(const float p[3]) {
    return isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}
static int line_of_sight(const float from[3], const float to[3], const struct Object *target) {
    Vec3f origin = { from[0], from[1], from[2] };
    Vec3f direction = { to[0]-from[0], to[1]-from[1], to[2]-from[2] };
    Vec3f hit; struct Surface *surface = NULL;
    find_surface_on_ray(origin, direction, &surface, hit, 1.0f);
    return !surface || (target && surface->object == target);
}
static int native_enemy(const struct Object *o) {
    /* Never damage arbitrary interactables, NPCs, doors or progression objects. */
    if (o->behavior != bhvGoomba && o->behavior != bhvBobomb &&
        o->behavior != bhvSpindrift && o->behavior != bhvScuttlebug &&
        o->behavior != bhvPiranhaPlant && o->behavior != bhvSkeeter) return 0;
    if (o->oInteractType & (INTERACT_TEXT | INTERACT_DOOR | INTERACT_WARP_DOOR |
        INTERACT_STAR_OR_KEY | INTERACT_PLAYER | INTERACT_COIN)) return 0;
    return o->activeFlags && o->oIntangibleTimer == 0 && o->hitboxRadius > 0 && o->hitboxHeight > 0;
}
static int sphere_hits_cylinder(const float center[3], float radius,
                               float x, float y, float z, float width, float height) {
    /* Deliberate shape adapter for SM64 cylinders, not BM64 actor dispatch. */
    float dx = center[0]-x, dz = center[2]-z;
    float horizontal = fmaxf(0.0f, sqrtf(dx*dx+dz*dz)-width);
    float vertical = center[1] < y ? y-center[1] :
                     center[1] > y+height ? center[1]-y-height : 0.0f;
    return horizontal*horizontal + vertical*vertical < radius*radius;
}
static void apply_explosion(struct MarioState *m, HostExplosion *e) {
    if (!e->source.alive || !e->source.damaging || e->source.delay > 0) return;
    float radius = bm64_explosion_hit_radius(&e->source) * BM64_HOST_SCALE;
    for (int index = 0; index < OBJECT_POOL_CAPACITY; ++index) {
        struct Object *o = &gObjectPool[index];
        if (e->hit[index] || !native_enemy(o)) continue;
        float bottom = o->oPosY-o->hitboxDownOffset;
        if (!sphere_hits_cylinder(e->position, radius, o->oPosX, bottom,
                                 o->oPosZ, o->hitboxRadius, o->hitboxHeight)) continue;
        Vec3f target = { o->oPosX, bottom + o->hitboxHeight*.5f, o->oPosZ };
        if (!line_of_sight(e->position, target, o)) continue;
        attack_object(m, o, INT_FAST_ATTACK_OR_SHELL);
        e->hit[index] = 1;
    }
    Vec3f playerCenter = {m->pos[0], m->pos[1]+BM64_PLAYER_HEIGHT*BM64_HOST_SCALE, m->pos[2]};
    if (!e->self_hit && sphere_hits_cylinder(e->position, radius, m->pos[0], m->pos[1],
            m->pos[2], BM64_PLAYER_RADIUS*BM64_HOST_SCALE, BM64_PLAYER_TOP_OFFSET*BM64_HOST_SCALE) &&
        line_of_sight(e->position, playerCenter, NULL)) {
        e->self_hit = 1;
        /* One native health wedge per blast is an explicit SM64 mapping.
         * Native health processing/death is retained; source stun/hearts and
         * bomb-bounce traversal have not been ported. Never stack this every tick. */
        if (!m->invincTimer && !m->hurtCounter && !(m->flags & (MARIO_METAL_CAP|MARIO_VANISH_CAP)))
            m->hurtCounter += 4;
    }
    for (int index = 0; index < BM64_HOST_BOMBS; ++index) {
        HostBomb *b = &sBombs[index];
        if (!b->active || (b->source.state & BM64_BOMB_HELD)) continue;
        Vec3f center = {b->source.position[0]*BM64_HOST_SCALE,
            (b->source.position[1]+b->source.height)*BM64_HOST_SCALE,
            b->source.position[2]*BM64_HOST_SCALE};
        float dx=center[0]-e->position[0], dy=center[1]-e->position[1], dz=center[2]-e->position[2];
        float combined=radius+b->source.radius*BM64_HOST_SCALE;
        if (dx*dx+dy*dy+dz*dz < combined*combined && line_of_sight(e->position, center, NULL))
            b->source.state |= BM64_BOMB_EXPLODING;
    }
}
static void start_explosion(HostBomb *b) {
    for (int i=0; i<BM64_HOST_BOMBS; ++i) if (!sExplosions[i].source.alive) {
        HostExplosion *e=&sExplosions[i];
        memset(e, 0, sizeof(*e));
        int pumped=(b->source.state & BM64_BOMB_PUMPED)!=0;
        /* Source component zero is the damaging core. The original pumped
         * decorative components and grid-derived center placement are absent.
         * Here the bomb base is the explicit SM64 explosion center adapter.
         * Pumped caller 802756DC..802757DC selects fire9 at 80275734, then
         * component0/delay8 at 802757B0/B4 and calls SpawnExplosion at
         * 802757C4. Duration uses input9 before FULL_POWER forces amplitude6. */
        bm64_explosion_init(&e->source, 0, b->source.type, pumped ? 9 : b->source.fire_power, pumped ? 8 : 0);
        e->model=bm64_explosion_model_id(0, b->source.type);
        for (int j=0; j<3; ++j) e->position[j]=b->source.position[j]*BM64_HOST_SCALE;
        break;
    }
    if (sHeld>=0 && b==&sBombs[sHeld]) sHeld=-1;
    b->active=0;
}
static void source_animation(float speed) {
    int selected[2]={0,-1};
    int tier=speed<=0 ? 0 : speed<4.0f ? 1 : speed<8.5f ? 2 : 3;
    if (sThrowing) {
        selected[0]=11; selected[1]=-1;
    } else if (sKicking) {
        selected[0]=sKickAnimation; selected[1]=-1;
    } else if (sHeld>=0) {
        selected[0]=tier ? 8 : 9;
        selected[1]=tier ? tier*2 : -1;
    } else if (tier) {
        selected[0]=tier*2-1; selected[1]=tier*2;
    }
    for (int i=0; i<2; ++i) {
        if (sAnimation[i]!=selected[i]) { sAnimation[i]=selected[i]; sFrame[i]=0; }
        /* Source channel evaluator samples before advancing by one, and wraps
         * once on >= duration. The renderer composes these ordered channels. */
        sRender.animation[i]=sAnimation[i]; sRender.frame[i]=sFrame[i];
        if (sAnimation[i]>=0) {
            sFrame[i]+=1.0f;
            if (sFrame[i]>=sDurations[sAnimation[i]]) sFrame[i]-=sDurations[sAnimation[i]];
        }
    }
    if (sThrowing && sFrame[0]==0.0f) sThrowing=0;
    if (sKicking && sFrame[0]==0.0f) sKicking=0;
}
static int bomb_contact(struct MarioState *m, float offsetX, float offsetZ, int include_standing) {
    float x=m->pos[0]/BM64_HOST_SCALE+offsetX, y=m->pos[1]/BM64_HOST_SCALE+BM64_PLAYER_HEIGHT;
    float z=m->pos[2]/BM64_HOST_SCALE+offsetZ;
    for (int i=0;i<BM64_HOST_BOMBS;++i) {
        HostBomb *b=&sBombs[i];
        if (!b->active || (b->source.state & (BM64_BOMB_HELD|BM64_BOMB_EXPLODING)) ||
            (!include_standing && !b->left_owner)) continue;
        float dx=x-b->source.position[0], dy=y-b->source.position[1]-b->source.height;
        float dz=z-b->source.position[2], radius=BM64_PLAYER_RADIUS+b->source.radius;
        /* The host maps its contact query to the source player/bomb spheres.
         * Original integer-truncated sphere overlap is used only for these
         * two recovered shapes, never substituted for native enemy shapes. */
        if (fabsf(dx)>=radius || fabsf(dy)>=radius || fabsf(dz)>=radius) continue;
        int ix=(int)dx,iy=(int)dy,iz=(int)dz,ir=(int)radius;
        if (ix*ix+iy*iy+iz*iz<ir*ir) {
            Vec3f from={m->pos[0],m->pos[1]+BM64_PLAYER_HEIGHT*BM64_HOST_SCALE,m->pos[2]};
            Vec3f to={b->source.position[0]*BM64_HOST_SCALE,
                (b->source.position[1]+b->source.height)*BM64_HOST_SCALE,b->source.position[2]*BM64_HOST_SCALE};
            if (line_of_sight(from,to,NULL)) return i;
        }
    }
    return -1;
}
static void begin_kick(int mode,float probeX,float probeZ) {
    sKicking=1; sKickReleased=0; sKickMode=mode;
    sKickAnimation=sHeld>=0 ? 15 : 10;
    sKickProbeX=probeX; sKickProbeZ=probeZ;
    sKickPending=0; sKickCountdown=0;
}
static void contact_kick(struct MarioState *m, const Bm64MotionPlan *plan) {
    if (sKicking || sThrowing) { sKickPending=0; sKickCountdown=0; return; }
    int target=bomb_contact(m,plan->dx,plan->dz,0);
    if (sKickPending) {
        if (target<0 || plan->speed<=0) { sKickPending=0; sKickCountdown=0; }
        else if (--sKickCountdown==0) {
            sKickPending=0;
            if (sVertical.fall_velocity==0) begin_kick(1,plan->dx,plan->dz);
        }
    }
    /* 8024F9F0 initializes two only after the prior-countdown update, so a
     * newly acquired contact never consumes its first count in this tick. */
    if (!sKickPending && !sKicking && target>=0 && sVertical.fall_velocity==0) {
        sKickPending=1; sKickCountdown=2;
    }
}
static void apply_kick(struct MarioState *m) {
    if (sKickReleased || sFrame[0]!=11.0f) return;
    sKickReleased=1;
    /* Source mode one temporarily offsets the player to re-query contact.
     * The SM64 contact vector is the attempted source displacement. Mode two
     * (A while standing inside the placed bomb) uses the current position. */
    int target=bomb_contact(m,sKickMode==1?sKickProbeX:0,sKickMode==1?sKickProbeZ:0,1);
    if (target<0) return;
    HostBomb *b=&sBombs[target];
    if (b->source.state & BM64_BOMB_HELD) return;
    if (b->source.state & BM64_BOMB_AIRBORNE) {
        /* Original detaches the throw before refusing full-power bombs. Its
         * throw-state planar component is removed; LevelClass gravity stays. */
        b->source.state &= ~BM64_BOMB_AIRBORNE;
        b->speed=0;
    }
    if (b->source.type & BM64_BOMB_FULL_POWER) return;
    int heading=bm64_bomb_heading_from_degrees(sFacing.angle);
    if ((b->source.state & BM64_BOMB_ROLLING) && b->source.heading==heading) return;
    /* Original 80275C7C scalar transition. Kick never resets the live fuse. */
    b->source.state=(b->source.state & ~BM64_BOMB_AIRBORNE)|BM64_BOMB_ROLLING;
    b->source.owner=0; b->source.heading=heading;
    b->lift=b->fall_velocity=0; b->speed=15.0f;
    if (!b->roll_order) b->roll_order=++sRollOrder;
}
static void block_bomb_motion(struct MarioState *m, Vec3f next) {
    for (int i=0;i<BM64_HOST_BOMBS;++i) {
        HostBomb *b=&sBombs[i];
        if (!b->active || (b->source.state & (BM64_BOMB_HELD|BM64_BOMB_EXPLODING))) continue;
        float x=next[0]/BM64_HOST_SCALE-b->source.position[0];
        float y=next[1]/BM64_HOST_SCALE+BM64_PLAYER_HEIGHT-b->source.position[1]-b->source.height;
        float z=next[2]/BM64_HOST_SCALE-b->source.position[2];
        float radius=BM64_PLAYER_RADIUS+b->source.radius, distance=x*x+y*y+z*z;
        if (distance>=radius*radius) { b->left_owner=1; continue; }
        if (!b->left_owner || fabsf(y)>=radius) continue;
        /* Host solid-contact separation prevents walking through bombs. This
         * is not the original bomb/platform bounce or stacking solver. */
        float planar=sqrtf(x*x+z*z), separation=sqrtf(radius*radius-y*y);
        if (planar<=0.001f) { next[0]=m->pos[0]; next[2]=m->pos[2]; continue; }
        next[0]=(b->source.position[0]+x*(separation/planar))*BM64_HOST_SCALE;
        next[2]=(b->source.position[2]+z*(separation/planar))*BM64_HOST_SCALE;
    }
}
static int player_move(struct MarioState *m, const Bm64MotionPlan *plan) {
    Vec3f next={m->pos[0]+plan->dx*BM64_HOST_SCALE, m->pos[1], m->pos[2]+plan->dz*BM64_HOST_SCALE};
    block_bomb_motion(m,next);
    struct WallCollisionData walls={0};
    walls.x=next[0]; walls.y=next[1]; walls.z=next[2];
    walls.radius=BM64_PLAYER_RADIUS*BM64_HOST_SCALE;
    walls.offsetY=BM64_PLAYER_HEIGHT*BM64_HOST_SCALE;
    find_wall_collisions(&walls);
    next[0]=walls.x; next[2]=walls.z;
    struct Surface *floor=NULL, *ceil=NULL;
    float floorY=find_floor(next[0], next[1], next[2], &floor);
    float ceilY=find_ceil(next[0], next[1], next[2], &ceil);
    /* Host terrain is queried in SM64 units. Missing floors and spaces too
     * short for the original shape reject horizontal movement, as a bounded
     * safety adapter; original grid, steps and slopes are not being emulated. */
    if (!floor || (ceil && ceilY-floorY < BM64_PLAYER_TOP_OFFSET*BM64_HOST_SCALE)) {
        next[0]=m->pos[0]; next[2]=m->pos[2];
        floorY=find_floor(next[0], next[1], next[2], &floor);
        ceilY=find_ceil(next[0], next[1], next[2], &ceil);
    }
    if (!floor) return 0;
    sVertical.y=next[1]/BM64_HOST_SCALE;
    unsigned contacts=bm64_vertical_tick(&sVertical, 1, floorY/BM64_HOST_SCALE,
                                        ceil!=NULL, ceilY/BM64_HOST_SCALE);
    next[1]=sVertical.y*BM64_HOST_SCALE;
    if (!finite_position(next)) return 0;
    vec3f_copy(m->pos,next);
    m->floor=floor; m->floorHeight=floorY; m->ceil=ceil; m->ceilHeight=ceilY;
    mario_update_wall(m,&walls);
    m->waterLevel=find_water_level(next[0],next[2]);
    u32 action=(contacts & BM64_CONTACT_FLOOR) ? (plan->speed>0 ? ACT_WALKING : ACT_IDLE) : ACT_FREEFALL;
    if (m->action!=action) { m->prevAction=m->action; m->action=action; m->actionState=0; m->actionTimer=0; }
    m->terrainSoundAddend=mario_get_terrain_sound_addend(m);
    return 1;
}
static int pickup_bomb(struct MarioState *m, int index) {
    HostBomb *b=&sBombs[index];
    if (!b->active || sHeld>=0 || !bm64_bomb_pickup(&b->source)) return 0;
    float player[3]={m->pos[0]/BM64_HOST_SCALE, m->pos[1]/BM64_HOST_SCALE, m->pos[2]/BM64_HOST_SCALE};
    bm64_hold_begin(&b->hold, player, b->source.position, sFacing.angle,
                    BM64_PLAYER_RADIUS, 100.0f, b->source.radius);
    b->lift=b->fall_velocity=b->speed=0;
    b->roll_order=0; sHeld=index;
    return 1;
}
static void pickup_probe(struct MarioState *m) {
    float x=m->pos[0]/BM64_HOST_SCALE, y=m->pos[1]/BM64_HOST_SCALE, z=m->pos[2]/BM64_HOST_SCALE;
    float sn=bm64_sin_degrees(sFacing.angle), cs=bm64_cos_degrees(sFacing.angle);
    Vec3f probe={x+(sn*100.0f)*0.8f, y+60.0f, z+(cs*100.0f)*0.8f};
    for (int i=0;i<BM64_HOST_BOMBS;++i) {
        HostBomb *b=&sBombs[i];
        if (!b->active || (b->source.state & (BM64_BOMB_PUMPED|BM64_BOMB_EXPLODING))) continue;
        /* The original probe geometry is retained; this host overlap uses
         * sphere/cylinder projection rather than the original actor dispatcher. */
        if (!sphere_hits_cylinder(probe, 50.0f, b->source.position[0], b->source.position[1],
                                  b->source.position[2], b->source.radius, 2.0f*b->source.height)) continue;
        Vec3f from={m->pos[0],m->pos[1]+BM64_PLAYER_HEIGHT*BM64_HOST_SCALE,m->pos[2]};
        Vec3f to={b->source.position[0]*BM64_HOST_SCALE,(b->source.position[1]+b->source.height)*BM64_HOST_SCALE,b->source.position[2]*BM64_HOST_SCALE};
        if (!line_of_sight(from,to,NULL)) continue;
        if (pickup_bomb(m,i)) return;
    }
}
static void place_bomb(struct MarioState *m, int immediate_lift) {
    int count=0, slot=-1;
    for (int i=0;i<BM64_HOST_BOMBS;++i) {
        count+=sBombs[i].active!=0;
        if (!sBombs[i].active && slot<0) slot=i;
    }
    if (count>=BM64_INITIAL_BOMB_COUNT || slot<0 || sHeld>=0) return;
    /* Source spawn stores the player's feet Y. Collision center is base plus
     * shape height, so adding radius here would double the vertical offset. */
    HostBomb *b=&sBombs[slot]; memset(b,0,sizeof(*b));
    bm64_bomb_init(&b->source, BM64_BOMB_DEFAULT, 0, BM64_INITIAL_PLAYER_FIRE_POWER);
    for (int j=0;j<3;++j) b->source.position[j]=m->pos[j]/BM64_HOST_SCALE;
    b->active=1; b->fall_velocity=1.0f;
    if (immediate_lift) pickup_bomb(m,slot);
}
static int full_pump_blocked(const HostBomb *b) {
    struct WallCollisionData walls={0};
    walls.x=b->source.position[0]*BM64_HOST_SCALE;
    walls.y=b->source.position[1]*BM64_HOST_SCALE;
    walls.z=b->source.position[2]*BM64_HOST_SCALE;
    walls.radius=70.0f*BM64_HOST_SCALE; walls.offsetY=70.0f*BM64_HOST_SCALE;
    if (find_wall_collisions(&walls)>0) return 1;
    struct Surface *ceil=NULL;
    float ceiling=find_ceil(walls.x,walls.y,walls.z,&ceil);
    return ceil && ceiling < walls.y+140.0f*BM64_HOST_SCALE;
}
static void held_position(struct MarioState *m, HostBomb *b) {
    bm64_hold_tick(&b->hold,sFacing.angle,100.0f,b->source.radius);
    float x=b->hold.local[0], z=b->hold.local[2];
    if (b->hold.timer<=0) {
        float sn=bm64_sin_degrees(sFacing.angle),cs=bm64_cos_degrees(sFacing.angle);
        x=b->hold.local[0]*cs+b->hold.local[2]*sn;
        z=b->hold.local[2]*cs-b->hold.local[0]*sn;
    }
    b->source.position[0]=m->pos[0]/BM64_HOST_SCALE+x;
    b->source.position[1]=m->pos[1]/BM64_HOST_SCALE+b->hold.local[1];
    b->source.position[2]=m->pos[2]/BM64_HOST_SCALE+z;
}
static void stop_oldest_roll(void) {
    int first=-1;
    for (int i=0;i<BM64_HOST_BOMBS;++i)
        if (sBombs[i].active && (sBombs[i].source.state & BM64_BOMB_ROLLING) &&
            (first<0 || sBombs[i].roll_order<sBombs[first].roll_order)) first=i;
    if (first>=0) {
        bm64_bomb_stop_rolling(&sBombs[first].source);
        sBombs[first].speed=0; sBombs[first].roll_order=0;
    }
}
static void bomb_terrain(HostBomb *b) {
    float dx=0,dz=0;
    float speed=(b->source.state & BM64_BOMB_ROLLING) ? 15.0f : b->speed;
    bm64_bomb_flat_delta(b->source.heading,speed,&dx,&dz);
    Vec3f next={(b->source.position[0]+dx)*BM64_HOST_SCALE,b->source.position[1]*BM64_HOST_SCALE,
                (b->source.position[2]+dz)*BM64_HOST_SCALE};
    struct WallCollisionData walls={0};
    walls.x=next[0]; walls.y=next[1]; walls.z=next[2];
    walls.radius=b->source.radius*BM64_HOST_SCALE; walls.offsetY=b->source.height*BM64_HOST_SCALE;
    if (find_wall_collisions(&walls)>0) {
        /* Wall stop is an SM64 terrain response. The original slope solver,
         * obstacle reflection and bomb-bounce contact state are not ported. */
        bm64_bomb_stop_rolling(&b->source); b->speed=0; b->roll_order=0;
    }
    next[0]=walls.x; next[2]=walls.z;
    struct Surface *floor=NULL,*ceil=NULL;
    float floorY=find_floor(next[0],next[1],next[2],&floor)/BM64_HOST_SCALE;
    float ceilY=find_ceil(next[0],next[1],next[2],&ceil)/BM64_HOST_SCALE;
    if (!floor) { b->active=0; return; }
    float y=b->source.position[1];
    /* Shared original LevelClass scalar integration order. Here flat floor
     * contact settles the bomb; source bomb bridges/bounces remain excluded. */
    if ((y-b->fall_velocity)+b->lift<=floorY) {
        y=floorY; b->fall_velocity=b->lift=0;
        b->source.state=(b->source.state & ~BM64_BOMB_AIRBORNE)|BM64_BOMB_GROUNDED;
        if (!(b->source.state & BM64_BOMB_ROLLING)) b->speed=0;
    } else {
        y=(y+b->lift)-b->fall_velocity;
        if (b->fall_velocity<=BM64_PLAYER_TERMINAL_FALL) b->fall_velocity+=BM64_PLAYER_GRAVITY;
        else b->fall_velocity=BM64_PLAYER_TERMINAL_FALL;
    }
    float top=b->source.radius+b->source.height;
    if (ceil && (ceilY<y || ceilY<y+top)) {
        if (b->lift!=0) b->fall_velocity=0;
        b->lift=0; y=ceilY-top;
    }
    b->source.position[0]=next[0]/BM64_HOST_SCALE;
    b->source.position[1]=y;
    b->source.position[2]=next[2]/BM64_HOST_SCALE;
    if (!finite_position(b->source.position)) b->active=0;
}
static void update_bombs(struct MarioState *m) {
    for (int i=0;i<BM64_HOST_BOMBS;++i) {
        HostBomb *b=&sBombs[i]; if (!b->active) continue;
        if (b->source.state & BM64_BOMB_HELD) held_position(m,b);
        bm64_bomb_visual_tick(&b->source,0,full_pump_blocked(b));
        bm64_bomb_fuse_tick(&b->source,0);
        if (!(b->source.state & BM64_BOMB_HELD)) bomb_terrain(b);
        if (b->active && bm64_bomb_commit_scale_explosion(&b->source)) start_explosion(b);
    }
    for (int i=0;i<BM64_HOST_BOMBS;++i) if (sExplosions[i].source.alive) {
        apply_explosion(m,&sExplosions[i]);
        bm64_explosion_tick_original(&sExplosions[i].source);
    }
}
static void render_snapshot(struct MarioState *m) {
    vec3f_copy(sRender.position,m->pos); sRender.yaw=sFacing.angle;
    sRender.ticks=sTicks; sRender.held=sHeld>=0; sRender.pumped=0; sRender.explosions=0;
    memset(sRender.bombs,0,sizeof(sRender.bombs));
    int rendered=0;
    for (int i=0;i<BM64_HOST_BOMBS;++i) if (sBombs[i].active) {
        HostBomb *b=&sBombs[i];
        sRender.pumped+=(b->source.state & BM64_BOMB_PUMPED)!=0;
        if (rendered>=BM64_HOST_BOMBS) continue;
        Bm64RenderBomb *out=&sRender.bombs[rendered++];
        out->active=1; out->model=bm64_bomb_model_id(b->source.type); out->scale=b->source.scale;
        out->yaw=0; out->opacity=255;
        for (int j=0;j<3;++j) out->position[j]=b->source.position[j]*BM64_HOST_SCALE;
    }
    for (int i=0;i<BM64_HOST_BOMBS;++i) if (sExplosions[i].source.alive) {
        HostExplosion *e=&sExplosions[i]; ++sRender.explosions;
        if (e->source.delay>0 || rendered>=BM64_HOST_BOMBS) continue;
        Bm64RenderBomb *out=&sRender.bombs[rendered++];
        out->active=1; out->model=e->model; out->scale=e->source.scale;
        out->yaw=e->source.rotation; out->opacity=(uint8_t)e->source.opacity;
        vec3f_copy(out->position,e->position);
    }
    bm64_runtime_submit(&sRender);
}
static void release_held_bomb(float magnitude) {
    if (sHeld<0) return;
    HostBomb *b=&sBombs[sHeld];
    /* Original default stick-range selector, sampled at the release event:
     * standing is mode one; mode zero is not the standing B throw. */
    int mode=bm64_throw_mode(magnitude,0,0);
    Bm64BombThrowPreset preset;
    if (!bm64_bomb_throw_preset(mode,&preset)) return;
    bm64_bomb_release(&b->source);
    b->source.heading=bm64_bomb_heading_from_degrees(sFacing.angle);
    b->lift=preset.lift; b->fall_velocity=preset.downward_velocity; b->speed=preset.planar_speed;
    b->left_owner=0; b->roll_order=0; sHeld=-1;
}
static void input_actions(struct MarioState *m, unsigned pressed, unsigned down, float magnitude) {
    if (sKicking) { apply_kick(m); return; }
    if (sThrowing) {
        /* Original logical action six uses channel zero track eleven, and
         * releases exactly once at frame twelve before animation advances. */
        if (sFrame[0]<12.0f && sHeld<0) { sThrowing=0; return; }
        if (sAnimation[0]==11 && bm64_throw_releases(sFrame[0],sThrowReleased)) {
            release_held_bomb(magnitude); sThrowReleased=1;
        }
        return;
    }
    if ((pressed & B_BUTTON) && sHeld>=0) {
        sThrowing=1; sThrowReleased=0;
        return;
    }
    if (pressed & R_TRIG) stop_oldest_roll();
    if (pressed & A_BUTTON) {
        if (sHeld>=0) bm64_bomb_pump_input(&sBombs[sHeld].source);
        else {
            int standing=bomb_contact(m,0,0,1);
            if (!(down & B_BUTTON) && standing>=0 && !sBombs[standing].left_owner && sVertical.fall_velocity==0)
                begin_kick(2,0,0);
            else place_bomb(m,(down & B_BUTTON)!=0);
        }
    } else if ((pressed & B_BUTTON) && sHeld<0) pickup_probe(m);
    /* Remote detonation is deliberately absent from the initial inventory;
     * the source Z action requires the remote upgrade, which is not granted. */
}
int bm64_adapter_update(struct MarioState *m) {
    if (!m || m->playerIndex!=0) return 0;
    if (!sSelected || !bm64_runtime_enabled() || !m->marioObj || !m->controller || !m->area || !m->floor ||
        !finite_position(m->pos)) { bm64_adapter_suspend(); return 0; }
    if (sPlayer && (sPlayer!=m || sArea!=m->area || sLevel!=gCurrLevelNum)) bm64_adapter_suspend();
    if (sHavePosition) {
        float x=m->pos[0]-sLastPosition[0], y=m->pos[1]-sLastPosition[1], z=m->pos[2]-sLastPosition[2];
        /* Teleport boundary is a host safety rule, never swept as bomb motion. */
        if (x*x+y*y+z*z>256.0f*256.0f) bm64_adapter_suspend();
    }
    if (!supported(m->action) || m->heldObj || m->riddenObj || m->heldByObj || m->quicksandDepth>1.0f ||
        (m->input & INPUT_SQUISHED) || m->health<0x100) { bm64_adapter_suspend(); return 0; }
    if (!sPlayer) {
        sPlayer=m; sArea=m->area; sLevel=gCurrLevelNum;
        bm64_facing_init(&sFacing,(float)(u16)m->faceAngle[1]*(360.0f/65536.0f));
        sVertical.y=m->pos[1]/BM64_HOST_SCALE; sVertical.extra_y=0;
        /* Entering an existing native fall maps its velocity to source units. */
        sVertical.fall_velocity=m->action==ACT_FREEFALL ? -m->vel[1]/(BM64_SOURCE_TICKS*BM64_HOST_SCALE) : 0;
        sAnimation[0]=0; sAnimation[1]=-1;
        sRender.animation[0]=0; sRender.animation[1]=-1;
    }
    if (m->freeze || sCurrPlayMode==PLAY_MODE_PAUSED) {
        /* No source tick, button edge, animation, fuse or explosion is advanced. */
        if (bm64_runtime_visible()) { m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE; sOwnHide=1; }
        return 1;
    }
    if (m->pos[1]<m->waterLevel-100.0f) {
        bm64_adapter_suspend(); set_water_plunge_action(m); return 0;
    }
    int rawX=m->controller->rawStickX, rawY=m->controller->rawStickY;
    rawX=rawX < -128 ? -128 : rawX > 127 ? 127 : rawX;
    rawY=rawY < -128 ? -128 : rawY > 127 ? 127 : rawY;
    Bm64PolarInput polar=bm64_controller_polar((s8)rawX,(s8)rawY);
    s16 cameraYaw=m->area->camera ? m->area->camera->yaw : m->faceAngle[1];
    if (gLakituState.mode==CAMERA_MODE_NEWCAM)
        cameraYaw=get_first_person_enabled() ? gLakituState.yaw : -gNewCamera.yaw+0x4000;
    float world=wrap_degrees(polar.angle_degrees+(float)cameraYaw*(360.0f/65536.0f));
    Bm64MotionPlan plan={0};
    for (int tick=0; tick<BM64_SOURCE_TICKS; ++tick) {
        input_actions(m,tick==0 ? m->controller->buttonPressed : 0,m->controller->buttonDown,polar.magnitude);
        plan=bm64_plan_normal_motion(polar.magnitude,world,sThrowing ? 0x60u : sKicking ? 0x120u : 0u);
        contact_kick(m,&plan);
        if (sKicking) plan=bm64_plan_normal_motion(polar.magnitude,world,0x120u);
        bm64_facing_tick(&sFacing,&plan);
        if (!player_move(m,&plan)) { bm64_adapter_suspend(); return 0; }
        update_bombs(m);
        source_animation(plan.speed);
        ++sTicks;
    }
    m->faceAngle[1]=(s16)(s32)(sFacing.angle*(65536.0f/360.0f));
    m->forwardVel=plan.speed*BM64_SOURCE_TICKS*BM64_HOST_SCALE;
    m->vel[0]=plan.dx*BM64_SOURCE_TICKS*BM64_HOST_SCALE;
    m->vel[2]=plan.dz*BM64_SOURCE_TICKS*BM64_HOST_SCALE;
    m->vel[1]=-sVertical.fall_velocity*BM64_SOURCE_TICKS*BM64_HOST_SCALE;
    m->slideVelX=m->vel[0]; m->slideVelZ=m->vel[2];
    vec3f_copy(m->marioObj->header.gfx.pos,m->pos);
    vec3s_set(m->marioObj->header.gfx.angle,0,m->faceAngle[1],0);
    render_snapshot(m);
    int drawable = sWheelMode && bm64_runtime_snapshot(&sRender);
    if (drawable || bm64_runtime_visible()) { m->marioObj->header.gfx.node.flags|=GRAPH_RENDER_INVISIBLE; sOwnHide=1; }
    else restore_visibility();
    vec3f_copy(sLastPosition,m->pos); sHavePosition=1;
    return 1;
}
