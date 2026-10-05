#include "wheel_overlay_gl.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
static const int kWidth=1024,kHeight=768;
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
#ifndef GL_SAMPLES_PASSED
#define GL_SAMPLES_PASSED 0x8914
#endif
#ifndef GL_ANY_SAMPLES_PASSED
#define GL_ANY_SAMPLES_PASSED 0x8C2F
#endif
#ifndef GL_CURRENT_QUERY
#define GL_CURRENT_QUERY 0x8865
#define GL_QUERY_RESULT 0x8866
#endif
#ifndef GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE 0x8CD0
#endif
#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif
#ifndef GL_DEPTH
#define GL_DEPTH 0x1801
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE 0x8216
#endif
#ifndef GL_QUERY_BUFFER_BINDING
#define GL_QUERY_BUFFER_BINDING 0x9193
#endif
#ifndef GL_ANY_SAMPLES_PASSED_CONSERVATIVE
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE 0x8D6A
#endif
#ifndef GL_POLYGON_MODE
#define GL_POLYGON_MODE 0x0B40
#define GL_FILL 0x1B02
#endif
namespace {
#define GL_REQUIRED(X) \
 X(void,GetIntegerv,(GLenum,GLint*)) X(void,GetBooleanv,(GLenum,GLboolean*)) X(void,GetFloatv,(GLenum,GLfloat*)) \
 X(const GLubyte*,GetString,(GLenum)) X(GLboolean,IsEnabled,(GLenum)) X(void,Enable,(GLenum)) X(void,Disable,(GLenum)) \
 X(void,ActiveTexture,(GLenum)) X(void,BindTexture,(GLenum,GLuint)) X(void,GenTextures,(GLsizei,GLuint*)) X(void,DeleteTextures,(GLsizei,const GLuint*)) \
 X(void,TexParameteri,(GLenum,GLenum,GLint)) X(void,TexImage2D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) \
 X(void,TexSubImage2D,(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*)) X(void,PixelStorei,(GLenum,GLint)) \
 X(void,GenBuffers,(GLsizei,GLuint*)) X(void,BindBuffer,(GLenum,GLuint)) X(void,BufferData,(GLenum,GLsizeiptr,const void*,GLenum)) X(void,DeleteBuffers,(GLsizei,const GLuint*)) \
 X(GLuint,CreateShader,(GLenum)) X(void,ShaderSource,(GLuint,GLsizei,const GLchar* const*,const GLint*)) X(void,CompileShader,(GLuint)) \
 X(void,GetShaderiv,(GLuint,GLenum,GLint*)) X(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) X(void,DeleteShader,(GLuint)) \
 X(GLuint,CreateProgram,(void)) X(void,AttachShader,(GLuint,GLuint)) X(void,BindAttribLocation,(GLuint,GLuint,const GLchar*)) X(void,LinkProgram,(GLuint)) \
 X(void,GetProgramiv,(GLuint,GLenum,GLint*)) X(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) X(void,DeleteProgram,(GLuint)) X(void,UseProgram,(GLuint)) \
 X(GLint,GetUniformLocation,(GLuint,const GLchar*)) X(void,UniformMatrix4fv,(GLint,GLsizei,GLboolean,const GLfloat*)) X(void,Uniform1i,(GLint,GLint)) \
 X(void,EnableVertexAttribArray,(GLuint)) X(void,DisableVertexAttribArray,(GLuint)) X(void,VertexAttribPointer,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*)) \
 X(void,GetVertexAttribiv,(GLuint,GLenum,GLint*)) X(void,GetVertexAttribPointerv,(GLuint,GLenum,void**)) X(void,DrawArrays,(GLenum,GLint,GLsizei)) \
 X(void,BlendFuncSeparate,(GLenum,GLenum,GLenum,GLenum)) X(void,BlendEquationSeparate,(GLenum,GLenum)) \
 X(void,DepthFunc,(GLenum)) X(void,DepthMask,(GLboolean)) X(void,ColorMask,(GLboolean,GLboolean,GLboolean,GLboolean)) X(void,Viewport,(GLint,GLint,GLsizei,GLsizei)) X(void,Scissor,(GLint,GLint,GLsizei,GLsizei))
struct GLFunctions {
#define DECLARE(ret,name,args) ret (APIENTRY *name) args;
GL_REQUIRED(DECLARE)
#undef DECLARE
void(APIENTRY *GenVertexArrays)(GLsizei,GLuint*);void(APIENTRY *BindVertexArray)(GLuint);void(APIENTRY *DeleteVertexArrays)(GLsizei,const GLuint*);
void(APIENTRY *BindSampler)(GLuint,GLuint);void(APIENTRY *PolygonMode)(GLenum,GLenum);
const GLubyte*(APIENTRY *GetStringi)(GLenum,GLuint);
void(APIENTRY *GetFramebufferAttachmentParameteriv)(GLenum,GLenum,GLenum,GLint*);
void(APIENTRY *GenQueries)(GLsizei,GLuint*);void(APIENTRY *DeleteQueries)(GLsizei,const GLuint*);
void(APIENTRY *BeginQuery)(GLenum,GLuint);void(APIENTRY *EndQuery)(GLenum);
void(APIENTRY *GetQueryiv)(GLenum,GLenum,GLint*);void(APIENTRY *GetQueryObjectuiv)(GLuint,GLenum,GLuint*);
void(APIENTRY *DepthRange)(double,double);void(APIENTRY *DepthRangef)(GLfloat,GLfloat);
} gl;
bool loaded=false,initialized=false,isES=false,modern=false,useVAO=false,hasSamplers=false,hasUnpackBuffer=false;
bool hasUnpackRows=false,hasRasterizerDiscard=false;
GLuint program=0,texture=0,vbo=0,vao=0,hitQuery=0;
GLenum queryTarget=0;
bool hasQueryBuffer=false,hasAnyQuery=false,hasConservativeQuery=false,hasModernFramebuffer=false;
GLint matrixUniform=-1,samplerUniform=-1;
const GLenum capabilities[]={GL_BLEND,GL_DEPTH_TEST,GL_CULL_FACE,GL_SCISSOR_TEST,GL_STENCIL_TEST,GL_POLYGON_OFFSET_FILL,GL_SAMPLE_ALPHA_TO_COVERAGE,GL_SAMPLE_COVERAGE};
struct AttrState {GLint enabled,size,type,normalized,stride,buffer;void *pointer;};
/* Every mutated driver state is restored, including VAO-less GLES2 attribs,
 * texture-unit zero's sampler, unpack buffers/strides, and depth range. The
 * engine caches its own state, so merely resetting to defaults is incorrect. */
struct GLState {
    GLint active,tex,prg,array,vertexArray=0,sampler=0,unpack=4,row=0,skipRows=0,skipPixels=0,unpackBuffer=0;
    GLint vp[4],scissor[4],srcRGB,dstRGB,srcAlpha,dstAlpha,eqRGB,eqAlpha,depthFunction,polygon[2]={GL_FILL,GL_FILL};
    GLboolean enabled[8],depthWrite,colorWrite[4],raster=0;GLfloat depthRange[2];AttrState attrib[2];
    GLState(){
        gl.GetIntegerv(GL_ACTIVE_TEXTURE,&active);gl.ActiveTexture(GL_TEXTURE0);gl.GetIntegerv(GL_TEXTURE_BINDING_2D,&tex);
        if(hasSamplers)gl.GetIntegerv(GL_SAMPLER_BINDING,&sampler);
        gl.GetIntegerv(GL_CURRENT_PROGRAM,&prg);gl.GetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
        if(useVAO)gl.GetIntegerv(GL_VERTEX_ARRAY_BINDING,&vertexArray);
        else for(GLuint i=0;i<2;++i){
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&attrib[i].enabled);gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&attrib[i].size);
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&attrib[i].type);gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&attrib[i].normalized);
            gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&attrib[i].stride);gl.GetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&attrib[i].buffer);
            gl.GetVertexAttribPointerv(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&attrib[i].pointer);
        }
        gl.GetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);
        if(hasUnpackRows){gl.GetIntegerv(GL_UNPACK_ROW_LENGTH,&row);gl.GetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);gl.GetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);}
        if(!isES)gl.GetIntegerv(GL_POLYGON_MODE,polygon);
        if(hasUnpackBuffer)gl.GetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);
        gl.GetIntegerv(GL_VIEWPORT,vp);gl.GetIntegerv(GL_SCISSOR_BOX,scissor);gl.GetIntegerv(GL_BLEND_SRC_RGB,&srcRGB);gl.GetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
        gl.GetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);gl.GetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
        gl.GetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB);gl.GetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);gl.GetIntegerv(GL_DEPTH_FUNC,&depthFunction);
        gl.GetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);gl.GetBooleanv(GL_COLOR_WRITEMASK,colorWrite);gl.GetFloatv(GL_DEPTH_RANGE,depthRange);
        for(int i=0;i<8;++i)enabled[i]=gl.IsEnabled(capabilities[i]);
        if(hasRasterizerDiscard)raster=gl.IsEnabled(GL_RASTERIZER_DISCARD);
    }
    ~GLState(){
        gl.UseProgram(prg);
        if(useVAO)gl.BindVertexArray(vertexArray);
        else for(GLuint i=0;i<2;++i){gl.BindBuffer(GL_ARRAY_BUFFER,attrib[i].buffer);gl.VertexAttribPointer(i,attrib[i].size,attrib[i].type,attrib[i].normalized,attrib[i].stride,attrib[i].pointer);if(attrib[i].enabled)gl.EnableVertexAttribArray(i);else gl.DisableVertexAttribArray(i);}
        gl.BindBuffer(GL_ARRAY_BUFFER,array);
        gl.ActiveTexture(GL_TEXTURE0);gl.BindTexture(GL_TEXTURE_2D,tex);if(hasSamplers)gl.BindSampler(0,sampler);gl.ActiveTexture(active);
        gl.PixelStorei(GL_UNPACK_ALIGNMENT,unpack);
        if(hasUnpackRows){gl.PixelStorei(GL_UNPACK_ROW_LENGTH,row);gl.PixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);gl.PixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);}
        if(!isES){
            // Core profiles require FRONT_AND_BACK and always return equal modes.
            // Compatibility profiles can retain different front/back modes.
            if(polygon[0]==polygon[1])gl.PolygonMode(GL_FRONT_AND_BACK,polygon[0]);
            else {gl.PolygonMode(GL_FRONT,polygon[0]);gl.PolygonMode(GL_BACK,polygon[1]);}
        }
        if(hasUnpackBuffer)gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);
        gl.Viewport(vp[0],vp[1],vp[2],vp[3]);gl.Scissor(scissor[0],scissor[1],scissor[2],scissor[3]);gl.BlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha);gl.BlendEquationSeparate(eqRGB,eqAlpha);
        gl.DepthFunc(depthFunction);gl.DepthMask(depthWrite);gl.ColorMask(colorWrite[0],colorWrite[1],colorWrite[2],colorWrite[3]);
        if(isES)gl.DepthRangef(depthRange[0],depthRange[1]);else gl.DepthRange(depthRange[0],depthRange[1]);
        for(int i=0;i<8;++i){if(enabled[i])gl.Enable(capabilities[i]);else gl.Disable(capabilities[i]);}
        if(hasRasterizerDiscard){if(raster)gl.Enable(GL_RASTERIZER_DISCARD);else gl.Disable(GL_RASTERIZER_DISCARD);}
    }
};
bool loadGL(){
#define LOAD(ret,name,args) gl.name=(ret(APIENTRY*)args)SDL_GL_GetProcAddress("gl" #name);if(!gl.name)return false;
GL_REQUIRED(LOAD)
#undef LOAD
    const char *version=(const char*)gl.GetString(GL_VERSION);if(!version)return false;
    isES=std::strstr(version,"OpenGL ES")!=NULL;
    int major=0,minor=0;const char *number=version;while(*number&&(*number<'0'||*number>'9'))++number;
    std::sscanf(number,"%d.%d",&major,&minor);modern=!isES&&major>=3;
    // ES3 exposes both features independently of desktop shader/VAO selection.
    hasUnpackRows=!isES||major>=3;
    hasRasterizerDiscard=major>=3;
    hasUnpackBuffer=(!isES&&(major>2||(major==2&&minor>=1)))||(isES&&major>=3);
    hasSamplers=(!isES&&(major>3||(major==3&&minor>=3)))||(isES&&major>=3);
    gl.GenVertexArrays=(void(APIENTRY*)(GLsizei,GLuint*))SDL_GL_GetProcAddress("glGenVertexArrays");
    gl.BindVertexArray=(void(APIENTRY*)(GLuint))SDL_GL_GetProcAddress("glBindVertexArray");
    gl.DeleteVertexArrays=(void(APIENTRY*)(GLsizei,const GLuint*))SDL_GL_GetProcAddress("glDeleteVertexArrays");
    useVAO=modern&&gl.GenVertexArrays&&gl.BindVertexArray&&gl.DeleteVertexArrays;
    gl.BindSampler=(void(APIENTRY*)(GLuint,GLuint))SDL_GL_GetProcAddress("glBindSampler");hasSamplers=hasSamplers&&gl.BindSampler;
    gl.PolygonMode=(void(APIENTRY*)(GLenum,GLenum))SDL_GL_GetProcAddress("glPolygonMode");
    gl.DepthRange=(void(APIENTRY*)(double,double))SDL_GL_GetProcAddress("glDepthRange");
    gl.DepthRangef=(void(APIENTRY*)(GLfloat,GLfloat))SDL_GL_GetProcAddress("glDepthRangef");
    gl.GetStringi=(const GLubyte*(APIENTRY*)(GLenum,GLuint))SDL_GL_GetProcAddress("glGetStringi");
    auto extension=[&](const char *name){
        if(major>=3){
            if(!gl.GetStringi)return false;
            GLint count=0;gl.GetIntegerv(GL_NUM_EXTENSIONS,&count);
            for(GLint i=0;i<count;++i){const char *entry=(const char*)gl.GetStringi(GL_EXTENSIONS,(GLuint)i);if(entry&&!std::strcmp(entry,name))return true;}
            return false;
        }
        const char *entries=(const char*)gl.GetString(GL_EXTENSIONS);if(!entries)return false;
        const size_t length=std::strlen(name);const char *match=entries;
        while((match=std::strstr(match,name))!=NULL){if((match==entries||match[-1]==' ')&&(match[length]==' '||!match[length]))return true;match+=length;}
        return false;
    };
    hasModernFramebuffer=major>=3;
    gl.GetFramebufferAttachmentParameteriv=(void(APIENTRY*)(GLenum,GLenum,GLenum,GLint*))SDL_GL_GetProcAddress("glGetFramebufferAttachmentParameteriv");
    hasQueryBuffer=!isES&&(major>4||(major==4&&minor>=4)||extension("GL_ARB_query_buffer_object"));
    hasAnyQuery=(isES&&major>=3)||(!isES&&(major>3||(major==3&&minor>=3)||extension("GL_ARB_occlusion_query2")));
    hasConservativeQuery=(isES&&(major>=3||extension("GL_EXT_occlusion_query_boolean")))||(!isES&&(major>4||(major==4&&minor>=3)||extension("GL_ARB_ES3_compatibility")));
    queryTarget=0;
    const char *suffix="";
    if(!isES)queryTarget=GL_SAMPLES_PASSED;
    else if(major>=3)queryTarget=GL_ANY_SAMPLES_PASSED;
    else {
        if(extension("GL_EXT_occlusion_query_boolean")){queryTarget=GL_ANY_SAMPLES_PASSED;suffix="EXT";}
    }
    if(queryTarget){
#define QUERY_LOAD(name,type) gl.name=(type)SDL_GL_GetProcAddress((std::string("gl" #name)+suffix).c_str());
        QUERY_LOAD(GenQueries,void(APIENTRY*)(GLsizei,GLuint*))
        QUERY_LOAD(DeleteQueries,void(APIENTRY*)(GLsizei,const GLuint*))
        QUERY_LOAD(BeginQuery,void(APIENTRY*)(GLenum,GLuint))
        QUERY_LOAD(EndQuery,void(APIENTRY*)(GLenum))
        QUERY_LOAD(GetQueryiv,void(APIENTRY*)(GLenum,GLenum,GLint*))
        QUERY_LOAD(GetQueryObjectuiv,void(APIENTRY*)(GLuint,GLenum,GLuint*))
#undef QUERY_LOAD
        if(!gl.GenQueries||!gl.DeleteQueries||!gl.BeginQuery||!gl.EndQuery||!gl.GetQueryiv||!gl.GetQueryObjectuiv)queryTarget=0;
    }
    return (isES?gl.DepthRangef!=NULL:(gl.DepthRange!=NULL&&gl.PolygonMode!=NULL));
}
GLuint shader(GLenum type,const char *source){
    GLuint result=gl.CreateShader(type);gl.ShaderSource(result,1,&source,NULL);gl.CompileShader(result);GLint ok=0;gl.GetShaderiv(result,GL_COMPILE_STATUS,&ok);
    if(!ok){char error[512]={0};gl.GetShaderInfoLog(result,sizeof error-1,NULL,error);std::fprintf(stderr,"Character wheel shader: %s\n",error);gl.DeleteShader(result);return 0;}return result;
}
}
extern "C" int wheel_overlay_init(void){
    if(initialized)return 1;
    if(!SDL_GL_GetCurrentContext())return 0;
    loaded=loadGL();if(!loaded){std::fprintf(stderr,"Character wheel: required OpenGL functions unavailable\n");return 0;}
    GLState saved;
    const char *vsModern="#version 330 core\nin vec3 aPosition;in vec2 aUV;uniform mat4 uMVP;out vec2 vUV;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vUV=aUV;}";
    const char *fsModern="#version 330 core\nin vec2 vUV;uniform sampler2D uTexture;out vec4 color;void main(){color=texture(uTexture,vUV);}";
    const char *vsOld="#version 120\nattribute vec3 aPosition;attribute vec2 aUV;uniform mat4 uMVP;varying vec2 vUV;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vUV=aUV;}";
    const char *fsOld="#version 120\nvarying vec2 vUV;uniform sampler2D uTexture;void main(){gl_FragColor=texture2D(uTexture,vUV);}";
    const char *vsES="#version 100\nattribute vec3 aPosition;attribute vec2 aUV;uniform mat4 uMVP;varying vec2 vUV;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vUV=aUV;}";
    const char *fsES="#version 100\nprecision mediump float;varying vec2 vUV;uniform sampler2D uTexture;void main(){gl_FragColor=texture2D(uTexture,vUV);}";
    GLuint vs=shader(GL_VERTEX_SHADER,isES?vsES:(modern?vsModern:vsOld));
    GLuint fs=shader(GL_FRAGMENT_SHADER,isES?fsES:(modern?fsModern:fsOld));
    if(!vs||!fs){if(vs)gl.DeleteShader(vs);if(fs)gl.DeleteShader(fs);return 0;}
    program=gl.CreateProgram();gl.AttachShader(program,vs);gl.AttachShader(program,fs);gl.BindAttribLocation(program,0,"aPosition");gl.BindAttribLocation(program,1,"aUV");gl.LinkProgram(program);gl.DeleteShader(vs);gl.DeleteShader(fs);
    GLint ok=0;gl.GetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){char error[512]={0};gl.GetProgramInfoLog(program,sizeof error-1,NULL,error);std::fprintf(stderr,"Character wheel program: %s\n",error);gl.DeleteProgram(program);program=0;return 0;}
    matrixUniform=gl.GetUniformLocation(program,"uMVP");samplerUniform=gl.GetUniformLocation(program,"uTexture");
    gl.GenBuffers(1,&vbo);if(useVAO)gl.GenVertexArrays(1,&vao);
    gl.GenTextures(1,&texture);gl.ActiveTexture(GL_TEXTURE0);gl.BindTexture(GL_TEXTURE_2D,texture);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);gl.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(hasUnpackBuffer)gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(hasUnpackRows){gl.PixelStorei(GL_UNPACK_ROW_LENGTH,0);gl.PixelStorei(GL_UNPACK_SKIP_ROWS,0);gl.PixelStorei(GL_UNPACK_SKIP_PIXELS,0);}
    gl.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA,kWidth,kHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    initialized=true;return 1;
}
extern "C" int wheel_overlay_render(const unsigned char *rgba,int w,int h,int fbw,int fbh){
    if(!rgba||w!=kWidth||h!=kHeight||fbw<=0||fbh<=0||!SDL_GL_GetCurrentContext())return 0;
    if(!initialized&&!wheel_overlay_init())return 0;
    GLState saved;
    gl.ActiveTexture(GL_TEXTURE0);gl.BindTexture(GL_TEXTURE_2D,texture);if(hasSamplers)gl.BindSampler(0,0);
    if(hasUnpackBuffer)gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(hasUnpackRows){gl.PixelStorei(GL_UNPACK_ROW_LENGTH,0);gl.PixelStorei(GL_UNPACK_SKIP_ROWS,0);gl.PixelStorei(GL_UNPACK_SKIP_PIXELS,0);}
    gl.TexSubImage2D(GL_TEXTURE_2D,0,0,0,kWidth,kHeight,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    const float vertices[]={-1,1,0,0,0, -1,-1,0,0,1, 1,-1,0,1,1,
                            -1,1,0,0,0, 1,-1,0,1,1, 1,1,0,1,0};
    const float identity[]={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    gl.UseProgram(program);gl.UniformMatrix4fv(matrixUniform,1,GL_FALSE,identity);gl.Uniform1i(samplerUniform,0);
    if(useVAO)gl.BindVertexArray(vao);
    gl.BindBuffer(GL_ARRAY_BUFFER,vbo);gl.BufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STREAM_DRAW);
    gl.EnableVertexAttribArray(0);gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    gl.VertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    float scale=std::min(fbw/(float)kWidth,fbh/(float)kHeight);
    int dw=(int)(kWidth*scale),dh=(int)(kHeight*scale);
    gl.Viewport((fbw-dw)/2,(fbh-dh)/2,dw,dh);gl.Disable(GL_DEPTH_TEST);gl.DepthMask(GL_FALSE);
    gl.Enable(GL_BLEND);gl.BlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);
    gl.BlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    gl.ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    gl.Disable(GL_CULL_FACE);gl.Disable(GL_SCISSOR_TEST);gl.Disable(GL_STENCIL_TEST);gl.Disable(GL_POLYGON_OFFSET_FILL);
    gl.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);gl.Disable(GL_SAMPLE_COVERAGE);
    if(hasRasterizerDiscard)gl.Disable(GL_RASTERIZER_DISCARD);
    if(!isES)gl.PolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    gl.DrawArrays(GL_TRIANGLES,0,6);return 1;
}
extern "C" void wheel_overlay_shutdown(void) {
    if(initialized&&SDL_GL_GetCurrentContext()) {
        gl.DeleteTextures(1,&texture); gl.DeleteBuffers(1,&vbo);
        if(useVAO)gl.DeleteVertexArrays(1,&vao); gl.DeleteProgram(program);
    }
    initialized=false; program=texture=vbo=vao=0;
}
