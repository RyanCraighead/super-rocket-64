#include "spiderman_combat_host.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#include <wchar.h>
#endif
#include "sm64.h"
#include "behavior_data.h"
#include "object_constants.h"
#include "object_fields.h"
#include "object_list_processor.h"
#include "interaction.h"
#include "spiderman_world.h"
#include "pc/spiderman_runtime.h"
#include "../../codex/spiderman/controller/host_collision.h"
#include "../../codex/spiderman/combat/targeting_n64.h"
#include "../../codex/spiderman/combat/manipob_n64.h"
#include "../../codex/spiderman/movement/locomotion_n64.h"

#define BANK_BYTES 97077u
#define HOST_CAPACITY SMN64_COMBAT_ACTOR_CAPACITY
#define INCOMING_CAPACITY 16u
#define THROW_CAPACITY 8u
#define THROW_PATH_CAPACITY 2048u
/* Object.unused1 is reset by create_object and level spawn initialization and
 * has no other engine consumer. Reserving it prevents pool-slot ABA: native
 * allocation clears it even when the same behavior/sync ID/address is reused.
 * Tokens are published transactionally and never recycled during this process. */
typedef enum HostKind { HOST_GOOMBA=1,HOST_BOBOMB,HOST_SPINDRIFT,HOST_POKEY,HOST_BOX,HOST_EXPLOSION } HostKind;
typedef struct HostActor {
    struct Object *object;
    const BehaviorScript *behavior;
    uint32_t id,old_token,sync_id;
    HostKind kind;
    uint16_t type;
    int32_t position[3],lower,upper,radius,cached_distance;
    uint32_t old_status,status;
    int32_t action,intangible;
    uint32_t held;
    uint8_t write_status,write_capture,write_destroy,dome_hit,owned;
} HostActor;
typedef struct HostManip {
    uint8_t active,alpha;
    float origin_y_offset;
    struct Object *object;
    SmN64ManipOb source;
} HostManip;
typedef struct HostThrow {
    uint8_t active;
    float origin_y_offset;
    struct Object *object;
    SmN64Throwable source;
    HostManip manip; /* present only for actual native pickup ownership */
    int32_t path[THROW_PATH_CAPACITY][3];
} HostThrow;
typedef struct HostTrails {
    SmN64Trail trail[SPIDERMAN_HOST_TRAIL_CAPACITY];
    uint8_t live[SPIDERMAN_HOST_TRAIL_CAPACITY];
    uint8_t order[SPIDERMAN_HOST_TRAIL_CAPACITY];
    uint32_t count,active[2]; /* active contains slot+1, zero genuinely absent */
    uint32_t last_retain,last_effect;
    uint8_t retained,effected;
} HostTrails;
typedef struct HostState {
    int initialized,pending;
    struct MarioState *mario;
    const SmN64ClimbState *initial;
    SmN64CombatOwnerFrame frame;
    int bound;
    size_t incoming_count,incoming_map[INCOMING_CAPACITY];
    HostActor actors[HOST_CAPACITY];size_t count;
    /* Persist only across committed native lifetime tokens. An aborted pulse
     * cannot clear or mark recipients; recycled object pool slots never inherit
     * their predecessor's source once-hit bit. */
    uint32_t dome_mark_ids[OBJECT_POOL_CAPACITY];size_t dome_mark_count;
    uint8_t dome_reset_pending;
    SmN64CombatBank bank;
    SmN64ThrowableTypes throw_types;
    HostThrow thrown[THROW_CAPACITY],pending_thrown[THROW_CAPACITY];
    HostManip carried,pending_carried;
    int carry_enabled;
    SpidermanCarrySound sounds[16],pending_sounds[16];
    size_t sound_count,pending_sound_count;
    HostTrails trails,pending_trails;
    int trails_enabled;uint32_t trail_color;
    SmN64CombatOwnerServices services;
    SmN64CombatHit hit,last_hit;
    int have_hit,accepted,last_have,last_accepted;
} HostState;
typedef struct Incoming {
    struct Object *object;
    const BehaviorScript *behavior;
    uint32_t token,sync_id;
    SmN64CombatHit hit;
} Incoming;
static Incoming sIncoming[INCOMING_CAPACITY];
static size_t sIncomingCount;
static HostState sHost;
static uint32_t sNextToken=1;
static char sError[160];
static int carry_release(void *,uint32_t,const int32_t[3],int);
static int fail(const char *why){snprintf(sError,sizeof sError,"%s",why);return -2;}
const char *spiderman_combat_host_error(void){return sError;}

/* Portable SHA-256 (FIPS180-4), same algorithm used by original mesh intake. */
static uint32_t rotr(uint32_t v,unsigned n){return(v>>n)|(v<<(32-n));}
static void sha256(const unsigned char *data,size_t size,char out[65]){
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    size_t blocks=(size+9+63)/64;
    for(size_t n=0;n<blocks;n++){
        unsigned char block[64];uint32_t w[64];
        for(size_t j=0;j<64;j++){size_t p=n*64+j;block[j]=p<size?data[p]:p==size?128:0;if(p>=blocks*64-8)block[j]=(unsigned char)(((uint64_t)size*8)>>((blocks*64-1-p)*8));}
        for(unsigned i=0;i<16;i++)w[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];
        for(unsigned i=16;i<64;i++){uint32_t a=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3),b=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+a+w[i-7]+b;}
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(unsigned i=0;i<64;i++){uint32_t t1=v+(rotr(e,6)^rotr(e,11)^rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i],t2=(rotr(a,2)^rotr(a,13)^rotr(a,22))+((a&b)^(a&c)^(b&c));v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    for(unsigned i=0;i<8;i++)snprintf(out+i*8,9,"%08x",h[i]);
}
#ifdef _WIN32
/* Windows has no O_NOFOLLOW/O_CLOEXEC/O_NONBLOCK equivalents for open().
 * Acquire the final path component itself, reject reparse/device/directory
 * handles before reading, then transfer an exact disk handle to binary CRT I/O.
 * Security attributes NULL and _O_NOINHERIT both prohibit child inheritance.
 * Direct UNC/device namespaces, remote drive roots and alternate streams are
 * rejected before CreateFileW. No file creation, repair or fallback is used.
 * Parent-directory resolution has the same scope as POSIX O_NOFOLLOW, which
 * likewise protects the final component rather than every directory ancestor. */
static int windows_asset_open(const char *path){
    wchar_t input[4096],wide[4096],root[4];BY_HANDLE_FILE_INFORMATION info;HANDLE handle;int fd;DWORD length;UINT drive;
    if(!path||!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,input,4096))return -1;
    length=GetFullPathNameW(input,4096,wide,NULL);
    if(!length||length>=4096||!((wide[0]>=L'A'&&wide[0]<=L'Z')||(wide[0]>=L'a'&&wide[0]<=L'z'))||wide[1]!=L':'||(wide[2]!=L'\\'&&wide[2]!=L'/'))return -1;
    for(DWORD i=2;i<length;i++)if(wide[i]==L':')return -1;
    root[0]=wide[0];root[1]=L':';root[2]=L'\\';root[3]=0;drive=GetDriveTypeW(root);
    if(drive!=DRIVE_FIXED&&drive!=DRIVE_REMOVABLE&&drive!=DRIVE_CDROM&&drive!=DRIVE_RAMDISK)return -1;
    /* DOS device aliases are reserved in every path component, including when
     * suffixed with an extension or trailing spaces. Never open one first. */
    for(DWORD start=3;start<length;){
        DWORD end=start,base;wchar_t name[9];while(end<length&&wide[end]!=L'\\'&&wide[end]!=L'/')end++;
        base=start;while(base<end&&wide[base]!=L'.')base++;while(base>start&&wide[base-1]==L' ')base--;
        if(base-start<9){DWORD n=base-start;for(DWORD i=0;i<n;i++){wchar_t c=wide[start+i];name[i]=c>=L'a'&&c<=L'z'?c-(L'a'-L'A'):c;}name[n]=0;
            if(!wcscmp(name,L"CON")||!wcscmp(name,L"PRN")||!wcscmp(name,L"AUX")||!wcscmp(name,L"NUL")||!wcscmp(name,L"CLOCK$")||!wcscmp(name,L"CONIN$")||!wcscmp(name,L"CONOUT$")||
               (n==4&&(!wcsncmp(name,L"COM",3)||!wcsncmp(name,L"LPT",3))&&((name[3]>=L'0'&&name[3]<=L'9')||name[3]==0xb9||name[3]==0xb2||name[3]==0xb3)))return -1;
        }
        start=end+1;
    }
    handle=CreateFileW(wide,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,NULL);
    if(handle==INVALID_HANDLE_VALUE)return -1;
    if(GetFileType(handle)!=FILE_TYPE_DISK||!GetFileInformationByHandle(handle,&info)||
       (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))){CloseHandle(handle);return -1;}
    fd=_open_osfhandle((intptr_t)handle,_O_RDONLY|_O_BINARY|_O_NOINHERIT);
    if(fd<0)CloseHandle(handle);
    return fd;
}
#endif
static int bank_load(const char *directory){
    char path[4096],hash[65];struct stat before,after;int fd=-1,ok=0;size_t n=0;
    unsigned char *bytes=NULL;SmN64CombatBank *bank=NULL;
    if(!directory||!directory[0]||strlen(directory)>sizeof(path)-12)return fail("Invalid combat asset directory");
    if(snprintf(path,sizeof path,"%s/combat.bin",directory)>=(int)sizeof(path))return fail("Combat asset path too long");
#ifdef _WIN32
    fd=windows_asset_open(path);
#else
    fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);
#endif
    if(fd<0)return fail("Cannot open verified combat.bin sidecar");
    if(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size!=BANK_BYTES){fail("Combat sidecar must be exact-size regular file");goto done;}
    bytes=malloc(BANK_BYTES);bank=malloc(sizeof *bank);if(!bytes||!bank){fail("Combat bank allocation failed");goto done;}
    while(n<BANK_BYTES){ssize_t got=read(fd,bytes+n,BANK_BYTES-n);if(got<0&&errno==EINTR)continue;if(got<=0){fail("Combat sidecar read failed/shortened");goto done;}n+=(size_t)got;}
    {unsigned char extra;ssize_t got;do{got=read(fd,&extra,1);}while(got<0&&errno==EINTR);if(got!=0){fail("Combat sidecar grew/read failed");goto done;}}
    if(fstat(fd,&after)||after.st_size!=before.st_size||after.st_mtime!=before.st_mtime||after.st_ctime!=before.st_ctime){fail("Combat sidecar changed while reading");goto done;}
    sha256(bytes,n,hash);
    if(strcmp(hash,"eee23bdd1dd570faa239eeb3aa515c1d3cea780b6354b6665ab38304cf0598df")){fail("Combat sidecar SHA-256 mismatch");goto done;}
    if(smn64_combat_bank_load(bank,bytes,n)!=1){fail("Verified combat bank decoder rejected payload");goto done;}
    sHost.bank=*bank;ok=1;
done: if(fd>=0)close(fd);free(bytes);free(bank);return ok?1:-2;
}
static int throw_types_load(const char *dir){
    char path[4096],hash[65];unsigned char *bytes;struct stat info;size_t offset=0;
    if(snprintf(path,sizeof path,"%s/boot.bin",dir)>=(int)sizeof path)return -2;
    int fd;
#ifdef _WIN32
    fd=windows_asset_open(path);
#else
    fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);
#endif
    if(fd<0)return -2;
    if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_size!=995056){close(fd);return -2;}
    bytes=malloc(995056);if(!bytes){close(fd);return -2;}
    while(offset<995056){ssize_t n=read(fd,bytes+offset,995056-offset);if(n<0&&errno==EINTR)continue;if(n<=0){free(bytes);close(fd);return -2;}offset+=(size_t)n;}
    unsigned char extra;ssize_t n;do{n=read(fd,&extra,1);}while(n<0&&errno==EINTR);close(fd);
    if(n!=0){free(bytes);return -2;}sha256(bytes,995056,hash);
    int rc=-2;if(!strcmp(hash,"1d3ed3384f45ada2ebf6cb0666ddc7fec4c4ffdb6fdb993b8d566aa3cd4f3867"))rc=smn64_throwable_types_load(&sHost.throw_types,bytes,995056);free(bytes);return rc;
}
static int fixed(double v,int32_t *out){v=round(v*4096.0);if(!isfinite(v)||v<INT32_MIN||v>INT32_MAX)return 0;*out=(int32_t)v;return 1;}
static int32_t source_distance(const int32_t a[3],const int32_t b[3]){
    uint32_t sq=0;for(unsigned k=0;k<3;k++){uint32_t u=(uint32_t)a[k]-(uint32_t)b[k];int32_t v=(int32_t)(u>>12);if(u&0x80000000u)v-=1048576;sq+=(uint32_t)v*(uint32_t)v;}return(int32_t)sqrtf((float)sq);
}
static SmN64ClimbState *actor(void){return sHost.pending&&sHost.bound?sHost.frame.actor:NULL;}
static HostKind classify(const struct Object *o){
    if(o->behavior==bhvGoomba)return HOST_GOOMBA;
    if(o->behavior==bhvBobomb)return HOST_BOBOMB;
    if(o->behavior==bhvSpindrift)return HOST_SPINDRIFT;
    if(o->behavior==bhvPokeyBodyPart)return HOST_POKEY;
    if(o->behavior==bhvBreakableBoxSmall)return HOST_BOX;
    if(o->behavior==bhvExplosion)return HOST_EXPLOSION;
    return 0;
}
int spiderman_combat_host_owns_interaction(const struct Object *o){
    return o&&classify(o)&&(o->activeFlags&ACTIVE_FLAG_ACTIVE)&&
        !(o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM));
}
static int native_generation_live(const struct Object *o,uint32_t id){return o&&(o->activeFlags&ACTIVE_FLAG_ACTIVE)&&o->behavior==bhvBreakableBoxSmall&&o->unused1==id;}
static int native_owned(const struct Object *o){
    if(!o||!(o->activeFlags&ACTIVE_FLAG_ACTIVE))return 0;
    if(sHost.carried.active&&sHost.carried.object==o&&sHost.carried.source.motion.id==o->unused1&&o->behavior==bhvBreakableBoxSmall)return 1;
    for(size_t i=0;i<THROW_CAPACITY;i++)if(sHost.thrown[i].active&&sHost.thrown[i].object==o&&sHost.thrown[i].source.id==o->unused1&&o->behavior==bhvBreakableBoxSmall)return 1;
    return 0;
}
static int live(const HostActor *a){
    const struct Object *o=a->object;
    return o&&o->behavior==a->behavior&&o->unused1==a->old_token&&o->oSyncID==a->sync_id&&
        (o->activeFlags&ACTIVE_FLAG_ACTIVE)&&(a->owned||!(o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM)));
}
static HostActor *lookup(uint32_t id){if(!sHost.pending||!id)return NULL;for(size_t i=0;i<sHost.count;i++)if(sHost.actors[i].id==id&&live(&sHost.actors[i]))return&sHost.actors[i];return NULL;}
static int eligible(const HostActor *a){return a&&a->kind!=HOST_EXPLOSION&&a->intangible==0&&a->held==HELD_FREE&&a->action<100&&!(a->kind==HOST_BOBOMB&&a->action==BOBOMB_ACT_EXPLODE)&&!(a->status&INT_STATUS_INTERACTED);}
int spiderman_combat_host_dome_actor_at(uint32_t list,size_t index,SmN64DomeActor *out){
    if(!sHost.pending||!out||list>1)return -2;
    for(size_t i=0,at=0;i<sHost.count;i++){
        HostActor *a=&sHost.actors[i];
        if(a->kind==HOST_EXPLOSION||(uint32_t)(a->kind==HOST_BOX)!=list)continue;
        if(at++!=index)continue;
        if(!live(a))return -2;
        memset(out,0,sizeof *out);out->id=a->id;out->type=a->type;
        out->flags=(eligible(a)?0x10u:0u)|(a->dome_hit?0x100u:0u);
        memcpy(out->position,a->position,sizeof out->position);return 1;
    }
    return 0;
}
int spiderman_combat_host_dome_mark_hit(uint32_t id,uint8_t set){
    HostActor *a;if(!sHost.pending||set>1)return -2;
    a=lookup(id);if(!a||a->kind==HOST_EXPLOSION)return -2;
    a->dome_hit=set;return 1;
}
int spiderman_combat_host_dome_reset_hits(void){
    if(!sHost.pending)return -2;
    sHost.dome_reset_pending=1;
    for(size_t i=0;i<sHost.count;i++)sHost.actors[i].dome_hit=0;
    return 1;
}
static int line_clear(void *ctx,const int32_t from[3],const int32_t to[3]){(void)ctx;SpidermanWorldHit hit;int rc=spiderman_world_trace_ex(from,to,1,&hit);
    /* -4 is a real nearest dynamic-triangle intersection. Visibility needs
     * only obstruction, not the moving-surface attachment/velocity contract.
     * It therefore blocks sight/melee rather than failing the player tick. */
    return rc==-4?0:rc==1?!hit.present:rc;}
static int world_trace(void *ctx,const int32_t from[3],const int32_t to[3]){int rc=line_clear(ctx,from,to);return rc<0?rc:!rc;}
static int sweep(void *ctx,uint32_t id,const int32_t from[3],const int32_t to[3],int32_t radius,uint8_t *part,int32_t contact[3]){
    (void)ctx;HostActor *a=lookup(id);int rc;int32_t p[3];if(!sHost.pending)return -2;if(!a||a->kind==HOST_EXPLOSION)return 0;
    rc=smn64_host_sweep_cylinder(from,to,radius,a->position[0],a->position[2],a->lower,a->upper,a->radius,p);if(rc<=0)return rc;
    rc=line_clear(NULL,from,p);if(rc<=0)return rc;
    memcpy(contact,p,sizeof p);*part=0;return 1;
}
static int body_sweep(void *ctx,const int32_t from[3],const int32_t to[3],int32_t radius,uint32_t skip,uint32_t *id,uint16_t *type,int32_t contact[3]){
    if(!sHost.pending)return -2;
    for(size_t i=0;i<sHost.count;i++){HostActor *a=&sHost.actors[i];uint8_t part;int rc;if(a->id==skip||!eligible(a)||!live(a))continue;rc=sweep(ctx,a->id,from,to,radius,&part,contact);if(rc<0)return rc;if(rc){*id=a->id;*type=a->type;return 1;}}
    return 0;
}
/* Original49B40 endpoint normalization and center-projection arithmetic.
 * Deliberate binary32 rounding at every source operation, no reassociation. */
static float impact_projection(const int32_t from[3],const int32_t to[3],const int32_t center[3]){
    float a[3],d[3],c[3];
    for(unsigned k=0;k<3;k++){
        volatile float af=(float)from[k],bf=(float)to[k],cf=(float)center[k];
        volatile float aw=af*(1.0f/4096.0f),bw=bf*(1.0f/4096.0f),cw=cf*(1.0f/4096.0f);
        volatile float delta=bw-aw,offset=cw-aw;a[k]=aw;d[k]=delta;c[k]=offset;
    }
    (void)a;
    volatile float xx=d[0]*d[0],yy=d[1]*d[1],zz=d[2]*d[2],xy=xx+yy,sum=xy+zz;
    volatile float length=sqrtf(sum);
    if(length!=0){volatile float inverse=1.0f/length;for(unsigned k=0;k<3;k++){volatile float normalized=d[k]*inverse;d[k]=normalized;}}
    volatile float x=d[0]*c[0],y=d[1]*c[1],z=d[2]*c[2],xy_dot=x+y,result=xy_dot+z;
    return result;
}
int spiderman_combat_host_impact_sweep(const int32_t from[3],const int32_t to[3],uint32_t *id){
    float nearest=2147483648.0f;uint32_t selected=0;
    if(!sHost.pending||!from||!to||!id)return -2;
    for(size_t i=0;i<sHost.count;i++){
        HostActor *a=&sHost.actors[i];int32_t contact[3];
        if(!live(a))return -2;
        /* Actual native collision availability, not mutable damage acceptance.
         * No status/action/health predicate is consulted here. */
        if(a->kind==HOST_EXPLOSION||a->intangible!=0||a->held!=HELD_FREE)continue;
        int rc=smn64_host_sweep_cylinder(from,to,4096,a->position[0],a->position[2],a->lower,a->upper,a->radius,contact);
        if(rc<0)return rc;
        if(!rc)continue;
        float projection=impact_projection(from,to,a->position);
        if(projection>nearest)continue;
        nearest=projection;selected=a->id;
    }
    *id=selected;return selected?1:0;
}
static int root_sweep(void *ctx,const int32_t from[3],const int32_t to[3],int32_t radius){uint32_t id;uint16_t type;int32_t p[3];return body_sweep(ctx,from,to,radius,0,&id,&type,p);}
static int entry_record(HostActor *a,SmN64EntryActor *out){
    if(!a||!live(a)||a->kind==HOST_EXPLOSION)return 0;
    memset(out,0,sizeof *out);out->id=a->id;out->type=sHost.carry_enabled&&a->kind==HOST_BOX?0x191:a->type;out->field_dc=(uint16_t)eligible(a);out->flags_4a=eligible(a)?0x12:0x40;memcpy(out->position,a->position,sizeof out->position);out->cached_distance=a->cached_distance;return 1;
}
static int actor_at(void *ctx,uint32_t kind,size_t index,SmN64EntryActor *out){
    (void)ctx;if(!sHost.pending||!out||kind>1)return -2;
    if(kind==1&&!sHost.carry_enabled)return 0;
    for(size_t i=0,at=0;i<sHost.count;i++){
        if(sHost.actors[i].kind==HOST_EXPLOSION)continue;
        if(sHost.carry_enabled&&((sHost.actors[i].kind==HOST_BOX)!=(kind==1)))continue;
        if(at++!=index)continue;
        if(!live(&sHost.actors[i]))return -2;
        return entry_record(&sHost.actors[i],out);
    }
    return 0;
}
static int actor_by_id(void *ctx,uint32_t id,SmN64EntryActor *out){(void)ctx;return entry_record(lookup(id),out);}
static int pickup_trace(void *ctx,const int32_t from[3],const int32_t to[3],SmN64PickupTrace *out){
    uint32_t id;uint16_t type;int32_t p[3];int rc=body_sweep(ctx,from,to,0,0,&id,&type,p);if(rc<0)return rc;out->hit=(uint32_t)rc;out->distance=rc?source_distance(from,p):0;return 1;
}
static int pickup_eligible(void *ctx,uint32_t id){(void)ctx;HostActor *a=lookup(id);return sHost.carry_enabled&&a&&a->kind==HOST_BOX&&eligible(a)&&!sHost.pending_carried.active&&!a->write_capture&&!a->write_destroy;}
static int interactable(void *ctx,uint32_t *id){(void)ctx;if(!id||!sHost.pending)return -2;*id=0;return 0;}
static int actors(void *ctx,SmN64CombatActor *out,size_t cap,size_t *count){
    (void)ctx;if(!sHost.pending||!out||!count)return -2;size_t n=0;
    for(size_t i=0;i<sHost.count;i++)if(sHost.actors[i].kind!=HOST_EXPLOSION)n++;
    if(cap<n)return -2;
    n=0;
    for(size_t i=0;i<sHost.count;i++){HostActor *a=&sHost.actors[i];if(a->kind==HOST_EXPLOSION)continue;out[n].id=a->id;out[n].type=a->type;out[n].flags=live(a)&&eligible(a)?2:0;out[n].distance=a->cached_distance;n++;}*count=n;return 1;
}
int spiderman_combat_host_damage(const SmN64CombatHit *hit){
    HostActor *a;if(!sHost.pending||!hit)return -2;a=lookup(hit->actor);sHost.hit=*hit;sHost.have_hit=1;sHost.accepted=0;
    if(!eligible(a)||!hit->damage)return 0;
    /* Zero is source bone melee;2 aerial;6 impact;21/22 dome;23 throwable.
     * Actor messages5/6 are a distinct interface, never damage kinds. */
    if(hit->kind!=0&&hit->kind!=2&&hit->kind!=6&&hit->kind!=21&&hit->kind!=22&&hit->kind!=23)return 0;
    unsigned attack=hit->kind==2?ATTACK_FROM_ABOVE:hit->kind==0?ATTACK_PUNCH:ATTACK_FAST_ATTACK;
    a->status=INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|attack;
    if(a->kind==HOST_BOBOMB)a->status|=INT_STATUS_TOUCHED_BOB_OMB;
    if(a->kind==HOST_BOX)a->status=INT_STATUS_INTERACTED|INT_STATUS_WAS_ATTACKED|INT_STATUS_STOP_RIDING|ATTACK_KICK_OR_TRIP;
    a->write_status=1;
    /* Native huge-Goomba mailbox reacts to a weak strike but rejects damage. */
    sHost.accepted=!(a->kind==HOST_GOOMBA&&a->object->oGoombaSize==GOOMBA_SIZE_HUGE&&attack!=ATTACK_FROM_ABOVE);
    return sHost.accepted;
}
static int apply(void *ctx,const SmN64CombatHit *hit){(void)ctx;return spiderman_combat_host_damage(hit);}
int spiderman_combat_host_web_message(uint32_t id,uint32_t message){
    if(!sHost.pending)return -2;
    if(message!=5&&message!=6)return -1;
    /* None of these original SM64 scripts implement trap-sheet ownership,
     * restraint release or an over-shoulder yank path. Explicit rejection. */
    (void)lookup(id);return 0;
}
static int target_record(void *ctx,size_t index,SmN64TargetActor *out){
    (void)ctx;if(!sHost.pending||!out)return -2;HostActor *a=NULL;
    for(size_t i=0,at=0;i<sHost.count;i++){if(sHost.actors[i].kind==HOST_EXPLOSION||(sHost.carry_enabled&&sHost.actors[i].kind==HOST_BOX))continue;if(at++==index){a=&sHost.actors[i];break;}}
    if(!a)return 0;
    if(!live(a))return -2;
    memset(out,0,sizeof *out);
    out->id=out->generation=a->id;out->flags_48=eligible(a)?0x10:0x40;out->enabled_dc=(uint16_t)eligible(a);
    out->cached_distance=a->cached_distance;memcpy(out->position,a->position,sizeof out->position);return 1;
}
static int select_target(void *ctx,SmN64ComboOwner *owner,int32_t max,int32_t facing,int32_t dw,int32_t fw,uint32_t *out){
    (void)ctx;SmN64ClimbState *a=actor();if(!a||!owner||!out)return -2;
    SmN64Targeting state={0};SmN64TargetActor selected;SmN64TargetHost host={&sHost,target_record,NULL,line_clear,NULL};
    memcpy(state.position,owner->position,sizeof state.position);memcpy(state.inverse,a->basis.inverse,sizeof state.inverse);
    int rc=smn64_target_search(&state,max,facing,dw,fw,&host,&selected);if(rc!=1)return rc;*out=selected.id;return 1;
}
int spiderman_combat_host_select_target(const SmN64ComboOwner *owner,int32_t max,int32_t facing,int32_t dw,int32_t fw,uint32_t *id){
    if(!owner)return -2;
    SmN64ComboOwner copy=*owner;return select_target(NULL,&copy,max,facing,dw,fw,id);
}
static int target(void *ctx,uint32_t *id,uint16_t *type,int32_t p[3]){
    (void)ctx;if(!sHost.pending||!id||!type||!p)return -2;
    HostActor *selected=lookup(*id);if(!selected||selected->kind==HOST_EXPLOSION){*id=0;return 0;}
    *id=selected->id;*type=selected->type;memcpy(p,selected->position,sizeof selected->position);return 1;
}
static int pose(void *ctx,int clip,int frame,int16_t out[216]){(void)ctx;return spiderman_runtime_pose_s16(clip,frame,out,216);}
static int face(SmN64ClimbState *a,uint32_t heading){
    a->basis.yaw_delta=(int32_t)((heading-smn64_climb_heading(a))&4095);
    int rc=smn64_climb_basis(&a->basis,NULL);a->basis.yaw_delta=0;
    return rc==1?1:-2;
}
static int face_actor(void *,SmN64ComboOwner *,uint32_t);
static int entry_event(void *ctx,SmN64ClimbState *a,const SmN64EntryEvent *e){
    (void)ctx;if(!sHost.pending||!a||!e)return -2;
    if(e->interact_requested)return -2;
    if(e->pickup_requested&&!pickup_eligible(NULL,e->pickup_requested))return -2;
    if(e->throw_requested&&(!sHost.carry_enabled||!sHost.pending_carried.active||sHost.pending_carried.source.motion.id!=e->throw_requested))return -2;
    if(e->face_actor&&!lookup(e->face_actor))return -2;
    /* Original9910C stores the throw state,99124 plays195/201, then99138
     * calls A201C with that POST-entry state. This is the timed fast0 turn. */
    if(e->throw_requested&&e->face_actor){SmN64ComboOwner view={0};view.anim=a->anim;view.state=a->state;memcpy(view.position,a->position,sizeof view.position);memcpy(view.forward,a->basis.forward,sizeof view.forward);return face_actor(NULL,&view,e->face_actor);}
    return e->immediate_face?face(a,e->heading):1;
}
static int face_actor(void *ctx,SmN64ComboOwner *owner,uint32_t id){
    (void)ctx;HostActor *h=lookup(id);SmN64ClimbState *a=actor();if(!h||!a)return -2;
    SmN64Targeting view={0};uint16_t desired;
    memcpy(view.position,owner->position,sizeof view.position);memcpy(view.inverse,a->basis.inverse,sizeof view.inverse);
    if(smn64_target_face_heading(&view,h->position,smn64_climb_heading(a),(uint8_t)a->ceiling_class,&desired)!=1)return -2;
    uint32_t prior_state=a->state;a->state=owner->state;
    int rc=smn64_climb_turn(a,desired,0);a->state=prior_state;if(rc==1){memcpy(owner->forward,a->basis.forward,sizeof owner->forward);owner->heading_658=(uint32_t)a->basis.yaw_delta;owner->look_active=(uint32_t)a->turn_ticks;}return rc;
}
static void ground_pull(SmN64Ground *g,const SmN64ComboOwner *o){
    SmN64ClimbState *a=actor();memset(g,0,sizeof *g);g->anim=o->anim;g->state=o->state;g->aiming=(uint32_t)a->aiming;g->holding_object=(uint32_t)a->held_object;g->surface_mode=(uint32_t)a->adhered;g->collision=a->collision;g->ground_grace=a->ground_grace;g->jump_pressed=a->jump_pressed;g->copied_jump_pressed=a->jump_pressed;g->position_y=a->position[1];g->falling_origin_y=a->falling_origin_y;g->jump_velocity=a->launch_velocity;g->jump_variant=a->jump_variant;g->field_d20=a->d20;g->field_d24=a->d24;g->field_1184=a->field1184;g->acceleration_phase=a->run_ramp;
}
static void ground_push(SmN64ComboOwner *o,const SmN64Ground *g){
    SmN64ClimbState *a=actor();o->anim=g->anim;o->state=g->state;a->ground_grace=g->ground_grace;a->jump_pressed=g->jump_pressed;a->falling_origin_y=g->falling_origin_y;a->launch_velocity=g->jump_velocity;a->jump_variant=g->jump_variant;a->d20=g->field_d20;a->d24=g->field_d24;a->field1184=g->field_1184;a->run_ramp=g->acceleration_phase;
}
static int lost_ground(void *ctx,SmN64ComboOwner *o){(void)ctx;SmN64Ground g;if(!actor())return -2;ground_pull(&g,o);int rc=smn64_ground_lost(&g,sHost.frame.counts[212]);if(rc>=0)ground_push(o,&g);return rc;}
static int jump(void *ctx,SmN64ComboOwner *o){(void)ctx;SmN64Ground g;if(!actor())return -2;ground_pull(&g,o);int rc=smn64_ground_jump_request(&g,sHost.frame.counts[210],sHost.frame.counts[223]);if(rc>=0)ground_push(o,&g);return rc;}
static int stop(void *ctx,SmN64ComboOwner *o){
    (void)ctx;SmN64ClimbState *a=actor();if(!a)return -2;SmN64ClimbState next=*a;next.anim=o->anim;next.state=o->state;
    int rc=smn64_climb_stop(&next,sHost.frame.counts,sHost.frame.count);if(rc==1){*a=next;o->anim=a->anim;o->state=a->state;memcpy(o->position,a->position,sizeof o->position);memcpy(o->forward,a->basis.forward,sizeof o->forward);}return rc;
}
static int grab_actor(void *ctx,uint32_t id,SmN64GrabActor *out){(void)ctx;HostActor *a=lookup(id);if(!a||a->kind==HOST_EXPLOSION)return 0;out->id=a->id;out->type=a->type;memcpy(out->position,a->position,sizeof out->position);return 1;}
static int grab_request(void *ctx,SmN64ComboOwner *o,uint32_t id,const int32_t p[3]){(void)ctx;(void)o;(void)id;(void)p;return sHost.pending?0:-2;}
static int release_grab(void *ctx,uint32_t id){(void)ctx;return lookup(id)?1:0;}
static int move_grab(void *ctx,SmN64ComboOwner *o){(void)ctx;(void)o;return -2;}
static int fire_actor(void *ctx,uint32_t id,SmN64FireActor *out){
    (void)ctx;HostActor *a=lookup(id);if(!a||a->kind==HOST_EXPLOSION)return 0;memset(out,0,sizeof *out);out->id=id;out->type=a->type;out->health=eligible(a)?1:0;out->flags_3c8=0x10210;memcpy(out->position,a->position,sizeof out->position);return 1;
}
static int special_target(void *ctx,int32_t p[3],uint32_t *id){(void)ctx;(void)p;if(!sHost.pending)return -2;*id=0;return 0;}
static int fire_trace(void *ctx,const int32_t from[3],const int32_t to[3],SmN64FireSurface *out){
    (void)ctx;SpidermanWorldHit hit;int rc=spiderman_world_trace_ex(from,to,1,&hit);if(rc!=1)return rc;memset(out,0,sizeof *out);out->hit=(uint32_t)hit.present;out->surface_present=(uint32_t)hit.present;if(hit.present){memcpy(out->position,hit.position,sizeof out->position);memcpy(out->normal,hit.normal,sizeof out->normal);out->flags=hit.source_flags;}return 1;
}
typedef struct TrailPose {
    const SmN64ClimbState *actor;const SmN64Marker *markers;size_t marker_count;
    const int16_t *pose;const int32_t *body;
} TrailPose;
static int trail_marker(void *ctx,uint32_t marker,int32_t out[3]){
    TrailPose *p=ctx;if(!p||!p->actor||!p->markers||p->marker_count<7||!p->pose||!p->body||(marker!=5&&marker!=6))return -2;
    return smn64_marker_world(p->markers,p->marker_count,marker,p->pose,18,p->actor->basis.matrix,p->body,p->actor->position,0,out)==1?1:-2;
}
static SmN64Trail *trail_allocate(void *ctx){
    (void)ctx;HostTrails *t=&sHost.pending_trails;if(!sHost.pending||t->count>=SPIDERMAN_HOST_TRAIL_CAPACITY)return NULL;
    for(unsigned i=0;i<SPIDERMAN_HOST_TRAIL_CAPACITY;i++)if(!t->live[i]){
        memset(&t->trail[i],0,sizeof t->trail[i]);t->live[i]=1;
        memmove(&t->order[1],&t->order[0],t->count*sizeof t->order[0]);t->order[0]=(uint8_t)i;t->count++;return&t->trail[i];
    }
    return NULL;
}
static int trail_pair(SmN64TrailPair *pair){
    HostTrails *t=&sHost.pending_trails;memset(pair,0,sizeof *pair);
    for(unsigned i=0;i<2;i++)if(t->active[i]){
        unsigned slot=t->active[i]-1;if(slot>=SPIDERMAN_HOST_TRAIL_CAPACITY||!t->live[slot]||t->trail[slot].stopping||t->trail[slot].delete_requested)return -2;
        pair->trail[i]=&t->trail[slot];
    }
    if(pair->trail[0]&&pair->trail[0]==pair->trail[1])return -2;
    return 1;
}
static int trail_pair_store(const SmN64TrailPair *pair){
    HostTrails *t=&sHost.pending_trails;
    for(unsigned i=0;i<2;i++){
        if(!pair->trail[i]){t->active[i]=0;continue;}
        unsigned slot;for(slot=0;slot<SPIDERMAN_HOST_TRAIL_CAPACITY;slot++)if(pair->trail[i]==&t->trail[slot]&&t->live[slot])break;
        if(slot==SPIDERMAN_HOST_TRAIL_CAPACITY)return -2;
        t->active[i]=slot+1;
    }
    return 1;
}
static int trails_start(void){
    if(!sHost.trails_enabled)return fail("Original aerial trail renderer/capability not enabled");
    SmN64CombatOwnerFrame *f=&sHost.frame;SmN64TrailPair pair;
    if(!sHost.pending||!sHost.bound||trail_pair(&pair)!=1)return -2;
    TrailPose pose={f->actor,f->markers,f->marker_count,f->retained_pose,f->body_translation};SmN64TrailHost host={&pose,trail_marker,trail_allocate};
    if(smn64_trails_start(&pair,sHost.trail_color,&host)!=1)return fail("Original aerial trail allocation/pose failed");
    return trail_pair_store(&pair);
}
static int trails_stop(void){
    SmN64TrailPair pair;if(!sHost.pending||trail_pair(&pair)!=1)return -2;
    smn64_trails_stop(&pair);return trail_pair_store(&pair);
}
static int first_trail_present(void *ctx,uint32_t *present){
    (void)ctx;SmN64TrailPair pair;if(!sHost.pending||!present||trail_pair(&pair)!=1)return -2;
    *present=pair.trail[0]!=NULL;return 1;
}
static int air_interrupt_phase(void *ctx,SmN64AirAttack *air,uint32_t phase){
    (void)ctx;SmN64ClimbState *a=actor();if(!a||!air)return -2;
    if(phase==SMN64_AIR_INTERRUPT_START_TRAILS)return trails_start();
    if(phase==SMN64_AIR_INTERRUPT_STOP_TRAILS)return trails_stop();
    if(phase!=SMN64_AIR_INTERRUPT_ALIGN_FLAT)return -2;
    /* The typed owner has published the OLD clip/state/velocity and the newly
     * flat normal at this exact source call position. Unlike the later generic
     * air event, fallback stop belongs HERE, before helper run175/stop/zero. */
    for(unsigned k=0;k<3;k++)a->basis.normal[k]=(int16_t)air->normal[k];
    if(!smn64_climb_basis(&a->basis,NULL)&&smn64_climb_stop(a,sHost.frame.counts,sHost.frame.count)!=1)return -2;
    air->anim=a->anim;air->state=a->state;memcpy(air->forward,a->basis.forward,sizeof air->forward);
    for(unsigned k=0;k<3;k++)air->normal[k]=a->basis.normal[k];
    return 1;
}
static int air_event(void *ctx,SmN64ClimbState *a,SmN64AirAttack *air,const SmN64AirEvent *e){
    (void)ctx;if(!sHost.pending||!sHost.bound||a!=sHost.frame.actor||!air||!e)return -2;
    if(e->start_trails&&!e->face_target&&trails_start()!=1)return -2;
    if(e->detach_swing)return fail("Aerial swing detach was not composed by owner");
    /* Source916A0: pursuit force1 with horizontal-back forward; landing force0.
     * Source9996C surface entry: flat normal force0, THEN timed target facing.
     * No invented clearance query:9996C makes no9CF5C call. Owner must expose
     * PRE-entry state during face_target, as source turn duration depends on it. */
    if(e->align_normal){
        const int32_t *forward=(!e->face_target&&!e->stop_trails)?air->horizontal_back:NULL;
        for(unsigned k=0;k<3;k++)a->basis.normal[k]=(int16_t)air->normal[k];
        if(smn64_climb_basis(&a->basis,forward)!=1)return fail("Degenerate aerial basis needs original ordered stop fallback");
        memcpy(air->forward,a->basis.forward,sizeof air->forward);
        if(e->face_target)a->adhered=0; /*9996C clearsCF0 after basis, beforeA201C*/
    }
    if(e->face_target){
        SmN64ComboOwner view={0};view.state=a->state;memcpy(view.position,a->position,sizeof view.position);memcpy(view.forward,a->basis.forward,sizeof view.forward);
        if(face_actor(NULL,&view,air->target_id)!=1)return -2;
        memcpy(air->forward,a->basis.forward,sizeof air->forward);
    }
    if(e->start_trails&&e->face_target&&trails_start()!=1)return -2;
    if(e->stop_trails&&trails_stop()!=1)return -2;
    return 1;
}
/* Original stop only detaches active handles; genuine stopped effect objects
 * remain in the pending registry and fade independently until source cleanup. */
static int damage_event(void *ctx,SmN64ClimbState *a,SmN64CharacterCombat *c,const SmN64DamageEvent *e){
    (void)ctx;if(!sHost.pending||!sHost.bound||a!=sHost.frame.actor||!c||!e)return -2;
    if(e->drop_actor&&carry_release(NULL,e->drop_actor,NULL,1)!=1)return fail("Original held-object damage Smash failed");
    if(e->release_web||e->release_swing)return fail("Damage graphic release was not composed by owner");
    if(e->restore_armor_model||e->clear_armor_ui||e->stun_effect)return fail("Original armor/stun graphical lifecycle unavailable");
    if(e->exit_aim||e->unlock_camera)return fail("Original damage camera lifecycle unavailable");
    if(e->align_normal)return fail("Original damage basis event requires source composition");
    if(e->rumble_kind>1)return fail("Unsupported source damage presentation event");
    if(e->died&&c->damage.health!=0)return fail("Source death event has nonzero health");
    if(e->stop_trails&&trails_stop()!=1)return -2;
    /* Pose, health, reaction, velocity and death are already committed only to
     * the pending source actor by the owner. Original sound decisions/RNG are
     * queued there. This native checkpoint intentionally has silent audio and
     * rumble presentation; no gameplay effects or random draws are invented. */
    return 1;
}
static void bind_frame(void *ctx,const SmN64CombatOwnerFrame *f){
    (void)ctx;if(!f){sHost.bound=0;return;}
    if(spiderman_combat_host_bind(f)!=1)sHost.bound=0;
}
static int carry_object(void *,uint32_t,SmN64CarryObject *);
static int carry_pickup(void *,uint32_t);
static int carry_throw(void *,const SmN64ThrowRequest *);
static int carry_throw_ordered(void *,const SmN64ThrowRequest *,uint32_t[3]);
static int carry_stop(void *,SmN64Carry *);
static int carry_jump(void *,SmN64Carry *);
static int carry_hold(void *,uint32_t,const int32_t[3],uint16_t);
static int carry_release(void *,uint32_t,const int32_t[3],int);
static void services_init(void){
    SmN64CombatOwnerServices *s=&sHost.services;memset(s,0,sizeof *s);s->context=&sHost;s->bind_frame=bind_frame;s->bank=&sHost.bank;s->pose=pose;s->actors=actors;s->target=target;s->entry_event=entry_event;s->damage_event=damage_event;s->air_event=air_event;s->first_trail_present=first_trail_present;
    s->unavailable_commands=SMN64_COMMAND_TRAP|SMN64_COMMAND_YANK|SMN64_COMMAND_IMPACT|
        SMN64_COMMAND_DOME|SMN64_COMMAND_GRAB|SMN64_COMMAND_CARRY|SMN64_COMMAND_AIM|
        SMN64_COMMAND_MOUNTED|SMN64_COMMAND_INTERACT;
    s->carry=(SmN64CarryHost){&sHost,carry_object,carry_pickup,carry_throw,carry_stop,carry_jump};
    s->hold_actor=carry_hold;s->carry_release=carry_release;s->carry_throw_ordered=carry_throw_ordered;
    s->air_interrupt=(SmN64AirInterruptHost){&sHost,air_interrupt_phase};
    s->entry=(SmN64EntryHost){&sHost,actor_at,actor_by_id,line_clear,pickup_trace,pickup_eligible,interactable};
    s->root=(SmN64RootHost){&sHost,NULL,root_sweep,world_trace,NULL};s->melee=(SmN64CombatHost){&sHost,NULL,sweep,apply};s->air=(SmN64AirHost){&sHost,body_sweep,apply};
    s->combo=(SmN64ComboOwnerHost){&sHost,lost_ground,jump,NULL,select_target,face_actor,stop,grab_actor,grab_request,release_grab,move_grab};
    s->fire=(SmN64FireHost){&sHost,fire_actor,NULL,special_target,fire_trace};s->damage=(SmN64DamageHost){&sHost,line_clear};
}
int spiderman_combat_host_init(const char *dir){
    spiderman_combat_host_shutdown();if(bank_load(dir)!=1)return -2;/* Optional, fail-closed throwable capability. Ordinary combat does not need
     * boot bytes; missing/corrupt boot never enables pickup or object flight. */
    if(throw_types_load(dir)!=1)memset(&sHost.throw_types,0,sizeof sHost.throw_types);
    services_init();sHost.initialized=1;sError[0]=0;return 1;
}
void spiderman_combat_host_abort(void){sHost.pending=0;sHost.bound=0;sHost.count=0;sHost.have_hit=0;sHost.initial=NULL;sHost.mario=NULL;}
void spiderman_combat_host_reset_actor(void){
    spiderman_combat_host_present_objects();
    HostManip *held=&sHost.carried;struct Object *held_object=held->object;
    if(held->active&&held_object&&(held_object->activeFlags&ACTIVE_FLAG_ACTIVE)&&held_object->behavior==bhvBreakableBoxSmall&&held_object->unused1==held->source.motion.id&&held_object->oHeldState==HELD_HELD){held_object->oHeldState=HELD_FREE;held_object->oIntangibleTimer=0;}
    for(size_t i=0;i<THROW_CAPACITY;i++){HostThrow *t=&sHost.thrown[i];struct Object *o=t->object;
        if(t->active&&o&&(o->activeFlags&ACTIVE_FLAG_ACTIVE)&&o->behavior==bhvBreakableBoxSmall&&o->unused1==t->source.id&&o->oHeldState==HELD_HELD){o->oHeldState=HELD_FREE;o->oIntangibleTimer=0;}
    }
    spiderman_combat_host_abort();sIncomingCount=0;sHost.incoming_count=0;sHost.last_have=0;
    memset(&sHost.carried,0,sizeof sHost.carried);memset(&sHost.pending_carried,0,sizeof sHost.pending_carried);
    sHost.sound_count=sHost.pending_sound_count=0;
    memset(sHost.thrown,0,sizeof sHost.thrown);memset(sHost.pending_thrown,0,sizeof sHost.pending_thrown);
    memset(&sHost.trails,0,sizeof sHost.trails);memset(&sHost.pending_trails,0,sizeof sHost.pending_trails);
    sHost.dome_mark_count=0;
}
void spiderman_combat_host_shutdown(void){spiderman_combat_host_reset_actor();memset(&sHost,0,sizeof sHost);sError[0]=0;}
const SmN64CombatOwnerServices *spiderman_combat_host_services(void){return sHost.initialized?&sHost.services:NULL;}
int spiderman_combat_host_begin(struct MarioState *m,const SmN64ClimbState *source){
    /* Native update order, then linked-list insertion order. This is an explicit
     * cross-game mapping of source actor order, not distance sorting. */
    static const unsigned order[]={OBJ_LIST_PUSHABLE,OBJ_LIST_GENACTOR,OBJ_LIST_DESTRUCTIVE,OBJ_LIST_LEVEL,OBJ_LIST_DEFAULT,OBJ_LIST_EXT};
    unsigned char seen[OBJECT_POOL_CAPACITY]={0};size_t visited=0;
    if(!sHost.initialized||sHost.pending||!m||!source||!gObjectLists)return fail("Combat host cannot begin transaction");
    sHost.pending=1;sHost.mario=m;sHost.initial=source;sHost.count=0;sHost.bound=0;sHost.have_hit=0;sHost.dome_reset_pending=0;
    memcpy(sHost.pending_thrown,sHost.thrown,sizeof sHost.thrown);
    sHost.pending_carried=sHost.carried;sHost.pending_sound_count=0;
    sHost.pending_trails=sHost.trails;
    for(size_t i=0;i<THROW_CAPACITY;i++)if(sHost.pending_thrown[i].source.path_handle)sHost.pending_thrown[i].source.path=sHost.pending_thrown[i].path;
    for(size_t l=0;l<sizeof order/sizeof order[0];l++){
        struct ObjectNode *head=&gObjectLists[order[l]],*node=head->next;
        while(node!=head){
            uintptr_t p=(uintptr_t)node,base=(uintptr_t)gObjectPool;size_t slot;
            if(!node||p<base||p-base>=sizeof(struct Object)*OBJECT_POOL_CAPACITY||(p-base)%sizeof(struct Object)){spiderman_combat_host_abort();return fail("Invalid native actor-list node");}
            slot=(p-base)/sizeof(struct Object);if(seen[slot]||++visited>OBJECT_POOL_CAPACITY){spiderman_combat_host_abort();return fail("Cyclic/duplicate native actor list");}seen[slot]=1;
            struct Object *o=(struct Object *)node;HostKind kind=classify(o);node=node->next;
            int owned=native_owned(o);
            if(!kind||!(o->activeFlags&ACTIVE_FLAG_ACTIVE)||(!owned&&(o->activeFlags&(ACTIVE_FLAG_DORMANT|ACTIVE_FLAG_IN_DIFFERENT_ROOM))))continue;
            if(sHost.count>=HOST_CAPACITY){spiderman_combat_host_abort();return fail("Native combat actor capacity exceeded");}
            HostActor *a=&sHost.actors[sHost.count];memset(a,0,sizeof *a);a->object=o;a->behavior=o->behavior;a->old_token=o->unused1;a->sync_id=o->oSyncID;a->kind=kind;a->owned=(uint8_t)owned;
            if(o->unused1){if(o->unused1>=sNextToken){spiderman_combat_host_abort();return fail("Native lifetime field belongs to another owner");}a->id=o->unused1;}
            else {if(sNextToken==UINT32_MAX){spiderman_combat_host_abort();return fail("Native lifetime token space exhausted");}a->id=sNextToken++;}
            for(size_t j=0;j<sHost.count;j++)if(sHost.actors[j].id==a->id){spiderman_combat_host_abort();return fail("Duplicate native lifetime token");}
            /* Generic combat-recipient type130 is an explicit adapter role,
             * never a claim these are original Neversoft actor classes. */
            a->type=0x130;a->old_status=a->status=o->oInteractStatus;a->action=o->oAction;a->held=o->oHeldState;a->intangible=o->oIntangibleTimer;
            double radius=o->hurtboxRadius>0?o->hurtboxRadius:o->hitboxRadius,height=o->hurtboxHeight>0?o->hurtboxHeight:o->hitboxHeight;
            double bottom=(double)o->oPosY-o->hitboxDownOffset,top=bottom+height;
            if(radius<=0||height<=0||!fixed(o->oPosX,&a->position[0])||!fixed(-(bottom+height*.5),&a->position[1])||!fixed(-(double)o->oPosZ,&a->position[2])||!fixed(-top,&a->lower)||!fixed(-bottom,&a->upper)||!fixed(radius,&a->radius)){if(owned){spiderman_combat_host_abort();return fail("Owned native box collider became invalid");}continue;}
            a->cached_distance=source_distance(source->position,a->position);
            for(size_t j=0;j<sHost.dome_mark_count;j++)if(sHost.dome_mark_ids[j]==a->id){a->dome_hit=1;break;}
            ++sHost.count;
        }
    }
    /* A contact that expired before the source tick is no longer a recipient.
     * Compact live records only in this pending view; commit consumes the queue,
     * abort keeps its immutable input without allowing a stale record to poison
     * the next frame or hide later live contacts. */
    sHost.incoming_count=0;
    for(size_t j=0;j<sIncomingCount;j++)for(size_t i=0;i<sHost.count;i++){
        HostActor *a=&sHost.actors[i];Incoming *in=&sIncoming[j];
        if(a->object==in->object&&live(a)&&a->behavior==in->behavior&&a->old_token==in->token&&a->sync_id==in->sync_id){sHost.incoming_map[sHost.incoming_count++]=j;break;}
    }
    return 1;
}
int spiderman_combat_host_bind(const SmN64CombatOwnerFrame *frame){
    if(!sHost.pending||!frame||!frame->actor||!frame->counts||frame->count<300)return -2;
    sHost.frame=*frame;sHost.bound=1;return 1;
}
int spiderman_combat_host_commit(void){
    uint32_t marks[OBJECT_POOL_CAPACITY];size_t mark_count=0;
    if(!sHost.pending)return -2;
    for(size_t i=0;i<sHost.count;i++){HostActor *a=&sHost.actors[i];if(!live(a)||(uint32_t)a->object->oInteractStatus!=a->old_status||a->object->oAction!=a->action||a->object->oHeldState!=a->held||a->object->oIntangibleTimer!=a->intangible){spiderman_combat_host_abort();return fail("Native actor changed before combat commit");}}
    /* A dormant/off-room actor still has its native lifetime and retains the
     * original once-hit bit. Prune only a genuinely retired lifetime, never
     * merely absence from this frame's collision/recipient snapshot. */
    for(size_t i=0;!sHost.dome_reset_pending&&i<sHost.dome_mark_count;i++){
        int retained=0;uint32_t id=sHost.dome_mark_ids[i];
        for(size_t j=0;j<OBJECT_POOL_CAPACITY;j++)if((gObjectPool[j].activeFlags&ACTIVE_FLAG_ACTIVE)&&gObjectPool[j].unused1==id){retained=1;break;}
        if(retained)marks[mark_count++]=id;
    }
    for(size_t i=0;i<sHost.count;i++){
        HostActor *a=&sHost.actors[i];size_t at=0;while(at<mark_count&&marks[at]!=a->id)at++;
        if(a->dome_hit&&!a->write_destroy&&at==mark_count){
            if(mark_count>=OBJECT_POOL_CAPACITY){spiderman_combat_host_abort();return fail("Native dome lifetime mark capacity exceeded");}
            marks[mark_count++]=a->id;
        }else if((!a->dome_hit||a->write_destroy)&&at<mark_count){marks[at]=marks[--mark_count];}
    }
    for(size_t i=0;i<sHost.count;i++){HostActor *a=&sHost.actors[i];a->object->unused1=a->id;if(a->write_status)a->object->oInteractStatus=a->status;
        if(a->write_capture){a->object->oHeldState=HELD_HELD;a->object->heldByPlayerIndex=(uint32_t)sHost.mario->playerIndex;a->object->oIntangibleTimer=-1;}
        if(a->write_destroy)a->object->activeFlags=ACTIVE_FLAG_DEACTIVATED;
    }
    memcpy(sHost.thrown,sHost.pending_thrown,sizeof sHost.thrown);
    sHost.carried=sHost.pending_carried;sHost.sound_count=sHost.pending_sound_count;
    memcpy(sHost.sounds,sHost.pending_sounds,sHost.sound_count*sizeof *sHost.sounds);
    sHost.dome_mark_count=mark_count;memcpy(sHost.dome_mark_ids,marks,mark_count*sizeof *marks);
    sHost.trails=sHost.pending_trails;
    for(size_t i=0;i<THROW_CAPACITY;i++)if(sHost.thrown[i].source.path_handle)sHost.thrown[i].source.path=sHost.thrown[i].path;
    if(sHost.have_hit){sHost.last_hit=sHost.hit;sHost.last_accepted=sHost.accepted;sHost.last_have=1;}
    sIncomingCount=0;spiderman_combat_host_abort();return 1;
}
int spiderman_combat_host_last_hit(SmN64CombatHit *out,int *accepted){if(!out||!accepted||!sHost.last_have)return 0;*out=sHost.last_hit;*accepted=sHost.last_accepted;return 1;}

int spiderman_combat_host_queue_incoming(struct MarioState *m,struct Object *o){
    if(!sHost.initialized||sHost.pending||!m||!o||m->playerIndex!=0)return -2;
    if(!classify(o)||!(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->oDamageOrCoinValue<=0)return 0;
    for(size_t i=0;i<sIncomingCount;i++)if(sIncoming[i].object==o&&sIncoming[i].token==o->unused1&&sIncoming[i].behavior==o->behavior&&sIncoming[i].sync_id==o->oSyncID)return 1;
    if(sIncomingCount>=INCOMING_CAPACITY)return fail("Incoming source damage queue full");
    Incoming in={0};in.object=o;in.behavior=o->behavior;in.token=o->unused1;in.sync_id=o->oSyncID;
    in.hit.kind=8;in.hit.flags=0x0e;
    uint32_t wedges=(uint32_t)o->oDamageOrCoinValue;in.hit.damage=(uint16_t)(wedges>=8?100:(wedges*100+7)/8);
    int32_t delta[3];double player[3]={m->pos[0],-((double)m->pos[1]+96),-(double)m->pos[2]},enemy[3]={o->oPosX,-(double)o->oPosY,-(double)o->oPosZ};
    for(unsigned k=0;k<3;k++){if(!fixed(player[k],&in.hit.position[k]))return -2;double d=player[k]-enemy[k];if(!isfinite(d)||d<INT32_MIN||d>INT32_MAX)return -2;delta[k]=k==1?0:(int32_t)d;}
    smn64_combat_normalize(delta,in.hit.direction);sIncoming[sIncomingCount++]=in;return 1;
}
int spiderman_combat_host_incoming(size_t index,SmN64CombatHit *out){
    if(!sHost.pending||!out)return -2;
    if(index>=sHost.incoming_count)return 0;
    Incoming *in=&sIncoming[sHost.incoming_map[index]];HostActor *a=NULL;
    for(size_t i=0;i<sHost.count;i++)if(sHost.actors[i].object==in->object){a=&sHost.actors[i];break;}
    if(!a||!live(a)||a->behavior!=in->behavior||a->old_token!=in->token||a->sync_id!=in->sync_id)return -2;
    *out=in->hit;out->actor=a->id;return 1;
}
int spiderman_combat_host_incoming_result(size_t index,int accepted){
    SmN64CombatHit hit;if(accepted<0||accepted>1||spiderman_combat_host_incoming(index,&hit)!=1)return -2;
    HostActor *a=lookup(hit.actor);if(!a)return -2;
    if(accepted){if(a->write_status)return fail("Actor damage mailbox conflict");a->status=INT_STATUS_INTERACTED|INT_STATUS_ATTACKED_MARIO;a->write_status=1;}
    return 1;
}

typedef struct ThrowCall { HostThrow *entry;const SpidermanThrowableEffects *effects;uint32_t *rng; } ThrowCall;
static int thrown_release(void *ctx,uint32_t handle){ThrowCall *q=ctx;return q->entry->source.path_handle==handle?1:-2;}
static int thrown_actor(void *ctx,const int32_t from[3],const int32_t to[3],int32_t radius,SmN64ThrowableActor *out){
    ThrowCall *q=ctx;uint8_t part;int32_t p[3];
    for(size_t i=0;i<sHost.count;i++){HostActor *a=&sHost.actors[i];if(a->id==q->entry->source.id||a->kind==HOST_BOX||!eligible(a)||!live(a))continue;int rc=sweep(NULL,a->id,from,to,radius,&part,p);if(rc<0)return rc;if(rc){out->id=a->id;out->type=a->type;return 1;}}
    return 0;
}
static int thrown_world(void *ctx,uint32_t owner,const int32_t from[3],const int32_t to[3],SmN64ThrowableWorldHit *out){
    ThrowCall *q=ctx;if(q->entry->source.id!=owner)return -2;SpidermanWorldHit hit;int rc=spiderman_world_trace_ex(from,to,0,&hit);if(rc!=1)return rc;memset(out,0,sizeof *out);out->hit=(uint32_t)hit.present;if(hit.present){memcpy(out->position,hit.position,sizeof out->position);memcpy(out->normal,hit.normal,sizeof out->normal);}return 1;
}
static int thrown_first(void *ctx,const SmN64Throwable *s){ThrowCall *q=ctx;return q->effects->first_world_impact(q->effects->context,s);}
static int thrown_impact(void *ctx,const SmN64Throwable *s,const int16_t normal[3]){ThrowCall *q=ctx;return q->effects->impact(q->effects->context,s,normal,q->rng);}
static int thrown_destroy(void *ctx,uint32_t id){ThrowCall *q=ctx;HostActor *a=lookup(id);if(!a||q->entry->source.id!=id||a->kind!=HOST_BOX)return -2;a->write_destroy=1;q->entry->active=0;return 1;}
int spiderman_combat_host_throw_begin(const SmN64ThrowRequest *request,const SmN64Throwable *initial,const SpidermanThrowableEffects *effects){
    if(!sHost.pending||!request||!initial||!effects||!effects->impact||!effects->first_world_impact||sHost.throw_types.loaded!=1)return -2;
    HostActor *a=lookup(request->actor);if(!a||a->kind!=HOST_BOX||a->write_destroy)return 0;
    size_t slot=THROW_CAPACITY;for(size_t i=0;i<THROW_CAPACITY;i++){if(sHost.pending_thrown[i].active&&sHost.pending_thrown[i].source.id==a->id)return -2;if(!sHost.pending_thrown[i].active&&slot==THROW_CAPACITY)slot=i;}
    if(slot==THROW_CAPACITY)return fail("Source throwable capacity exceeded");
    if(request->use_path&&(request->path_ticks<2||request->path_ticks>THROW_PATH_CAPACITY))return -2;
    HostThrow *t=&sHost.pending_thrown[slot];memset(t,0,sizeof *t);t->source=*initial;t->source.id=a->id;t->source.flags_10c|=1;t->source.destroyed=0;t->source.path=NULL;t->source.path_handle=t->source.path_count=t->source.path_index=0;t->source.path_capacity=0;
    memcpy(t->source.position,a->position,sizeof a->position);memcpy(t->source.velocity,request->velocity,sizeof request->velocity);t->source.spin[0]=(int16_t)request->spin;
    if(request->use_path){if(smn64_throw_path(t->source.position,request->target,request->path_ticks,t->path,THROW_PATH_CAPACITY)!=1)return -2;t->source.path=t->path;t->source.path_handle=a->id;t->source.path_count=request->path_ticks;t->source.path_capacity=THROW_PATH_CAPACITY;}
    t->object=a->object;t->origin_y_offset=a->object->oPosY+(float)a->position[1]/4096.0f;t->active=1;a->write_capture=1;return 1;
}
int spiderman_combat_host_throwables_tick(int32_t elapsed,uint32_t rng[3],const SpidermanThrowableEffects *effects){
    if(!sHost.pending||!rng||elapsed<1||elapsed>6)return -2;
    /* Original82E38 prepends constructors to F6460. Native source proxies
     * are registered once in observed native-list order, with monotonic tokens;
     * descending token is therefore the mapped constructor-list order. Capture
     * and release do not reorder it. Slot reuse must not alter shared RNG order. */
    uint32_t before=UINT32_MAX;
    for(size_t pass=0;pass<THROW_CAPACITY;pass++){
        HostThrow *t=NULL;
        for(size_t i=0;i<THROW_CAPACITY;i++){HostThrow *candidate=&sHost.pending_thrown[i];if(candidate->active&&candidate->source.id<before&&(!t||candidate->source.id>t->source.id))t=candidate;}
        if(!t)break;
        before=t->source.id;
        if(!effects||!effects->impact||!effects->first_world_impact||!lookup(t->source.id))return -2;
        ThrowCall q={t,effects,rng};SmN64ThrowableHost host={&q,thrown_release,thrown_actor,thrown_world,apply,thrown_first,thrown_impact,thrown_destroy};SmN64ThrowableEvent event;t->source.elapsed=elapsed;
        int rc=smn64_throwable_tick(&t->source,&sHost.throw_types,rng,&host,&event);if(rc<0)return rc;
    }
    return 1;
}
static void present_manip(const HostManip *m){
    struct Object *o=m->object;if(!m->active||!o||!(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->behavior!=bhvBreakableBoxSmall||o->unused1!=m->source.motion.id)return;
    const SmN64Throwable *s=&m->source.motion;o->oPosX=s->position[0]/4096.f;o->oPosY=-s->position[1]/4096.f+m->origin_y_offset;o->oPosZ=-s->position[2]/4096.f;
    for(unsigned k=0;k<3;k++){o->header.gfx.pos[k]=k==0?o->oPosX:k==1?o->oPosY:o->oPosZ;o->header.gfx.angle[k]=(s16)(u16)((uint16_t)s->angles[k]*16u*(k?65535u:1u));}
    o->header.gfx.node.flags|=GRAPH_RENDER_ACTIVE;o->header.gfx.node.flags&=~GRAPH_RENDER_INVISIBLE;
}
void spiderman_combat_host_present_objects(void){
    present_manip(&sHost.carried);
    for(size_t i=0;i<THROW_CAPACITY;i++){HostThrow *t=&sHost.thrown[i];struct Object *o=t->object;
        if(!t->active||!o||!(o->activeFlags&ACTIVE_FLAG_ACTIVE)||o->behavior!=bhvBreakableBoxSmall||o->unused1!=t->source.id)continue;
        o->oPosX=t->source.position[0]/4096.0f;o->oPosY=-t->source.position[1]/4096.0f+t->origin_y_offset;o->oPosZ=-t->source.position[2]/4096.0f;
        for(unsigned k=0;k<3;k++){o->header.gfx.pos[k]=k==0?o->oPosX:k==1?o->oPosY:o->oPosZ;o->header.gfx.angle[k]=(s16)(u16)((uint16_t)t->source.angles[k]*16u*(k?65535u:1u));}
        o->header.gfx.node.flags|=GRAPH_RENDER_ACTIVE;o->header.gfx.node.flags&=~GRAPH_RENDER_INVISIBLE;
    }
}

int spiderman_combat_host_trails_enable(uint32_t color){
    if(!sHost.initialized||sHost.pending)return -2;
    sHost.trail_color=color;sHost.trails_enabled=1;return 1;
}
int spiderman_combat_host_trails_retain(const SmN64ClimbState *a,const SmN64Marker *markers,size_t count,const int16_t pose[216],const int32_t body[3],uint32_t tick){
    if(!sHost.pending||!a||!markers||count<7||!pose||!body)return -2;
    HostTrails *t=&sHost.pending_trails;if(t->retained&&t->last_retain==tick)return fail("Aerial trail pose tail repeated within source tick");
    SmN64TrailPair pair;if(trail_pair(&pair)!=1)return -2;TrailPose p={a,markers,count,pose,body};SmN64TrailHost host={&p,trail_marker,NULL};
    if(smn64_trails_retain(&pair,&host)!=1)return fail("Original aerial trail retained pose unavailable");
    t->retained=1;t->last_retain=tick;return 1;
}
int spiderman_combat_host_trails_effects(uint32_t tick){
    if(!sHost.pending)return -2;
    HostTrails *t=&sHost.pending_trails;if(t->effected&&t->last_effect==tick)return fail("Aerial trail effect pass repeated within source tick");
    for(unsigned i=0;i<t->count;i++){unsigned slot=t->order[i];if(slot>=SPIDERMAN_HOST_TRAIL_CAPACITY||!t->live[slot])return -2;if(smn64_trail_tick(&t->trail[slot])<0)return -2;}
    unsigned next=0;
    for(unsigned i=0;i<t->count;i++){
        unsigned slot=t->order[i];if(t->trail[slot].delete_requested){if(t->active[0]==slot+1||t->active[1]==slot+1)return -2;t->live[slot]=0;}
        else t->order[next++]=(uint8_t)slot;
    }
    t->count=next;t->effected=1;t->last_effect=tick;return 1;
}
int spiderman_combat_host_trails_snapshot(SmN64Trail *out,size_t capacity,size_t *count){
    if(!count||capacity<sHost.trails.count||(!out&&sHost.trails.count))return -2;
    for(unsigned i=0;i<sHost.trails.count;i++){
        unsigned slot=sHost.trails.order[i];if(slot>=SPIDERMAN_HOST_TRAIL_CAPACITY||!sHost.trails.live[slot]||sHost.trails.trail[slot].delete_requested)return -2;
        out[i]=sHost.trails.trail[slot];
    }
    *count=sHost.trails.count;return 1;
}

/* Explicit native small-box CManipOb profile. Native boxes have no Neversoft
 * authored links, hidden model shadow, explosion flag or debris list. Those
 * registries are genuinely empty; no effects/recipients are fabricated. */
static int manip_alpha(void *ctx,uint32_t id,uint8_t alpha){HostManip *m=ctx;if(!m||!m->active||m->source.motion.id!=id)return -2;m->alpha=alpha;return 1;}
static int manip_pulse(void *ctx,uint16_t node){HostManip *m=ctx;return m&&m->active&&node==0&&m->source.node==0?1:-2;}
static int manip_sound(void *ctx,uint32_t sound,const int32_t p[3]){
    HostManip *m=ctx;if(!m||!m->active||sHost.pending_sound_count>=16)return -2;
    SpidermanCarrySound *e=&sHost.pending_sounds[sHost.pending_sound_count++];e->sound=sound;memcpy(e->position,p,sizeof e->position);return 1;
}
static int manip_trace(void *ctx,const SmN64ClimbQuery *q,SmN64ClimbHit *out){
    (void)ctx;SpidermanWorldHit hit;int rc=spiderman_world_trace_ex(q->start,q->end,q->arg1,&hit);if(rc!=1)return rc;
    memset(out,0,sizeof *out);out->present=hit.present;if(hit.present){memcpy(out->position,hit.position,sizeof out->position);memcpy(out->normal,hit.normal,sizeof out->normal);out->distance=hit.distance;out->has_surface=hit.surface!=NULL;}return 1;
}
static int manip_ground(void *ctx,const int32_t p[3],int32_t above,int32_t below,uint32_t objects,int32_t *y){return smn64_climb_floor_query(p,above,below,(int32_t)objects,manip_trace,ctx,y);}
static int manip_stimulus(void *ctx,const int32_t p[3],uint32_t radius){
    /* Every vetted native enemy profile uses source body flags4A=0x12,
     * never0x200, so source61744's qualifying-recipient registry is empty. */
    HostManip *m=ctx;(void)p;return m&&m->active&&radius==2000?1:-2;
}
static SmN64ManipObHost manip_host(HostManip *m){SmN64ManipObHost h={m,manip_alpha,NULL,manip_pulse,manip_sound,manip_ground,manip_stimulus,NULL};return h;}
static HostThrow *find_throw(uint32_t id){for(size_t i=0;i<THROW_CAPACITY;i++)if(sHost.pending_thrown[i].active&&sHost.pending_thrown[i].source.id==id)return&sHost.pending_thrown[i];return NULL;}
static int native_first_impact(void *ctx,const SmN64Throwable *source){(void)ctx;(void)source;return fail("Small native box unexpectedly requested large first-impact path");}
static int native_impact(void *ctx,const SmN64Throwable *source,const int16_t normal[3],uint32_t rng[3]){
    (void)ctx;(void)normal;(void)rng;HostThrow *t=find_throw(source->id);if(!t||!t->manip.active)return -2;
    t->manip.source.motion=*source;SmN64ManipObHost h=manip_host(&t->manip);return smn64_manipob_impact_empty(&t->manip.source,&h);
}
static const SpidermanThrowableEffects native_throw_effects={NULL,native_first_impact,native_impact};
static int carry_object(void *ctx,uint32_t id,SmN64CarryObject *out){
    (void)ctx;if(!sHost.pending||!out)return -2;HostActor *a=lookup(id);
    if(!a){HostManip *m=&sHost.pending_carried;if(m->active&&m->source.motion.id==id&&native_generation_live(m->object,id))return fail("Live held native box absent from recipient snapshot");return 0;}
    if(a->write_destroy)return 0;
    memset(out,0,sizeof *out);out->id=id;
    HostManip *m=&sHost.pending_carried;
    if(m->active&&m->source.motion.id==id){out->flags_10c=m->source.motion.flags_10c;out->hold_radius=m->source.hold_radius;out->yaw=m->source.motion.angles[1];memcpy(out->position,m->source.motion.position,sizeof out->position);}
    else {memcpy(out->position,a->position,sizeof out->position);out->hold_radius=(int16_t)(a->radius/4096);out->yaw=(int16_t)((-(int32_t)a->object->oFaceAngleYaw/16)&4095);}
    return 1;
}
static int carry_pickup(void *ctx,uint32_t id){
    (void)ctx;if(!pickup_eligible(NULL,id))return 0;HostActor *a=lookup(id);if(!a||a->radius/4096>INT16_MAX)return -2;
    HostManip *m=&sHost.pending_carried;memset(m,0,sizeof *m);m->active=1;m->alpha=255;m->object=a->object;
    m->origin_y_offset=a->object->oPosY+(float)a->position[1]/4096.f;
    if(smn64_manipob_construct_scalars(&m->source,0,0,(int16_t)(a->radius/4096),0,0)!=1)return -2;
    m->source.motion.id=id;memcpy(m->source.motion.position,a->position,sizeof a->position);
    m->source.motion.angles[1]=(int16_t)((-(int32_t)a->object->oFaceAngleYaw/16)&4095);
    SmN64ManipObHost h=manip_host(m);if(smn64_manipob_pickup(&m->source,&h)!=1)return -2;
    a->write_capture=1;return 1;
}
static int carry_hold(void *ctx,uint32_t id,const int32_t p[3],uint16_t yaw){
    (void)ctx;HostManip *m=&sHost.pending_carried;if(!sHost.pending||!m->active||m->source.motion.id!=id||!lookup(id))return -2;
    memcpy(m->source.motion.position,p,sizeof m->source.motion.position);m->source.motion.angles[1]=(int16_t)yaw;return 1;
}
static int carry_stop(void *ctx,SmN64Carry *s){
    (void)ctx;SmN64ClimbState *a=actor();if(!a||!s)return -2;SmN64ClimbState next=*a;next.anim=s->anim;next.state=s->state;next.held_object=(int32_t)s->held_actor;
    int rc;if(s->held_actor){SmN64CarryObject o;if(carry_object(NULL,s->held_actor,&o)!=1)return -2;rc=smn64_climb_stop_carrying(&next,sHost.frame.counts,sHost.frame.count,o.flags_10c);}
    else rc=smn64_climb_stop(&next,sHost.frame.counts,sHost.frame.count);
    if(rc==1){*a=next;s->anim=next.anim;s->state=next.state;memcpy(s->position,next.position,sizeof s->position);memcpy(s->forward,next.basis.forward,sizeof s->forward);}return rc;
}
static int carry_jump(void *ctx,SmN64Carry *s){
    (void)ctx;SmN64ClimbState *a=actor();if(!a||!s)return -2;
    SmN64ComboOwner view={0};view.anim=s->anim;view.state=s->state;memcpy(view.position,s->position,sizeof view.position);memcpy(view.forward,s->forward,sizeof view.forward);
    /* The composed pending actor still contains the pre-release hold until
     * carry_step returns; expose the original already-cleared pointer here. */
    int32_t saved=a->held_object;a->held_object=(int32_t)s->held_actor;int rc=jump(NULL,&view);a->held_object=saved;
    if(rc>=0){s->anim=view.anim;s->state=view.state;}return rc;
}
static int carry_throw(void *ctx,const SmN64ThrowRequest *request){(void)ctx;(void)request;return fail("Native carry requires source-ordered release RNG");}
static int carry_throw_ordered(void *ctx,const SmN64ThrowRequest *request,uint32_t rng[3]){
    (void)ctx;HostManip *m=&sHost.pending_carried;if(!sHost.pending||!m->active||m->source.motion.id!=request->actor||!rng)return -2;
    if(spiderman_combat_host_throw_begin(request,&m->source.motion,&native_throw_effects)!=1)return -2;
    HostThrow *t=find_throw(request->actor);if(!t)return -2;t->manip=*m;t->origin_y_offset=m->origin_y_offset;
    /* Far-path allocation/count/index precede original7F714 release tail. */
    t->manip.source.motion=t->source;SmN64ManipObHost h=manip_host(&t->manip);
    int rc=smn64_manipob_release(&t->manip.source,request->use_path?SMN64_MANIPOB_THROW_PATH:SMN64_MANIPOB_THROW,request->velocity,rng,&h);
    if(rc!=1)return -2;
    t->source=t->manip.source.motion;m->active=0;return 1;
}
int spiderman_combat_host_carry_enable(void){
    if(!sHost.initialized||sHost.pending||sHost.throw_types.loaded!=1)return -2;
    sHost.carry_enabled=1;sHost.services.unavailable_commands&=~SMN64_COMMAND_CARRY;return 1;
}
int spiderman_combat_host_carry_tick(SmN64CombatOwnerFrame *frame,SmN64CharacterCombat *combat){
    if(!sHost.pending||!frame||!combat||!frame->actor)return -2;
    if(!sHost.carry_enabled)return 1;
    if(sHost.pending_carried.active&&!lookup(sHost.pending_carried.source.motion.id)){
        if(native_generation_live(sHost.pending_carried.object,sHost.pending_carried.source.motion.id))return fail("Live held native box absent from update snapshot");
        sHost.pending_carried.active=0;
    }
    for(size_t i=0;i<THROW_CAPACITY;i++)if(sHost.pending_thrown[i].active&&!lookup(sHost.pending_thrown[i].source.id)){
        if(native_generation_live(sHost.pending_thrown[i].object,sHost.pending_thrown[i].source.id))return fail("Live thrown native box absent from update snapshot");
        sHost.pending_thrown[i].active=0;
    }
    return spiderman_combat_host_throwables_tick(frame->actor->anim.elapsed_ticks,frame->actor->random_state,&native_throw_effects);
}
static int manip_live(const HostManip *m){return m->active&&m->object&&(m->object->activeFlags&ACTIVE_FLAG_ACTIVE)&&m->object->behavior==bhvBreakableBoxSmall&&m->object->unused1==m->source.motion.id;}
int spiderman_combat_host_object_alpha(const struct Object *o,uint8_t *alpha){
    if(!o||!alpha)return 0;
    if(manip_live(&sHost.carried)&&sHost.carried.object==o){*alpha=sHost.carried.alpha;return 1;}
    for(size_t i=0;i<THROW_CAPACITY;i++){const HostThrow *t=&sHost.thrown[i];if(t->active&&manip_live(&t->manip)&&t->object==o){*alpha=t->manip.alpha;return 1;}}
    return 0;
}
int spiderman_combat_host_carry_snapshot(SpidermanCarrySnapshot *out){
    if(!out)return -1;
    memset(out,0,sizeof *out);
    if(manip_live(&sHost.carried)){out->held_actor=sHost.carried.source.motion.id;out->alpha=sHost.carried.alpha;memcpy(out->held_position,sHost.carried.source.motion.position,sizeof out->held_position);}
    for(size_t i=0;i<THROW_CAPACITY;i++)if(sHost.thrown[i].active)out->flight_count++;
    out->sound_count=(uint32_t)sHost.sound_count;return 1;
}
static int manip_destroy(void *ctx,uint32_t id){HostManip *m=ctx;HostActor *a=lookup(id);if(!m||!m->active||m->source.motion.id!=id||!a||a->kind!=HOST_BOX)return -2;a->write_destroy=1;return 1;}
static int carry_release(void *ctx,uint32_t id,const int32_t velocity[3],int smash){
    (void)ctx;HostManip *m=&sHost.pending_carried;SmN64ClimbState *a=actor();if(!sHost.pending||!a||!m->active||m->source.motion.id!=id)return -2;
    if(!lookup(id)){m->active=0;return 1;}
    if(smash){SmN64ManipObHost h=manip_host(m);h.destroy=manip_destroy;if(smn64_manipob_smash_empty(&m->source,&h)!=1)return -2;m->active=0;return 1;}
    if(!velocity)return -2;
    SmN64ThrowRequest request={0};request.actor=id;memcpy(request.velocity,velocity,sizeof request.velocity);
    if(spiderman_combat_host_throw_begin(&request,&m->source.motion,&native_throw_effects)!=1)return -2;
    HostThrow *t=find_throw(id);if(!t)return -2;t->manip=*m;t->origin_y_offset=m->origin_y_offset;t->manip.source.motion=t->source;
    SmN64ManipObHost h=manip_host(&t->manip);if(smn64_manipob_release(&t->manip.source,SMN64_MANIPOB_DROP,velocity,a->random_state,&h)!=1)return -2;
    t->source=t->manip.source.motion;m->active=0;return 1;
}
