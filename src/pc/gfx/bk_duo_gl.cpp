/* Original BK mesh/material draw adapter. Receives independently evaluated
 * original pivot-bone world palettes; does not invent poses or motion.
 * The GL state/material backend is reused in design from the reviewed BM64 module,
 * but this separate translation unit leaves original Bomberman and Link rendering unchanged.
 * RDP limitations remain: TEXEL0/1 share one texture, approximate lighting,
 * alpha cutout, no fog/noise/chroma-key/LOD-fraction emulation.
 */
#include "bk_duo_gl.h"
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
#ifndef BK_DUO_CPU_ONLY
#include <SDL2/SDL.h>
#ifdef USE_GLES
#include <SDL2/SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif
#ifndef APIENTRY
#define APIENTRY
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
#ifndef GL_POLYGON_MODE
#define GL_POLYGON_MODE 0x0B40
#define GL_FILL 0x1B02
#endif
#endif

namespace {
using Json = nlohmann::json;
const size_t MAX_JSON = 32u*1024u*1024u, MAX_VERTICES=200000, MAX_TRIANGLES=200000, MAX_MATERIALS=1024;
typedef std::array<float,16> Matrix;
#ifndef BK_DUO_CPU_ONLY
Matrix multiply(const Matrix &a,const Matrix &b) {
    Matrix r={};for(int c=0;c<4;++c)for(int row=0;row<4;++row)for(int k=0;k<4;++k)r[c*4+row]+=a[k*4+row]*b[c*4+k];return r;
}
#endif
struct Bone {int parent=-1;};
struct Vertex {float position[3],uv[2];unsigned char color[4];int matrix,uvScale[2];bool lit,uvProcessed,billboard;};
struct Triangle {unsigned int index[3],material;std::vector<std::pair<int,int>> selectors;};
struct Material {
    float prim[4],env[4],scale[2],diffuse[3],ambient[3];int wrap[2],shift[2],origin[2],mask[2];
    int rgb[2][4],alpha[2][4],cycle=1,width=1,height=1;
    bool textured=false;uint32_t geometry=0;std::vector<unsigned char> rgba;
};
struct DrawVertex {float position[3],uv[2],shade[4];};
struct Batch {unsigned int material;size_t first,count;};
struct Asset {std::vector<Bone> bones;size_t paletteCount=0;std::vector<Vertex> vertices;std::vector<Triangle> triangles;std::vector<Material> materials;};
long long integer(const Json &j,long long lo,long long hi,const char *what) {
    if(!j.is_number_integer()||j.is_boolean())throw std::runtime_error(std::string("invalid ")+what);
    if(j.is_number_unsigned()&&j.get<uint64_t>()>uint64_t(hi))throw std::runtime_error(std::string("out of range ")+what);
    long long n=j.get<long long>();if(n<lo||n>hi)throw std::runtime_error(std::string("out of range ")+what);return n;
}
const Json &array(const Json &j,size_t n,const char *what) {if(!j.is_array()||j.size()!=n)throw std::runtime_error(std::string("invalid ")+what);return j;}
std::vector<unsigned char> readFile(const std::string &path,size_t max,size_t exact=0) {
    return oot_asset_path::readFile(path, max, exact);
}
std::string realPath(const std::string &path) { return oot_asset_path::canonical(path); }
std::string texturePath(const std::string &base,const std::string &file) {
    return oot_asset_path::child(realPath(base), file, 240);
}
void visit(const Asset &a,size_t index,std::vector<unsigned char> &state) {
    if(state[index]==1)throw std::runtime_error("cyclic original pivot hierarchy");
    if(state[index]==2)return;
    state[index]=1;int parent=a.bones[index].parent;
    if(parent>=0)visit(a,size_t(parent),state);
    state[index]=2;
}
float number(const Json &j,float lo,float hi,const char *what) {
    if(!j.is_number()||j.is_boolean())throw std::runtime_error(std::string("invalid ")+what);
    float n=j.get<float>();if(!std::isfinite(n)||n<lo||n>hi)throw std::runtime_error(std::string("out of range ")+what);return n;
}
void decodeCombine(Material &m,uint32_t a,uint32_t b) {
    const int rgb[2][4]={{int((a>>20)&15),int((b>>28)&15),int((a>>15)&31),int((b>>15)&7)},
                         {int((a>>5)&15),int((b>>24)&15),int(a&31),int((b>>6)&7)}};
    const int alpha[2][4]={{int((a>>12)&7),int((b>>12)&7),int((a>>9)&7),int((b>>9)&7)},
                           {int((b>>21)&7),int((b>>3)&7),int((b>>18)&7),int(b&7)}};
    std::memcpy(m.rgb,rgb,sizeof rgb);std::memcpy(m.alpha,alpha,sizeof alpha);
}
Asset loadAsset(const std::string &input) {
    std::string path=input;if(path.size()<5||path.substr(path.size()-5)!=".json")path+="/mesh.json";
    path=realPath(path);size_t slash=path.find_last_of("/\\");std::string base=slash==std::string::npos?".":path.substr(0,slash+1);
    auto bytes=readFile(path,MAX_JSON);Json j=Json::parse(bytes.begin(),bytes.end(),[](int depth,Json::parse_event_t,Json &){if(depth>32)throw std::runtime_error("JSON nesting limit exceeded");return true;});
    if(j.at("schema_version")!="bk-original-mesh-v1")throw std::runtime_error("unsupported original Banjo-Kazooie mesh schema");
    const Json &s=j.at("skeleton"), &bs=s.at("bones");
    if(!bs.is_array()||bs.empty()||bs.size()>109)throw std::runtime_error("invalid original bone count");
    number(s.at("translation_scale"),0.000001f,1000000.0f,"translation scale");
    Asset a;a.paletteCount=bs.size()+1;a.bones.resize(bs.size());
    for(size_t i=0;i<bs.size();++i){const Json &bone=bs[i];integer(bone.at("index"),i,i,"bone index");
        integer(bone.at("bone_id"),0,108,"animation bone ID");
        a.bones[i].parent=bone.at("parent").is_null()?-1:int(integer(bone.at("parent"),-1,bs.size()-1,"bone parent"));
        for(const Json &v:array(bone.at("pivot"),3,"bone pivot"))number(v,-1000000.0f,1000000.0f,"bone pivot");
    }
    std::vector<unsigned char> boneState(bs.size(),0);for(size_t i=0;i<bs.size();++i)visit(a,i,boneState);
    const Json &ms=j.at("materials");if(!ms.is_array()||ms.empty()||ms.size()>MAX_MATERIALS)throw std::runtime_error("material count out of range");size_t textureBytes=0;
    for(const Json &v:ms){Material m;for(int k=0;k<4;++k){m.prim[k]=float(integer(array(v.at("prim_color"),4,"primitive color")[k],0,255,"primitive color"))/255;m.env[k]=float(integer(array(v.at("env_color"),4,"environment color")[k],0,255,"environment color"))/255;}
        for(int k=0;k<3;++k){m.diffuse[k]=v.contains("light_diffuse")?float(integer(array(v.at("light_diffuse"),3,"diffuse light")[k],0,255,"diffuse light"))/255:0.65f;m.ambient[k]=v.contains("light_ambient")?float(integer(array(v.at("light_ambient"),3,"ambient light")[k],0,255,"ambient light"))/255:0.35f;}
        m.geometry=uint32_t(integer(v.at("geometry_mode"),0,0xffffffffLL,"geometry mode"));const Json &combine=array(v.at("combine"),2,"combine");decodeCombine(m,uint32_t(integer(combine[0],0,0xffffffffLL,"combine")),uint32_t(integer(combine[1],0,0xffffffffLL,"combine")));
        if(v.contains("other_mode_h"))m.cycle=int((integer(v.at("other_mode_h"),0,0xffffffffLL,"other mode high")>>20)&3);
        if(v.contains("cycle_type"))m.cycle=int(integer(v.at("cycle_type"),0,3,"cycle type"));
        if(!v.at("texture_enabled").is_boolean())throw std::runtime_error("invalid texture_enabled");
        m.textured=v.at("texture_enabled").get<bool>();
        const Json &scale=array(v.at("texture_scale"),2,"texture scale");const Json &tile=v.at("tile");
        for(int k=0;k<2;++k){const char *axis=k?"t":"s";m.scale[k]=float(integer(scale[k],0,65535,"texture scale"))/65536.0f;m.wrap[k]=int(integer(tile.at(std::string("cm_")+axis),0,3,"texture wrap"));m.shift[k]=int(integer(tile.at(std::string("shift_")+axis),0,15,"texture shift"));m.mask[k]=int(integer(tile.at(std::string("mask_")+axis),0,15,"texture mask"));m.origin[k]=int(integer(tile.at(k?"ult":"uls"),0,4095,"texture origin"));}
        const Json &tex=v.at("texture");if(tex.is_null()&&combine[0]==0xffffff&&combine[1]==0xfffe7d3e)m.textured=false;if(m.textured){if(!tex.is_object())throw std::runtime_error("enabled texture missing");m.width=int(integer(tex.at("width"),1,4096,"texture width"));m.height=int(integer(tex.at("height"),1,4096,"texture height"));size_t n=size_t(m.width)*m.height*4;textureBytes+=n;if(textureBytes>64u*1024u*1024u)throw std::runtime_error("texture budget exceeded");m.rgba=readFile(texturePath(base,tex.at("rgba_file").get<std::string>()),n,n);}else m.rgba.assign(4,255);
        a.materials.push_back(std::move(m));
    }
    const Json &vs=j.at("vertices");if(!vs.is_array()||vs.empty()||vs.size()>MAX_VERTICES)throw std::runtime_error("vertex count out of range");a.vertices.reserve(vs.size());
    for(const Json &v:vs){Vertex out;const Json &p=array(v.at("position"),3,"vertex position"),&uv=array(v.at("uv"),2,"UV"),&c=array(v.at("color_normal"),4,"color/normal");for(int k=0;k<3;++k)out.position[k]=float(integer(p[k],-32768,32767,"vertex position"));for(int k=0;k<2;++k)out.uv[k]=float(integer(uv[k],-32768,32767,"UV"));for(int k=0;k<4;++k)out.color[k]=(unsigned char)integer(c[k],0,255,"color/normal");out.matrix=int(integer(v.at("joint_index"),-1,(int)a.bones.size()-1,"vertex matrix index"));if(out.matrix<0)out.matrix=int(a.bones.size());if(!v.at("lit").is_boolean())throw std::runtime_error("invalid vertex lighting");out.lit=v.at("lit").get<bool>();
        if(!v.at("billboard").is_boolean())throw std::runtime_error("invalid billboard flag");
        out.billboard=v.at("billboard").get<bool>();out.uvProcessed=false;out.uvScale[0]=out.uvScale[1]=-1;
        if(v.contains("uv_processed")){if(!v.at("uv_processed").is_boolean())throw std::runtime_error("invalid processed UV flag");out.uvProcessed=v.at("uv_processed").get<bool>();}
        if(v.contains("uv_scale")){const Json &uvScale=array(v.at("uv_scale"),2,"vertex UV scale");for(int k=0;k<2;++k)out.uvScale[k]=int(integer(uvScale[k],0,65535,"vertex UV scale"));}
        a.vertices.push_back(out);}
    const Json &ts=j.at("triangles");if(!ts.is_array()||ts.empty()||ts.size()>MAX_TRIANGLES)throw std::runtime_error("triangle count out of range");a.triangles.reserve(ts.size());
    for(const Json &t:ts){Triangle out;const Json &idx=array(t.at("indices"),3,"triangle");for(int k=0;k<3;++k)out.index[k]=(unsigned int)integer(idx[k],0,(long long)a.vertices.size()-1,"vertex index");out.material=(unsigned int)integer(t.at("material"),0,(long long)a.materials.size()-1,"material index");const Json &selectors=t.at("selectors");
        if(!selectors.is_array()||selectors.size()>16)throw std::runtime_error("invalid triangle selectors");
        std::array<bool,64> seenSelectors={};
        for(const Json &selector:selectors){array(selector,2,"triangle selector");int index=int(integer(selector[0],0,63,"selector index"));int branch=int(integer(selector[1],1,64,"selector branch"));if(seenSelectors[index])throw std::runtime_error("duplicate triangle selector");seenSelectors[index]=true;out.selectors.push_back({index,branch});}
        a.triangles.push_back(std::move(out));}return a;
}
#if !defined(BK_DUO_CPU_ONLY) || defined(BK_DUO_TESTING)
void textureUV(const Vertex &v,const Material &m,float out[2]) {
    for(int axis=0;axis<2;++axis) {
        // Texture scale belongs to G_VTX, whereas tile shift/origin belong to
        // rasterization. G_MODIFYVTX ST supplies already-scaled coordinates.
        int raw=int(v.uv[axis]);
        if(!v.uvProcessed) {
            int scale=v.uvScale[axis]>=0?v.uvScale[axis]:int(m.scale[axis]*65536.0f);
            int64_t product=int64_t(raw)*scale;
            raw=int(product>=0?product/65536:-((-product+65535)/65536));
        }
        float shift=m.shift[axis]<=10?std::ldexp(1.0f,-m.shift[axis]):std::ldexp(1.0f,16-m.shift[axis]);
        out[axis]=(float(raw)/32.0f*shift-m.origin[axis]/4.0f)/float(axis?m.height:m.width);
    }
}
#endif
bool finite(const float *p,size_t n){if(!p)return false;for(size_t i=0;i<n;++i)if(!std::isfinite(p[i]))return false;return true;}
bool pose(const Asset &a,const float *joints,const float *billboards,size_t count,std::vector<DrawVertex> &vertices) {
    if(count!=a.paletteCount||!finite(joints,count*16)||!finite(billboards,count*16))return false;
    for(size_t i=0;i<count;++i)for(const float *all:{joints,billboards}) {
        const float *m=all+i*16;
        if(std::fabs(m[3])>1e-5f||std::fabs(m[7])>1e-5f||std::fabs(m[11])>1e-5f||std::fabs(m[15]-1)>1e-5f)return false;
        for(int k=0;k<16;++k)if(std::fabs(m[k])>1e7f)return false;
    }
    vertices.resize(a.vertices.size());const float light[3]={.26726124f,.80178373f,.53452248f};
    for(size_t i=0;i<a.vertices.size();++i){const Vertex &v=a.vertices[i];DrawVertex &out=vertices[i];const float *m=(v.billboard?billboards:joints)+v.matrix*16;
        for(int k=0;k<3;++k)out.position[k]=m[k]*v.position[0]+m[4+k]*v.position[1]+m[8+k]*v.position[2]+m[12+k];
        float brightness=1;if(v.lit){float n[3],length=0;for(int k=0;k<3;++k){n[k]=0;for(int q=0;q<3;++q){int c=int(v.color[q]);if(c>=128)c-=256;n[k]+=m[q*4+k]*float(c);}length+=n[k]*n[k];}length=std::sqrt(length);float dot=0;if(length>0)for(int k=0;k<3;++k)dot+=n[k]/length*light[k];brightness=.35f+.65f*std::max(0.0f,dot);}
        for(int k=0;k<3;++k)out.shade[k]=v.lit?brightness:float(v.color[k])/255;
        out.shade[3]=float(v.color[3])/255;out.uv[0]=v.uv[0];out.uv[1]=v.uv[1];
    }return true;
}
}
#ifndef BK_DUO_CPU_ONLY
namespace {
#define GL_FUNCTIONS(X) \
 X(void,GetIntegerv,(GLenum,GLint*)) X(void,GetBooleanv,(GLenum,GLboolean*)) X(void,GetFloatv,(GLenum,GLfloat*)) \
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
    void(APIENTRY *DepthRange)(double,double)=nullptr;void(APIENTRY *DepthRangef)(GLfloat,GLfloat)=nullptr;
    bool es=false,modern=false,vaoSupported=false,samplers=false,unpackRows=false,unpackBuffer=false,raster=false;
    bool load(){
#define LOAD(ret,name,args) name=(ret(APIENTRY*)args)SDL_GL_GetProcAddress("gl" #name);if(!name)return false;
GL_FUNCTIONS(LOAD)
#undef LOAD
        const char *v=(const char*)GetString(GL_VERSION);if(!v)return false;es=std::strstr(v,"OpenGL ES")!=NULL;while(*v&&(*v<'0'||*v>'9'))++v;int major=0,minor=0;std::sscanf(v,"%d.%d",&major,&minor);
        modern=!es&&major>=3;unpackRows=!es||major>=3;raster=major>=3;unpackBuffer=(!es&&(major>2||(major==2&&minor>=1)))||(es&&major>=3);samplers=(!es&&(major>3||(major==3&&minor>=3)))||(es&&major>=3);
#define OPT(name,type) name=(type)SDL_GL_GetProcAddress("gl" #name)
        OPT(GenVertexArrays,void(APIENTRY*)(GLsizei,GLuint*));OPT(BindVertexArray,void(APIENTRY*)(GLuint));OPT(DeleteVertexArrays,void(APIENTRY*)(GLsizei,const GLuint*));
        OPT(BindSampler,void(APIENTRY*)(GLuint,GLuint));OPT(PolygonMode,void(APIENTRY*)(GLenum,GLenum));OPT(DepthRange,void(APIENTRY*)(double,double));OPT(DepthRangef,void(APIENTRY*)(GLfloat,GLfloat));
#undef OPT
        vaoSupported=major>=3&&GenVertexArrays&&BindVertexArray&&DeleteVertexArrays;samplers=samplers&&BindSampler;
        return (es?DepthRangef!=NULL:(DepthRange&&PolygonMode))&&(!modern||vaoSupported);
    }
};
const GLenum CAPABILITIES[]={GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_POLYGON_OFFSET_FILL,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
struct AttrState {GLint enabled,size,type,normalized,stride,buffer;void *pointer;};
struct GLState {
    GLFunctions &g;GLint active,texture,program,array,vao=0,sampler=0,unpack=4,row=0,skipRows=0,skipPixels=0,unpackBuffer=0,vp[4],depthFunc,polygon[2]={GL_FILL,GL_FILL};
    GLint blendSrcRGB,blendDstRGB,blendSrcAlpha,blendDstAlpha,blendEqRGB,blendEqAlpha,cullFace,frontFace;
    GLboolean enabled[8],depthWrite,colorWrite[4],raster=0;GLfloat depthRange[2];AttrState attr[3];
    explicit GLState(GLFunctions &gl):g(gl){
        g.GetIntegerv(GL_ACTIVE_TEXTURE,&active);g.ActiveTexture(GL_TEXTURE0);g.GetIntegerv(GL_TEXTURE_BINDING_2D,&texture);if(g.samplers)g.GetIntegerv(GL_SAMPLER_BINDING,&sampler);
        g.GetIntegerv(GL_BLEND_SRC_RGB,&blendSrcRGB);g.GetIntegerv(GL_BLEND_DST_RGB,&blendDstRGB);g.GetIntegerv(GL_BLEND_SRC_ALPHA,&blendSrcAlpha);g.GetIntegerv(GL_BLEND_DST_ALPHA,&blendDstAlpha);g.GetIntegerv(GL_BLEND_EQUATION_RGB,&blendEqRGB);g.GetIntegerv(GL_BLEND_EQUATION_ALPHA,&blendEqAlpha);
        g.GetIntegerv(GL_CULL_FACE_MODE,&cullFace);g.GetIntegerv(GL_FRONT_FACE,&frontFace);
        g.GetIntegerv(GL_CURRENT_PROGRAM,&program);g.GetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        if(g.vaoSupported)g.GetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);else for(GLuint i=0;i<3;++i){g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&attr[i].enabled);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&attr[i].size);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&attr[i].type);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&attr[i].normalized);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&attr[i].stride);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&attr[i].buffer);g.GetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&attr[i].pointer);}
        g.GetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);if(g.unpackRows){g.GetIntegerv(GL_UNPACK_ROW_LENGTH,&row);g.GetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);g.GetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);}if(g.unpackBuffer)g.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
        if(!g.es)g.GetIntegerv(GL_POLYGON_MODE,polygon);
        g.GetIntegerv(GL_VIEWPORT,vp);g.GetIntegerv(GL_DEPTH_FUNC,&depthFunc);g.GetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);g.GetBooleanv(GL_COLOR_WRITEMASK,colorWrite);g.GetFloatv(GL_DEPTH_RANGE,depthRange);
        for(int i=0;i<8;++i)enabled[i]=g.IsEnabled(CAPABILITIES[i]);
        if(g.raster)raster=g.IsEnabled(GL_RASTERIZER_DISCARD);
    }
    ~GLState(){
        g.CullFace(cullFace);g.FrontFace(frontFace);
        g.BlendFuncSeparate(blendSrcRGB,blendDstRGB,blendSrcAlpha,blendDstAlpha);g.BlendEquationSeparate(blendEqRGB,blendEqAlpha);
        g.UseProgram(program);if(g.vaoSupported)g.BindVertexArray(vao);else for(GLuint i=0;i<3;++i){g.BindBuffer(GL_ARRAY_BUFFER,attr[i].buffer);g.VertexAttribPointer(i,attr[i].size,attr[i].type,attr[i].normalized,attr[i].stride,attr[i].pointer);if(attr[i].enabled)g.EnableVertexAttribArray(i);else g.DisableVertexAttribArray(i);}g.BindBuffer(GL_ARRAY_BUFFER,array);
        g.ActiveTexture(GL_TEXTURE0);g.BindTexture(GL_TEXTURE_2D,texture);if(g.samplers)g.BindSampler(0,sampler);g.ActiveTexture(active);
        g.PixelStorei(GL_UNPACK_ALIGNMENT,unpack);if(g.unpackRows){g.PixelStorei(GL_UNPACK_ROW_LENGTH,row);g.PixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);g.PixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);}if(g.unpackBuffer)g.BindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
        if(!g.es){if(polygon[0]==polygon[1])g.PolygonMode(GL_FRONT_AND_BACK,polygon[0]);else{g.PolygonMode(GL_FRONT,polygon[0]);g.PolygonMode(GL_BACK,polygon[1]);}}
        g.Viewport(vp[0],vp[1],vp[2],vp[3]);g.DepthFunc(depthFunc);g.DepthMask(depthWrite);g.ColorMask(colorWrite[0],colorWrite[1],colorWrite[2],colorWrite[3]);if(g.es)g.DepthRangef(depthRange[0],depthRange[1]);else g.DepthRange(depthRange[0],depthRange[1]);for(int i=0;i<8;++i){if(enabled[i])g.Enable(CAPABILITIES[i]);else g.Disable(CAPABILITIES[i]);}if(g.raster){if(raster)g.Enable(GL_RASTERIZER_DISCARD);else g.Disable(GL_RASTERIZER_DISCARD);}
    }
};
}
#endif
struct BkDuoGL {
    BkDuoErrorFn callback=nullptr;void *user=nullptr;std::string error;float opacity=1;std::unique_ptr<Asset> asset;
    std::array<int,64> selectors={};
    std::vector<DrawVertex> posed,stream;std::vector<Batch> batches;
#ifndef BK_DUO_CPU_ONLY
    GLFunctions gl;SDL_GLContext context=nullptr;GLuint program=0,vbo=0,vao=0;std::vector<GLuint> textures;
    GLint uMVP=-1,uTexture=-1,uPrim=-1,uEnv=-1,uRgb0=-1,uRgb1=-1,uAlpha0=-1,uAlpha1=-1,uCycle=-1,uWrap=-1,uPeriod=-1,uOpacity=-1;
#endif
    bool fail(const std::string &message){bool changed=error!=message;error=message;if(callback&&changed)callback(user,error.c_str());return false;}
};
#ifndef BK_DUO_CPU_ONLY
namespace {
void releaseGL(BkDuoGL &r) {
    if(r.context&&SDL_GL_GetCurrentContext()==r.context){GLFunctions &g=r.gl;if(r.program)g.DeleteProgram(r.program);if(r.vbo)g.DeleteBuffers(1,&r.vbo);if(r.vao)g.DeleteVertexArrays(1,&r.vao);if(!r.textures.empty())g.DeleteTextures((GLsizei)r.textures.size(),r.textures.data());}
    r.context=nullptr;r.program=r.vbo=r.vao=0;r.textures.clear();
}
GLuint shader(BkDuoGL &r,GLenum type,const std::string &source) {
    GLFunctions &g=r.gl;GLuint s=g.CreateShader(type);const char *p=source.c_str();g.ShaderSource(s,1,&p,NULL);g.CompileShader(s);GLint ok=0;g.GetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char error[1024]={};g.GetShaderInfoLog(s,sizeof(error)-1,NULL,error);r.fail(std::string("Banjo-Kazooie shader failed: ")+error);g.DeleteShader(s);return 0;}return s;
}
bool initGL(BkDuoGL &r) {
    if(!SDL_GL_GetCurrentContext())return r.fail("Banjo-Kazooie draw needs a current SDL OpenGL context");
    if(r.context==SDL_GL_GetCurrentContext()&&r.program)return true;
    if(r.context&&r.context!=SDL_GL_GetCurrentContext())return r.fail("Banjo-Kazooie renderer belongs to a different GL context; recreate it after context loss");
    if(!r.gl.load())return r.fail("Banjo-Kazooie renderer OpenGL functions unavailable");
    GLFunctions &g=r.gl;GLState saved(g);r.context=SDL_GL_GetCurrentContext();
    std::string prefix=g.es?"#version 100\nprecision mediump float;\n":g.modern?"#version 130\n":"#version 120\n";
    std::string vs=prefix+(g.modern?"in vec3 aPosition;in vec2 aUV;in vec4 aShade;out vec2 vUV;out vec4 vShade;":"attribute vec3 aPosition;attribute vec2 aUV;attribute vec4 aShade;varying vec2 vUV;varying vec4 vShade;");
    vs+="uniform mat4 uMVP;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vUV=aUV;vShade=aShade;}";
    std::string fs=prefix+(g.modern?"in vec2 vUV;in vec4 vShade;out vec4 outColor;\n":"varying vec2 vUV;varying vec4 vShade;\n");
    fs+=
        "uniform sampler2D uTexture;uniform vec4 uPrim,uEnv,uPeriod,uOpacity;uniform ivec4 uRgb0,uRgb1,uAlpha0,uAlpha1,uWrap;uniform int uCycle;\n"
        "float wrapCoord(float x,int mode,float period){if(mode>=2||period<=0.0)return x;if(mode==1)return (1.0-abs(mod(x/period,2.0)-1.0))*period;return fract(x/period)*period;}\n"
        "vec3 rgb(int s,int term,vec4 comb,vec4 tex){if(s==0)return comb.rgb;if(s==1||s==2)return tex.rgb;if(s==3)return uPrim.rgb;if(s==4)return vShade.rgb;if(s==5)return uEnv.rgb;if(s==6&&(term==0||term==3))return vec3(1.0);if(term==2){if(s==7)return vec3(comb.a);if(s==8||s==9)return vec3(tex.a);if(s==10)return vec3(uPrim.a);if(s==11)return vec3(vShade.a);if(s==12)return vec3(uEnv.a);}return vec3(0.0);}\n"
        "float alpha(int s,int term,vec4 comb,vec4 tex){if(s==0)return term==2?0.0:comb.a;if(s==1||s==2)return tex.a;if(s==3)return uPrim.a;if(s==4)return vShade.a;if(s==5)return uEnv.a;if(s==6&&term!=2)return 1.0;return 0.0;}\n"
        "vec4 cycle(ivec4 c,ivec4 a,vec4 comb,vec4 tex){vec3 color=(rgb(c.x,0,comb,tex)-rgb(c.y,1,comb,tex))*rgb(c.z,2,comb,tex)+rgb(c.w,3,comb,tex);float opacity=(alpha(a.x,0,comb,tex)-alpha(a.y,1,comb,tex))*alpha(a.z,2,comb,tex)+alpha(a.w,3,comb,tex);return clamp(vec4(color,opacity),0.0,1.0);}\n"
        "void main(){vec2 uv=vec2(wrapCoord(vUV.x,uWrap.x,uPeriod.x),wrapCoord(vUV.y,uWrap.y,uPeriod.y));vec4 tex=";
    fs+=g.modern?"texture(uTexture,uv);":"texture2D(uTexture,uv);";
    fs+="vec4 color=vec4(0.0);if(uCycle==1)color=cycle(uRgb0,uAlpha0,color,tex);color=cycle(uRgb1,uAlpha1,color,tex);if(uCycle==2)color=tex;if(uCycle==3)color=uPrim;if(color.a<0.1)discard;color.a*=uOpacity.x;";
    fs+=g.modern?"outColor=color;}":"gl_FragColor=color;}";
    GLuint v=shader(r,GL_VERTEX_SHADER,vs),f=shader(r,GL_FRAGMENT_SHADER,fs);if(!v||!f){if(v)g.DeleteShader(v);if(f)g.DeleteShader(f);releaseGL(r);return false;}
    r.program=g.CreateProgram();g.AttachShader(r.program,v);g.AttachShader(r.program,f);g.BindAttribLocation(r.program,0,"aPosition");g.BindAttribLocation(r.program,1,"aUV");g.BindAttribLocation(r.program,2,"aShade");g.LinkProgram(r.program);g.DeleteShader(v);g.DeleteShader(f);GLint ok=0;g.GetProgramiv(r.program,GL_LINK_STATUS,&ok);if(!ok){char error[1024]={};g.GetProgramInfoLog(r.program,sizeof(error)-1,NULL,error);r.fail(std::string("Banjo-Kazooie shader program failed: ")+error);releaseGL(r);return false;}
#define U(name) r.name=g.GetUniformLocation(r.program,#name)
    U(uMVP);U(uTexture);U(uPrim);U(uEnv);U(uRgb0);U(uRgb1);U(uAlpha0);U(uAlpha1);U(uCycle);U(uWrap);U(uPeriod);U(uOpacity);
#undef U
    g.GenBuffers(1,&r.vbo);if(g.vaoSupported)g.GenVertexArrays(1,&r.vao);r.textures.resize(r.asset->materials.size());g.GenTextures((GLsizei)r.textures.size(),r.textures.data());
    g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);if(g.unpackBuffer)g.BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);g.PixelStorei(GL_UNPACK_ALIGNMENT,1);if(g.unpackRows){g.PixelStorei(GL_UNPACK_ROW_LENGTH,0);g.PixelStorei(GL_UNPACK_SKIP_ROWS,0);g.PixelStorei(GL_UNPACK_SKIP_PIXELS,0);}
    for(size_t i=0;i<r.textures.size();++i){const Material &m=r.asset->materials[i];g.BindTexture(GL_TEXTURE_2D,r.textures[i]);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);g.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,m.width,m.height,0,GL_RGBA,GL_UNSIGNED_BYTE,m.rgba.data());}return true;
}
}
#endif
extern "C" BkDuoGL *bk_duo_gl_create(BkDuoErrorFn callback,void *user) {
    try{BkDuoGL *r=new BkDuoGL;r->callback=callback;r->user=user;for(size_t i=18;i<=41;++i)r->selectors[i]=1;return r;}catch(...){return NULL;}
}
extern "C" int bk_duo_gl_load(BkDuoGL *r,const char *path) {
    if(!r)return 0;
#ifndef BK_DUO_CPU_ONLY
    releaseGL(*r);
#endif
    r->asset.reset();r->posed.clear();r->stream.clear();r->batches.clear();
    if(!path||!*path)return r->fail("Banjo-Kazooie asset path is empty");
    try{r->asset.reset(new Asset(loadAsset(path)));r->error.clear();
        if(r->callback)for(const Material &m:r->asset->materials)if(m.geometry&0x00040000u){r->callback(r->user,"Banjo-Kazooie appearance warning: original texture-generation materials currently use stored UVs; reflection highlights are approximate");break;}
        return 1;}catch(const std::exception &e){return r->fail(std::string("Banjo-Kazooie assets rejected: ")+e.what());}catch(...){return r->fail("Banjo-Kazooie asset load failed");}
}
extern "C" int bk_duo_gl_is_loaded(const BkDuoGL *r){return r&&r->asset?1:0;}
extern "C" const char *bk_duo_gl_last_error(const BkDuoGL *r){return r?r->error.c_str():"null Banjo-Kazooie renderer";}
extern "C" int bk_duo_gl_set_selectors(BkDuoGL *r,const int *selectors,size_t count){
    if(!r||!selectors||count!=64)return 0;
    for(size_t i=0;i<count;++i)if(selectors[i]<0||selectors[i]>64)return 0;
    std::copy(selectors,selectors+count,r->selectors.begin());return 1;
}
extern "C" int bk_duo_gl_set_opacity(BkDuoGL *r,float opacity){if(!r||!std::isfinite(opacity)||opacity<0||opacity>1)return 0;r->opacity=opacity;return 1;}
extern "C" int bk_duo_gl_draw(BkDuoGL *r,const float view[16],const float projection[16],const int viewport[4],const float *joints,const float *billboards,size_t jointCount) {
    if(!r||!r->asset)return 0;
    if(!finite(view,16)||!finite(projection,16)||!viewport||viewport[2]<=0||viewport[3]<=0)return r->fail("Invalid Banjo-Kazooie camera/viewport");
    try {
        if(!pose(*r->asset,joints,billboards,jointCount,r->posed))return r->fail("Invalid or missing original Banjo-Kazooie joint palettes");
#ifndef BK_DUO_CPU_ONLY
        if(!initGL(*r))return 0;
        GLFunctions &g=r->gl;GLState saved(g);
        r->stream.clear();r->batches.clear();r->stream.reserve(r->asset->triangles.size()*3);
        for(const Triangle &tri:r->asset->triangles){bool visible=true;for(const auto &s:tri.selectors)if(r->selectors[s.first]!=s.second){visible=false;break;}if(!visible)continue;if(r->batches.empty()||r->batches.back().material!=tri.material){Batch b={tri.material,r->stream.size(),0};r->batches.push_back(b);}const Material &m=r->asset->materials[tri.material];
            for(int k=0;k<3;++k){DrawVertex v=r->posed[tri.index[k]];textureUV(r->asset->vertices[tri.index[k]],m,v.uv);if(r->asset->vertices[tri.index[k]].lit){float dot=std::max(0.0f,(v.shade[0]-.35f)/.65f);for(int q=0;q<3;++q)v.shade[q]=std::min(1.0f,m.ambient[q]+m.diffuse[q]*dot);}r->stream.push_back(v);}r->batches.back().count+=3;
        }
        Matrix v,p;std::copy(view,view+16,v.begin());std::copy(projection,projection+16,p.begin());Matrix mvp=multiply(p,v);g.UseProgram(r->program);g.UniformMatrix4fv(r->uMVP,1,GL_FALSE,mvp.data());g.Uniform1i(r->uTexture,0);float opacity[4]={r->opacity,0,0,0};g.Uniform4fv(r->uOpacity,1,opacity);
        if(g.vaoSupported)g.BindVertexArray(r->vao);
        g.BindBuffer(GL_ARRAY_BUFFER,r->vbo);g.BufferData(GL_ARRAY_BUFFER,(GLsizeiptr)(r->stream.size()*sizeof(DrawVertex)),r->stream.data(),GL_STREAM_DRAW);
        g.EnableVertexAttribArray(0);g.EnableVertexAttribArray(1);g.EnableVertexAttribArray(2);g.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,position));g.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,uv));g.VertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,shade));
        g.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);g.Enable(GL_DEPTH_TEST);g.DepthFunc(GL_LEQUAL);g.DepthMask(GL_TRUE);if(g.es)g.DepthRangef(0,1);else g.DepthRange(0,1);g.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        for(GLenum cap:CAPABILITIES)if(cap!=GL_DEPTH_TEST)g.Disable(cap);
        if(r->opacity<1){g.Enable(GL_BLEND);g.BlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);g.BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);}
        if(g.raster)g.Disable(GL_RASTERIZER_DISCARD);
        if(!g.es)g.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);
        for(const Batch &b:r->batches){const Material &m=r->asset->materials[b.material];
            /* BK uses the original F3DEX 0x1000/0x2000 cull bits. Preserving
             * these prevents coplanar front/back wing faces from z-fighting. */
            unsigned cull=m.geometry&0x3000u;g.FrontFace(GL_CCW);
            if(cull){g.Enable(GL_CULL_FACE);g.CullFace(cull==0x3000u?GL_FRONT_AND_BACK:cull==0x1000u?GL_FRONT:GL_BACK);}else g.Disable(GL_CULL_FACE);
            g.BindTexture(GL_TEXTURE_2D,r->textures[b.material]);g.Uniform4fv(r->uPrim,1,m.prim);g.Uniform4fv(r->uEnv,1,m.env);g.Uniform4iv(r->uRgb0,1,m.rgb[0]);g.Uniform4iv(r->uRgb1,1,m.rgb[1]);g.Uniform4iv(r->uAlpha0,1,m.alpha[0]);g.Uniform4iv(r->uAlpha1,1,m.alpha[1]);g.Uniform1i(r->uCycle,m.cycle);GLint wrap[4]={m.wrap[0],m.wrap[1],0,0};g.Uniform4iv(r->uWrap,1,wrap);float period[4]={m.mask[0]?float(1<<m.mask[0])/m.width:0,m.mask[1]?float(1<<m.mask[1])/m.height:0,0,0};g.Uniform4fv(r->uPeriod,1,period);g.DrawArrays(GL_TRIANGLES,(GLint)b.first,(GLsizei)b.count);}
        r->error.clear();return 1;
#else
        return r->fail("OpenGL drawing unavailable in CPU-only renderer build");
#endif
    }catch(const std::exception &e){return r->fail(std::string("Banjo-Kazooie draw failed: ")+e.what());}catch(...){return r->fail("Banjo-Kazooie draw failed");}
}
extern "C" void bk_duo_gl_destroy(BkDuoGL *r){if(!r)return;
#ifndef BK_DUO_CPU_ONLY
    releaseGL(*r);
#endif
    delete r;
}
#ifdef BK_DUO_TESTING
extern "C" size_t bk_duo_test_visible_triangles(const BkDuoGL *r){
    if(!r||!r->asset)return 0;
    size_t count=0;
    for(const Triangle &tri:r->asset->triangles){bool visible=true;for(const auto &s:tri.selectors)if(r->selectors[s.first]!=s.second){visible=false;break;}if(visible)++count;}return count;
}
extern "C" int bk_duo_test_uv(const BkDuoGL *r,size_t vertex,size_t material,float uv[2]) {
    if(!r||!r->asset||!uv||vertex>=r->asset->vertices.size()||material>=r->asset->materials.size())return 0;
    textureUV(r->asset->vertices[vertex],r->asset->materials[material],uv);return 1;
}
extern "C" size_t bk_duo_test_palette_floats(const BkDuoGL *r){return r&&r->asset?r->asset->paletteCount*16:0;}
extern "C" size_t bk_duo_test_position_floats(const BkDuoGL *r){return r&&r->asset?r->asset->vertices.size()*3:0;}
extern "C" int bk_duo_test_pose(BkDuoGL *r,const float *joints,const float *billboards,size_t count,float *positions,size_t capacity) {
    if(!r||!r->asset||!positions||capacity<bk_duo_test_position_floats(r))return 0;
    try{if(!pose(*r->asset,joints,billboards,count,r->posed))return 0;for(size_t i=0;i<r->posed.size();++i)std::copy(r->posed[i].position,r->posed[i].position+3,positions+i*3);return 1;}catch(...){return 0;}
}
#endif
