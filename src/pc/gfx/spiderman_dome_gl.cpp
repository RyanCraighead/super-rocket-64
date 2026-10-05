/* Ordinary source dome/ring host embedding. GL capability/state restoration
 * machinery adapted verbatim from spiderman_web_gl.cpp; source geometry and
 * material packets come solely from the recovered ordinary dome producers. */
#include "spiderman_dome_gl.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#ifdef __FAST_MATH__
#error "Original dome rendering requires no fast-math or FP contraction"
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
#ifdef SPIDERMAN_DOME_TEST_FORCE_NO_VAO
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
struct Batch {size_t first,count;unsigned texture;};
bool geometry(const float *view,const float *projection,const int *vp,const SpidermanDomePacket *packets,size_t count,std::vector<DrawVertex>&stream,std::vector<Batch>&batches){
    if(!view||!projection||!vp||count>SPIDERMAN_DOME_SCENE_CAPACITY||(count&&!packets)||std::fegetround()!=FE_TONEAREST)return false;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(view[i])||!std::isfinite(projection[i])||std::fabs(view[i])>1e12f||std::fabs(projection[i])>1e12f)return false;
    if(vp[0]<-65536||vp[0]>65536||vp[1]<-65536||vp[1]>65536||vp[2]<=0||vp[2]>65536||vp[3]<=0||vp[3]>65536)return false;
    float biased[16];if(!smn64_dome_host_projection(projection,biased))return false;
    double vpMatrix[16]={};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)vpMatrix[c*4+r]+=double(biased[k*4+r])*view[c*4+k];
    for(size_t at=0;at<count;++at){const auto&d=packets[at].draw;const auto&m=packets[at].material;
        const bool ring=d.model_slot==226;const unsigned side=ring?32:64;
        if((!ring&&d.model_slot!=248&&d.model_slot!=249)||(d.model_slot==249?d.node>=5:d.node!=0))return false;
        const unsigned corners=ring?324:d.model_slot==248?450:d.node==2?45:90;
        if(d.corner_count!=corners||d.texture_slot!=(ring?404:403)||m.texture_slot!=d.texture_slot||m.width!=side||m.height!=side||m.combiner[0]!=(ring?0xfc129bffu:0xfc50d3ffu)||m.combiner[1]!=0xfffffe38u||m.depth_compare!=1||m.depth_write||m.uses_scroll!=(ring?1:0)||m.writes_fog>1)return false;
        if(m.render_mode!=(m.writes_fog?0x01504a50u:0x0c184b50u)||(m.writes_fog&&m.fog_rgba[3]!=254))return false;
        unsigned nearS=0,nearT=0;
        if(ring){nearS=(m.tile_size[0]>>12)&4095;nearT=m.tile_size[0]&4095;if(nearS>128||nearT||m.tile_size[0]!=(0xf2000000u|(nearS<<12))||m.tile_size[1]!=(0x01000000u|((127+nearS)<<12)|127))return false;}
        else if(m.tile_size[0]!=0xf2000000u||m.tile_size[1]!=0x010fc0fcu)return false;
        if(d.model_s16_16[3]||d.model_s16_16[7]||d.model_s16_16[11]||d.model_s16_16[15]!=65536)return false;
        const size_t first=stream.size();
        for(unsigned i=0;i<d.corner_count;++i){const auto&v=d.vertices[i];const double xyz[4]={double(v.x),double(v.y),double(v.z),1};double world[4]={};
            for(unsigned r=0;r<4;++r)for(unsigned k=0;k<4;++k)world[r]+=double(d.model_s16_16[k*4+r])*xyz[k]/65536.;
            world[0]*=16;world[1]*=-16;world[2]*=-16;
            DrawVertex out={};for(unsigned r=0;r<4;++r){double value=0;for(unsigned k=0;k<4;++k)value+=vpMatrix[k*4+r]*world[k];if(!std::isfinite(value)||std::fabs(value)>std::numeric_limits<float>::max())return false;out.position[r]=float(value);}
            out.uv[0]=float((double(v.s)/32.+.5-double(nearS)/4.)/side);out.uv[1]=float((double(v.t)/32.+.5-double(nearT)/4.)/side);
            for(unsigned c=0;c<4;++c)out.shade[c]=m.environment_rgba[c]/255.f;
            stream.push_back(out);
        }
        batches.push_back({first,stream.size()-first,ring?1u:0u});
    }return true;
}
}
struct SpidermanDomeGL {
    std::string error,hostWarning;bool loaded=false;
    std::array<std::vector<unsigned char>,2> pixels;
    std::vector<DrawVertex> stream;std::vector<Batch> batches;
#ifndef SPIDERMAN_CPU_ONLY
    GLFunctions gl;SDL_GLContext context=nullptr;GLuint program=0,vbo=0,vao=0;std::array<GLuint,2> textures={};GLint uTexture=-1;
#endif
    int fail(const std::string&s){error=s;return 0;}
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
bool checkGL(SpidermanDomeGL &r,const char *phase) {
    const auto flags=drainErrors(r.gl);if(flags.text.empty())return true;
    return r.fail(std::string("Spider-Man dome GL error during ")+phase+": "+flags.text+(flags.bounded?"":" (error queue did not clear)"));
}
bool beginGLBoundary(SpidermanDomeGL &r) {
    if(!SDL_GL_GetCurrentContext())return r.fail("Spider-Man dome draw needs a current SDL GL context");
    if(r.context&&r.context!=SDL_GL_GetCurrentContext())return r.fail("Spider-Man dome renderer belongs to a different GL context; recreate after context loss");
    // Resolve only GetError before issuing ANY renderer GL command, including
    // version/extension probes. This is the attribution boundary in the borrowed
    // host context. A stale host flag does not prove our later draw failed.
    r.gl.GetError=(GLenum(APIENTRY*)(void))SDL_GL_GetProcAddress("glGetError");
    if(!r.gl.GetError)return r.fail("Spider-Man dome GL error query unavailable");
    const auto flags=drainErrors(r.gl);
    if(!flags.text.empty()){
        const std::string message="Inherited host GL error before Spider-Man dome calls: "+flags.text;
        if(message!=r.hostWarning){r.hostWarning=message;std::fprintf(stderr,"%s\n",message.c_str());}
    }
    if(!flags.bounded||flags.contextLost)return r.fail("Spider-Man dome cannot enter a lost/unresponsive host GL context: "+flags.text);
    return true;
}
GLuint shader(SpidermanDomeGL &r,GLenum type,const std::string &source) {
    auto &g=r.gl;GLuint s=g.CreateShader(type);const char *p=source.c_str();g.ShaderSource(s,1,&p,nullptr);g.CompileShader(s);GLint good=0;g.GetShaderiv(s,GL_COMPILE_STATUS,&good);
    if(!good){char log[1024]={};g.GetShaderInfoLog(s,sizeof(log)-1,nullptr,log);r.fail(std::string("Spider-Man dome shader failed: ")+log);g.DeleteShader(s);return 0;}return s;
}
void releaseGL(SpidermanDomeGL &r) {
    if(r.context&&SDL_GL_GetCurrentContext()==r.context){auto &g=r.gl;if(r.program)g.DeleteProgram(r.program);if(r.vbo)g.DeleteBuffers(1,&r.vbo);if(r.vao)g.DeleteVertexArrays(1,&r.vao);g.DeleteTextures(2,r.textures.data());}
    r.context=nullptr;r.program=r.vbo=r.vao=0;r.textures={};
}
bool initGL(SpidermanDomeGL&r){
    if(r.context==SDL_GL_GetCurrentContext()&&r.program)return true;
    const bool functions=r.gl.load();if(!checkGL(r,"capability discovery"))return false;if(!functions)return r.fail("Spider-Man dome GL functions unavailable");
    auto&g=r.gl;GLState saved(g);r.context=SDL_GL_GetCurrentContext();
    const std::string prefix=g.es?"#version 100\nprecision mediump float;\n":g.modern?"#version 130\n":"#version 120\n";
    std::string vs=(g.es?"#version 100\nprecision highp float;\n":prefix)+(g.modern?std::string("in vec4 aPosition;in vec2 aUV;in vec4 aShade;out vec2 vUV;out vec4 vShade;"):g.es?"attribute vec4 aPosition;attribute vec2 aUV;attribute vec4 aShade;varying mediump vec2 vUV;varying mediump vec4 vShade;":"attribute vec4 aPosition;attribute vec2 aUV;attribute vec4 aShade;varying vec2 vUV;varying vec4 vShade;");
    vs+="void main(){gl_Position=aPosition;vUV=aUV;vShade=aShade;}";
    std::string fs=prefix+(g.modern?"in vec2 vUV;in vec4 vShade;out vec4 outColor;":"varying vec2 vUV;varying vec4 vShade;");fs+="uniform sampler2D uTexture;void main(){vec4 color=";fs+=g.modern?"texture(uTexture,vUV)*vShade;outColor=color;}":"texture2D(uTexture,vUV)*vShade;gl_FragColor=color;}";
    GLuint v=shader(r,GL_VERTEX_SHADER,vs),f=shader(r,GL_FRAGMENT_SHADER,fs);if(!v||!f){if(v)g.DeleteShader(v);if(f)g.DeleteShader(f);releaseGL(r);return false;}
    r.program=g.CreateProgram();g.AttachShader(r.program,v);g.AttachShader(r.program,f);g.BindAttribLocation(r.program,0,"aPosition");g.BindAttribLocation(r.program,1,"aUV");g.BindAttribLocation(r.program,2,"aShade");g.LinkProgram(r.program);g.DeleteShader(v);g.DeleteShader(f);GLint good=0;g.GetProgramiv(r.program,GL_LINK_STATUS,&good);if(!good){releaseGL(r);return r.fail("Spider-Man dome shader link failed");}
    r.uTexture=g.GetUniformLocation(r.program,"uTexture");g.GenBuffers(1,&r.vbo);if(g.vaoSupported)g.GenVertexArrays(1,&r.vao);g.GenTextures(2,r.textures.data());
    g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);if(g.unpackBuffer)g.BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);g.PixelStorei(GL_UNPACK_ALIGNMENT,1);if(g.unpackRows){g.PixelStorei(GL_UNPACK_ROW_LENGTH,0);g.PixelStorei(GL_UNPACK_SKIP_ROWS,0);g.PixelStorei(GL_UNPACK_SKIP_PIXELS,0);}
    for(unsigned i=0;i<2;++i){const unsigned side=i?32:64;g.BindTexture(GL_TEXTURE_2D,r.textures[i]);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);g.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,side,side,0,GL_RGBA,GL_UNSIGNED_BYTE,r.pixels[i].data());}
    if(!checkGL(r,"shader and texture initialization")){releaseGL(r);return false;}return true;
}
int submit(SpidermanDomeGL &r,const int viewport[4]) {
    if(!beginGLBoundary(r))return 0;
    const bool initialized=initGL(r),restored=checkGL(r,"initialization state restoration");if(!initialized||!restored)return 0;
    auto &g=r.gl;auto draw=[&](){GLState saved(g);g.UseProgram(r.program);g.Uniform1i(r.uTexture,0);if(g.vaoSupported)g.BindVertexArray(r.vao);g.BindBuffer(GL_ARRAY_BUFFER,r.vbo);g.BufferData(GL_ARRAY_BUFFER,GLsizeiptr(r.stream.size()*sizeof(DrawVertex)),r.stream.data(),GL_STREAM_DRAW);
        for(int i=0;i<3;++i){g.EnableVertexAttribArray(i);if(g.divisorAttrs)g.VertexAttribDivisor(i,0);}g.VertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,position));g.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,uv));g.VertexAttribPointer(2,4,GL_FLOAT,GL_FALSE,sizeof(DrawVertex),(void*)offsetof(DrawVertex,shade));
        for(GLenum cap:CAPABILITIES)g.Disable(cap);
        g.Enable(GL_CULL_FACE);g.CullFace(GL_BACK);g.FrontFace(GL_CCW);g.Enable(GL_DEPTH_TEST);g.DepthFunc(GL_LEQUAL);g.DepthMask(GL_FALSE);g.Enable(GL_BLEND);g.BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);g.BlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);g.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);g.Viewport(viewport[0],viewport[1],viewport[2],viewport[3]);if(g.es)g.DepthRangef(0,1);else g.DepthRange(0,1);if(g.compat)g.Disable(GL_ALPHA_TEST);if(!g.es){g.Disable(GL_COLOR_LOGIC_OP);g.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);}if(g.raster)g.Disable(GL_RASTERIZER_DISCARD);g.ActiveTexture(GL_TEXTURE0);if(g.samplers)g.BindSampler(0,0);g.BindTexture(GL_TEXTURE_2D,r.textures[0]);if(!checkGL(r,"vertex upload/draw setup"))return 0;for(const auto &batch:r.batches){g.BindTexture(GL_TEXTURE_2D,r.textures[batch.texture]);g.DrawArrays(GL_TRIANGLES,GLint(batch.first),GLsizei(batch.count));}return checkGL(r,"dome triangle submission")?1:0;};
    int result=draw();if(!checkGL(r,"draw state restoration"))result=0;return result;
}
}
#endif
extern "C" SpidermanDomeGL *spiderman_dome_gl_create(void){try{return new SpidermanDomeGL;}catch(...){return nullptr;}}
extern "C" int spiderman_dome_gl_load(SpidermanDomeGL*r,const SpidermanDomeAssets*a){
    if(!r)return 0;
#ifndef SPIDERMAN_CPU_ONLY
    if(r->context&&r->context!=SDL_GL_GetCurrentContext())return r->fail("Dome reload requires owning GL context");
    releaseGL(*r);
#endif
    r->loaded=false;for(auto&p:r->pixels)p.clear();
    try{if(!spiderman_dome_assets_ready(a))return r->fail("Verified original dome assets unavailable");std::array<std::vector<unsigned char>,2> pixels;
        for(unsigned i=0;i<2;++i){pixels[i].resize(i?4096:16384);if(!spiderman_dome_assets_copy_texture(a,403+i,pixels[i].data(),pixels[i].size()))return r->fail("Original dome texture copy failed");}
        r->pixels.swap(pixels);r->loaded=true;r->error.clear();return 1;
    }catch(...){return r->fail("Original dome texture allocation failed");}
}
extern "C" int spiderman_dome_gl_ready(const SpidermanDomeGL*r){return r&&r->loaded;}
extern "C" const char *spiderman_dome_gl_error(const SpidermanDomeGL*r){return r?r->error.c_str():"Null original dome renderer";}
extern "C" const char *spiderman_dome_gl_host_warning(const SpidermanDomeGL*r){return r?r->hostWarning.c_str():"";}
extern "C" int spiderman_dome_gl_draw(SpidermanDomeGL*r,const float view[16],const float projection[16],const int vp[4],const SpidermanDomePacket*packets,size_t count){
    if(!r)return 0;
    try{r->stream.clear();r->batches.clear();if(!r->loaded||!geometry(view,projection,vp,packets,count,r->stream,r->batches)){r->stream.clear();r->batches.clear();return r->fail("Invalid original dome frame/material/camera or unavailable assets");}if(r->stream.empty()){r->error.clear();return 1;}
#ifndef SPIDERMAN_CPU_ONLY
        if(!submit(*r,vp)){r->stream.clear();r->batches.clear();return 0;}r->error.clear();return 1;
#else
        return r->fail("OpenGL unavailable in CPU-only dome build");
#endif
    }catch(...){r->stream.clear();r->batches.clear();return r->fail("Original dome draw allocation failed");}
}
extern "C" void spiderman_dome_gl_destroy(SpidermanDomeGL*r){if(!r)return;
#ifndef SPIDERMAN_CPU_ONLY
    releaseGL(*r);
#endif
    delete r;
}
#ifdef SPIDERMAN_TESTING
extern "C" ptrdiff_t spiderman_dome_test_geometry(const float view[16],const float projection[16],const int vp[4],const SpidermanDomePacket*packets,size_t count,float*out,size_t capacity){try{std::vector<DrawVertex>stream;std::vector<Batch>batches;if(!geometry(view,projection,vp,packets,count,stream,batches)||capacity<stream.size()*10||(!out&&!stream.empty()))return -1;size_t at=0;for(const auto&v:stream){for(float n:v.position)out[at++]=n;for(float n:v.uv)out[at++]=n;for(float n:v.shade)out[at++]=n;}return ptrdiff_t(stream.size());}catch(...){return -1;}}
#endif
