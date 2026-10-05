/* Original Spider-Man asset/pose rendering. The isolated GL dispatch and state
 * guard design is adapted from bk_duo_gl.cpp (and its BM64 predecessor) in this
 * repository. Indexed draw-buffer and double depth-range preservation matches
 * the separately tested spiderman_web_gl.cpp guard. No existing character backend is modified or invoked.
 * Original matrices and sidecar pixels are retained. The boot light rig,
 * normalized normals and nearest texture filtering are a host approximation,
 * not N64 RDP/RSP pixel parity. No inferred gameplay or animation cadence. */
#include "spiderman_gl.h"
#include "../utils/json.hpp"
#include "../utils/oot_asset_path.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifndef SPIDERMAN_CPU_ONLY
#include <SDL2/SDL.h>
#ifdef USE_GLES
#include <SDL2/SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef GL_MAX_VIEWPORTS
#define GL_MAX_VIEWPORTS 0x825B
#endif
#ifndef GL_MAX_DRAW_BUFFERS
#define GL_MAX_DRAW_BUFFERS 0x8824
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER_BINDING
#define GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_SAMPLER_BINDING
#define GL_SAMPLER_BINDING 0x8919
#endif
#ifndef GL_RASTERIZER_DISCARD
#define GL_RASTERIZER_DISCARD 0x8C89
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_SKIP_ROWS 0x0CF3
#define GL_UNPACK_SKIP_PIXELS 0x0CF4
#endif
#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif
#ifndef GL_VERTEX_ATTRIB_ARRAY_INTEGER
#define GL_VERTEX_ATTRIB_ARRAY_INTEGER 0x88FD
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR 0x88FE
#define GL_VERTEX_ATTRIB_ARRAY_LONG 0x874E
#endif
#ifndef GL_VERTEX_ATTRIB_ARRAY_LONG
#define GL_VERTEX_ATTRIB_ARRAY_LONG 0x874E
#endif
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST 0x0BC0
#define GL_COLOR_LOGIC_OP 0x0BF2
#endif
#ifndef GL_CONTEXT_FLAGS
#define GL_CONTEXT_FLAGS 0x821E
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x00000001
#endif
#ifndef GL_CONTEXT_PROFILE_MASK
#define GL_CONTEXT_PROFILE_MASK 0x9126
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#endif
#ifndef GL_POLYGON_MODE
#define GL_POLYGON_MODE 0x0B40
#define GL_FILL 0x1B02
#endif
#endif

namespace {
using Json = nlohmann::json;
typedef std::array<float, 16> Matrix;
typedef std::array<Matrix, 18> Frame;
typedef std::array<int16_t,216> IntegerFrame;
struct MarkerRecord { int16_t xyz[3]; uint16_t joint; };
struct Vertex { float position[3]; int st[2]; unsigned char rgba[4]; int matrix; };
struct Triangle { unsigned index[3], material; };
struct Texture { int width, height, wrap[2]; std::vector<unsigned char> rgba; };
struct Material { unsigned texture; bool normals, doubleSided; };
struct DrawVertex { float position[3], uv[2], shade[4]; };
struct Batch { unsigned material; size_t first, count; };
struct Asset {
    std::vector<Vertex> vertices; std::vector<Triangle> triangles;
    std::vector<Texture> textures; std::vector<Material> materials;
    std::array<std::vector<Frame>, 300> clips;
    std::array<std::vector<IntegerFrame>,300> integerClips;
    std::array<MarkerRecord,9> markers={};bool markersLoaded=false;
};
long long integer(const Json &j, long long lo, long long hi, const char *what) {
    if (!j.is_number_integer() || j.is_boolean()
        || (j.is_number_unsigned() && j.get<uint64_t>() > uint64_t(hi)))
        throw std::runtime_error(std::string("invalid integer: ") + what);
    const auto n = j.get<long long>();
    if (n < lo || n > hi) throw std::runtime_error(std::string("out of range: ") + what);
    return n;
}
float number(const Json &j, float lo, float hi, const char *what) {
    if (!j.is_number() || j.is_boolean()) throw std::runtime_error(std::string("invalid number: ") + what);
    float n = j.get<float>();
    if (!std::isfinite(n) || n < lo || n > hi) throw std::runtime_error(std::string("out of range: ") + what);
    return n;
}
const Json &array(const Json &j, size_t n, const char *what) {
    if (!j.is_array() || j.size() != n) throw std::runtime_error(std::string("wrong count: ") + what);
    return j;
}
bool boolean(const Json &j, const char *what) {
    if (!j.is_boolean()) throw std::runtime_error(std::string("invalid boolean: ") + what);
    return j.get<bool>();
}
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

Json readJson(const std::string &path,size_t limit=32u*1024u*1024u) {
    const auto bytes = readRegularFile(path,limit);
    return Json::parse(bytes.begin(), bytes.end(), [](int depth, Json::parse_event_t, Json &) {
        if (depth > 32) throw std::runtime_error("JSON nesting limit");
        return true;
    });
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
void checkHash(const std::vector<unsigned char> &bytes,const Json &expected,const char *what) {
    if(!expected.is_string()||expected.get<std::string>()!=sha256(bytes))throw std::runtime_error(std::string("SHA-256 mismatch: ")+what);
}

std::array<MarkerRecord,9> loadMarkers(const std::string &input) {
    std::string path=input;
    if(path.size()<5||path.substr(path.size()-5)!=".json")path+="/original-markers.json";
    const Json j=readJson(assetCanonical(path),64u*1024u);
    const auto &source=j.at("source"),&contract=j.at("contract");
    if(j.at("schema")!="n64codexlab.spiderman.markers.v1"
        ||source.at("rom_sha256")!="feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c"
        ||source.at("boot_sha256")!="1d3ed3384f45ada2ebf6cb0666ddc7fec4c4ffdb6fdb993b8d566aa3cd4f3867"
        ||source.at("model_shell_sha256")!="da72d0f76a9aa6643b3947a9990e64865a292a4ea4a5f7f2f8e359987bcbb3e8"
        ||source.at("table_sha256")!="90af21644169104c249026b57b8c5465cfd101d885ee96f24eed55f786fdb5d1")
        throw std::runtime_error("Wrong original Spider-Man marker identity");
    integer(source.at("boot_load_base"),0x80016ae0LL,0x80016ae0LL,"marker boot base");
    integer(source.at("table_address"),0x800f7964LL,0x800f7964LL,"marker table address");
    integer(source.at("model_slot"),117,117,"marker model slot");
    integer(source.at("model_joint_count"),18,18,"marker joint count");
    integer(j.at("count"),9,9,"marker count");
    const auto &range=array(source.at("table_boot_range"),2,"marker table source range");
    integer(range[0],921220,921220,"marker source start");integer(range[1],921296,921296,"marker source end");
    if(contract.at("record")!="3 big-endian signed16 authored coordinates + big-endian unsigned16 joint"
        ||contract.at("pose")!="12 signed16: row-major fixed12 rotation, then authored integer translation"
        ||contract.at("body_translation")!="retained integer matrix translation, separate from current body position"
        ||contract.at("position")!="native signed32 fixed12 world coordinates")
        throw std::runtime_error("Unsupported original marker coordinate contract");
    std::array<MarkerRecord,9> result={};std::vector<unsigned char> tableBytes={0,0,0,9};
    const auto &records=array(j.at("markers"),9,"marker records");
    for(size_t i=0;i<9;++i) {
        const auto &record=records[i];integer(record.at("index"),i,i,"marker index");
        integer(record.at("source_boot_offset"),921224+i*8,921224+i*8,"marker record source offset");
        integer(record.at("source_bytes"),8,8,"marker record source size");
        const auto &xyz=array(record.at("xyz_s16"),3,"marker coordinates");
        std::vector<unsigned char> bytes;bytes.reserve(8);
        for(int k=0;k<3;++k){result[i].xyz[k]=int16_t(integer(xyz[k],-32768,32767,"marker signed coordinate"));const uint16_t n=uint16_t(result[i].xyz[k]);bytes.push_back((unsigned char)(n>>8));bytes.push_back((unsigned char)n);}
        result[i].joint=uint16_t(integer(record.at("joint"),0,17,"marker joint"));
        bytes.push_back((unsigned char)(result[i].joint>>8));bytes.push_back((unsigned char)result[i].joint);
        checkHash(bytes,record.at("sha256"),"original marker record");
        tableBytes.insert(tableBytes.end(),bytes.begin(),bytes.end());
    }
    // This digest is pinned to the known original table, not merely compared
    // with a hash supplied by the same file. No authored records are bundled.
    checkHash(tableBytes,source.at("table_sha256"),"exact original marker table");
    return result;
}

void originalExecution(const Json &j) {
    if (j.at("kind") != "original N64 MIPS execution, cached decoded 0x2C frame path"
        || j.at("entry_address") != "0x80071AA4" || j.at("angle_matrix_address") != "0x800521A0"
        || j.at("pose_conversion_address") != "0x8005A244"
        || j.at("matrices_f32_layout") != "column-major 4x4, raw model units, absolute per-part rotation and world translation")
        throw std::runtime_error("Missing or incompatible original pose execution provenance");
}
void visit(size_t i, const std::array<int, 18> &parents, std::array<unsigned char, 18> &state) {
    if (state[i] == 1) throw std::runtime_error("Cyclic original hierarchy");
    if (state[i] == 2) return;
    state[i] = 1; if (parents[i] >= 0) visit(size_t(parents[i]), parents, state); state[i] = 2;
}
Matrix originalMatrix(const Json &raw) {
    array(raw, 16, "original matrix"); Matrix m;
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) {
        const int i = c * 4 + r;
        m[i] = number(raw[i], c == 3 ? -1e6f : -1.001f, c == 3 ? 1e6f : 1.001f, "original matrix");
        if (r == 3 && m[i] != (c == 3 ? 1.0f : 0.0f)) throw std::runtime_error("Nonaffine original matrix");
    }
    // The exported source R/T has no scale. Reject zero, skew and reflected
    // rotations; host scale belongs solely to the explicit instance transform.
    for (int a = 0; a < 3; ++a) for (int b = a; b < 3; ++b) {
        float dot = 0; for (int r = 0; r < 3; ++r) dot += m[a * 4 + r] * m[b * 4 + r];
        if (std::fabs(dot - (a == b ? 1.0f : 0.0f)) > 0.001f) throw std::runtime_error("Nonrigid original rotation");
    }
    const float determinant = m[0]*(m[5]*m[10]-m[9]*m[6]) - m[4]*(m[1]*m[10]-m[9]*m[2]) + m[8]*(m[1]*m[6]-m[5]*m[2]);
    if (std::fabs(determinant - 1.0f) > .001f) throw std::runtime_error("Reflected original rotation");
    return m;
}
Asset loadAsset(const std::string &input) {
    std::string path = input;
    if (path.size() < 5 || path.substr(path.size()-5) != ".json") path += "/spiderman_model.json";
    path = assetCanonical(path);
    const std::string base = path.substr(0, path.find_last_of("/\\"));
    const Json j = readJson(path); const auto &source = j.at("source");
    if (j.at("schema") != "n64codexlab.spiderman.original.v1"
        || source.at("rom_sha256") != "feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c"
        || source.at("rom_byte_order") != "big_endian" || source.at("model_name") != "spidey"
        || source.at("model_slot") != 117 || source.at("render_slot") != 251 || source.at("render_group") != 2
        || source.at("shell_sha256") != "da72d0f76a9aa6643b3947a9990e64865a292a4ea4a5f7f2f8e359987bcbb3e8"
        || source.at("render_sha256") != "1fa8b3258c9ef5adab4f60c7f383471fccb1787646064d2c68c99538346ec9f8")
        throw std::runtime_error("Wrong original Spider-Man USA 1.0 asset identity");
    const auto &contract = j.at("coordinate_contract");
    if (contract.at("vertex_multiplier") != 8 || contract.at("world_divisor") != 36
        || contract.at("texture_st_divisor") != 32 || contract.at("texture_half_texel_bias") != 16
        || contract.at("matrix_binding") != "global G_MTX"
        || contract.at("animation_timing") != "unknown") throw std::runtime_error("Unsupported original coordinate/timing contract");
    originalExecution(j.at("original_pose_execution"));
    const auto &light = j.at("light_rig");
    if (light.at("ambient") != Json({70,70,70}) || light.at("light") != Json({105,105,105})
        || light.at("direction") != Json({0,-127,0})) throw std::runtime_error("Unsupported original boot light rig");
    const auto &counts = j.at("counts");
    for (const auto &pair : {std::make_pair("objects",18), {"raw_pool_vertices",419}, {"render_vertices",1668},
            {"visible_triangles",556}, {"textures",13}, {"animation_slots",300}, {"decoded_clips",300}, {"decoded_frames",4196}})
        integer(counts.at(pair.first), pair.second, pair.second, pair.first);
    std::array<int, 18> parents; std::array<unsigned char, 18> visited = {};
    const auto &objects = array(j.at("objects"),18,"original bones");
    for (size_t i = 0; i < 18; ++i) {
        integer(objects[i].at("index"), i, i, "bone index");
        parents[i] = int(integer(objects[i].at("parent"),-1,17,"bone parent"));
        for (const auto &v : array(objects[i].at("bind_translation_fixed12"),3,"bind translation")) integer(v,INT32_MIN,INT32_MAX,"bind translation");
    }
    for (size_t i = 0; i < 18; ++i) visit(i,parents,visited);
    Asset a; a.textures.resize(13); std::array<bool,13> seen = {}; size_t textureBytes = 0;
    for (const auto &t : array(j.at("textures"),13,"original textures")) {
        const size_t slot = size_t(integer(t.at("slot"),2390,2402,"texture slot")-2390);
        if (seen[slot]) throw std::runtime_error("Duplicate texture slot");
        seen[slot] = true;
        Texture &out = a.textures[slot];
        out.width = int(integer(t.at("width"),1,4096,"texture width")); out.height = int(integer(t.at("height"),1,4096,"texture height"));
        out.wrap[0] = int(integer(t.at("wrap_s"),0,3,"texture wrap S")); out.wrap[1] = int(integer(t.at("wrap_t"),0,3,"texture wrap T"));
        integer(t.at("undecoded_payload_bytes"),0,0,"undecoded texture data");
        if (boolean(t.at("has_auxiliary_plane"),"auxiliary texture plane") || t.at("render_class") != "opaque") throw std::runtime_error("Unsupported source texture mode");
        const auto &level = array(t.at("levels"),1,"base-only original texture")[0];
        integer(level.at("width"),out.width,out.width,"texture level width"); integer(level.at("height"),out.height,out.height,"texture level height");
        const size_t bytes = size_t(out.width)*out.height*4; textureBytes += bytes;
        if (textureBytes > 64u*1024u*1024u) throw std::runtime_error("Texture memory budget exceeded");
        out.rgba = readRegularFile(assetChild(base,level.at("file").get<std::string>(),240),bytes,bytes);
        checkHash(out.rgba,level.at("sha256"),"original texture pixels");
    }
    for (const auto &m : array(j.at("materials"),34,"original materials")) {
        Material out; out.texture = unsigned(integer(m.at("texture_slot"),2390,2402,"material texture")-2390);
        integer(m.at("face_flags"),0,255,"original face flags");
        out.normals = boolean(m.at("has_normals"),"material normals"); out.doubleSided = boolean(m.at("double_sided"),"material culling");
        if (boolean(m.at("semi_transparent"),"material transparency") || integer(m.at("blend_rate"),0,3,"blend rate") != 0)
            throw std::runtime_error("Unsupported translucent original material; no guessed blend mode");
        a.materials.push_back(out);
    }
    for (const auto &v : array(j.at("vertices"),1668,"original render vertices")) {
        Vertex out; out.matrix = int(integer(v.at("matrix"),0,17,"vertex matrix")); integer(v.at("node"),0,0,"vertex node");
        integer(v.at("pool_vertex"),0,418,"raw vertex index");
        const auto &p = array(v.at("position"),3,"vertex position"), &uv = array(v.at("st"),2,"vertex ST"), &rgba = array(v.at("rgba"),4,"vertex normal/color");
        for (int k=0;k<3;++k) out.position[k] = float(integer(p[k],-32768,32767,"vertex position"));
        for (int k=0;k<2;++k) out.st[k] = int(integer(uv[k],-32768,32767,"vertex ST"));
        for (int k=0;k<4;++k) out.rgba[k] = (unsigned char)integer(rgba[k],0,255,"vertex normal/color");
        a.vertices.push_back(out);
    }
    for (const auto &t : array(j.at("triangles"),556,"original triangles")) {
        array(t,4,"triangle"); Triangle out;
        for (int k=0;k<3;++k) out.index[k] = unsigned(integer(t[k],0,1667,"triangle vertex index"));
        out.material = unsigned(integer(t[3],0,33,"triangle material")); a.triangles.push_back(out);
    }
    size_t totalFrames=0;
    for (const auto &ref : array(j.at("animations"),300,"original animation slots")) {
        const int slot = int(integer(ref.at("slot"),0,299,"animation slot"));
        if (!a.clips[slot].empty()) throw std::runtime_error("Duplicate original animation slot");
        const size_t count = size_t(integer(ref.at("frame_count"),1,4196,"frame count"));
        totalFrames += count; if (totalFrames > 4196) throw std::runtime_error("Animation frame budget exceeded");
        integer(ref.at("bone_count"),18,18,"clip bone count");
        if (!boolean(ref.at("original_pose_execution"),"original pose execution") || ref.at("status") != "decoded") throw std::runtime_error("Missing original executed poses");
        const auto clip = readJson(assetChild(base,ref.at("file").get<std::string>(),240));
        integer(clip.at("slot"),slot,slot,"clip slot"); integer(clip.at("frame_count"),count,count,"clip frames"); integer(clip.at("bone_count"),18,18,"clip bones");
        if (clip.at("status") != "decoded" || !clip.at("frame_rate").is_null() || !clip.at("action_role").is_null() || !clip.at("loop_mode").is_null()
            || clip.at("source_sha256") != ref.at("source_sha256") || clip.at("decoded_s16le_sha256") != ref.at("decoded_s16le_sha256"))
            throw std::runtime_error("Original clip provenance/timing mismatch");
        originalExecution(clip.at("original_pose_execution"));
        // Validate the authored six-s16 source samples too; they are never
        // fed through host trig as a replacement for the executed matrices.
        std::vector<unsigned char> sampleBytes; sampleBytes.reserve(count*18*6*2);
        for (const auto &frame : array(clip.at("frames"),count,"source frames"))
            for (const auto &bone : array(frame,18,"source frame bones"))
                for (const auto &n : array(bone,6,"source transform")) {
                    const uint16_t value=uint16_t(integer(n,-32768,32767,"source transform"));
                    sampleBytes.push_back((unsigned char)value);sampleBytes.push_back((unsigned char)(value>>8));
                }
        checkHash(sampleBytes,clip.at("decoded_s16le_sha256"),"original decoded sample grid");
        const auto &poses = array(clip.at("original_poses"),count,"original executed frames");
        auto &frames = a.clips[slot]; frames.reserve(count);
        auto &integerFrames=a.integerClips[slot];integerFrames.reserve(count);
        for (const auto &p : poses) {
            Frame frame; const auto &matrices = array(p.at("matrices_f32"),18,"original matrix palette");
            std::vector<unsigned char> matrixBytes;matrixBytes.reserve(18*16*4);
            for (size_t bone=0;bone<18;++bone) {
                frame[bone]=originalMatrix(matrices[bone]);
                for(float value:frame[bone]){uint32_t bits;static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559,"Original matrices require IEEE binary32");std::memcpy(&bits,&value,4);for(int byte=3;byte>=0;--byte)matrixBytes.push_back((unsigned char)(bits>>(8*byte)));}
            }
            checkHash(matrixBytes,p.at("matrices_f32be_sha256"),"original executed matrix frame");
            IntegerFrame integerFrame;std::vector<unsigned char> integerBytes;integerBytes.reserve(432);
            const auto &integers=array(p.at("poses_s16"),18,"original integer pose joints");
            for(size_t bone=0;bone<18;++bone)for(size_t cell=0;cell<12;++cell){
                const auto &joint=array(integers[bone],12,"original integer joint pose");
                const int16_t n=int16_t(integer(joint[cell],-32768,32767,"original integer pose cell"));
                integerFrame[bone*12+cell]=n;integerBytes.push_back((unsigned char)(uint16_t(n)>>8));integerBytes.push_back((unsigned char)uint16_t(n));
            }
            checkHash(integerBytes,p.at("poses_s16be_sha256"),"original integer pose frame");
            frames.push_back(frame);integerFrames.push_back(integerFrame);
        }
    }
    if (totalFrames != 4196) throw std::runtime_error("Missing original executed frames");
    return a;
}
bool finite(const float *values, size_t n, float limit=1e7f) {
    if (!values) return false;
    for(size_t i=0;i<n;++i) if(!std::isfinite(values[i]) || std::fabs(values[i]) > limit) return false;
    return true;
}
bool validFrame(const Asset &a, int slot, int frame) { return slot>=0 && slot<300 && frame>=0 && size_t(frame)<a.clips[slot].size(); }
void textureUV(const Vertex &v,const Texture &t,float uv[2]) {
    uv[0]=(v.st[0]+16.0f)/(32.0f*t.width); uv[1]=(v.st[1]+16.0f)/(32.0f*t.height);
}
bool validBody(const int16_t *body) {
    if(!body)return false;
    // Original9D258 normalizes only right. Inward retains -normal and forward
    // retains its cross-product magnitude. Do not impose a new near-unit or
    // determinant gate on source-defined, potentially non-unit matrices.
    // Match only the source column-degeneracy guard; never synthesize its
    // gameplay fallback here (that transition belongs to the original owner).
    for(int col=0;col<3;++col){int magnitude=0;for(int row=0;row<3;++row)magnitude+=std::abs(int(body[row*3+col]));if(magnitude<2048)return false;}
    return true;
}
bool pose(const Asset &a,int slot,int frame,const float *position,float yaw,float scale,std::vector<DrawVertex> &out,const int16_t *body=nullptr) {
    if(!validFrame(a,slot,frame)||!finite(position,3)||!std::isfinite(scale)||scale<=0||scale>10000||(body?!validBody(body):!finite(&yaw,1)))return false;
    float co=1,si=0;
    if(!body){const float angle=std::fmod(yaw,360.0f)*0.017453292519943295f;co=std::cos(angle);si=std::sin(angle);}
    Frame palette; const Frame &raw=a.clips[slot][size_t(frame)];
    for(size_t bone=0;bone<18;++bone) {
        Matrix &m=palette[bone]; m=raw[bone];
        for(int col=0;col<4;++col) {
            const float factor=scale*(col==3?1.0f/36:8.0f/36);
            if(body) {
                for(int row=0;row<3;++row){float n=float(body[row*3])*(1.0f/4096)*raw[bone][col*4];
                    n+=float(body[row*3+1])*(1.0f/4096)*raw[bone][col*4+1];
                    n+=float(body[row*3+2])*(1.0f/4096)*raw[bone][col*4+2];
                    m[col*4+row]=n*factor*(row==0?1.0f:-1.0f);
                }
            } else {
                const float x=raw[bone][col*4]*factor, y=-raw[bone][col*4+1]*factor, z=-raw[bone][col*4+2]*factor;
                m[col*4]=co*x+si*z; m[col*4+1]=y; m[col*4+2]=-si*x+co*z;
            }
        }
        for(int r=0;r<3;++r) m[12+r]+=position[r];
    }
    out.resize(a.vertices.size());
    for(size_t i=0;i<a.vertices.size();++i) {
        const Vertex &v=a.vertices[i]; const Matrix &m=palette[size_t(v.matrix)]; DrawVertex &d=out[i];
        for(int r=0;r<3;++r) d.position[r]=m[r]*v.position[0]+m[4+r]*v.position[1]+m[8+r]*v.position[2]+m[12+r];
        if(!finite(d.position,3,1e10f))return false;
        // Store the original signed normal transformed through source R/T.
        float n[3]={};for(int r=0;r<3;++r)for(int c=0;c<3;++c)n[r]+=m[c*4+r]*(v.rgba[c]<128?int(v.rgba[c]):int(v.rgba[c])-256);
        const float len=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        const float diffuse=len>0?std::max(0.0f,n[1]/len):0;
        for(int k=0;k<3;++k)d.shade[k]=(70.0f+105.0f*diffuse)/255.0f;
        d.shade[3]=v.rgba[3]/255.0f; d.uv[0]=d.uv[1]=0;
    }
    return true;
}
#ifndef SPIDERMAN_CPU_ONLY
Matrix multiply(const Matrix &a,const Matrix &b) {
    Matrix result={};for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)result[c*4+r]+=a[k*4+r]*b[c*4+k];return result;
}
#endif
}

#ifndef SPIDERMAN_CPU_ONLY
namespace {
#define GL_FUNCTIONS(X) \
 X(GLenum,GetError,(void)) X(void,GetIntegerv,(GLenum,GLint*)) X(void,GetBooleanv,(GLenum,GLboolean*)) X(void,GetFloatv,(GLenum,GLfloat*)) \
 X(const GLubyte*,GetString,(GLenum)) X(GLboolean,IsEnabled,(GLenum)) X(void,Enable,(GLenum)) X(void,Disable,(GLenum)) \
 X(void,ActiveTexture,(GLenum)) X(void,BindTexture,(GLenum,GLuint)) X(void,GenTextures,(GLsizei,GLuint*)) X(void,DeleteTextures,(GLsizei,const GLuint*)) \
 X(void,TexParameteri,(GLenum,GLenum,GLint)) X(void,TexImage2D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) X(void,PixelStorei,(GLenum,GLint)) \
 X(void,GenBuffers,(GLsizei,GLuint*)) X(void,BindBuffer,(GLenum,GLuint)) X(void,BufferData,(GLenum,GLsizeiptr,const void*,GLenum)) X(void,DeleteBuffers,(GLsizei,const GLuint*)) \
 X(GLuint,CreateShader,(GLenum)) X(void,ShaderSource,(GLuint,GLsizei,const GLchar* const*,const GLint*)) X(void,CompileShader,(GLuint)) \
 X(void,GetShaderiv,(GLuint,GLenum,GLint*)) X(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) X(void,DeleteShader,(GLuint)) \
 X(GLuint,CreateProgram,(void)) X(void,AttachShader,(GLuint,GLuint)) X(void,BindAttribLocation,(GLuint,GLuint,const GLchar*)) X(void,LinkProgram,(GLuint)) \
 X(void,GetProgramiv,(GLuint,GLenum,GLint*)) X(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) X(void,DeleteProgram,(GLuint)) X(void,UseProgram,(GLuint)) \
 X(GLint,GetUniformLocation,(GLuint,const GLchar*)) X(void,UniformMatrix4fv,(GLint,GLsizei,GLboolean,const GLfloat*)) X(void,Uniform1i,(GLint,GLint)) X(void,Uniform4iv,(GLint,GLsizei,const GLint*)) X(void,Uniform4fv,(GLint,GLsizei,const GLfloat*)) \
 X(void,EnableVertexAttribArray,(GLuint)) X(void,DisableVertexAttribArray,(GLuint)) X(void,VertexAttribPointer,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*)) \
 X(void,GetVertexAttribiv,(GLuint,GLenum,GLint*)) X(void,GetVertexAttribPointerv,(GLuint,GLenum,void**)) X(void,DrawArrays,(GLenum,GLint,GLsizei)) \
 X(void,CullFace,(GLenum)) X(void,FrontFace,(GLenum)) X(void,BlendFuncSeparate,(GLenum,GLenum,GLenum,GLenum)) X(void,BlendEquationSeparate,(GLenum,GLenum)) X(void,DepthFunc,(GLenum)) X(void,DepthMask,(GLboolean)) X(void,ColorMask,(GLboolean,GLboolean,GLboolean,GLboolean)) X(void,Viewport,(GLint,GLint,GLsizei,GLsizei))
struct GLFunctions {
#define DECLARE(ret,name,args) ret(APIENTRY *name)args=nullptr;
GL_FUNCTIONS(DECLARE)
#undef DECLARE
    void(APIENTRY *GenVertexArrays)(GLsizei,GLuint*)=nullptr;void(APIENTRY *BindVertexArray)(GLuint)=nullptr;void(APIENTRY *DeleteVertexArrays)(GLsizei,const GLuint*)=nullptr;
    void(APIENTRY *BindSampler)(GLuint,GLuint)=nullptr;void(APIENTRY *PolygonMode)(GLenum,GLenum)=nullptr;
    void(APIENTRY *GetDoublev)(GLenum,double*)=nullptr;
    void(APIENTRY *DepthRange)(double,double)=nullptr;void(APIENTRY *DepthRangef)(GLfloat,GLfloat)=nullptr;
    void(APIENTRY *VertexAttribIPointer)(GLuint,GLint,GLenum,GLsizei,const void*)=nullptr;
    void(APIENTRY *VertexAttribLPointer)(GLuint,GLint,GLenum,GLsizei,const void*)=nullptr;
    void(APIENTRY *VertexAttribDivisor)(GLuint,GLuint)=nullptr;
    void(APIENTRY *GetBooleani)(GLenum,GLuint,GLboolean*)=nullptr;
    void(APIENTRY *GetIntegeri)(GLenum,GLuint,GLint*)=nullptr;
    GLboolean(APIENTRY *IsEnabledi)(GLenum,GLuint)=nullptr;
    void(APIENTRY *Enablei)(GLenum,GLuint)=nullptr;void(APIENTRY *Disablei)(GLenum,GLuint)=nullptr;
    void(APIENTRY *ColorMaski)(GLuint,GLboolean,GLboolean,GLboolean,GLboolean)=nullptr;
    void(APIENTRY *BlendFuncSeparatei)(GLuint,GLenum,GLenum,GLenum,GLenum)=nullptr;
    void(APIENTRY *BlendEquationSeparatei)(GLuint,GLenum,GLenum)=nullptr;
    void(APIENTRY *GetFloatViewport)(GLenum,GLuint,GLfloat*)=nullptr;
    void(APIENTRY *GetDoubleViewport)(GLenum,GLuint,double*)=nullptr;
    void(APIENTRY *ViewportIndexed)(GLuint,GLfloat,GLfloat,GLfloat,GLfloat)=nullptr;
    void(APIENTRY *DepthRangeIndexed)(GLuint,double,double)=nullptr;
    void(APIENTRY *DepthRangeIndexedf)(GLuint,GLfloat,GLfloat)=nullptr;
    GLboolean(APIENTRY *IsEnabledViewport)(GLenum,GLuint)=nullptr;
    void(APIENTRY *EnableViewport)(GLenum,GLuint)=nullptr;void(APIENTRY *DisableViewport)(GLenum,GLuint)=nullptr;
    bool indexedViewports=false;GLint viewportCount=1;
    bool indexedMasks=false,indexedBlend=false;GLint drawBuffers=1;
    bool es=false,compat=false,modern=false,vaoSupported=false,samplers=false,unpackRows=false,unpackBuffer=false,raster=false;
    bool integerAttrs=false,longAttrs=false,divisorAttrs=false;
#ifdef SPIDERMAN_TESTING
    bool injectProbeError=false;
#endif
    bool load(){
#define LOAD(ret,name,args) name=(ret(APIENTRY*)args)SDL_GL_GetProcAddress("gl" #name);if(!name)return false;
GL_FUNCTIONS(LOAD)
#undef LOAD
        const char *v=(const char*)GetString(GL_VERSION);if(!v)return false;
        es=std::strstr(v,"OpenGL ES")!=nullptr;while(*v&&(*v<'0'||*v>'9'))++v;
        int major=0,minor=0;std::sscanf(v,"%d.%d",&major,&minor);
        std::string extensionList;
        if(major>=3) {
            auto getStringi=(const GLubyte*(APIENTRY*)(GLenum,GLuint))SDL_GL_GetProcAddress("glGetStringi");
            if(!getStringi)return false;
            GLint count=0;GetIntegerv(GL_NUM_EXTENSIONS,&count);if(count<0||count>16384)return false;
            for(GLint i=0;i<count;++i){const char *name=(const char*)getStringi(GL_EXTENSIONS,GLuint(i));if(name){extensionList+=name;extensionList+=' ';}}
        } else {const char *list=(const char*)GetString(GL_EXTENSIONS);if(list)extensionList=list;}
        const char *extensions=extensionList.c_str();
        auto extension=[&](const char *name){
            if(!extensions)return false;
            const size_t n=std::strlen(name);const char *at=extensions;
            while((at=std::strstr(at,name))!=nullptr){if((at==extensions||at[-1]==' ')&&(at[n]==' '||at[n]=='\0'))return true;at+=n;}return false;
        };
        compat=!es;
        if(!es&&major==3&&minor==0){GLint flags=0;GetIntegerv(GL_CONTEXT_FLAGS,&flags);compat=(flags&GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)==0;}
        if(!es&&major==3&&minor==1)compat=extension("GL_ARB_compatibility");
        if(!es&&(major>3||(major==3&&minor>=2))){GLint profile=0;GetIntegerv(GL_CONTEXT_PROFILE_MASK,&profile);compat=(profile&GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)!=0;}
        modern=!es&&major>=3;unpackRows=!es||major>=3||extension("GL_EXT_unpack_subimage");
        raster=major>=3||extension("GL_EXT_transform_feedback")||extension("GL_NV_transform_feedback");
        unpackBuffer=(!es&&(major>2||(major==2&&minor>=1)))||(es&&major>=3)||extension("GL_ARB_pixel_buffer_object")||extension("GL_NV_pixel_buffer_object");
        samplers=(!es&&(major>3||(major==3&&minor>=3)))||(es&&major>=3)||extension("GL_ARB_sampler_objects");
        integerAttrs=major>=3||extension("GL_EXT_gpu_shader4");
        longAttrs=(!es&&(major>4||(major==4&&minor>=1)))||extension("GL_ARB_vertex_attrib_64bit");
        divisorAttrs=(!es&&(major>3||(major==3&&minor>=3)))||(es&&major>=3)||extension("GL_ARB_instanced_arrays")||extension("GL_ANGLE_instanced_arrays")||extension("GL_EXT_instanced_arrays")||extension("GL_NV_instanced_arrays");
#define OPT(name,type) name=(type)SDL_GL_GetProcAddress("gl" #name)
        OPT(GenVertexArrays,void(APIENTRY*)(GLsizei,GLuint*));OPT(BindVertexArray,void(APIENTRY*)(GLuint));OPT(DeleteVertexArrays,void(APIENTRY*)(GLsizei,const GLuint*));
        OPT(BindSampler,void(APIENTRY*)(GLuint,GLuint));OPT(PolygonMode,void(APIENTRY*)(GLenum,GLenum));OPT(DepthRange,void(APIENTRY*)(double,double));OPT(DepthRangef,void(APIENTRY*)(GLfloat,GLfloat));
        OPT(VertexAttribIPointer,void(APIENTRY*)(GLuint,GLint,GLenum,GLsizei,const void*));OPT(VertexAttribLPointer,void(APIENTRY*)(GLuint,GLint,GLenum,GLsizei,const void*));OPT(VertexAttribDivisor,void(APIENTRY*)(GLuint,GLuint));
#undef OPT
        if(!es){GetDoublev=(void(APIENTRY*)(GLenum,double*))SDL_GL_GetProcAddress("glGetDoublev");if(!GetDoublev)return false;}
        if(major<3&&extension("GL_OES_vertex_array_object")) {
            GenVertexArrays=(void(APIENTRY*)(GLsizei,GLuint*))SDL_GL_GetProcAddress("glGenVertexArraysOES");
            BindVertexArray=(void(APIENTRY*)(GLuint))SDL_GL_GetProcAddress("glBindVertexArrayOES");
            DeleteVertexArrays=(void(APIENTRY*)(GLsizei,const GLuint*))SDL_GL_GetProcAddress("glDeleteVertexArraysOES");
        }
        if(major<3&&extension("GL_EXT_gpu_shader4"))VertexAttribIPointer=(void(APIENTRY*)(GLuint,GLint,GLenum,GLsizei,const void*))SDL_GL_GetProcAddress("glVertexAttribIPointerEXT");
        if(divisorAttrs&&!((!es&&(major>3||(major==3&&minor>=3)))||(es&&major>=3))) {
            const char *name=extension("GL_ARB_instanced_arrays")?"glVertexAttribDivisorARB":extension("GL_ANGLE_instanced_arrays")?"glVertexAttribDivisorANGLE":extension("GL_EXT_instanced_arrays")?"glVertexAttribDivisorEXT":"glVertexAttribDivisorNV";
            VertexAttribDivisor=(void(APIENTRY*)(GLuint,GLuint))SDL_GL_GetProcAddress(name);
        }
        const bool esIndexed=(es&&(major>3||(major==3&&minor>=2)))||extension("GL_EXT_draw_buffers_indexed")||extension("GL_OES_draw_buffers_indexed");
        indexedMasks=(!es&&(major>=3||extension("GL_EXT_draw_buffers2")))||esIndexed;
        indexedBlend=(!es&&(major>=4||extension("GL_ARB_draw_buffers_blend")))||esIndexed;
        if(indexedMasks){
            // ES2 indexed-write extensions provide no portable indexed query
            // contract sufficient to restore arbitrary borrowed host state.
            if(es&&major<3)return false;
            const bool oldDesktop=!es&&major<3;
            const std::string suffix=es&&!(major>3||(major==3&&minor>=2))?(extension("GL_EXT_draw_buffers_indexed")?"EXT":"OES"):"";
            auto proc=[&](const char *core,const char *old){return SDL_GL_GetProcAddress((oldDesktop?std::string(old):std::string(core)+suffix).c_str());};
            GetBooleani=(void(APIENTRY*)(GLenum,GLuint,GLboolean*))proc("glGetBooleani_v","glGetBooleanIndexedvEXT");
            GetIntegeri=(void(APIENTRY*)(GLenum,GLuint,GLint*))proc("glGetIntegeri_v","glGetIntegerIndexedvEXT");
            // ES integer queries are core ES3.0, Boolean queries only ES3.1.
            // EXT/OES add indexed mask pnames to the integer query too.
            if(es&&major>=3){GetBooleani=(major>3||minor>=1)?(void(APIENTRY*)(GLenum,GLuint,GLboolean*))SDL_GL_GetProcAddress("glGetBooleani_v"):nullptr;GetIntegeri=(void(APIENTRY*)(GLenum,GLuint,GLint*))SDL_GL_GetProcAddress("glGetIntegeri_v");}
            IsEnabledi=(GLboolean(APIENTRY*)(GLenum,GLuint))proc("glIsEnabledi","glIsEnabledIndexedEXT");
            Enablei=(void(APIENTRY*)(GLenum,GLuint))proc("glEnablei","glEnableIndexedEXT");Disablei=(void(APIENTRY*)(GLenum,GLuint))proc("glDisablei","glDisableIndexedEXT");
            ColorMaski=(void(APIENTRY*)(GLuint,GLboolean,GLboolean,GLboolean,GLboolean))proc("glColorMaski","glColorMaskIndexedEXT");
            if((!GetBooleani&&!GetIntegeri)||!IsEnabledi||!Enablei||!Disablei||!ColorMaski)return false;
            GetIntegerv(GL_MAX_DRAW_BUFFERS,&drawBuffers);if(drawBuffers<1||drawBuffers>256)return false;
            if(indexedBlend){const std::string blendSuffix=!es&&major<4?"ARB":suffix;
                BlendFuncSeparatei=(void(APIENTRY*)(GLuint,GLenum,GLenum,GLenum,GLenum))SDL_GL_GetProcAddress(("glBlendFuncSeparatei"+blendSuffix).c_str());
                BlendEquationSeparatei=(void(APIENTRY*)(GLuint,GLenum,GLenum))SDL_GL_GetProcAddress(("glBlendEquationSeparatei"+blendSuffix).c_str());
                if(!GetIntegeri||!BlendFuncSeparatei||!BlendEquationSeparatei)return false;
            }
        }
        indexedViewports=(!es&&(major>4||(major==4&&minor>=1)||extension("GL_ARB_viewport_array")))||(es&&(extension("GL_OES_viewport_array")||extension("GL_NV_viewport_array")));
        if(indexedViewports){
            const std::string suffix=es?(extension("GL_OES_viewport_array")?"OES":"NV"):"";
            GetFloatViewport=(void(APIENTRY*)(GLenum,GLuint,GLfloat*))SDL_GL_GetProcAddress(("glGetFloati_v"+suffix).c_str());
            ViewportIndexed=(void(APIENTRY*)(GLuint,GLfloat,GLfloat,GLfloat,GLfloat))SDL_GL_GetProcAddress(("glViewportIndexedf"+suffix).c_str());
            // OES viewport arrays use ES3.2 core enables, or the advertised
            // EXT/OES indexed enables on earlier ES versions. NV has its own.
            const std::string enableSuffix=!es?"":suffix=="NV"?"NV":(major>3||(major==3&&minor>=2))?"":extension("GL_EXT_draw_buffers_indexed")?"EXT":"OES";
            IsEnabledViewport=(GLboolean(APIENTRY*)(GLenum,GLuint))SDL_GL_GetProcAddress(("glIsEnabledi"+enableSuffix).c_str());
            EnableViewport=(void(APIENTRY*)(GLenum,GLuint))SDL_GL_GetProcAddress(("glEnablei"+enableSuffix).c_str());
            DisableViewport=(void(APIENTRY*)(GLenum,GLuint))SDL_GL_GetProcAddress(("glDisablei"+enableSuffix).c_str());
            if(!GetFloatViewport||!ViewportIndexed||!IsEnabledViewport||!EnableViewport||!DisableViewport)return false;
            if(es){DepthRangeIndexedf=(void(APIENTRY*)(GLuint,GLfloat,GLfloat))SDL_GL_GetProcAddress(("glDepthRangeIndexedf"+suffix).c_str());if(!DepthRangeIndexedf)return false;}
            else{GetDoubleViewport=(void(APIENTRY*)(GLenum,GLuint,double*))SDL_GL_GetProcAddress("glGetDoublei_v");DepthRangeIndexed=(void(APIENTRY*)(GLuint,double,double))SDL_GL_GetProcAddress("glDepthRangeIndexed");if(!GetDoubleViewport||!DepthRangeIndexed)return false;}
            GetIntegerv(GL_MAX_VIEWPORTS,&viewportCount);if(viewportCount<1||viewportCount>256)return false;
        }
        vaoSupported=(major>=3||extension("GL_ARB_vertex_array_object")||extension("GL_OES_vertex_array_object"))&&GenVertexArrays&&BindVertexArray&&DeleteVertexArrays;
        if((samplers&&!BindSampler)||(integerAttrs&&!VertexAttribIPointer)||(longAttrs&&!VertexAttribLPointer)||(divisorAttrs&&!VertexAttribDivisor))return false;
#ifdef SPIDERMAN_TEST_FORCE_NO_VAO
        if(!modern)vaoSupported=false; // Exercise guarded legacy extension-state fallback.
#endif
#ifdef SPIDERMAN_TESTING
        if(injectProbeError){injectProbeError=false;Viewport(0,0,-1,1);}
#endif
        return (es?DepthRangef!=nullptr:(DepthRange&&PolygonMode))&&(!modern||vaoSupported);
    }
};
const GLenum CAPABILITIES[]={GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_POLYGON_OFFSET_FILL,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
struct AttrState {GLint enabled,size,type,normalized,stride,buffer,integer=0,longType=0,divisor=0;void *pointer;};
struct IndexedDrawState {GLboolean blend=0,mask[4]={};GLint srcRGB=0,dstRGB=0,srcAlpha=0,dstAlpha=0,eqRGB=0,eqAlpha=0;};
struct IndexedViewportState {GLfloat rect[4]={};double range[2]={};GLboolean scissor=0;};
struct GLState {
    GLFunctions &g;GLint active,texture,program,array,vao=0,sampler=0,unpack=4,row=0,skipRows=0,skipPixels=0,unpackBuffer=0,vp[4],depthFunc,polygon[2]={GL_FILL,GL_FILL};
    GLint blendSrcRGB,blendDstRGB,blendSrcAlpha,blendDstAlpha,blendEqRGB,blendEqAlpha,cullFace,frontFace;
    GLboolean enabled[8],depthWrite,colorWrite[4],raster=0,alphaTest=0,logicOp=0;double depthRange[2];AttrState attr[3];
    std::vector<IndexedDrawState> indexed;
    std::vector<IndexedViewportState> viewports;
    explicit GLState(GLFunctions &gl):g(gl){
        // Unindexed Viewport/DepthRange/SCISSOR_TEST operations broadcast to
        // all viewport-array slots. Preserve their fractional rectangles and
        // independent depth/scissor state, including slots not drawn here.
        if(g.indexedViewports){viewports.resize(size_t(g.viewportCount));for(GLuint i=0;i<viewports.size();++i){auto &s=viewports[i];g.GetFloatViewport(GL_VIEWPORT,i,s.rect);if(g.es){GLfloat range[2]={};g.GetFloatViewport(GL_DEPTH_RANGE,i,range);s.range[0]=range[0];s.range[1]=range[1];}else g.GetDoubleViewport(GL_DEPTH_RANGE,i,s.range);s.scissor=g.IsEnabledViewport(GL_SCISSOR_TEST,i);}}

        if(g.indexedMasks){indexed.resize(size_t(g.drawBuffers));for(GLuint i=0;i<indexed.size();++i){auto &s=indexed[i];s.blend=g.IsEnabledi(GL_BLEND,i);if(g.GetBooleani)g.GetBooleani(GL_COLOR_WRITEMASK,i,s.mask);else{GLint mask[4]={};g.GetIntegeri(GL_COLOR_WRITEMASK,i,mask);for(int c=0;c<4;++c)s.mask[c]=mask[c]?GL_TRUE:GL_FALSE;}if(g.indexedBlend){g.GetIntegeri(GL_BLEND_SRC_RGB,i,&s.srcRGB);g.GetIntegeri(GL_BLEND_DST_RGB,i,&s.dstRGB);g.GetIntegeri(GL_BLEND_SRC_ALPHA,i,&s.srcAlpha);g.GetIntegeri(GL_BLEND_DST_ALPHA,i,&s.dstAlpha);g.GetIntegeri(GL_BLEND_EQUATION_RGB,i,&s.eqRGB);g.GetIntegeri(GL_BLEND_EQUATION_ALPHA,i,&s.eqAlpha);}}}
        g.GetIntegerv(GL_ACTIVE_TEXTURE,&active);g.ActiveTexture(GL_TEXTURE0);g.GetIntegerv(GL_TEXTURE_BINDING_2D,&texture);if(g.samplers)g.GetIntegerv(GL_SAMPLER_BINDING,&sampler);
        g.GetIntegerv(GL_BLEND_SRC_RGB,&blendSrcRGB);g.GetIntegerv(GL_BLEND_DST_RGB,&blendDstRGB);g.GetIntegerv(GL_BLEND_SRC_ALPHA,&blendSrcAlpha);g.GetIntegerv(GL_BLEND_DST_ALPHA,&blendDstAlpha);g.GetIntegerv(GL_BLEND_EQUATION_RGB,&blendEqRGB);g.GetIntegerv(GL_BLEND_EQUATION_ALPHA,&blendEqAlpha);
        g.GetIntegerv(GL_CULL_FACE_MODE,&cullFace);g.GetIntegerv(GL_FRONT_FACE,&frontFace);
        g.GetIntegerv(GL_CURRENT_PROGRAM,&program);g.GetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        if(g.vaoSupported)g.GetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);else for(GLuint i=0;i<3;++i){g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&attr[i].enabled);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&attr[i].size);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&attr[i].type);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&attr[i].normalized);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&attr[i].stride);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&attr[i].buffer);g.GetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&attr[i].pointer);
            if(g.integerAttrs)g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_INTEGER,&attr[i].integer);
            if(g.longAttrs)g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_LONG,&attr[i].longType);
            if(g.divisorAttrs)g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_DIVISOR,&attr[i].divisor);}
        g.GetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);if(g.unpackRows){g.GetIntegerv(GL_UNPACK_ROW_LENGTH,&row);g.GetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);g.GetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);}if(g.unpackBuffer)g.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
        if(!g.es)g.GetIntegerv(GL_POLYGON_MODE,polygon);
        g.GetIntegerv(GL_VIEWPORT,vp);g.GetIntegerv(GL_DEPTH_FUNC,&depthFunc);g.GetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);g.GetBooleanv(GL_COLOR_WRITEMASK,colorWrite);if(g.es){GLfloat value[2]={};g.GetFloatv(GL_DEPTH_RANGE,value);depthRange[0]=value[0];depthRange[1]=value[1];}else g.GetDoublev(GL_DEPTH_RANGE,depthRange);
        for(int i=0;i<8;++i)enabled[i]=g.IsEnabled(CAPABILITIES[i]);
        if(g.raster)raster=g.IsEnabled(GL_RASTERIZER_DISCARD);
        if(g.compat)alphaTest=g.IsEnabled(GL_ALPHA_TEST);
        if(!g.es)logicOp=g.IsEnabled(GL_COLOR_LOGIC_OP);
    }
    ~GLState(){
        if(g.compat){if(alphaTest)g.Enable(GL_ALPHA_TEST);else g.Disable(GL_ALPHA_TEST);}
        if(!g.es){if(logicOp)g.Enable(GL_COLOR_LOGIC_OP);else g.Disable(GL_COLOR_LOGIC_OP);}
        g.CullFace(cullFace);g.FrontFace(frontFace);
        g.BlendFuncSeparate(blendSrcRGB,blendDstRGB,blendSrcAlpha,blendDstAlpha);g.BlendEquationSeparate(blendEqRGB,blendEqAlpha);
        g.UseProgram(program);if(g.vaoSupported)g.BindVertexArray(vao);else for(GLuint i=0;i<3;++i){g.BindBuffer(GL_ARRAY_BUFFER,attr[i].buffer);
            if(attr[i].longType)g.VertexAttribLPointer(i,attr[i].size,attr[i].type,attr[i].stride,attr[i].pointer);
            else if(attr[i].integer)g.VertexAttribIPointer(i,attr[i].size,attr[i].type,attr[i].stride,attr[i].pointer);
            else g.VertexAttribPointer(i,attr[i].size,attr[i].type,attr[i].normalized,attr[i].stride,attr[i].pointer);
            if(g.divisorAttrs)g.VertexAttribDivisor(i,GLuint(attr[i].divisor));
            if(attr[i].enabled)g.EnableVertexAttribArray(i);else g.DisableVertexAttribArray(i);}g.BindBuffer(GL_ARRAY_BUFFER,array);
        g.ActiveTexture(GL_TEXTURE0);g.BindTexture(GL_TEXTURE_2D,texture);if(g.samplers)g.BindSampler(0,sampler);g.ActiveTexture(active);
        g.PixelStorei(GL_UNPACK_ALIGNMENT,unpack);if(g.unpackRows){g.PixelStorei(GL_UNPACK_ROW_LENGTH,row);g.PixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);g.PixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);}if(g.unpackBuffer)g.BindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
        if(!g.es){if(polygon[0]==polygon[1])g.PolygonMode(GL_FRONT_AND_BACK,polygon[0]);else{g.PolygonMode(GL_FRONT,polygon[0]);g.PolygonMode(GL_BACK,polygon[1]);}}
        g.Viewport(vp[0],vp[1],vp[2],vp[3]);g.DepthFunc(depthFunc);g.DepthMask(depthWrite);g.ColorMask(colorWrite[0],colorWrite[1],colorWrite[2],colorWrite[3]);if(g.es)g.DepthRangef(depthRange[0],depthRange[1]);else g.DepthRange(depthRange[0],depthRange[1]);for(int i=0;i<8;++i){if(enabled[i])g.Enable(CAPABILITIES[i]);else g.Disable(CAPABILITIES[i]);}if(g.raster){if(raster)g.Enable(GL_RASTERIZER_DISCARD);else g.Disable(GL_RASTERIZER_DISCARD);}
        for(GLuint i=0;i<viewports.size();++i){const auto &s=viewports[i];g.ViewportIndexed(i,s.rect[0],s.rect[1],s.rect[2],s.rect[3]);if(g.es)g.DepthRangeIndexedf(i,s.range[0],s.range[1]);else g.DepthRangeIndexed(i,s.range[0],s.range[1]);if(s.scissor)g.EnableViewport(GL_SCISSOR_TEST,i);else g.DisableViewport(GL_SCISSOR_TEST,i);}
        for(GLuint i=0;i<indexed.size();++i){const auto &s=indexed[i];if(s.blend)g.Enablei(GL_BLEND,i);else g.Disablei(GL_BLEND,i);g.ColorMaski(i,s.mask[0],s.mask[1],s.mask[2],s.mask[3]);if(g.indexedBlend){g.BlendFuncSeparatei(i,s.srcRGB,s.dstRGB,s.srcAlpha,s.dstAlpha);g.BlendEquationSeparatei(i,s.eqRGB,s.eqAlpha);}}
    }
};
}
#endif
struct SpidermanGL {
    SpidermanErrorFn callback=nullptr; void *user=nullptr; std::string error,hostWarning;
    std::unique_ptr<Asset> asset; std::vector<DrawVertex> posed, stream; std::vector<Batch> batches;
#ifndef SPIDERMAN_CPU_ONLY
    GLFunctions gl; SDL_GLContext context=nullptr; GLuint program=0,vbo=0,vao=0; std::vector<GLuint> textures;
    GLint uMVP=-1,uTexture=-1;
#endif
    int fail(const std::string &message) { const bool changed=message!=error; error=message; if(changed&&callback)callback(user,error.c_str());return 0; }
};
#ifndef SPIDERMAN_CPU_ONLY
namespace {
void releaseGL(SpidermanGL &r) {
    if(r.context && SDL_GL_GetCurrentContext()==r.context) {
        auto &g=r.gl; if(r.program)g.DeleteProgram(r.program);if(r.vbo)g.DeleteBuffers(1,&r.vbo);if(r.vao)g.DeleteVertexArrays(1,&r.vao);
        if(!r.textures.empty())g.DeleteTextures(GLsizei(r.textures.size()),r.textures.data());
    }
    r.context=nullptr;r.program=r.vbo=r.vao=0;r.textures.clear();
}
struct ErrorFlags { std::string text; bool bounded=true,contextLost=false; };
ErrorFlags drainErrors(GLFunctions &g) {
    ErrorFlags result;
    for(int i=0;i<64;++i){const GLenum code=g.GetError();if(code==GL_NO_ERROR)return result;
        char detail[32];std::snprintf(detail,sizeof(detail),"0x%x",unsigned(code));
        if(!result.text.empty())result.text+=", ";
        result.text+=detail;
        if(code==0x0507)result.contextLost=true; // GL_CONTEXT_LOST, including ES.
    }
    result.bounded=false;return result;
}
bool checkGL(SpidermanGL &r,const char *phase) {
    const auto flags=drainErrors(r.gl);if(flags.text.empty())return true;
    return r.fail(std::string("Spider-Man GL error during ")+phase+": "+flags.text+(flags.bounded?"":" (error queue did not clear)"));
}
bool beginGLBoundary(SpidermanGL &r) {
    if(!SDL_GL_GetCurrentContext())return r.fail("Spider-Man draw needs a current SDL GL context");
    if(r.context&&r.context!=SDL_GL_GetCurrentContext())return r.fail("Spider-Man renderer belongs to a different GL context; recreate after context loss");
    // Resolve only GetError before issuing ANY renderer GL command, including
    // version/extension probes. This is the attribution boundary in the borrowed
    // host context. A stale host flag does not prove our later draw failed.
    r.gl.GetError=(GLenum(APIENTRY*)(void))SDL_GL_GetProcAddress("glGetError");
    if(!r.gl.GetError)return r.fail("Spider-Man GL error query unavailable");
    const auto flags=drainErrors(r.gl);
    if(!flags.text.empty()){
        const std::string message="Inherited host GL error before Spider-Man calls: "+flags.text;
        if(message!=r.hostWarning){r.hostWarning=message;if(r.callback)r.callback(r.user,message.c_str());else std::fprintf(stderr,"%s\n",message.c_str());}
    }
    if(!flags.bounded||flags.contextLost)return r.fail("Spider-Man cannot enter a lost/unresponsive host GL context: "+flags.text);
    return true;
}
GLuint shader(SpidermanGL &r,GLenum type,const std::string &source) {
    auto &g=r.gl;GLuint s=g.CreateShader(type);const char *p=source.c_str();g.ShaderSource(s,1,&p,nullptr);g.CompileShader(s);GLint good=0;g.GetShaderiv(s,GL_COMPILE_STATUS,&good);
    if(!good){char log[1024]={};g.GetShaderInfoLog(s,sizeof(log)-1,nullptr,log);r.fail(std::string("Spider-Man shader failed: ")+log);g.DeleteShader(s);return 0;}return s;
}
bool initGL(SpidermanGL &r) {
    if(!SDL_GL_GetCurrentContext())return r.fail("Spider-Man draw needs a current SDL GL context");
    if(r.context==SDL_GL_GetCurrentContext()&&r.program)return true;
    if(r.context&&r.context!=SDL_GL_GetCurrentContext())return r.fail("Spider-Man renderer belongs to a different GL context; recreate after context loss");
    const bool functionsLoaded=r.gl.load();
    if(!checkGL(r,"capability discovery"))return false;
    if(!functionsLoaded)return r.fail("Spider-Man GL functions unavailable");
    auto &g=r.gl;
    GLState saved(g);r.context=SDL_GL_GetCurrentContext();
    const std::string prefix=g.es?"#version 100\nprecision mediump float;\n":g.modern?"#version 130\n":"#version 120\n";
    const std::string vertexPrefix=g.es?"#version 100\nprecision highp float;\n":prefix;
    std::string vs=vertexPrefix+(g.modern?"in vec3 aPosition;in vec2 aUV;in vec4 aShade;out vec2 vUV;out vec4 vShade;":g.es?"attribute vec3 aPosition;attribute vec2 aUV;attribute vec4 aShade;varying mediump vec2 vUV;varying mediump vec4 vShade;":"attribute vec3 aPosition;attribute vec2 aUV;attribute vec4 aShade;varying vec2 vUV;varying vec4 vShade;");
    vs+="uniform mat4 uMVP;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vUV=aUV;vShade=aShade;}";
    std::string fs=prefix+(g.modern?"in vec2 vUV;in vec4 vShade;out vec4 outColor;":"varying vec2 vUV;varying vec4 vShade;");
    fs+="uniform sampler2D uTexture;void main(){vec4 color=";fs+=g.modern?"texture(uTexture,vUV)":"texture2D(uTexture,vUV)";
    fs+="*vShade;if(color.a<=0.5)discard;";fs+=g.modern?"outColor=color;}":"gl_FragColor=color;}";
    GLuint v=shader(r,GL_VERTEX_SHADER,vs),f=shader(r,GL_FRAGMENT_SHADER,fs);
    if(!v||!f){if(v)g.DeleteShader(v);if(f)g.DeleteShader(f);releaseGL(r);return false;}
    r.program=g.CreateProgram();g.AttachShader(r.program,v);g.AttachShader(r.program,f);g.BindAttribLocation(r.program,0,"aPosition");g.BindAttribLocation(r.program,1,"aUV");g.BindAttribLocation(r.program,2,"aShade");g.LinkProgram(r.program);g.DeleteShader(v);g.DeleteShader(f);
    GLint good=0;g.GetProgramiv(r.program,GL_LINK_STATUS,&good);
    if(!good){char log[1024]={};g.GetProgramInfoLog(r.program,sizeof(log)-1,nullptr,log);r.fail(std::string("Spider-Man shader link failed: ")+log);releaseGL(r);return false;}
    r.uMVP=g.GetUniformLocation(r.program,"uMVP");r.uTexture=g.GetUniformLocation(r.program,"uTexture");
    g.GenBuffers(1,&r.vbo);if(g.vaoSupported)g.GenVertexArrays(1,&r.vao);r.textures.resize(r.asset->textures.size());g.GenTextures(GLsizei(r.textures.size()),r.textures.data());
    g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);if(g.unpackBuffer)g.BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);g.PixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(g.unpackRows){g.PixelStorei(GL_UNPACK_ROW_LENGTH,0);g.PixelStorei(GL_UNPACK_SKIP_ROWS,0);g.PixelStorei(GL_UNPACK_SKIP_PIXELS,0);}
    for(size_t i=0;i<r.textures.size();++i) {
        const Texture &t=r.asset->textures[i];g.BindTexture(GL_TEXTURE_2D,r.textures[i]);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        auto wrap=[](int mode){return mode&2?GL_CLAMP_TO_EDGE:mode&1?GL_MIRRORED_REPEAT:GL_REPEAT;};
        g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap(t.wrap[0]));g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap(t.wrap[1]));
        g.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t.width,t.height,0,GL_RGBA,GL_UNSIGNED_BYTE,t.rgba.data());
    }
    if(!checkGL(r,"shader and texture initialization")){releaseGL(r);return false;}
    return true;
}
}
#endif
extern "C" SpidermanGL *spiderman_gl_create(SpidermanErrorFn callback,void *user) {
    try {auto *r=new SpidermanGL;r->callback=callback;r->user=user;return r;}catch(...){return nullptr;}
}
extern "C" int spiderman_gl_load(SpidermanGL *r,const char *path) {
    if(!r)return 0;
    r->asset.reset();r->posed.clear();r->stream.clear();r->batches.clear();
#ifndef SPIDERMAN_CPU_ONLY
    // Retain GL names for later correct-context cleanup, rather than overwrite
    // live names in an unrelated context. The CPU asset is already hidden.
    if(r->context&&SDL_GL_GetCurrentContext()!=r->context)return r->fail("Reload requires the owning Spider-Man GL context");
    releaseGL(*r);
#endif
    if(!path||!*path)return r->fail("Missing original Spider-Man asset directory");
    try {r->asset.reset(new Asset(loadAsset(path)));r->error.clear();r->hostWarning.clear();return 1;}
    catch(const std::exception &e){return r->fail(std::string("Original Spider-Man assets rejected: ")+e.what());}
    catch(...){return r->fail("Original Spider-Man asset load failed");}
}
extern "C" int spiderman_gl_is_loaded(const SpidermanGL *r){return r&&r->asset?1:0;}
extern "C" const char *spiderman_gl_last_error(const SpidermanGL *r){return r?r->error.c_str():"null Spider-Man renderer";}
extern "C" const char *spiderman_gl_last_host_warning(const SpidermanGL *r){return r?r->hostWarning.c_str():"";}
extern "C" int spiderman_gl_frame_count(const SpidermanGL *r,int slot){return r&&r->asset&&slot>=0&&slot<300?int(r->asset->clips[size_t(slot)].size()):0;}
extern "C" int spiderman_gl_valid_native_body_matrix(const int16_t matrix[9]) {return validBody(matrix);}
extern "C" int spiderman_gl_pose_s16(const SpidermanGL *r,int slot,int frame,int16_t *out,size_t capacity) {
    if(!r||!r->asset||!out||capacity<216||!validFrame(*r->asset,slot,frame))return 0;
    const auto &p=r->asset->integerClips[size_t(slot)][size_t(frame)];std::copy(p.begin(),p.end(),out);return 1;
}
extern "C" int spiderman_gl_load_markers(SpidermanGL *r,const char *path) {
    if(!r||!r->asset)return r?r->fail("Original model must be loaded before markers"):0;
    r->asset->markersLoaded=false;r->asset->markers={};
    if(!path||!*path)return r->fail("Missing original marker sidecar path");
    try{r->asset->markers=loadMarkers(path);r->asset->markersLoaded=true;r->error.clear();return 1;}
    catch(const std::exception &e){return r->fail(std::string("Original Spider-Man markers rejected: ")+e.what());}
    catch(...){return r->fail("Original Spider-Man marker load failed");}
}
extern "C" int spiderman_gl_marker_count(const SpidermanGL *r){return r&&r->asset&&r->asset->markersLoaded?9:0;}
extern "C" int spiderman_gl_marker_record(const SpidermanGL *r,int index,int16_t xyz[3],uint16_t *joint) {
    if(!r||!r->asset||!r->asset->markersLoaded||index<0||index>=9||!xyz||!joint)return 0;
    const auto &m=r->asset->markers[size_t(index)];std::copy(m.xyz,m.xyz+3,xyz);*joint=m.joint;return 1;
}
static int draw(SpidermanGL *r,const float view[16],const float projection[16],const int viewport[4],int clip,int frame,const float position[3],float yaw,float scale,const int16_t *body) {
    if(!r||!r->asset)return 0;
    if(!finite(view,16)||!finite(projection,16)||!viewport||viewport[2]<=0||viewport[3]<=0||viewport[2]>65536||viewport[3]>65536)return r->fail("Invalid Spider-Man camera/viewport");
    try {
        if(!pose(*r->asset,clip,frame,position,yaw,scale,r->posed,body))return r->fail("Invalid exact original Spider-Man frame/host transform");
#ifndef SPIDERMAN_CPU_ONLY
        if(!beginGLBoundary(*r))return 0;
        const bool initialized=initGL(*r);
        const bool initRestored=checkGL(*r,"initialization state restoration");
        if(!initialized||!initRestored)return 0;
        auto &g=r->gl;
        auto submit=[&]()->int {
        GLState saved(g);
        r->stream.clear();r->batches.clear();r->stream.reserve(r->asset->triangles.size()*3);
        for(const auto &tri:r->asset->triangles) {
            if(r->batches.empty()||r->batches.back().material!=tri.material){Batch b={tri.material,r->stream.size(),0};r->batches.push_back(b);}
            const auto &m=r->asset->materials[tri.material];const auto &t=r->asset->textures[m.texture];
            for(int corner=0;corner<3;++corner){const size_t index=tri.index[corner];DrawVertex v=r->posed[index];textureUV(r->asset->vertices[index],t,v.uv);
                if(!m.normals)for(int k=0;k<3;++k)v.shade[k]=r->asset->vertices[index].rgba[k]/255.0f;
                r->stream.push_back(v);
            }
            r->batches.back().count+=3;
        }
        Matrix v,p;std::copy(view,view+16,v.begin());std::copy(projection,projection+16,p.begin());const auto mvp=multiply(p,v);
        if(!finite(mvp.data(),16,1e15f))return r->fail("Unbounded Spider-Man camera product");
        g.UseProgram(r->program);g.UniformMatrix4fv(r->uMVP,1,GL_FALSE,mvp.data());g.Uniform1i(r->uTexture,0);
        if(g.vaoSupported)g.BindVertexArray(r->vao);
        g.BindBuffer(GL_ARRAY_BUFFER,r->vbo);g.BufferData(GL_ARRAY_BUFFER,GLsizeiptr(r->stream.size()*sizeof(DrawVertex)),r->stream.data(),GL_STREAM_DRAW);
        for(int i=0;i<3;++i){g.EnableVertexAttribArray(GLuint(i));if(g.divisorAttrs)g.VertexAttribDivisor(GLuint(i),0);}
        g.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,position));g.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,uv));g.VertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,shade));
        g.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);g.Enable(GL_DEPTH_TEST);g.DepthFunc(GL_LEQUAL);g.DepthMask(GL_TRUE);g.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        if(g.es)g.DepthRangef(0,1);else g.DepthRange(0,1);
        for(GLenum cap:CAPABILITIES)if(cap!=GL_DEPTH_TEST)g.Disable(cap);
        if(g.compat)g.Disable(GL_ALPHA_TEST);
        if(!g.es)g.Disable(GL_COLOR_LOGIC_OP);
        if(g.raster)g.Disable(GL_RASTERIZER_DISCARD);
        if(!g.es)g.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);g.FrontFace(GL_CCW);
        if(!checkGL(*r,"vertex upload/draw setup"))return 0;
        for(const auto &batch:r->batches) {
            const auto &m=r->asset->materials[batch.material];
            if(m.doubleSided)g.Disable(GL_CULL_FACE);else{g.Enable(GL_CULL_FACE);g.CullFace(GL_BACK);}
            g.BindTexture(GL_TEXTURE_2D,r->textures[m.texture]);g.DrawArrays(GL_TRIANGLES,GLint(batch.first),GLsizei(batch.count));
        }
        if(!checkGL(*r,"triangle submission"))return 0;
        return 1;
        };
        const int submitted=submit();
        const bool restored=checkGL(*r,"draw state restoration");
        if(!submitted||!restored)return 0;
        r->error.clear();return 1;
#else
        return r->fail("OpenGL drawing unavailable in CPU-only Spider-Man renderer build");
#endif
    }catch(const std::exception &e){return r->fail(std::string("Spider-Man draw failed: ")+e.what());}catch(...){return r->fail("Spider-Man draw failed");}
}
extern "C" int spiderman_gl_draw(SpidermanGL *r,const float view[16],const float projection[16],const int viewport[4],int clip,int frame,const float position[3],float yaw,float scale) {
    return draw(r,view,projection,viewport,clip,frame,position,yaw,scale,nullptr);
}
extern "C" int spiderman_gl_draw_native(SpidermanGL *r,const float view[16],const float projection[16],const int viewport[4],int clip,int frame,const float position[3],float scale,const int16_t body[9]) {
    if(!body)return r?r->fail("Missing original native body basis"):0;
    return draw(r,view,projection,viewport,clip,frame,position,0,scale,body);
}
extern "C" void spiderman_gl_destroy(SpidermanGL *r){if(!r)return;
#ifndef SPIDERMAN_CPU_ONLY
    releaseGL(*r);
#endif
    delete r;
}
#ifdef SPIDERMAN_TESTING
extern "C" int spiderman_test_regular_file(const char *path,size_t limit){try{if(!path)return 0;readRegularFile(path,limit);return 1;}catch(...){return 0;}}
extern "C" void spiderman_test_inject_probe_error(SpidermanGL *r){
#ifndef SPIDERMAN_CPU_ONLY
    if(r)r->gl.injectProbeError=true;
#else
    (void)r;
#endif
}
extern "C" size_t spiderman_test_position_floats(const SpidermanGL *r){return r&&r->asset?r->asset->vertices.size()*3:0;}
extern "C" int spiderman_test_pose(SpidermanGL *r,int slot,int frame,const float position[3],float yaw,float scale,float *positions,size_t capacity) {
    if(!r||!r->asset||!positions||capacity<spiderman_test_position_floats(r))return 0;
    try{if(!pose(*r->asset,slot,frame,position,yaw,scale,r->posed))return 0;
        for(size_t i=0;i<r->posed.size();++i)std::copy(r->posed[i].position,r->posed[i].position+3,positions+i*3);
        return 1;
    }catch(...){return 0;}
}
extern "C" int spiderman_test_pose_native(SpidermanGL *r,int slot,int frame,const float position[3],float scale,const int16_t body[9],float *out,size_t capacity) {
    if(!r||!r->asset||!body||!out||capacity<spiderman_test_position_floats(r))return 0;
    try{if(!pose(*r->asset,slot,frame,position,0,scale,r->posed,body))return 0;for(size_t i=0;i<r->posed.size();++i)std::copy(r->posed[i].position,r->posed[i].position+3,out+i*3);return 1;}catch(...){return 0;}
}
extern "C" int spiderman_test_vertex_native(SpidermanGL *r,int slot,int frame,const float position[3],float scale,const int16_t body[9],size_t vertex,float out[3],float shade[4]) {
    if(!r||!r->asset||!body||!out||!shade||vertex>=r->asset->vertices.size())return 0;
    try{if(!pose(*r->asset,slot,frame,position,0,scale,r->posed,body))return 0;const auto &v=r->posed[vertex];std::copy(v.position,v.position+3,out);std::copy(v.shade,v.shade+4,shade);return 1;}catch(...){return 0;}
}
extern "C" int spiderman_test_matrix(const SpidermanGL *r,int slot,int frame,int bone,float output[16]) {
    if(!r||!r->asset||!output||!validFrame(*r->asset,slot,frame)||bone<0||bone>=18)return 0;
    const auto &m=r->asset->clips[size_t(slot)][size_t(frame)][size_t(bone)];std::copy(m.begin(),m.end(),output);return 1;
}
extern "C" int spiderman_test_uv(const SpidermanGL *r,size_t vertex,size_t material,float uv[2]) {
    if(!r||!r->asset||!uv||vertex>=r->asset->vertices.size()||material>=r->asset->materials.size())return 0;
    textureUV(r->asset->vertices[vertex],r->asset->textures[r->asset->materials[material].texture],uv);return 1;
}
#endif
