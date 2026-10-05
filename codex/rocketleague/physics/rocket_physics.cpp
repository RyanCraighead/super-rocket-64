#include "rocket_physics.h"
#include "metal_water.h"
#include "RocketSim.h"
#include "BulletCollision/CollisionShapes/btTriangleMesh.h"
#include "BulletCollision/CollisionDispatch/btInternalEdgeUtility.h"
#include "BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h"
#include "BulletCollision/BroadphaseCollision/btCollisionAlgorithm.h"
#include "BulletCollision/CollisionShapes/btTriangleShape.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace RocketSim;
namespace {
thread_local std::string error;
bool finite3(const float *v) { return v && std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]); }
float axis(float x) { return std::isfinite(x) ? std::max(-1.f, std::min(1.f,x)) : 0.f; }
Vec fromHost(const float *v) { return {v[2]/ROCKET_HOST_SCALE,v[0]/ROCKET_HOST_SCALE,v[1]/ROCKET_HOST_SCALE}; }
void toHost(const Vec &v,float *out,float scale=ROCKET_HOST_SCALE) { out[0]=v.y*scale;out[1]=v.z*scale;out[2]=v.x*scale; }
struct HostFace { btVector3 point,normal; uint8_t material; };
struct Mesh {
    const unsigned *surfaceMode=nullptr;
    std::vector<HostFace> faces;
    std::vector<RocketTriangle> sourceTriangles;
    std::unique_ptr<btTriangleMesh> triangles;
    std::unique_ptr<btTriangleInfoMap> edges;
    std::unique_ptr<btBvhTriangleMeshShape> shape;
    std::unique_ptr<btRigidBody> body;
    btTransform previousTransform=btTransform::getIdentity();
    btTransform targetTransform=btTransform::getIdentity();
    std::array<float,3> hostPosition{};
    bool hasPose=false,hasMotion=false;
};
constexpr int HOST_MESH_TAG=0x534d3634;
bool validMaterial(unsigned material) {
    if ((material & ~15u) || (material & 3u) == 3u) return false;
    return !(material & ROCKET_MATERIAL_RACE_SLIDE) || material ==
        (ROCKET_MATERIAL_RACE_SLIDE | ROCKET_MATERIAL_VERY_SLIPPERY | ROCKET_MATERIAL_SLIDING);
}
float materialGrip(const Mesh *mesh,int index) {
    if(!mesh||!mesh->surfaceMode||*mesh->surfaceMode!=ROCKET_SURFACES_NATIVE||
       index<0||(size_t)index>=mesh->faces.size())return 1.f;
    unsigned material=mesh->faces[index].material;
    // Retain the ordinary native ice coefficient on the scoped race slopes.
    // This restores steer/brake response without full Car-grip traction.
    if(material&ROCKET_MATERIAL_RACE_SLIDE)return .25f;
    if(material&ROCKET_MATERIAL_SLIDING)return 0.f;
    // Ratios of native neutral slide losses: .02/.08 (ice), .04/.08
    // (slippery). These are a car adaptation, not Mario controller parity.
    return material==ROCKET_MATERIAL_VERY_SLIPPERY?.25f:material==ROCKET_MATERIAL_SLIPPERY?.5f:1.f;
}
struct MaterialAtPoint : btTriangleCallback {
    const Mesh *mesh;btVector3 point;float grip=1.f,distance=.02f;
    MaterialAtPoint(const Mesh *m,const btVector3 &p):mesh(m),point(p){}
    void processTriangle(btVector3 *v,int,int index) override {
        btVector3 normal=(v[1]-v[0]).cross(v[2]-v[0]).normalized();
        float d=std::fabs((point-v[0]).dot(normal));if(d>distance)return;
        for(int k=0;k<3;++k)if((v[(k+1)%3]-v[k]).cross(point-v[k]).dot(normal)<-.00001f)return;
        distance=d;grip=materialGrip(mesh,index);
    }
};
constexpr size_t MAX_HOST_TRIANGLES=100000;
constexpr size_t MAX_HOST_PLATFORMS=4096;
constexpr float PLATFORM_VERTEX_TOLERANCE=3.f;
constexpr float PLATFORM_TELEPORT_DISTANCE=128.f;
constexpr float PLATFORM_TELEPORT_ANGLE=.35f;
// SM64 faces have an authored front. A two-sided triangle can create a back
// contact at a sharp lawn/ramp seam and drive the car below the whole level.
// Filter those chassis manifolds; never rewrite their anchors or teleport the
// body. Suspension rays retain the pinned backend's separate ray semantics.
ContactAddedCallback arenaContact=nullptr;
char rejectedHostContact;
bool rejectContact(const btManifoldPoint &point,const btVector3 &normal,const btVector3 &plane,
                   const btConvexShape *convex,const btTransform &transform,bool meshIsB) {
    if(point.m_normalWorldOnB.dot(normal)*(meshIsB?1.f:-1.f)<0)return true;
    btVector3 support=transform*convex->localGetSupportingVertex(transform.getBasis().transpose()*normal);
    return (support-plane).dot(normal)<-SIMD_EPSILON;
}
bool finite9(const float *v) {
    if(!v)return false;
    for(int i=0;i<9;++i)if(!std::isfinite(v[i]))return false;
    return true;
}
btTransform platformTransform(const RocketPlatform &platform) {
    if(!finite3(platform.position)||!finite9(platform.basis))
        throw std::runtime_error("Invalid platform transform");
    for(int i=0;i<3;++i)if(std::fabs(platform.position[i])>1000000.f)
        throw std::runtime_error("Platform position out of range");

    // API matrices are host-space XYZ columns. Build the row-major Bullet
    // matrix, then conjugate by host -> RocketSim's proper axis rotation.
    btMatrix3x3 hostBasis(
        platform.basis[0],platform.basis[3],platform.basis[6],
        platform.basis[1],platform.basis[4],platform.basis[7],
        platform.basis[2],platform.basis[5],platform.basis[8]);
    btVector3 x=hostBasis.getColumn(0),y=hostBasis.getColumn(1),z=hostBasis.getColumn(2);
    if(std::fabs(x.length2()-1.f)>.02f||std::fabs(y.length2()-1.f)>.02f||
       std::fabs(z.length2()-1.f)>.02f||std::fabs(x.dot(y))>.02f||
       std::fabs(x.dot(z))>.02f||std::fabs(y.dot(z))>.02f||
       x.cross(y).dot(z)<.98f)
        throw std::runtime_error("Platform basis must be a proper rotation");
    const btMatrix3x3 hostToRocket(0,0,1, 1,0,0, 0,1,0);
    btMatrix3x3 rocketBasis=hostToRocket*hostBasis*hostToRocket.transpose();
    Vec hostOrigin=fromHost(platform.position);
    btVector3 origin=hostOrigin*UU_TO_BT;
    return btTransform(rocketBasis,origin);
}
btTransform interpolate(const btTransform &from,const btTransform &to,float amount) {
    btQuaternion a,b;
    from.getBasis().getRotation(a);to.getBasis().getRotation(b);
    btQuaternion q=a.slerp(b,btClamped(amount,btScalar(0),btScalar(1)));
    q.normalize();
    return btTransform(q,from.getOrigin().lerp(to.getOrigin(),amount));
}
float rotationDistance(const btTransform &a,const btTransform &b) {
    btQuaternion qa,qb;a.getBasis().getRotation(qa);b.getBasis().getRotation(qb);
    qa.normalize();qb.normalize();
    btScalar dot=btFabs(qa.dot(qb));
    return float(btScalar(2)*btAcos(btClamped(dot,btScalar(0),btScalar(1))));
}
btVector3 angularVelocity(const btTransform &from,const btTransform &to,float dt) {
    btQuaternion a,b;from.getBasis().getRotation(a);to.getBasis().getRotation(b);
    a.normalize();b.normalize();
    btQuaternion delta=b*a.inverse();delta.normalize();
    if(delta.w()<0)delta=btQuaternion(-delta.x(),-delta.y(),-delta.z(),-delta.w());
    btScalar angle=delta.getAngle();
    if(angle<SIMD_EPSILON)return btVector3(0,0,0);
    return delta.getAxis()*(angle/dt);
}
void validatePlatformTriangles(const RocketPlatform &platform) {
    if(platform.count==0||platform.count>MAX_HOST_TRIANGLES||!platform.triangles)
        throw std::runtime_error("Invalid platform collision geometry");
    for(size_t i=0;i<platform.count;++i)if(!validMaterial(platform.triangles[i].material))throw std::runtime_error("Invalid platform material");
    for(size_t i=0;i<platform.count;++i)for(int v=0;v<3;++v){
        const float *point=platform.triangles[i].v[v];
        if(!finite3(point))throw std::runtime_error("Nonfinite platform vertex");
        Vec host=fromHost(point);
        if(host.Length()>1000000.f)throw std::runtime_error("Platform vertex out of range");
    }
}
bool samePlatformGeometry(const Mesh &mesh,const RocketPlatform &platform) {
    if(mesh.sourceTriangles.size()!=platform.count)return false;
    for(size_t i=0;i<platform.count;++i)if(mesh.sourceTriangles[i].material!=platform.triangles[i].material)return false;
    constexpr float tolerance=PLATFORM_VERTEX_TOLERANCE;
    for(size_t i=0;i<platform.count;++i)for(int v=0;v<3;++v)for(int axis=0;axis<3;++axis)
        if(std::fabs(mesh.sourceTriangles[i].v[v][axis]-platform.triangles[i].v[v][axis])>tolerance)
            return false;
    return true;
}
void populateMesh(Mesh &mesh,const RocketPlatform &platform,const btTransform &initialTransform,
                  void *arenaUserPointer,bool kinematic) {
    mesh.sourceTriangles.assign(platform.triangles,platform.triangles+platform.count);
    mesh.triangles=std::make_unique<btTriangleMesh>();
    for(size_t i=0;i<platform.count;++i){
        btVector3 p[3];
        for(int v=0;v<3;++v){auto r=fromHost(platform.triangles[i].v[v]);p[v]=r*UU_TO_BT;}
        btVector3 normal=(p[1]-p[0]).cross(p[2]-p[0]);
        if(normal.length2()>1e-12f){
            mesh.triangles->addTriangle(p[0],p[1],p[2],false);
            mesh.faces.push_back({p[0],normal.normalized(),platform.triangles[i].material});
        }
    }
    if(mesh.triangles->getNumTriangles()==0)throw std::runtime_error("Empty platform collision geometry");
    mesh.shape=std::make_unique<btBvhTriangleMeshShape>(mesh.triangles.get(),true);
    mesh.edges=std::make_unique<btTriangleInfoMap>();
    btGenerateInternalEdgeInfo(mesh.shape.get(),mesh.edges.get());
    mesh.shape->setTriangleInfoMap(mesh.edges.get());
    mesh.body=std::make_unique<btRigidBody>(0,nullptr,mesh.shape.get());
    mesh.body->setWorldTransform(initialTransform);
    mesh.body->setInterpolationWorldTransform(initialTransform);
    mesh.body->setFriction(RLConst::ARENA_COLLISION_BASE_FRICTION);
    mesh.body->setRestitution(RLConst::ARENA_COLLISION_BASE_RESTITUTION);
    mesh.body->setUserPointer(arenaUserPointer);
    mesh.body->setUserIndex2(HOST_MESH_TAG);
    if(kinematic){
        int flags=mesh.body->getCollisionFlags();
        flags&=~btCollisionObject::CF_STATIC_OBJECT;
        flags|=btCollisionObject::CF_KINEMATIC_OBJECT;
        mesh.body->setCollisionFlags(flags);
        mesh.body->setActivationState(DISABLE_DEACTIVATION);
    }
    mesh.previousTransform=mesh.targetTransform=initialTransform;
    mesh.hasPose=kinematic;
}
void removePlatform(RocketWorld *w,Mesh &mesh);
bool hostContact(btManifoldPoint &point,const btCollisionObjectWrapper *a,int partA,int indexA,
                 const btCollisionObjectWrapper *b,int partB,int indexB) {
    const btCollisionObjectWrapper *mesh=nullptr,*convex=nullptr;bool meshIsB=true;
    if(b->getCollisionObject()->getUserIndex2()==HOST_MESH_TAG){mesh=b;convex=a;}
    else if(a->getCollisionObject()->getUserIndex2()==HOST_MESH_TAG){mesh=a;convex=b;meshIsB=false;}
    btVector3 normal;
    bool hostCar=mesh&&convex->getCollisionObject()->getUserIndex()==BT_USERINFO_TYPE_CAR&&
        mesh->getCollisionShape()->getShapeType()==TRIANGLE_SHAPE_PROXYTYPE&&convex->getCollisionShape()->isConvex();
    if(hostCar){
        const auto *triangle=static_cast<const btTriangleShape*>(mesh->getCollisionShape());
        const auto *shape=static_cast<const btConvexShape*>(convex->getCollisionShape());
        triangle->calcNormal(normal);normal=mesh->getWorldTransform().getBasis()*normal;
        btVector3 plane=mesh->getWorldTransform()*triangle->getVertexPtr(0);
        // Use the transformed child shape, including Octane's hitbox offset.
        if(rejectContact(point,normal,plane,shape,convex->getWorldTransform(),meshIsB)){
            // Bullet ignores the callback return value. The per-world near
            // callback removes this point before solving. Do not let it set
            // RocketSim's worldContact state in the original callback.
            point.m_userPersistentData=&rejectedHostContact;
            return true;
        }
    }
    return arenaContact?arenaContact(point,a,partA,indexA,b,partB,indexB):true;
}
void hostNearCallback(btBroadphasePair &pair,btCollisionDispatcher &dispatcher,const btDispatcherInfo &info) {
    btCollisionDispatcher::defaultNearCallback(pair,dispatcher,info);
    if(!pair.m_algorithm)return;
    btManifoldArray manifolds;pair.m_algorithm->getAllContactManifolds(manifolds);
    for(int i=0;i<manifolds.size();i++){
        auto *manifold=manifolds[i];
        const btCollisionObject *a=manifold->getBody0(),*b=manifold->getBody1();
        bool meshIsB=b->getUserIndex2()==HOST_MESH_TAG;
        const btCollisionObject *meshBody=meshIsB?b:a,*car=meshIsB?a:b;
        if(meshBody->getUserIndex2()!=HOST_MESH_TAG||car->getUserIndex()!=BT_USERINFO_TYPE_CAR)continue;
        const auto *mesh=static_cast<const Mesh*>(meshBody->getCollisionShape()->getUserPointer());
        // The pinned car has one offset convex hitbox. Reclassify persistent
        // points too; new-point callbacks alone miss cached backface contacts.
        if(!mesh||!car->getCollisionShape()->isCompound())continue;
        const auto *compound=static_cast<const btCompoundShape*>(car->getCollisionShape());
        if(compound->getNumChildShapes()!=1||!compound->getChildShape(0)->isConvex())continue;
        const auto *convex=static_cast<const btConvexShape*>(compound->getChildShape(0));
        btTransform transform=car->getWorldTransform()*compound->getChildTransform(0);
        for(int j=manifold->getNumContacts()-1;j>=0;j--){
            auto &point=manifold->getContactPoint(j);
            int index=meshIsB?point.m_index1:point.m_index0;
            bool reject=point.m_userPersistentData==&rejectedHostContact;
            if(index>=0&&(size_t)index<mesh->faces.size()){
                const auto &face=mesh->faces[index];
                reject|=rejectContact(point,meshBody->getWorldTransform().getBasis()*face.normal,
                    meshBody->getWorldTransform()*face.point,convex,transform,meshIsB);
            }
            if(reject){point.m_userPersistentData=nullptr;manifold->removeContactPoint(j);}
        }
    }
}

}
struct RocketWorld {
    std::unique_ptr<Arena> arena;
    Car *car=nullptr;
    std::array<Mesh,2> meshes;
    std::unordered_map<uint64_t,std::unique_ptr<Mesh>> platforms;
    uint64_t frame=0,ticks=0;
    bool haveFrame=false,ready=false,metalWater=false;
    Vec dryGravity;
    unsigned surfaceMode=ROCKET_SURFACES_CAR, speedPercent=100, jumpPercent=100;
    RocketEnvironment environment={};
    unsigned inhibited=7;
    int boostMode=ROCKET_BOOST_COIN_ONLY;
    bool temporaryBoost=false;
    float coinBoost=100.f;
    bool waterPresent=false,waterMetal=false;
    float waterLevel=0;
    int waterMode=ROCKET_WATER_DRY;
    Vec waterCurrent={0,0,0};
    RocketWaterQuery waterQuery=nullptr;
    ~RocketWorld() {
        // Meshes belong to this bridge; Arena must not destroy their shapes.
        if(arena){
            for(auto &entry:platforms)if(entry.second->body)arena->_bulletWorld.removeRigidBody(entry.second->body.get());
            for(auto &mesh:meshes)if(mesh.body)arena->_bulletWorld.removeRigidBody(mesh.body.get());
        }
        arena.reset();
    }
};
// Called only at the verified wheel-impulse construction site in the generated
// backend TU. It uses that substep's real suspension contact and BVH triangle,
// including mixed materials and dynamic layers. The pinned vendor is untouched.
extern "C" float rocket_host_wheel_grip(const void *opaqueWheel) {
    const auto &wheel=*static_cast<const btWheelInfoRL*>(opaqueWheel);
    const auto *body=static_cast<const btCollisionObject*>(wheel.m_raycastInfo.m_groundObject);
    if(!body||body->getUserIndex2()!=HOST_MESH_TAG)return 1.f;
    const auto *mesh=static_cast<const Mesh*>(body->getCollisionShape()->getUserPointer());
    if(!mesh||!mesh->surfaceMode||*mesh->surfaceMode!=ROCKET_SURFACES_NATIVE)return 1.f;
    btVector3 p=body->getWorldTransform().inverse()*wheel.m_raycastInfo.m_contactPointWS;
    MaterialAtPoint query(mesh,p);btVector3 extent(.02f,.02f,.02f);
    mesh->shape->processAllTriangles(&query,p-extent,p+extent);
    return query.grip;
}
extern "C" int rocket_world_set_environment(RocketWorld *w,const RocketEnvironment *environment) {
    if(!w)return 0;
    w->environment={}; // Reject invalid input without retaining an old force.
    if(!environment)return 1;
    if(!finite3(environment->drift)||!finite3(environment->acceleration))return 0;
    for(int i=0;i<3;++i)if(std::fabs(environment->drift[i])>30000.f||std::fabs(environment->acceleration[i])>100000.f)return 0;
    w->environment=*environment;return 1;
}
extern "C" void rocket_world_set_surface_mode(RocketWorld *w,unsigned mode) {
    if(w)w->surfaceMode=mode==ROCKET_SURFACES_NATIVE?mode:ROCKET_SURFACES_CAR;
}
extern "C" int rocket_world_set_speed(RocketWorld *w,unsigned percent) {
    if(!w||!rocket_speed_valid(percent))return 0;
    w->speedPercent=percent;return 1;
}
extern "C" unsigned rocket_world_speed(RocketWorld *w) {return w?w->speedPercent:100;}
extern "C" int rocket_world_set_jump_height(RocketWorld *w,unsigned percent) {
    if(!w||!rocket_jump_valid(percent))return 0;
    w->jumpPercent=percent;return 1;
}
extern "C" unsigned rocket_world_jump_height(RocketWorld *w) {return w?w->jumpPercent:100;}
static thread_local RocketWorld *steppingEnvironment=nullptr;
extern "C" float rocket_host_car_jump_impulse(const void *car) {
    auto *w=steppingEnvironment;
    return w&&w->car==car?rocket_jump_impulse_scale(w->jumpPercent):1.f;
}
extern "C" float rocket_host_car_jump_hold(const void *car) {
    auto *w=steppingEnvironment;
    return w&&w->car==car?rocket_jump_hold_scale(w->jumpPercent):1.f;
}
extern "C" float rocket_host_car_speed(const void *car) {
    auto *w=steppingEnvironment;
    return w&&w->car==car?rocket_speed_multiplier(w->speedPercent):1.f;
}
struct EnvironmentStep {
    RocketWorld *previous;
    explicit EnvironmentStep(RocketWorld *world):previous(steppingEnvironment){steppingEnvironment=world;}
    ~EnvironmentStep(){steppingEnvironment=previous;}
};
static void environmentTick(btDynamicsWorld *world,btScalar dt) {
    auto *w=steppingEnvironment;
    if(!w||world!=&w->arena->_bulletWorld)return;
    // Apply native displacement after tire control, so an idle brake cannot
    // erase wind/current before it reaches the collision solver.
    btVector3 drift=fromHost(w->environment.drift)*UU_TO_BT;
    btVector3 acceleration=fromHost(w->environment.acceleration)*UU_TO_BT;
    if(!drift.isZero()||!acceleration.isZero())
        w->car->_rigidBody.setLinearVelocity(w->car->_rigidBody.getLinearVelocity()+drift+acceleration*dt);
}
namespace {
void removePlatform(RocketWorld *w,Mesh &mesh) {
    if(mesh.body)w->arena->_bulletWorld.removeRigidBody(mesh.body.get());
    mesh.body.reset();mesh.shape.reset();mesh.edges.reset();mesh.triangles.reset();mesh.faces.clear();mesh.sourceTriangles.clear();
}
void clearPlatforms(RocketWorld *w) {
    for(auto &entry:w->platforms)removePlatform(w,*entry.second);
    w->platforms.clear();
}
bool teleportBetween(const Mesh &mesh,const RocketPlatform &platform,const btTransform &target) {
    float distance2=0;
    for(int i=0;i<3;++i){float d=platform.position[i]-mesh.hostPosition[i];distance2+=d*d;}
    return distance2>PLATFORM_TELEPORT_DISTANCE*PLATFORM_TELEPORT_DISTANCE||
           rotationDistance(mesh.targetTransform,target)>PLATFORM_TELEPORT_ANGLE;
}
void zeroPlatformVelocity(Mesh &mesh) {
    mesh.body->setLinearVelocity(btVector3(0,0,0));
    mesh.body->setAngularVelocity(btVector3(0,0,0));
}
void snapPlatform(RocketWorld *w,Mesh &mesh,const btTransform &transform) {
    auto *proxy=mesh.body->getBroadphaseHandle();
    if(proxy)w->arena->_bulletWorld.getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(
        proxy,w->arena->_bulletWorld.getDispatcher());
    mesh.body->setInterpolationWorldTransform(transform);
    mesh.body->setWorldTransform(transform);
    zeroPlatformVelocity(mesh);
    w->arena->_bulletWorld.updateSingleAabb(mesh.body.get());
    mesh.previousTransform=mesh.targetTransform=transform;
    mesh.hasMotion=false;
}
void stepPlatforms(RocketWorld *w,int step) {
    const float dt=ROCKET_TICK_SECONDS;
    for(auto &entry:w->platforms){
        Mesh &mesh=*entry.second;
        if(!mesh.hasMotion){zeroPlatformVelocity(mesh);continue;}
        float fromAmount=float(step)/float(ROCKET_SUBSTEPS);
        float toAmount=float(step+1)/float(ROCKET_SUBSTEPS);
        btTransform from=interpolate(mesh.previousTransform,mesh.targetTransform,fromAmount);
        btTransform to=interpolate(mesh.previousTransform,mesh.targetTransform,toAmount);
        mesh.body->setInterpolationWorldTransform(from);
        mesh.body->setWorldTransform(to);
        mesh.body->setLinearVelocity((to.getOrigin()-from.getOrigin())/dt);
        mesh.body->setAngularVelocity(angularVelocity(from,to,dt));
        w->arena->_bulletWorld.updateSingleAabb(mesh.body.get());
    }
}
void finishPlatformFrame(RocketWorld *w) {
    for(auto &entry:w->platforms){
        Mesh &mesh=*entry.second;
        mesh.previousTransform=mesh.targetTransform;
        mesh.hasMotion=false;
    }
}
}
extern "C" void rocket_to_host(const float rl[3],float host[3],float scale) {
    float x=rl[0],y=rl[1],z=rl[2];host[0]=y*scale;host[1]=z*scale;host[2]=x*scale;
}
extern "C" void rocket_from_host(const float host[3],float rl[3],float scale) {
    float x=host[0],y=host[1],z=host[2];rl[0]=z/scale;rl[1]=x/scale;rl[2]=y/scale;
}
extern "C" const char *rocket_world_error(void) { return error.c_str(); }
extern "C" RocketWorld *rocket_world_create(void) {
    try {
        RocketSim::InitFromMem({},true); // No RL arena assets, dumper, or game process.
        auto w=std::make_unique<RocketWorld>();
        ArenaConfig config;config.useCustomBroadphase=false;
        w->arena.reset(Arena::Create(GameMode::THE_VOID,config,120));
        // Arena installs its global callback at construction. Chain it without
        // changing pinned source; filtering is restricted to our tagged meshes.
        if(gContactAddedCallback!=hostContact)arenaContact=gContactAddedCallback;
        gContactAddedCallback=hostContact;
        w->arena->_bulletWorldParams.collisionDispatcher.setNearCallback(hostNearCallback);
        // RocketSim's contact callback owns worldUserInfo (Arena*).
        w->arena->_bulletWorld.setInternalTickCallback(environmentTick,w->arena.get(),true);
        // Host character mode has no ball. Remove it from contact detection;
        // Arena retains ownership and still performs its harmless bookkeeping.
        w->arena->_bulletWorld.removeRigidBody(&w->arena->ball->_rigidBody);
        w->car=w->arena->AddCar(Team::BLUE,CAR_CONFIG_OCTANE);
        w->dryGravity=w->arena->GetMutatorConfig().gravity;
        return w.release();
    } catch(const std::exception &e) {error=e.what();return nullptr;}
}
extern "C" void rocket_world_destroy(RocketWorld *w) { delete w; }
extern "C" int rocket_world_set_boost_mode(RocketWorld *w,int mode) {
    if(!w || (mode!=ROCKET_BOOST_COIN_ONLY && mode!=ROCKET_BOOST_INFINITE))return 0;
    w->boostMode=mode;return 1;
}
extern "C" void rocket_world_interrupt(RocketWorld *w) { if(w)w->inhibited=7; }
extern "C" void rocket_world_set_water(RocketWorld *w,int present,float level,int metal) {
    if(!w)return;
    w->waterPresent=present&&std::isfinite(level);
    w->waterLevel=w->waterPresent?level:0;
    w->waterMetal=!!metal;
    if(!w->waterPresent)w->waterCurrent={0,0,0};
    w->waterMode=rocket_water_classify(w->waterMode,w->waterPresent,w->waterLevel,
        w->car->GetState().pos.z*ROCKET_HOST_SCALE,w->waterMetal);
    rocket_world_set_metal_water(w,w->waterMode==ROCKET_WATER_METAL);
}
extern "C" void rocket_world_set_water_current(RocketWorld *w,const float velocity[3]) {
    if(!w)return;
    w->waterCurrent=finite3(velocity)?fromHost(velocity):Vec(0,0,0);
    for(int i=0;i<3;i++)w->waterCurrent[i]=std::clamp(w->waterCurrent[i],-2000.f,2000.f);
}
extern "C" void rocket_world_set_water_query(RocketWorld *w,RocketWaterQuery query){if(w)w->waterQuery=query;}
static void sampleWater(RocketWorld *w) {
    const auto p=w->car->_rigidBody.getWorldTransform().getOrigin();
    if(w->waterQuery){
        float level=0;
        int present=w->waterQuery(p.y()*BT_TO_UU*ROCKET_HOST_SCALE,p.x()*BT_TO_UU*ROCKET_HOST_SCALE,&level);
        rocket_world_set_water(w,present,level,w->waterMetal);
    }
    w->waterMode=rocket_water_classify(w->waterMode,w->waterPresent,w->waterLevel,
        p.z()*BT_TO_UU*ROCKET_HOST_SCALE,w->waterMetal);
    // Geometry-driven runtime mode is authoritative at every substep.
    // The standalone Metal API remains usable by native-falling fixtures.
    if(w->waterQuery||w->waterPresent)rocket_world_set_metal_water(w,w->waterMode==ROCKET_WATER_METAL);
}
extern "C" int rocket_world_boost_mode(RocketWorld *w) { return w?w->boostMode:ROCKET_BOOST_COIN_ONLY; }
extern "C" void rocket_world_set_temporary_boost(RocketWorld *w,int active) { if(w)w->temporaryBoost=!!active; }
extern "C" int rocket_world_collect_coin(RocketWorld *w) {
    if(!w||!w->ready)return 0;
    if(w->boostMode==ROCKET_BOOST_COIN_ONLY){
        w->coinBoost=std::min(100.f,w->car->_internalState.boost+5.f);
        w->car->_internalState.boost=w->coinBoost;
    }
    return 1;
}
extern "C" void rocket_world_set_metal_water(RocketWorld *w,int active) {
    if(!w||w->metalWater==!!active)return;
    w->metalWater=!!active;
    auto config=w->arena->GetMutatorConfig();
    config.gravity=w->metalWater?Vec(0,0,0):w->dryGravity;
    w->arena->SetMutatorConfig(config);
}
extern "C" int rocket_world_mesh(RocketWorld *w,int layer,const RocketTriangle *triangles,size_t count) {
    try {
        if(!w||layer<0||layer>1||count>100000||(!triangles&&count)) throw std::runtime_error("Invalid collision input");
        Mesh next;next.surfaceMode=&w->surfaceMode;
        if(count) {
            next.triangles=std::make_unique<btTriangleMesh>();
            for(size_t i=0;i<count;++i) {
                if(!validMaterial(triangles[i].material))throw std::runtime_error("Invalid surface material");
                btVector3 p[3];
                for(int v=0;v<3;++v) {
                    if(!finite3(triangles[i].v[v])) throw std::runtime_error("Nonfinite collision vertex");
                    auto r=fromHost(triangles[i].v[v]);
                    if(r.Length()>1000000.f) throw std::runtime_error("Collision vertex out of range");
                    p[v]=r*UU_TO_BT;
                }
                if((p[1]-p[0]).cross(p[2]-p[0]).length2()>1e-12f){
                    next.triangles->addTriangle(p[0],p[1],p[2],false);
                    next.faces.push_back({p[0],(p[1]-p[0]).cross(p[2]-p[0]).normalized(),triangles[i].material});
                }
            }
            if(next.triangles->getNumTriangles()==0) throw std::runtime_error("Empty collision geometry");
            next.shape=std::make_unique<btBvhTriangleMeshShape>(next.triangles.get(),true);
            next.edges=std::make_unique<btTriangleInfoMap>();
            btGenerateInternalEdgeInfo(next.shape.get(),next.edges.get());
            next.shape->setTriangleInfoMap(next.edges.get());
            next.body=std::make_unique<btRigidBody>(0,nullptr,next.shape.get());
            next.body->setWorldTransform(btTransform::getIdentity());
            next.body->setFriction(RLConst::ARENA_COLLISION_BASE_FRICTION);
            next.body->setRestitution(RLConst::ARENA_COLLISION_BASE_RESTITUTION);
            next.body->setUserPointer(w->arena.get());
            next.body->setUserIndex2(HOST_MESH_TAG);
        }
        auto &old=w->meshes[layer];
        if(old.body) w->arena->_bulletWorld.removeRigidBody(old.body.get());
        // Explicit destruction order: rigid body, shape, backing triangle array.
        old.body.reset();old.shape.reset();old.edges.reset();old.triangles.reset();old=std::move(next);
        if(old.body){old.shape->setUserPointer(&old);w->arena->_bulletWorld.addRigidBody(old.body.get());}
        return 1;
    } catch(const std::exception &e) {error=e.what();return 0;}
}
extern "C" int rocket_world_platforms(RocketWorld *w,const RocketPlatform *platforms,size_t count) {
    try {
        if(!w||count>MAX_HOST_PLATFORMS||(!platforms&&count))
            throw std::runtime_error("Invalid platform snapshot");
        struct Prepared {
            uint64_t id;
            const RocketPlatform *input;
            btTransform target;
            bool discontinuity;
            std::unique_ptr<Mesh> replacement;
        };
        std::vector<Prepared> prepared;
        prepared.reserve(count);
        std::unordered_set<uint64_t> seen;
        seen.reserve(count);
        size_t totalTriangles=0;
        for(size_t i=0;i<count;++i){
            const RocketPlatform &platform=platforms[i];
            if(platform.object_id==0||!seen.insert(platform.object_id).second)
                throw std::runtime_error("Platform IDs must be unique and nonzero");
            if(platform.count>MAX_HOST_TRIANGLES-totalTriangles)
                throw std::runtime_error("Platform snapshot exceeds triangle limit");
            totalTriangles+=platform.count;
            validatePlatformTriangles(platform);
            btTransform target=platformTransform(platform);
            auto old=w->platforms.find(platform.object_id);
            bool discontinuity=old!=w->platforms.end()&&teleportBetween(*old->second,platform,target);
            std::unique_ptr<Mesh> replacement;
            if(old==w->platforms.end()||!samePlatformGeometry(*old->second,platform)){
                replacement=std::make_unique<Mesh>();replacement->surfaceMode=&w->surfaceMode;
                btTransform initial=(old!=w->platforms.end()&&!discontinuity)?old->second->targetTransform:target;
                populateMesh(*replacement,platform,initial,w->arena.get(),true);
                replacement->targetTransform=target;
                replacement->previousTransform=initial;
                replacement->hasMotion=old!=w->platforms.end()&&!discontinuity;
                replacement->hasPose=true;
                for(int axis=0;axis<3;++axis)replacement->hostPosition[axis]=platform.position[axis];
            }
            prepared.push_back({platform.object_id,&platform,target,discontinuity,std::move(replacement)});
        }

        for(auto it=w->platforms.begin();it!=w->platforms.end();){
            if(!seen.count(it->first)){removePlatform(w,*it->second);it=w->platforms.erase(it);}
            else ++it;
        }
        for(auto &update:prepared){
            auto old=w->platforms.find(update.id);
            if(update.replacement){
                if(old!=w->platforms.end()){
                    removePlatform(w,*old->second);
                    w->platforms.erase(old);
                }
                Mesh *mesh=update.replacement.get();
                mesh->shape->setUserPointer(mesh);
                w->arena->_bulletWorld.addRigidBody(mesh->body.get());
                w->platforms.emplace(update.id,std::move(update.replacement));
            } else {
                Mesh &mesh=*old->second;
                if(update.discontinuity)snapPlatform(w,mesh,update.target);
                else {
                    mesh.previousTransform=mesh.targetTransform;
                    mesh.targetTransform=update.target;
                    mesh.hasMotion=true;
                }
                for(int axis=0;axis<3;++axis)mesh.hostPosition[axis]=update.input->position[axis];
            }
        }
        return 1;
    } catch(const std::exception &e) {error=e.what();return 0;}
}
extern "C" int rocket_world_reset(RocketWorld *w,const float *position,const float *velocity,float yaw) {
    if(!w||!finite3(position)||!finite3(velocity)||!std::isfinite(yaw)) {error="Invalid reset state";return 0;}
    for(int i=0;i<3;++i)if(std::fabs(position[i])>1000000||std::fabs(velocity[i])>100000){error="Reset out of range";return 0;}
    try {
    rocket_world_set_water(w,0,0,0);
    w->waterQuery=nullptr;
    rocket_world_set_metal_water(w,0);
    CarState state;
    state.pos=fromHost(position);state.vel=fromHost(velocity);
    state.rotMat=RotMat({std::cos(yaw),std::sin(yaw),0},{-std::sin(yaw),std::cos(yaw),0},{0,0,1});
    if(w->ready)w->coinBoost=w->car->_internalState.boost;
    state.isOnGround=false;state.boost=w->coinBoost;
    // SetState alone leaves btVehicleRL steering/friction/suspension history.
    // Replace the car so a warm-world reset agrees with a new car's trajectory.
    Car *next=w->arena->AddCar(Team::BLUE,CAR_CONFIG_OCTANE);
    next->SetState(state);w->arena->RemoveCar(w->car);w->car=next;w->car->controls={};
    clearPlatforms(w);
    w->ticks=0;w->haveFrame=false;w->inhibited=7;w->ready=true;w->environment={};w->temporaryBoost=false;
    // Discard old warm-start/contact history on teleports and ownership changes.
    auto *proxy=w->car->_rigidBody.getBroadphaseHandle();
    if(proxy) w->arena->_bulletWorld.getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(proxy,w->arena->_bulletWorld.getDispatcher());
    w->arena->_bulletWorld.updateSingleAabb(&w->car->_rigidBody);
    return 1;
    } catch(const std::exception &e) {error=e.what();w->ready=false;return 0;}
}
extern "C" int rocket_world_recover(RocketWorld *w,const RocketSnapshot *pose) {
    try {
    if(!w||!w->ready||!pose||!finite3(pose->position))return 0;
    for(int k=0;k<3;k++)if(std::fabs(pose->position[k])>1000000.f)return 0;
    for(int i=0;i<3;i++)for(int j=i;j<3;j++){
        float dot=0;
        for(int k=0;k<3;k++)dot+=pose->basis[i*3+k]*pose->basis[j*3+k];
        if(!std::isfinite(dot)||std::fabs(dot-(i==j?1.f:0.f))>.015f)return 0;
    }
    auto state=w->car->GetState();
    state.pos=fromHost(pose->position);state.vel={0,0,0};state.angVel={0,0,0};
    state.rotMat=RotMat(fromHost(pose->basis)*ROCKET_HOST_SCALE,
        fromHost(pose->basis+3)*ROCKET_HOST_SCALE,fromHost(pose->basis+6)*ROCKET_HOST_SCALE);
    if(state.rotMat.forward.Cross(state.rotMat.right).Dot(state.rotMat.up)<.98f)return 0;
    state.isOnGround=false;
    for(int i=0;i<4;i++)state.wheelsWithContact[i]=false;
    state.worldContact={};
    // Replacement drops suspension/manifold history without resetting fuel,
    // jump/flip expenditure, timers, or the monotonically increasing tick count.
    Car *next=w->arena->AddCar(Team::BLUE,CAR_CONFIG_OCTANE);
    next->SetState(state);w->arena->RemoveCar(w->car);w->car=next;
    w->car->controls={};w->inhibited=7;
    w->arena->_bulletWorld.updateSingleAabb(&w->car->_rigidBody);
    return 1;
    } catch(const std::exception &e) {error=e.what();return 0;}
}
// Swimming up also releases wall/ceiling adhesion. Contact normals come
// from the current suspension rays and accepted chassis manifolds, never a
// guessed nearby surface. Cap separation speed instead of accumulating jump
// impulses; ordinary swimming, dry jumps and Metal retain their own paths.
static void waterWallEscape(RocketWorld *w) {
    auto &body=w->car->_rigidBody;
    std::array<btVector3,12> normals;size_t count=0;
    auto add=[&](btVector3 normal) {
        if(normal.length2()<.5f)return;
        normal.normalize();
        if(normal.z()>=.5f)return; // leave floors and traversable slopes alone
        for(size_t i=0;i<count;++i)if(normals[i].dot(normal)>.99f)return;
        if(count<normals.size())normals[count++]=normal;
    };
    for(int i=0;i<4;++i) {
        const auto &ray=w->car->_bulletVehicle.m_wheelInfo[i].m_raycastInfo;
        if(ray.m_isInContact)add(ray.m_contactNormalWS);
    }
    auto *dispatcher=w->arena->_bulletWorld.getDispatcher();
    for(int i=0;i<dispatcher->getNumManifolds();++i) {
        auto *manifold=dispatcher->getManifoldByIndexInternal(i);
        bool first=manifold->getBody0()==&body;
        if(!first&&manifold->getBody1()!=&body)continue;
        for(int j=0;j<manifold->getNumContacts();++j) {
            const auto &point=manifold->getContactPoint(j);
            if(point.m_userPersistentData==&rejectedHostContact||point.getDistance()>.02f)continue;
            add(point.m_normalWorldOnB*(first?1.f:-1.f));
        }
    }
    if(!count)return;
    const float scale=rocket_speed_multiplier(w->speedPercent)*UU_TO_BT;
    btVector3 velocity=body.getLinearVelocity(),before=velocity;
    // Two passes cover ordinary corners without unbounded iterative impulses.
    for(int pass=0;pass<2;++pass)for(size_t i=0;i<count;++i) {
        float missing=220.f*scale-velocity.dot(normals[i]);
        if(missing>0)velocity+=normals[i]*std::min(missing,180.f*scale);
    }
    btVector3 delta=velocity-before;const float limit=440.f*scale;
    if(delta.length2()>limit*limit)delta=delta.normalized()*limit;
    body.setLinearVelocity(before+delta);
}
extern "C" int rocket_world_frame(RocketWorld *w,uint64_t frame,const RocketInput *in,int paused,int blocked) {
    if(!w||!w->ready||!in) {error="World not ready or missing input";return -1;}
    unsigned held=(in->jump?1u:0u)|(in->boost?2u:0u)|(in->powerslide?4u:0u);
    // Observe interruption even if called twice in the same host frame.
    if(paused||blocked) w->inhibited=7;
    if(w->haveFrame&&frame<=w->frame) return 0;
    w->frame=frame;w->haveFrame=true;
    if(paused) return 0;
    CarControls c;
    if(!blocked) {
        w->inhibited &= held;held &= ~w->inhibited;
        c.throttle=axis(in->throttle);c.steer=axis(in->steer);
        c.pitch=axis(in->pitch);c.yaw=axis(in->yaw);c.roll=axis(in->roll);
        c.jump=(held&1)!=0;c.boost=(held&2)!=0;c.handbrake=(held&4)!=0;
    }
    w->car->controls=c;
    // Never inherit a backend recharge mutator, including after a mode change.
    w->arena->_mutatorConfig.rechargeBoostEnabled=false;
    for(int step=0;step<ROCKET_SUBSTEPS;++step) {
        stepPlatforms(w,step);
        auto &body=w->car->_rigidBody;
        const float height=body.getWorldTransform().getOrigin().z()*BT_TO_UU*ROCKET_HOST_SCALE;
        sampleWater(w);
        w->car->controls=c;
        if(w->metalWater) {
            auto velocity=w->car->_rigidBody.getLinearVelocity();
            velocity.setZ(rocket_metal_water_velocity(velocity.z()*BT_TO_UU*ROCKET_HOST_SCALE)
                /ROCKET_HOST_SCALE*UU_TO_BT);
            w->car->_rigidBody.setLinearVelocity(velocity);
        }

        if(w->waterMode==ROCKET_WATER_JET) {
            // Jump becomes held swim-up; native car jump/flip impulses would
            // fight 3D swimming. Pitch/yaw/air-roll retain the car mapping.
            w->car->controls.jump=false;
            if(c.jump)waterWallEscape(w);
            w->car->_internalState.isFlipping=false;
            const float depth=w->waterLevel-height;
            const float buoyancy=std::clamp((depth-110.f)*1.5f,-75.f,75.f);
            btVector3 acceleration=w->car->GetForwardDir()*(c.throttle*420.f*rocket_speed_multiplier(w->speedPercent));
            acceleration.setZ(acceleration.z()+buoyancy+(c.jump?650.f*rocket_speed_multiplier(w->speedPercent):0.f));
            // Cancel only this body's gravity. Never rewrite the arena mutator:
            // Metal's separate sinking adapter must keep owning heavy motion.
            body.applyCentralForce((acceleration*UU_TO_BT-body.getGravity())/body.getInvMass());
            btVector3 flow=w->waterCurrent*UU_TO_BT;
            body.setLinearVelocity(flow+(body.getLinearVelocity()-flow)*std::exp(-.9f*ROCKET_TICK_SECONDS));
            body.setAngularVelocity(body.getAngularVelocity()*std::exp(-.8f*ROCKET_TICK_SECONDS));
        }
        // A temporary allowance, not a coin-balance refill. Write only boost:
        // SetState would reset physics caches and tick history every substep.
        float finiteBoost=w->car->_internalState.boost;
        bool unlimited=w->boostMode==ROCKET_BOOST_INFINITE||w->temporaryBoost||w->waterMode==ROCKET_WATER_JET;
        if(unlimited)w->car->_internalState.boost=100.f;
        {EnvironmentStep scope(w);w->arena->Step();}
        btVector3 drift=fromHost(w->environment.drift)*UU_TO_BT;
        if(!drift.isZero())w->car->_rigidBody.setLinearVelocity(w->car->_rigidBody.getLinearVelocity()-drift);
        if(unlimited)w->car->_internalState.boost=finiteBoost;
    }
    finishPlatformFrame(w);
    sampleWater(w);
    w->ticks+=ROCKET_SUBSTEPS;
    return ROCKET_SUBSTEPS;
}
extern "C" int rocket_world_bump(RocketWorld *w,const float delta[3]) {
    if(!w||!w->ready||!delta||!finite3(delta))return 0;
    float magnitude=0;for(int k=0;k<3;k++)magnitude+=delta[k]*delta[k];
    if(magnitude>2400.1f*2400.1f)return 0;
    auto &body=w->car->_rigidBody;
    Vec velocity=Vec(body.getLinearVelocity())+fromHost(delta)*UU_TO_BT;
    if(velocity.Length()>6000.f/ROCKET_HOST_SCALE*UU_TO_BT)return 0;
    body.setLinearVelocity(velocity);body.activate(true);
    return 1;
}
extern "C" int rocket_world_snapshot(RocketWorld *w,RocketSnapshot *out) {
    if(!w||!w->ready||!out)return 0;
    auto state=w->car->GetState();RocketSnapshot s={};
    toHost(state.pos,s.position);toHost(state.vel,s.velocity);toHost(state.angVel,s.angular_velocity,1);
    toHost(state.rotMat.forward,s.basis,1);toHost(state.rotMat.right,s.basis+3,1);toHost(state.rotMat.up,s.basis+6,1);
    if(!finite3(s.position)||!finite3(s.velocity)||!finite3(s.angular_velocity)||!finite3(s.basis)||!finite3(s.basis+3)||!finite3(s.basis+6)) {error="Nonfinite physics result";return 0;}
    s.boost=state.boost;s.jump_time=state.jumpTime;s.flip_time=state.flipTime;s.air_time=state.airTime;
    s.ticks=w->ticks;s.grounded=state.isOnGround;s.jumped=state.hasJumped;s.double_jumped=state.hasDoubleJumped;
    s.water_mode=w->waterMode;
    s.flipped=state.hasFlipped;s.flipping=state.isFlipping;s.boosting=state.isBoosting;
    for(int i=0;i<4;++i) {
        s.wheel_contacts[i]=state.wheelsWithContact[i];
        const auto &wheel=w->car->_bulletVehicle.m_wheelInfo[i];
        // Read suspension, never call updateWheelTransform (which clears contact).
        Vec local=Vec(wheel.m_chassisConnectionPointCS)*BT_TO_UU;
        local+=Vec(wheel.m_wheelDirectionCS)*(wheel.m_raycastInfo.m_suspensionLength*BT_TO_UU);
        Vec pos=state.pos+state.rotMat.forward*local.x+state.rotMat.right*local.y+state.rotMat.up*local.z;
        toHost(pos,s.wheel_position[i]);s.wheel_steer[i]=wheel.m_steerAngle;
        s.wheel_radius[i]=wheel.m_wheelsRadius*BT_TO_UU*ROCKET_HOST_SCALE;
    }
    *out=s;return 1;
}
