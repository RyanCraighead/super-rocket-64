/* GL state isolation copied from bm64_bomberman_gl.cpp in this host.
 * Kept separate to avoid changing the existing character renderers. */
#pragma once
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
 X(void,BlendFuncSeparate,(GLenum,GLenum,GLenum,GLenum)) X(void,BlendEquationSeparate,(GLenum,GLenum)) X(void,DepthFunc,(GLenum)) X(void,DepthMask,(GLboolean)) X(void,ColorMask,(GLboolean,GLboolean,GLboolean,GLboolean)) X(void,Viewport,(GLint,GLint,GLsizei,GLsizei))
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
    GLint blendSrcRGB,blendDstRGB,blendSrcAlpha,blendDstAlpha,blendEqRGB,blendEqAlpha;
    GLboolean enabled[8],depthWrite,colorWrite[4],raster=0;GLfloat depthRange[2];AttrState attr[3];
    explicit GLState(GLFunctions &gl):g(gl){
        g.GetIntegerv(GL_ACTIVE_TEXTURE,&active);g.ActiveTexture(GL_TEXTURE0);g.GetIntegerv(GL_TEXTURE_BINDING_2D,&texture);if(g.samplers)g.GetIntegerv(GL_SAMPLER_BINDING,&sampler);
        g.GetIntegerv(GL_BLEND_SRC_RGB,&blendSrcRGB);g.GetIntegerv(GL_BLEND_DST_RGB,&blendDstRGB);g.GetIntegerv(GL_BLEND_SRC_ALPHA,&blendSrcAlpha);g.GetIntegerv(GL_BLEND_DST_ALPHA,&blendDstAlpha);g.GetIntegerv(GL_BLEND_EQUATION_RGB,&blendEqRGB);g.GetIntegerv(GL_BLEND_EQUATION_ALPHA,&blendEqAlpha);
        g.GetIntegerv(GL_CURRENT_PROGRAM,&program);g.GetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        if(g.vaoSupported)g.GetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);else for(GLuint i=0;i<3;++i){g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&attr[i].enabled);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&attr[i].size);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&attr[i].type);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&attr[i].normalized);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&attr[i].stride);g.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&attr[i].buffer);g.GetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&attr[i].pointer);}
        g.GetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);if(g.unpackRows){g.GetIntegerv(GL_UNPACK_ROW_LENGTH,&row);g.GetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);g.GetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);}if(g.unpackBuffer)g.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
        if(!g.es)g.GetIntegerv(GL_POLYGON_MODE,polygon);
        g.GetIntegerv(GL_VIEWPORT,vp);g.GetIntegerv(GL_DEPTH_FUNC,&depthFunc);g.GetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);g.GetBooleanv(GL_COLOR_WRITEMASK,colorWrite);g.GetFloatv(GL_DEPTH_RANGE,depthRange);
        for(int i=0;i<8;++i)enabled[i]=g.IsEnabled(CAPABILITIES[i]);
        if(g.raster)raster=g.IsEnabled(GL_RASTERIZER_DISCARD);
    }
    ~GLState(){
        g.BlendFuncSeparate(blendSrcRGB,blendDstRGB,blendSrcAlpha,blendDstAlpha);g.BlendEquationSeparate(blendEqRGB,blendEqAlpha);
        g.UseProgram(program);if(g.vaoSupported)g.BindVertexArray(vao);else for(GLuint i=0;i<3;++i){g.BindBuffer(GL_ARRAY_BUFFER,attr[i].buffer);g.VertexAttribPointer(i,attr[i].size,attr[i].type,attr[i].normalized,attr[i].stride,attr[i].pointer);if(attr[i].enabled)g.EnableVertexAttribArray(i);else g.DisableVertexAttribArray(i);}g.BindBuffer(GL_ARRAY_BUFFER,array);
        g.ActiveTexture(GL_TEXTURE0);g.BindTexture(GL_TEXTURE_2D,texture);if(g.samplers)g.BindSampler(0,sampler);g.ActiveTexture(active);
        g.PixelStorei(GL_UNPACK_ALIGNMENT,unpack);if(g.unpackRows){g.PixelStorei(GL_UNPACK_ROW_LENGTH,row);g.PixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);g.PixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);}if(g.unpackBuffer)g.BindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
        if(!g.es){if(polygon[0]==polygon[1])g.PolygonMode(GL_FRONT_AND_BACK,polygon[0]);else{g.PolygonMode(GL_FRONT,polygon[0]);g.PolygonMode(GL_BACK,polygon[1]);}}
        g.Viewport(vp[0],vp[1],vp[2],vp[3]);g.DepthFunc(depthFunc);g.DepthMask(depthWrite);g.ColorMask(colorWrite[0],colorWrite[1],colorWrite[2],colorWrite[3]);if(g.es)g.DepthRangef(depthRange[0],depthRange[1]);else g.DepthRange(depthRange[0],depthRange[1]);for(int i=0;i<8;++i){if(enabled[i])g.Enable(CAPABILITIES[i]);else g.Disable(CAPABILITIES[i]);}if(g.raster){if(raster)g.Enable(GL_RASTERIZER_DISCARD);else g.Disable(GL_RASTERIZER_DISCARD);}
    }
};
}
