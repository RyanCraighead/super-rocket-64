/* Private original payload loader. Bounded regular-file and SHA helpers copied
 * from spiderman_web_gl.cpp; fingerprint constants derived from checked-in
 * dome_render/asset_identities.json, never from a caller-supplied manifest. */
#include "spiderman_dome_runtime.h"
#include "utils/oot_asset_path.h"
#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef __FAST_MATH__
#error "Original dome requires strict source arithmetic"
#endif
namespace {
// Preserve the shared helper's confinement rules, but use a nonblocking-style
// Win32 handle for canonicalization too: the shared canonical() opens before
// our regular-file reader is reached. No existing character helper is changed.
std::string assetCanonical(const std::string &path) {
#ifdef _WIN32
    const std::wstring wide=oot_asset_path::widePath(path);
    oot_asset_path::PathHandle handle(CreateFileW(wide.c_str(),0,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,nullptr));
    if(handle.value==INVALID_HANDLE_VALUE||GetFileType(handle.value)!=FILE_TYPE_DISK)
        throw std::runtime_error("Cannot resolve ordinary original asset path: "+path);
    const DWORD count=GetFinalPathNameByHandleW(handle.value,nullptr,0,FILE_NAME_NORMALIZED);
    if(!count||count>32768)throw std::runtime_error("Invalid canonical original asset path length");
    std::vector<wchar_t> buffer(size_t(count)+1);
    const DWORD length=GetFinalPathNameByHandleW(handle.value,buffer.data(),DWORD(buffer.size()),FILE_NAME_NORMALIZED);
    if(!length||length>=buffer.size())throw std::runtime_error("Cannot resolve original asset path: "+path);
    return oot_asset_path::utf8Path(std::wstring(buffer.data(),length));
#else
    return oot_asset_path::canonical(path);
#endif
}
std::string assetChild(const std::string &base,const std::string &relative,size_t maxLength) {
    oot_asset_path::validatePath(base);
    if(relative.empty()||relative.size()>maxLength||relative[0]=='/'||relative.find_first_of("\\\\:")!=std::string::npos||relative.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid asset-relative path");
    size_t start=0;while(start<=relative.size()){
        const size_t end=relative.find('/',start);const std::string part=relative.substr(start,end==std::string::npos?end:end-start);
        if(part.empty()||part=="."||part=="..")throw std::runtime_error("Asset path may not traverse directories");
        if(end==std::string::npos)break;
        start=end+1;
    }
    const std::string prefix=base+(base.back()=='/'?"":"/");const auto result=assetCanonical(prefix+relative);
    if(result.size()<=prefix.size()||result.compare(0,prefix.size(),prefix)!=0)throw std::runtime_error("Asset path escapes local directory");
    return result;
}

// Read only bounded ordinary files, proving the type on the opened descriptor.
// A pre-stat plus fopen is unsafe: a FIFO can replace the path before open and
// block before any size check. Canonical relative-path confinement is retained
// separately by the existing shared path helpers.
std::vector<unsigned char> readRegularFile(const std::string &path,size_t limit,size_t exact=0) {
    oot_asset_path::validatePath(path);
#ifdef _WIN32
    const std::wstring wide=oot_asset_path::widePath(path);
    oot_asset_path::PathHandle handle(CreateFileW(wide.c_str(),GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr));
    if(handle.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open original asset: "+path);
    BY_HANDLE_FILE_INFORMATION info={};
    if(GetFileType(handle.value)!=FILE_TYPE_DISK||!GetFileInformationByHandle(handle.value,&info)
        ||(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
        throw std::runtime_error("Original asset must be an ordinary disk file: "+path);
    const uint64_t size=(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    if(size>limit||(exact&&size!=exact))throw std::runtime_error("Original asset size mismatch/limit: "+path);
    auto readAt=[&](unsigned char *out,size_t count,size_t offset)->size_t {
        OVERLAPPED operation={};operation.Offset=DWORD(offset);operation.OffsetHigh=DWORD(uint64_t(offset)>>32);
        const DWORD requested=DWORD(std::min<size_t>(count,0x40000000u));
        BOOL ok=ReadFile(handle.value,out,requested,nullptr,&operation);
        if(!ok){const DWORD error=GetLastError();if(error==ERROR_HANDLE_EOF)return 0;if(error!=ERROR_IO_PENDING)throw std::runtime_error("Cannot read original asset: "+path);}
        DWORD done=0;if(!GetOverlappedResult(handle.value,&operation,&done,TRUE)){
            if(GetLastError()==ERROR_HANDLE_EOF)return 0;
            throw std::runtime_error("Cannot complete original asset read: "+path);
        }
        return done;
    };
#else
    int flags=O_RDONLY|O_NONBLOCK;
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
    struct Descriptor {int value;explicit Descriptor(int v):value(v){}~Descriptor(){if(value>=0)::close(value);}Descriptor(const Descriptor&)=delete;Descriptor&operator=(const Descriptor&)=delete;};
    Descriptor handle(::open(path.c_str(),flags));
    if(handle.value<0)throw std::runtime_error("Cannot open original asset: "+path);
    struct stat info={};
    if(::fstat(handle.value,&info)!=0||!S_ISREG(info.st_mode))throw std::runtime_error("Original asset must be a regular file: "+path);
    if(info.st_size<0)throw std::runtime_error("Invalid original asset size: "+path);
    const uint64_t size=uint64_t(info.st_size);
    if(size>limit||(exact&&size!=exact))throw std::runtime_error("Original asset size mismatch/limit: "+path);
    auto readAt=[&](unsigned char *out,size_t count,size_t)->size_t {
        ssize_t done;do{done=::read(handle.value,out,count);}while(done<0&&errno==EINTR);
        if(done<0)throw std::runtime_error("Cannot read original asset: "+path);
        return size_t(done);
    };
#endif
    std::vector<unsigned char> bytes(static_cast<size_t>(size));size_t offset=0;
    while(offset<bytes.size()){const size_t n=readAt(bytes.data()+offset,bytes.size()-offset,offset);if(!n)throw std::runtime_error("Original asset shortened during read: "+path);offset+=n;}
    unsigned char extra=0;
    if(readAt(&extra,1,offset)!=0)throw std::runtime_error("Original asset grew during read: "+path);
    return bytes;
}

// Portable SHA-256 per FIPS 180-4, used for exported payload corruption checks.
// Manifest provenance labels alone are not authentication of an untrusted ROM.
uint32_t rotr(uint32_t n,unsigned bits){return(n>>bits)|(n<<(32-bits));}
std::string sha256(const std::vector<unsigned char> &input) {
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
    std::vector<unsigned char> data=input;const uint64_t bits=uint64_t(data.size())*8;data.push_back(128);
    while(data.size()%64!=56)data.push_back(0);
    for(int i=7;i>=0;--i)data.push_back((unsigned char)(bits>>(i*8)));
    for(size_t offset=0;offset<data.size();offset+=64) {
        uint32_t w[64];for(int i=0;i<16;++i){w[i]=0;for(int b=0;b<4;++b)w[i]=(w[i]<<8)|data[offset+i*4+b];}
        for(int i=16;i<64;++i){uint32_t a=rotr(w[i-15],7)^rotr(w[i-15],18)^(w[i-15]>>3),b=rotr(w[i-2],17)^rotr(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+a+w[i-7]+b;}
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(int i=0;i<64;++i){uint32_t s1=rotr(e,6)^rotr(e,11)^rotr(e,25),ch=(e&f)^(~e&g),t1=v+s1+ch+k[i]+w[i],s0=rotr(a,2)^rotr(a,13)^rotr(a,22),maj=(a&b)^(a&c)^(b&c),t2=s0+maj;v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    char result[65]={};for(int i=0;i<8;++i)std::snprintf(result+i*8,9,"%08x",unsigned(h[i]));return result;
}
struct Identity {uint16_t slot,node,vertices,corners;const char *vertexFile,*vertexHash,*cornerFile,*cornerHash;};
const Identity identities[]={
    {226,0,74,324,"ring_0.vtxbe","18ce65db3bc5db913374ad2cfec5a19931e5595e268ead6bcbdcd921856a3091","ring_0.cornersbe","8a6348ca9bb712a64fc947db761b2148727a1c71d1eda3808abe93d74bea6f47"},
    {248,0,127,450,"webdome2_0.vtxbe","136017ed551307a043ea8864767be9e046e7959c821110eb6030e70ea36ceea2","webdome2_0.cornersbe","6f5dadaf69946190e03e4fb3c823bcefbe6063ddb12eaaba188c13c753997e29"},
    {249,0,30,90,"webdome3_0.vtxbe","01e0aabda6068a995e25dd55763a03cd8f1afaf6aa02a7d95dbf01320310c8da","webdome3_0.cornersbe","013330dd96147ecbda4916ee884075c87f39404a227525790e314f731fec226c"},
    {249,1,30,90,"webdome3_1.vtxbe","b62a81690c68a8c3ec4a2bd749bafe01c0f2f38d3e1e22782440fdef436a90d3","webdome3_1.cornersbe","adc4c57ee8322856ff58d5a130a990828adb914772fdc510a76aa19c9c2da9ab"},
    {249,2,16,45,"webdome3_2.vtxbe","c941a4215bd9f97a414b6de05245beed9d2decd9befc69a949e7f5db26a0bfd4","webdome3_2.cornersbe","e8866437e5483b1e024dac7cad5ebb5b289c4775ff1b0452d72d5192bcf01059"},
    {249,3,30,90,"webdome3_3.vtxbe","69570aff3b17431263637c53189fd32b33d2b36eb73524b8a51fc2e81665fc33","webdome3_3.cornersbe","ff533e166c1c5de4ec099b5c9fb6cc92e42f88db92564edc3eae39011f2133cd"},
    {249,4,30,90,"webdome3_4.vtxbe","5e354bc649eea842c99d74ec5593770b80620e641c4d932af9c3b37f6f490f09","webdome3_4.cornersbe","9a6656c436b0277fc84a7bcaf9dc10c5182a670859ac934fe30eea29cba37648"},
};
int indexOf(uint16_t slot,uint16_t node){for(unsigned i=0;i<7;++i)if(identities[i].slot==slot&&identities[i].node==node)return int(i);return -1;}
}
struct SpidermanDomeAssets {
    bool ready=false;
    std::array<SmN64DomeGeometry,7> geometry={};
    std::array<std::vector<uint8_t>,2> pixels;
    std::string error;
};
extern "C" SpidermanDomeAssets *spiderman_dome_assets_create(void){try{return new SpidermanDomeAssets;}catch(...){return nullptr;}}
extern "C" void spiderman_dome_assets_destroy(SpidermanDomeAssets *a){delete a;}
extern "C" int spiderman_dome_assets_ready(const SpidermanDomeAssets *a){return a&&a->ready;}
extern "C" const char *spiderman_dome_assets_error(const SpidermanDomeAssets *a){return a?a->error.c_str():"Null original dome assets";}
extern "C" int spiderman_dome_assets_load(SpidermanDomeAssets *a,const char *directory){
    if(!a)return 0;
    a->ready=false;a->geometry={};for(auto&p:a->pixels)p.clear();
    try{
        if(!directory||!*directory)throw std::runtime_error("Missing private original dome asset directory");
        const std::string root=assetCanonical(directory);
        std::array<SmN64DomeGeometry,7> geometry={};std::array<std::vector<uint8_t>,2> pixels;
        for(unsigned i=0;i<7;++i){const auto &id=identities[i];const size_t vb=id.vertices*16u,cb=id.corners*8u;
            const auto vertices=readRegularFile(assetChild(root,id.vertexFile,64),vb,vb);
            const auto corners=readRegularFile(assetChild(root,id.cornerFile,64),cb,cb);
            if(sha256(vertices)!=id.vertexHash||sha256(corners)!=id.cornerHash)throw std::runtime_error("Original dome mesh fingerprint mismatch");
            if(!smn64_dome_geometry_decode(id.slot,id.node,vertices.data(),vertices.size(),corners.data(),corners.size(),&geometry[i]))throw std::runtime_error("Original dome mesh structure rejected");
        }
        const char *hashes[]={"6da481c2ab172a52dd718e6e2bc8cb2ec36743c38868115ae4efd84a35ef14c8","4193e1ec35019d2f7fc1420b85b671496310f6a033f02d57564d55208cd9c8fe"};
        for(unsigned i=0;i<2;++i){const size_t bytes=i?4096:16384;pixels[i]=readRegularFile(assetChild(root,"texture_"+std::to_string(403+i)+"_0.rgba",64),bytes,bytes);if(sha256(pixels[i])!=hashes[i])throw std::runtime_error("Original dome texture fingerprint mismatch");}
        a->geometry=geometry;a->pixels.swap(pixels);a->ready=true;a->error.clear();return 1;
    }catch(const std::exception&e){a->error=e.what();return 0;}catch(...){a->error="Original dome asset load failed";return 0;}
}
extern "C" int spiderman_dome_assets_copy_pool(const SpidermanDomeAssets*a,uint16_t slot,uint16_t node,SmN64DomeVertex*out,size_t capacity,size_t*count){
    const int i=indexOf(slot,node);if(!a||!a->ready||i<0||!out||!count||capacity<a->geometry[i].vertex_count)return 0;
    static_assert(sizeof(SmN64DomeVertex)==sizeof(SmN64DomeRenderVertex),"source pool layouts");
    static_assert(offsetof(SmN64DomeVertex,rgba)==offsetof(SmN64DomeRenderVertex,rgba),"source pool color offset");
    std::memcpy(out,a->geometry[i].vertices,a->geometry[i].vertex_count*sizeof(*out));*count=a->geometry[i].vertex_count;return 1;
}
extern "C" int spiderman_dome_assets_copy_geometry(const SpidermanDomeAssets*a,uint16_t slot,uint16_t node,SmN64DomeGeometry*out){const int i=indexOf(slot,node);if(!a||!a->ready||i<0||!out)return 0;*out=a->geometry[i];return 1;}
extern "C" int spiderman_dome_assets_copy_texture(const SpidermanDomeAssets*a,uint16_t slot,uint8_t*out,size_t capacity){if(!a||!a->ready||(slot!=403&&slot!=404)||!out||capacity<a->pixels[slot-403].size())return 0;std::copy(a->pixels[slot-403].begin(),a->pixels[slot-403].end(),out);return 1;}
extern "C" int spiderman_dome_snapshot(const SpidermanDomeAssets*a,const SpidermanDomeInstance*i,SpidermanDomeSnapshot*out){
    if(!a||!a->ready||!i||!out)return -1;
    const int index=indexOf(i->model_slot,i->node);
    if(index<0||i->body.poisoned||i->body.model!=i->node||!i->body.id||!i->body.render||!i->graphical_serial||i->body.render_bd!=1||(i->body.render_bc!=254&&i->body.render_bc!=255)||!i->current_pool||i->current_count!=a->geometry[index].vertex_count)return -1;
    if(!i->body.alive||(i->body.flags&1u))return 0;
    SpidermanDomeSnapshot value={};SmN64DomeRenderVertex current[SMN64_DOME_MAX_VERTICES];int32_t matrix[16];
    std::memcpy(current,i->current_pool,i->current_count*sizeof(*current));
    if(!smn64_dome_zero_rotation_matrix(i->body.position,i->body.flags,i->body.scale,matrix)||!smn64_dome_mesh_submit(&a->geometry[index],current,i->current_count,matrix,&value.draw))return -1;
    value.graphical_serial=i->graphical_serial;for(unsigned c=0;c<3;++c)value.logical_rgb[c]=uint8_t(i->body.rgb>>(8*c));value.instance_bc=i->body.render_bc;*out=value;return 1;
}
extern "C" int spiderman_dome_prepare(const SpidermanDomeSnapshot*s,const SpidermanDomeRenderState*before,SpidermanDomePacket*out,SpidermanDomeRenderState*after){
    if(!s||!before||!out||!after||!s->graphical_serial||indexOf(s->draw.model_slot,s->draw.node)<0||s->draw.corner_count!=identities[indexOf(s->draw.model_slot,s->draw.node)].corners||s->draw.texture_slot!=(s->draw.model_slot==226?404:403)||before->texture_cache_valid>1||before->scroll404.updated>1||!std::isfinite(before->source_delta_seconds)||before->source_delta_seconds<0||before->source_delta_seconds>60||std::fegetround()!=FE_TONEAREST)return 0;
    SpidermanDomeRenderState state=*before;SpidermanDomePacket packet={};packet.draw=s->draw;
    if(!state.texture_cache_valid||state.texture_slot!=s->draw.texture_slot){
        if(s->draw.texture_slot==404){uint32_t tile[2];if(!smn64_dome_scroll_select(&state.scroll404,state.source_delta_seconds,tile))return 0;}
        state.texture_slot=s->draw.texture_slot;state.texture_cache_valid=1;std::memset(state.environment_rgba,255,4);state.render_mode=0x0c184b50u;
    }
    if(!smn64_dome_material_for_actor(s->draw.texture_slot,s->logical_rgb,s->instance_bc,state.fog_rgba,&state.scroll404,&packet.material))return 0;
    std::memcpy(state.environment_rgba,packet.material.environment_rgba,4);state.render_mode=packet.material.render_mode;
    if(packet.material.writes_fog){std::memcpy(state.fog_rgba,packet.material.fog_rgba,4);state.texture_cache_valid=0;}
    *out=packet;*after=state;return 1;
}
