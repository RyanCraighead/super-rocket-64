/* Original-source Spider-Man web submission. GL capability/state and safe-file
 * helpers adapted from spiderman_gl.cpp snapshot b010fa8dea5da5720d53d9dc0e20f120d0fd75e3139ea2ed70514b9643414da9.
 * Indexed-state corrections use body snapshot 5671a3913ac9569fa1d3bc87cdaf340bd2ff48611e224fb6e63f9bfb87016307.
 * Source gameplay geometry is consumed unchanged; host clipping/rasterization
 * is explicitly not an N64 RSP/RDP emulation. No RNG or lifetime advancement. */
#include "spiderman_web_gl.h"
#include "spiderman_web_attack_gl.h"
#include "spiderman_effects_gl.h"
#include "../utils/oot_asset_path.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cfenv>
#ifdef __FAST_MATH__
#error "Original web submission requires no fast-math and FP contraction disabled"
#endif
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
namespace {
struct DrawVertex {float position[4],uv[2],shade[4];};
struct Clip {double p[4];};
bool camera(const float *view,const float *projection,const int *vp,const int *native) {
    if(!view||!projection||!vp||!native||std::fegetround()!=FE_TONEAREST)return false;
    for(int i=0;i<16;++i)if(!std::isfinite(view[i])||!std::isfinite(projection[i])||std::fabs(view[i])>1e12f||std::fabs(projection[i])>1e12f)return false;
    return vp[0]>=-65536&&vp[0]<=65536&&vp[1]>=-65536&&vp[1]<=65536&&vp[2]>0&&vp[2]<=65536&&vp[3]>0&&vp[3]<=65536&&native[0]>0&&native[0]<=65536&&native[1]>0&&native[1]<=65536;
}
// 6AD80 -> B9588: binary32 conversion/scales, half-away-from-zero, then SH.
int16_t sourceCoordinate(int32_t fixed) {
    const float whole=float(fixed)*(1.0f/4096.0f),quarter=whole*.25f;
    const float rounded=quarter>0?quarter+.5f:quarter-.5f;
    const uint32_t bits=uint32_t(int32_t(rounded))&65535u;
    return bits<=32767u?int16_t(bits):int16_t(-1-int32_t(65535u-bits));
}
Clip transform(const int32_t xyz[3],const double m[16]) {
    // Source line model scale .25 and its native world/render 1/16 conversion
    // are explicit: (.25 * source_s16) *16 == source_s16*4 physics units.
    double p[4]={double(sourceCoordinate(xyz[0]))*4,-double(sourceCoordinate(xyz[1]))*4,-double(sourceCoordinate(xyz[2]))*4,1};
    Clip c={};for(int r=0;r<4;++r)for(int k=0;k<4;++k)c.p[r]+=m[k*4+r]*p[k];return c;
}
// Homogeneous Liang-Barsky; includes positive w before perspective expansion.
// Side planes are expanded by the ribbon halfwidth so an edge whose center is
// just outside the viewport still reaches the viewport through its width.
bool clipSegment(Clip &a,Clip &b,const int native[2],double *start=nullptr,double *end=nullptr) {
    const double marginX=1.5/native[0],marginY=1.5/native[1];
    const double planes[7][5]={{1,0,0,1+marginX,0},{-1,0,0,1+marginX,0},{0,1,0,1+marginY,0},{0,-1,0,1+marginY,0},{0,0,1,1,0},{0,0,-1,1,0},{0,0,0,1,-1e-7}};
    double lo=0,hi=1;for(const auto &plane:planes){double fa=plane[4],fb=plane[4];for(int k=0;k<4;++k){fa+=plane[k]*a.p[k];fb+=plane[k]*b.p[k];}if(fa<0&&fb<0)return false;if(fa<0)lo=std::max(lo,fa/(fa-fb));else if(fb<0)hi=std::min(hi,fa/(fa-fb));if(lo>hi)return false;}
    if(start)*start=lo;
    if(end)*end=hi;
    const Clip original=a;for(int k=0;k<4;++k){a.p[k]=original.p[k]+lo*(b.p[k]-original.p[k]);b.p[k]=original.p[k]+hi*(b.p[k]-original.p[k]);}return true;
}
void ribbon(Clip a,Clip b,const int native[2],const uint8_t rgb[3],std::vector<DrawVertex> &out) {
    if(!clipSegment(a,b,native))return;
    const double dx=(b.p[0]/b.p[3]-a.p[0]/a.p[3])*native[0]*.5,dy=(b.p[1]/b.p[3]-a.p[1]/a.p[3])*native[1]*.5;
    const double length=std::hypot(dx,dy);if(!std::isfinite(length)||length<1e-12)return;
    const double ox=-dy/length*1.5/native[0],oy=dx/length*1.5/native[1];
    DrawVertex corners[4]={};for(int i=0;i<4;++i){const Clip &p=i<2?a:b;const double sign=(i&1)?-1:1;for(int k=0;k<4;++k)corners[i].position[k]=float(p.p[k]);corners[i].position[0]=float(p.p[0]+sign*ox*p.p[3]);corners[i].position[1]=float(p.p[1]+sign*oy*p.p[3]);for(int k=0;k<3;++k)corners[i].shade[k]=rgb[k]/255.0f;corners[i].shade[3]=std::max(rgb[0],std::max(rgb[1],rgb[2]))/255.0f;}
    const unsigned indices[6]={0,1,2,2,1,3};for(unsigned i:indices)out.push_back(corners[i]);
}
bool geometry(const float *view,const float *projection,const int *vp,const int *native,const SpidermanWebStrandInstance *strands,size_t count,std::vector<DrawVertex> &out,int selectedChain=-1) {
    out.clear();if(!camera(view,projection,vp,native)||count>64||(count&&!strands))return false;
    for(size_t s=0;s<count;++s)if(!strands[s].geometry||strands[s].geometry->count<1||strands[s].geometry->count>40)return false;
    double m[16]={};for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)m[c*4+r]+=double(projection[k*4+r])*view[c*4+k];
    out.reserve(count*102*6);
    for(size_t s=0;s<count;++s){const auto &strand=*strands[s].geometry;for(int chain=0;chain<2;++chain){if(selectedChain>=0&&chain!=selectedChain)continue;const int32_t (*points)[3]=chain?strand.secondary:strand.primary;const int vertices=std::min(64,1+strand.count*(chain?2:1));Clip previous=transform(strand.anchor,m);for(int i=1;i<vertices;++i){Clip current=transform(points[i-1],m);if(i!=32)ribbon(previous,current,native,strands[s].line_rgb,out);previous=current;}}}
    return true;
}
struct Batch {size_t first,count;unsigned texture;};
bool quadGeometry(const float *view,const float *projection,const SpidermanWebQuad *quads,size_t count,std::vector<DrawVertex> &out,std::vector<Batch> &batches,bool trail=false) {
    if(count>4096||(count&&!quads))return false;
    for(size_t i=0;i<count;++i){const auto &q=quads[i];if((trail?q.texture_slot!=41:(q.texture_slot<46||q.texture_slot>47))||q.model_s16_16[3]||q.model_s16_16[7]||q.model_s16_16[11]||q.model_s16_16[15]!=65536)return false;for(uint8_t index:q.indices)if(index>3)return false;if(q.texture_slot==47)for(int v=1;v<4;++v)if(q.rgba[v][3]!=q.rgba[0][3])return false;}
    double m[16]={};for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)m[c*4+r]+=double(projection[k*4+r])*view[c*4+k];
    for(size_t i=0;i<count;++i){const auto &q=quads[i];DrawVertex vertices[4]={};const int width=trail?20:q.texture_slot==46?24:32,height=trail?10:width;
        for(int v=0;v<4;++v){double host[4]={0,0,0,1};for(int r=0;r<3;++r){host[r]=q.model_s16_16[12+r]/65536.0;for(int k=0;k<3;++k)host[r]+=q.model_s16_16[k*4+r]/65536.0*q.xyz[v][k];host[r]*=r==0?16:-16;}
            for(int r=0;r<4;++r){double p=0;for(int k=0;k<4;++k)p+=m[k*4+r]*host[k];vertices[v].position[r]=float(p);if(!std::isfinite(vertices[v].position[r]))return false;}
            for(int k=0;k<2;++k)vertices[v].uv[k]=(float(q.st[v][k])+16)/(32*(k?height:width));
            for(int k=0;k<4;++k)vertices[v].shade[k]=q.rgba[v][k]/255.0f;
        }
        const unsigned texture=trail?3:q.texture_slot-45;if(batches.empty()||batches.back().texture!=texture)batches.push_back({out.size(),0,texture});for(unsigned index:q.indices)out.push_back(vertices[index]);batches.back().count+=6;
    }return true;
}
// Source6B0DC lines use independently shaded endpoints and identity model.
// Interpolate both colors at homogeneous clip intersections before expanding
// the source widthfield0 into the same1.5-native-pixel host policy as web lines.
bool attackLineGeometry(const float *view,const float *projection,const int *vp,const int *native,const SpidermanWebAttackLine *lines,size_t count,std::vector<DrawVertex>&out){
    if(!camera(view,projection,vp,native)||count>4096||(count&&!lines))return false;
    double matrix[16]={};for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k)matrix[c*4+r]+=double(projection[k*4+r])*view[c*4+k];
    for(size_t i=0;i<count;++i){Clip p[2]={};const auto &line=lines[i];for(int v=0;v<2;++v){double host[4]={double(line.xyz[v][0])*16,-double(line.xyz[v][1])*16,-double(line.xyz[v][2])*16,1};for(int r=0;r<4;++r)for(int k=0;k<4;++k)p[v].p[r]+=matrix[k*4+r]*host[k];}
        double begin=0,end=1;if(!clipSegment(p[0],p[1],native,&begin,&end))continue;
        const double dx=(p[1].p[0]/p[1].p[3]-p[0].p[0]/p[0].p[3])*native[0]*.5,dy=(p[1].p[1]/p[1].p[3]-p[0].p[1]/p[0].p[3])*native[1]*.5;
        const double length=std::hypot(dx,dy);if(!std::isfinite(length)||length<1e-12)continue;
        const double ox=-dy/length*1.5/native[0],oy=dx/length*1.5/native[1];DrawVertex corners[4]={};
        for(int v=0;v<4;++v){const Clip &point=p[v/2];const double sign=(v&1)?-1:1;for(int k=0;k<4;++k)corners[v].position[k]=float(point.p[k]);corners[v].position[0]=float(point.p[0]+sign*ox*point.p[3]);corners[v].position[1]=float(point.p[1]+sign*oy*point.p[3]);const double t=v<2?begin:end;for(int k=0;k<4;++k)corners[v].shade[k]=float((line.rgba[0][k]+t*(int(line.rgba[1][k])-int(line.rgba[0][k])))/255);}
        const unsigned index[6]={0,1,2,2,1,3};for(unsigned v:index)out.push_back(corners[v]);
    }return true;
}
bool decalProjection(const float *projection,float *out){
    if(!projection||!out||std::fegetround()!=FE_TONEAREST)return false;
    for(int i=0;i<16;++i)if(!std::isfinite(projection[i]))return false;
    const unsigned zeros[]={1,2,3,4,6,7,12,13,15};for(unsigned i:zeros)if(projection[i]!=0)return false;
    if(projection[0]<=0||projection[5]<=0||projection[10]>=-1||projection[11]!=-1||projection[14]>=0)return false;
    // CE11C scales left/right/bottom/top/NEAR, but passes FAR unchanged at
    // CE1CC..1D8. Recover host nearPlane/farPlane in double to avoid unnecessary loss;
    // the original frustum owner and fixed-matrix quantization remain external.
    const double nearPlane=double(projection[14])/(double(projection[10])-1),farPlane=double(projection[14])/(double(projection[10])+1);const float factor=1.05f*1.05f;const double biasedNear=nearPlane*factor;
    if(!(nearPlane>0&&farPlane>biasedNear))return false;
    float result[16];std::copy(projection,projection+16,result);result[10]=float(-(farPlane+biasedNear)/(farPlane-biasedNear));result[14]=float(-(2*farPlane*biasedNear)/(farPlane-biasedNear));if(!std::isfinite(result[10])||!std::isfinite(result[14]))return false;std::copy(result,result+16,out);return true;
}
bool attackQuadGeometry(const float *view,const float *projection,const SpidermanWebAttackQuad *quads,size_t count,std::vector<DrawVertex>&out,std::vector<Batch>&batches,bool dome=false){
    if(count>4096||(count&&!quads))return false;
    for(size_t i=0;i<count;++i){const auto&q=quads[i].submitted;if((dome?q.texture_slot!=403:(q.texture_slot!=38&&q.texture_slot!=111&&q.texture_slot!=108))||q.model_s16_16[3]||q.model_s16_16[7]||q.model_s16_16[11]||q.model_s16_16[15]!=65536)return false;for(unsigned index:q.indices)if(index>3)return false;}
    float biased[16]={};bool needsBias=dome;for(size_t i=0;i<count;++i)needsBias|=quads[i].submitted.texture_slot==108;if(needsBias&&!decalProjection(projection,biased))return false;
    double standard[16]={},decal[16]={};for(int c=0;c<4;++c)for(int r=0;r<4;++r)for(int k=0;k<4;++k){standard[c*4+r]+=double(projection[k*4+r])*view[c*4+k];if(needsBias)decal[c*4+r]+=double(biased[k*4+r])*view[c*4+k];}
    for(size_t i=0;i<count;++i){const auto&q=quads[i].submitted;const double*m=(q.texture_slot==108||dome)?decal:standard;DrawVertex vertices[4]={};
        for(int v=0;v<4;++v){double host[4]={0,0,0,1};for(int r=0;r<3;++r){host[r]=q.model_s16_16[12+r]/65536.0;for(int k=0;k<3;++k)host[r]+=q.model_s16_16[k*4+r]/65536.0*q.xyz[v][k];host[r]*=r==0?16:-16;}for(int r=0;r<4;++r){double p=0;for(int k=0;k<4;++k)p+=m[k*4+r]*host[k];vertices[v].position[r]=float(p);if(!std::isfinite(vertices[v].position[r]))return false;}for(int k=0;k<2;++k)vertices[v].uv[k]=(float(q.st[v][k])+16)/1024;for(int k=0;k<4;++k)vertices[v].shade[k]=q.rgba[v][k]/255.0f;
            // Slot111 consumes ENV alpha, not source Vtx alpha. Carry that
            // independent exact byte in the shader's alpha input for this mode.
            if(q.texture_slot==111)vertices[v].shade[3]=quads[i].environment_alpha/255.0f;
            if(dome){for(int k=0;k<2;k++)vertices[v].uv[k]=(float(q.st[v][k])+16)/2048;vertices[v].shade[0]=vertices[v].shade[1]=vertices[v].shade[2]=0;vertices[v].shade[3]=quads[i].environment_alpha/255.0f;}
        }
        const unsigned texture=dome?7:q.texture_slot==38?4:q.texture_slot==111?5:6;if(batches.empty()||batches.back().texture!=texture)batches.push_back({out.size(),0,texture});for(unsigned index:q.indices)out.push_back(vertices[index]);batches.back().count+=6;
    }return true;
}
}
struct SpidermanWebGL {
    SpidermanWebErrorFn callback=nullptr;void *user=nullptr;std::string error,hostWarning;
    bool loaded=false,trailLoaded=false,attackLoaded=false,domeLoaded=false;std::array<std::vector<unsigned char>,3> attackPixels;std::array<std::vector<unsigned char>,2> pixels;std::vector<unsigned char> trailPixels,domePixels;
    std::vector<DrawVertex> stream;std::vector<Batch> batches;
#ifndef SPIDERMAN_CPU_ONLY
    GLFunctions gl;SDL_GLContext context=nullptr;GLuint program=0,vbo=0,vao=0;std::array<GLuint,8> textures={};GLint uTexture=-1,uRepeat=-1,uMaterial=-1;
#endif
    int fail(const std::string &s){bool changed=s!=error;error=s;if(changed&&callback)callback(user,s.c_str());return 0;}
};

#ifndef SPIDERMAN_CPU_ONLY
namespace {
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
bool checkGL(SpidermanWebGL &r,const char *phase) {
    const auto flags=drainErrors(r.gl);if(flags.text.empty())return true;
    return r.fail(std::string("Spider-Man web GL error during ")+phase+": "+flags.text+(flags.bounded?"":" (error queue did not clear)"));
}
bool beginGLBoundary(SpidermanWebGL &r) {
    if(!SDL_GL_GetCurrentContext())return r.fail("Spider-Man web draw needs a current SDL GL context");
    if(r.context&&r.context!=SDL_GL_GetCurrentContext())return r.fail("Spider-Man web renderer belongs to a different GL context; recreate after context loss");
    // Resolve only GetError before issuing ANY renderer GL command, including
    // version/extension probes. This is the attribution boundary in the borrowed
    // host context. A stale host flag does not prove our later draw failed.
    r.gl.GetError=(GLenum(APIENTRY*)(void))SDL_GL_GetProcAddress("glGetError");
    if(!r.gl.GetError)return r.fail("Spider-Man web GL error query unavailable");
    const auto flags=drainErrors(r.gl);
    if(!flags.text.empty()){
        const std::string message="Inherited host GL error before Spider-Man web calls: "+flags.text;
        if(message!=r.hostWarning){r.hostWarning=message;if(r.callback)r.callback(r.user,message.c_str());else std::fprintf(stderr,"%s\n",message.c_str());}
    }
    if(!flags.bounded||flags.contextLost)return r.fail("Spider-Man web cannot enter a lost/unresponsive host GL context: "+flags.text);
    return true;
}
GLuint shader(SpidermanWebGL &r,GLenum type,const std::string &source) {
    auto &g=r.gl;GLuint s=g.CreateShader(type);const char *p=source.c_str();g.ShaderSource(s,1,&p,nullptr);g.CompileShader(s);GLint good=0;g.GetShaderiv(s,GL_COMPILE_STATUS,&good);
    if(!good){char log[1024]={};g.GetShaderInfoLog(s,sizeof(log)-1,nullptr,log);r.fail(std::string("Spider-Man web shader failed: ")+log);g.DeleteShader(s);return 0;}return s;
}
void releaseGL(SpidermanWebGL &r) {
    if(r.context&&SDL_GL_GetCurrentContext()==r.context){auto &g=r.gl;if(r.program)g.DeleteProgram(r.program);if(r.vbo)g.DeleteBuffers(1,&r.vbo);if(r.vao)g.DeleteVertexArrays(1,&r.vao);g.DeleteTextures(8,r.textures.data());}
    r.context=nullptr;r.program=r.vbo=r.vao=0;r.textures={};
}
bool initGL(SpidermanWebGL &r) {
    if(r.context==SDL_GL_GetCurrentContext()&&r.program)return true;
    const bool functions=r.gl.load();if(!checkGL(r,"capability discovery"))return false;if(!functions)return r.fail("Spider-Man web GL functions unavailable");
    auto &g=r.gl;GLState saved(g);r.context=SDL_GL_GetCurrentContext();
    const std::string prefix=g.es?"#version 100\nprecision mediump float;\n":g.modern?"#version 130\n":"#version 120\n";
    std::string vs=(g.es?"#version 100\nprecision highp float;\n":prefix)+(g.modern?std::string("in vec4 aPosition;in vec2 aUV;in vec4 aShade;out vec2 vUV;out vec4 vShade;"):g.es?"attribute vec4 aPosition;attribute vec2 aUV;attribute vec4 aShade;varying mediump vec2 vUV;varying mediump vec4 vShade;":"attribute vec4 aPosition;attribute vec2 aUV;attribute vec4 aShade;varying vec2 vUV;varying vec4 vShade;");
    vs+="void main(){gl_Position=aPosition;vUV=aUV;vShade=aShade;}";
    std::string fs=prefix+(g.modern?"in vec2 vUV;in vec4 vShade;out vec4 outColor;":"varying vec2 vUV;varying vec4 vShade;");fs+="uniform sampler2D uTexture;uniform int uRepeat;uniform int uMaterial;void main(){vec2 uv=uRepeat!=0?fract(vUV):vUV;vec4 texel=";fs+=g.modern?"texture(uTexture,uv)":"texture2D(uTexture,uv)";fs+=";vec4 color=texel*vShade;if(uMaterial==1){color=vec4(texel.rgb,texel.a*vShade.a);}else if(uMaterial==2){color=vec4(vShade.rgb,texel.a*vShade.a);}";fs+=g.modern?"outColor=color;}":"gl_FragColor=color;}";
    GLuint v=shader(r,GL_VERTEX_SHADER,vs),f=shader(r,GL_FRAGMENT_SHADER,fs);if(!v||!f){if(v)g.DeleteShader(v);if(f)g.DeleteShader(f);releaseGL(r);return false;}
    r.program=g.CreateProgram();g.AttachShader(r.program,v);g.AttachShader(r.program,f);g.BindAttribLocation(r.program,0,"aPosition");g.BindAttribLocation(r.program,1,"aUV");g.BindAttribLocation(r.program,2,"aShade");g.LinkProgram(r.program);g.DeleteShader(v);g.DeleteShader(f);GLint good=0;g.GetProgramiv(r.program,GL_LINK_STATUS,&good);if(!good){releaseGL(r);return r.fail("Spider-Man web shader link failed");}
    r.uTexture=g.GetUniformLocation(r.program,"uTexture");r.uRepeat=g.GetUniformLocation(r.program,"uRepeat");r.uMaterial=g.GetUniformLocation(r.program,"uMaterial");g.GenBuffers(1,&r.vbo);if(g.vaoSupported)g.GenVertexArrays(1,&r.vao);g.GenTextures(8,r.textures.data());
    g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);if(g.unpackBuffer)g.BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);g.PixelStorei(GL_UNPACK_ALIGNMENT,1);if(g.unpackRows){g.PixelStorei(GL_UNPACK_ROW_LENGTH,0);g.PixelStorei(GL_UNPACK_SKIP_ROWS,0);g.PixelStorei(GL_UNPACK_SKIP_PIXELS,0);}
    const unsigned char white[4]={255,255,255,255};
    for(int i=0;i<8;++i){
        const int width=i==0?1:i==1?24:i==3?20:i==7?64:32,height=i==3?10:width;
        g.BindTexture(GL_TEXTURE_2D,r.textures[i]);
        g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,i==7?GL_LINEAR:GL_NEAREST);
        g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,i==7?GL_LINEAR:GL_NEAREST);
        g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,i==7?GL_REPEAT:GL_CLAMP_TO_EDGE);
        g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,i==7?GL_REPEAT:GL_CLAMP_TO_EDGE);
        const bool loaded=i==0||(i==7?r.domeLoaded:i>=4?r.attackLoaded:i==3?r.trailLoaded:r.loaded);
        if(loaded)g.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,
            i==0?white:i==7?r.domePixels.data():i>=4?r.attackPixels[i-4].data():i==3?r.trailPixels.data():r.pixels[i-1].data());
    }
    if(!checkGL(r,"shader and texture initialization")){releaseGL(r);return false;}return true;
}
int submit(SpidermanWebGL &r,const int viewport[4]) {
    if(!beginGLBoundary(r))return 0;
    const bool initialized=initGL(r),restored=checkGL(r,"initialization state restoration");if(!initialized||!restored)return 0;
    auto &g=r.gl;auto draw=[&](){GLState saved(g);g.UseProgram(r.program);g.Uniform1i(r.uTexture,0);if(g.vaoSupported)g.BindVertexArray(r.vao);g.BindBuffer(GL_ARRAY_BUFFER,r.vbo);g.BufferData(GL_ARRAY_BUFFER,GLsizeiptr(r.stream.size()*sizeof(DrawVertex)),r.stream.data(),GL_STREAM_DRAW);
        for(int i=0;i<3;++i){g.EnableVertexAttribArray(i);if(g.divisorAttrs)g.VertexAttribDivisor(i,0);}g.VertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,position));g.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,uv));g.VertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,shade));
        for(GLenum cap:CAPABILITIES)g.Disable(cap);
        g.Enable(GL_DEPTH_TEST);g.DepthFunc(GL_LEQUAL);g.DepthMask(GL_FALSE);g.Enable(GL_BLEND);g.BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);g.BlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);g.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);g.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(g.es)g.DepthRangef(0,1);else g.DepthRange(0,1);if(g.compat)g.Disable(GL_ALPHA_TEST);if(!g.es){g.Disable(GL_COLOR_LOGIC_OP);g.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);}if(g.raster)g.Disable(GL_RASTERIZER_DISCARD);g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);g.BindTexture(GL_TEXTURE_2D,r.textures[0]);if(!checkGL(r,"vertex upload/draw setup"))return 0;for(const auto &batch:r.batches){g.BindTexture(GL_TEXTURE_2D,r.textures[batch.texture]);g.Uniform1i(r.uRepeat,batch.texture==1||batch.texture==4||batch.texture==7?1:0);g.Uniform1i(r.uMaterial,batch.texture==5?1:batch.texture==6?2:0);g.DrawArrays(GL_TRIANGLES,GLint(batch.first),GLsizei(batch.count));}return checkGL(r,"web triangle submission")?1:0;};
    int result=draw();if(!checkGL(r,"draw state restoration"))result=0;return result;
}
}
#endif
extern "C" SpidermanWebGL *spiderman_web_gl_create(SpidermanWebErrorFn callback,void *user){try{auto *r=new SpidermanWebGL;r->callback=callback;r->user=user;return r;}catch(...){return nullptr;}}
extern "C" int spiderman_web_gl_load_textures(SpidermanWebGL *r,const char *directory){
    if(!r)return 0;
    r->loaded=false;for(auto &p:r->pixels)p.clear();
#ifndef SPIDERMAN_CPU_ONLY
    if(r->context&&r->context!=SDL_GL_GetCurrentContext())return r->fail("Reload requires owning Spider-Man web context");
    releaseGL(*r);
#endif
    try{if(!directory||!*directory)throw std::runtime_error("Missing original web texture directory");const std::string base=assetCanonical(directory);const char *files[]={"web_texture_46_mip_0.rgba","web_texture_47_mip_0.rgba"},*hash[]={"96dfdc699d0d9585561c004662071a180be322b171aa4768ed2665b8f373ba55","477366a19bb3e188532d0bfdd8f390cf99ce7ad5bb0a81aa26168238d16ee8a0"};
        for(int i=0;i<2;++i){const size_t bytes=i?4096:2304;r->pixels[i]=readRegularFile(assetChild(base,files[i],64),bytes,bytes);if(sha256(r->pixels[i])!=hash[i])throw std::runtime_error("Original web texture hash mismatch");}r->loaded=true;r->error.clear();return 1;
    }catch(const std::exception &e){for(auto &p:r->pixels)p.clear();return r->fail(e.what());}catch(...){for(auto &p:r->pixels)p.clear();return r->fail("Original web texture load failed");}}
/* Explicit original connected-trail material. This does not broaden the web
 * quad API's accepted slots46/47 or alter its producer/rasterization policy. */
extern "C" int spiderman_web_gl_load_trail_texture(SpidermanWebGL *r,const char *directory){
    if(!r)return 0;
    r->trailLoaded=false;r->trailPixels.clear();
#ifndef SPIDERMAN_CPU_ONLY
    if(r->context&&r->context!=SDL_GL_GetCurrentContext())return r->fail("Reload requires owning Spider-Man trail context");
    releaseGL(*r);
#endif
    try{if(!directory||!*directory)throw std::runtime_error("Missing original trail texture directory");const std::string base=assetCanonical(directory);
        auto pixels=readRegularFile(assetChild(base,"trail_texture_41_mip_0.rgba",64),800,800);
        if(sha256(pixels)!="561ab488c9d1f7dd7dcef8b40fd148bcfe4362a3c76554194225b65dc8bbe4f4")throw std::runtime_error("Original trail texture hash mismatch");
        r->trailPixels.swap(pixels);r->trailLoaded=true;r->error.clear();return 1;
    }catch(const std::exception &e){return r->fail(e.what());}catch(...){return r->fail("Original trail texture load failed");}
}
extern "C" int spiderman_web_gl_trail_texture_loaded(const SpidermanWebGL *r){return r&&r->trailLoaded;}
extern "C" int spiderman_web_gl_draw_trail_quads(SpidermanWebGL *r,const float view[16],const float projection[16],const int viewport[4],const SpidermanWebQuad *quads,size_t count){
    if(!r)return 0;
    try{r->stream.clear();r->batches.clear();const int native[2]={1,1};
        if(!camera(view,projection,viewport,native)||(count&&!r->trailLoaded)||!quadGeometry(view,projection,quads,count,r->stream,r->batches,true)){r->stream.clear();r->batches.clear();return r->fail("Invalid original trail snapshot/camera/viewport or missing verified texture");}
        if(r->stream.empty()){r->error.clear();return 1;}
#ifndef SPIDERMAN_CPU_ONLY
        if(!submit(*r,viewport)){r->stream.clear();return 0;}r->error.clear();return 1;
#else
        return r->fail("OpenGL unavailable in CPU-only trail build");
#endif
    }catch(const std::exception &e){r->stream.clear();return r->fail(e.what());}catch(...){r->stream.clear();return r->fail("Spider-Man trail drawing failed");}
}
extern "C" int spiderman_web_attack_decal_projection(const float projection[16],float out[16]){return decalProjection(projection,out)?1:0;}
extern "C" int spiderman_web_attack_gl_load_textures(SpidermanWebAttackGL*r,const char*directory){
    if(!r)return 0;
    r->attackLoaded=false;for(auto&p:r->attackPixels)p.clear();
#ifndef SPIDERMAN_CPU_ONLY
    if(r->context&&r->context!=SDL_GL_GetCurrentContext())return r->fail("Reload requires owning Spider-Man attack context");
    releaseGL(*r);
#endif
    try{if(!directory||!*directory)throw std::runtime_error("Missing original attack texture directory");const std::string base=assetCanonical(directory);const unsigned slots[3]={38,111,108};const char*hashes[3]={"d1e94d04a0f1f8ac53548f22702e13b3986156c2da09601b875571b21bce7eef","fa1efd4c0f11a4cad1dbe8c8fe6d904ba0dbef69605f266079f243526c7650ee","91207872d6b22b53f7083a56a159a5c0994fd829d1955e17c63324141a6809a7"};std::array<std::vector<unsigned char>,3>pixels;
        for(int i=0;i<3;++i){const std::string name="web_attack_texture_"+std::to_string(slots[i])+"_mip_0.rgba";pixels[i]=readRegularFile(assetChild(base,name,64),4096,4096);if(sha256(pixels[i])!=hashes[i])throw std::runtime_error("Original attack texture hash mismatch");}r->attackPixels.swap(pixels);r->attackLoaded=true;r->error.clear();return 1;
    }catch(const std::exception&e){return r->fail(e.what());}catch(...){return r->fail("Original attack texture load failed");}
}
extern "C" int spiderman_web_attack_gl_textures_loaded(const SpidermanWebAttackGL*r){return r&&r->attackLoaded;}
extern "C" int spiderman_effects_gl_load_dome_texture(SpidermanWebGL*r,const char*directory){
    if(!r)return 0;
    r->domeLoaded=false;r->domePixels.clear();
#ifndef SPIDERMAN_CPU_ONLY
    if(r->context&&r->context!=SDL_GL_GetCurrentContext())return r->fail("Reload requires owning Spider-Man effect context");
    releaseGL(*r);
#endif
    try{
        if(!directory||!*directory)throw std::runtime_error("Missing original dome texture directory");
        const std::string base=assetCanonical(directory);
        auto pixels=readRegularFile(assetChild(base,"texture_403_0.rgba",64),16384,16384);
        if(sha256(pixels)!="6da481c2ab172a52dd718e6e2bc8cb2ec36743c38868115ae4efd84a35ef14c8")throw std::runtime_error("Original dome texture403 hash mismatch");
        r->domePixels.swap(pixels);r->domeLoaded=true;r->error.clear();return 1;
    }catch(const std::exception&e){return r->fail(e.what());}catch(...){return r->fail("Original dome texture403 load failed");}
}
extern "C" int spiderman_effects_gl_dome_texture_loaded(const SpidermanWebGL*r){return r&&r->domeLoaded;}
extern "C" int spiderman_web_attack_gl_draw(SpidermanWebAttackGL*r,const float view[16],const float projection[16],const int vp[4],const int native[2],const SpidermanWebAttackQuad*quads,size_t quadCount,const SpidermanWebAttackLine*lines,size_t lineCount){
    if(!r)return 0;
    try{r->stream.clear();r->batches.clear();if(!camera(view,projection,vp,native)||(quadCount&&!r->attackLoaded)||!attackQuadGeometry(view,projection,quads,quadCount,r->stream,r->batches))return r->fail("Invalid original attack quads/camera or missing original textures");const size_t first=r->stream.size();if(!attackLineGeometry(view,projection,vp,native,lines,lineCount,r->stream)){r->stream.clear();r->batches.clear();return r->fail("Invalid original attack lines/camera");}if(r->stream.size()>first)r->batches.push_back({first,r->stream.size()-first,0});if(r->stream.empty()){r->error.clear();return 1;}
#ifndef SPIDERMAN_CPU_ONLY
        if(!submit(*r,vp)){r->stream.clear();return 0;}r->error.clear();return 1;
#else
        return r->fail("OpenGL unavailable in CPU-only attack build");
#endif
    }catch(const std::exception&e){r->stream.clear();return r->fail(e.what());}catch(...){r->stream.clear();return r->fail("Spider-Man attack drawing failed");}
}
extern "C" int spiderman_effects_gl_draw_ordered(SpidermanWebGL*r,const float view[16],const float projection[16],const int vp[4],const int native[2],const SpidermanEffectPrimitive*primitives,size_t count,uint8_t initialEnvironment,uint8_t*finalEnvironment){
    if(!r)return 0;
    try{r->stream.clear();r->batches.clear();if(!camera(view,projection,vp,native)||count>4096||(count&&!primitives))return r->fail("Invalid ordered original-effect frame/camera");std::vector<DrawVertex>temporary;uint8_t environment=initialEnvironment;
        for(size_t i=0;i<count;++i){const auto&p=primitives[i];bool ok=false;const size_t first=r->stream.size();
            switch(p.kind){
                case SPIDERMAN_EFFECT_WEB_QUAD:if(p.source.quad){float biased[16];const bool polygon=p.source.quad->texture_slot==47;ok=r->loaded&&(!polygon||decalProjection(projection,biased))&&quadGeometry(view,polygon?biased:projection,p.source.quad,1,r->stream,r->batches);if(ok&&polygon)environment=p.source.quad->rgba[0][3];}break;
                case SPIDERMAN_EFFECT_TRAIL_QUAD:ok=r->trailLoaded&&quadGeometry(view,projection,p.source.quad,1,r->stream,r->batches,true);break;
                case SPIDERMAN_EFFECT_ATTACK_QUAD:if(p.source.attack_quad){SpidermanWebAttackQuad quad=*p.source.attack_quad;bool uniform=true;if(quad.submitted.texture_slot==108)for(unsigned v=1;v<4;++v)uniform&=quad.submitted.rgba[v][3]==quad.submitted.rgba[0][3];if(quad.submitted.texture_slot==111)quad.environment_alpha=environment;ok=uniform&&r->attackLoaded&&attackQuadGeometry(view,projection,&quad,1,r->stream,r->batches);if(ok&&quad.submitted.texture_slot==108)environment=quad.submitted.rgba[0][3];}break;
                case SPIDERMAN_EFFECT_DOME_SHATTER:if(p.source.attack_quad){const auto&q=*p.source.attack_quad;ok=r->domeLoaded&&attackQuadGeometry(view,projection,&q,1,r->stream,r->batches,true);if(ok)environment=q.environment_alpha;}break;
                case SPIDERMAN_EFFECT_PRIMARY_CHAIN:case SPIDERMAN_EFFECT_SECONDARY_CHAIN:
                    ok=geometry(view,projection,vp,native,p.source.strand,1,temporary,p.kind==SPIDERMAN_EFFECT_PRIMARY_CHAIN?0:1);if(ok)r->stream.insert(r->stream.end(),temporary.begin(),temporary.end());break;
                case SPIDERMAN_EFFECT_ATTACK_LINE:ok=attackLineGeometry(view,projection,vp,native,p.source.line,1,r->stream);break;
                default:break;
            }
            if(!ok||r->stream.size()>131072){r->stream.clear();r->batches.clear();return r->fail("Invalid ordered source primitive, capacity or missing exact material");}
            if((p.kind==SPIDERMAN_EFFECT_PRIMARY_CHAIN||p.kind==SPIDERMAN_EFFECT_SECONDARY_CHAIN||p.kind==SPIDERMAN_EFFECT_ATTACK_LINE)&&r->stream.size()>first){if(!r->batches.empty()&&r->batches.back().texture==0)r->batches.back().count+=r->stream.size()-first;else r->batches.push_back({first,r->stream.size()-first,0});}
        }
        if(r->stream.empty()){r->error.clear();if(finalEnvironment)*finalEnvironment=environment;return 1;}
#ifndef SPIDERMAN_CPU_ONLY
        if(!submit(*r,vp)){r->stream.clear();return 0;}r->error.clear();if(finalEnvironment)*finalEnvironment=environment;return 1;
#else
        return r->fail("OpenGL unavailable in CPU-only mixed-effect build");
#endif
    }catch(const std::exception&e){r->stream.clear();return r->fail(e.what());}catch(...){r->stream.clear();return r->fail("Ordered original effects drawing failed");}
}
extern "C" int spiderman_web_gl_textures_loaded(const SpidermanWebGL *r){return r&&r->loaded;}
extern "C" const char *spiderman_web_gl_last_error(const SpidermanWebGL *r){return r?r->error.c_str():"null Spider-Man web renderer";}
extern "C" const char *spiderman_web_gl_last_host_warning(const SpidermanWebGL *r){return r?r->hostWarning.c_str():"";}
extern "C" int spiderman_web_gl_draw_frame(SpidermanWebGL *r,const float view[16],const float projection[16],const int viewport[4],const int native[2],const SpidermanWebStrandInstance *strands,size_t count,const SpidermanWebQuad *quads,size_t quadCount){
    if(!r)return 0;
    try{r->batches.clear();if(!geometry(view,projection,viewport,native,strands,count,r->stream))return r->fail("Invalid original web snapshot/camera/viewport");if(!r->stream.empty())r->batches.push_back({0,r->stream.size(),0});if((quadCount&&!r->loaded)||!quadGeometry(view,projection,quads,quadCount,r->stream,r->batches)){r->stream.clear();r->batches.clear();return r->fail("Invalid original web quad snapshot or missing verified texture assets");}if(r->stream.empty()){r->error.clear();return 1;}
#ifndef SPIDERMAN_CPU_ONLY
        if(!submit(*r,viewport)){r->stream.clear();return 0;}r->error.clear();return 1;
#else
        return r->fail("OpenGL unavailable in CPU-only web build");
#endif
    }catch(const std::exception &e){r->stream.clear();return r->fail(e.what());}catch(...){r->stream.clear();return r->fail("Spider-Man web drawing failed");}}
extern "C" int spiderman_web_gl_draw_instances(SpidermanWebGL *r,const float view[16],const float projection[16],const int viewport[4],const int native[2],const SpidermanWebStrandInstance *strands,size_t count){return spiderman_web_gl_draw_frame(r,view,projection,viewport,native,strands,count,nullptr,0);}
extern "C" int spiderman_web_gl_draw(SpidermanWebGL *r,const float view[16],const float projection[16],const int viewport[4],const int native[2],const SmN64Strand *strands,size_t count){
    if(count>64||(count&&!strands))return r?r->fail("Invalid original web strand count/pointer"):0;
    std::array<SpidermanWebStrandInstance,64> instances={};for(size_t i=0;i<count;++i){instances[i].geometry=strands+i;std::fill(instances[i].line_rgb,instances[i].line_rgb+3,uint8_t(162));}
    return spiderman_web_gl_draw_instances(r,view,projection,viewport,native,instances.data(),count);
}
extern "C" void spiderman_web_gl_destroy(SpidermanWebGL *r){if(!r)return;
#ifndef SPIDERMAN_CPU_ONLY
    releaseGL(*r);
#endif
    delete r;}
#ifdef SPIDERMAN_TESTING
extern "C" int16_t spiderman_web_test_source_coordinate(int32_t fixed){return sourceCoordinate(fixed);}
extern "C" ptrdiff_t spiderman_web_test_geometry(const float view[16],const float projection[16],const int viewport[4],const int native[2],const SmN64Strand *strands,size_t count,float *out,size_t capacity){try{if(count>64||(count&&!strands))return -1;std::array<SpidermanWebStrandInstance,64> instances={};for(size_t i=0;i<count;++i){instances[i].geometry=strands+i;std::fill(instances[i].line_rgb,instances[i].line_rgb+3,uint8_t(162));}std::vector<DrawVertex> stream;if(!geometry(view,projection,viewport,native,instances.data(),count,stream)||capacity<stream.size()*4||(!out&&!stream.empty()))return -1;size_t offset=0;for(const auto &v:stream)for(float c:v.position)out[offset++]=c;return ptrdiff_t(stream.size());}catch(...){return -1;}}
extern "C" int spiderman_web_test_texture(const SpidermanWebGL *r,unsigned slot,unsigned char *out,size_t capacity){if(!r||!r->loaded||slot<46||slot>47||!out||capacity<r->pixels[slot-46].size())return 0;std::copy(r->pixels[slot-46].begin(),r->pixels[slot-46].end(),out);return 1;}
extern "C" int spiderman_web_test_trail_texture(const SpidermanWebGL *r,unsigned char *out,size_t capacity){if(!r||!r->trailLoaded||!out||capacity<r->trailPixels.size())return 0;std::copy(r->trailPixels.begin(),r->trailPixels.end(),out);return 1;}
extern "C" int spiderman_web_attack_test_texture(const SpidermanWebAttackGL*r,unsigned slot,unsigned char*out,size_t capacity){const int index=slot==38?0:slot==111?1:slot==108?2:-1;if(!r||!r->attackLoaded||index<0||!out||capacity<r->attackPixels[index].size())return 0;std::copy(r->attackPixels[index].begin(),r->attackPixels[index].end(),out);return 1;}
extern "C" ptrdiff_t spiderman_web_attack_test_lines(const float view[16],const float projection[16],const int vp[4],const int native[2],const SpidermanWebAttackLine*lines,size_t count,float*out,size_t capacity){try{std::vector<DrawVertex>stream;if(!attackLineGeometry(view,projection,vp,native,lines,count,stream)||capacity<stream.size()*8||(!out&&!stream.empty()))return -1;size_t offset=0;for(const auto&v:stream){for(float p:v.position)out[offset++]=p;for(float c:v.shade)out[offset++]=c;}return ptrdiff_t(stream.size());}catch(...){return -1;}}
extern "C" int spiderman_web_test_regular_file(const char *path,size_t limit){try{if(!path)return 0;readRegularFile(path,limit);return 1;}catch(...){return 0;}}
extern "C" void spiderman_web_test_inject_probe_error(SpidermanWebGL *r){
#ifndef SPIDERMAN_CPU_ONLY
    if(r)r->gl.injectProbeError=true;
#else
    (void)r;
#endif
}
#endif
