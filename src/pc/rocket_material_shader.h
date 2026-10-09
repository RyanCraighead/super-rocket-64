#pragma once
#include <string>
#include <utility>
/* Shared with the offscreen renderer test. Owned diffuse/UV data uses our
 * compact host lighting. This does not reproduce the Rocket League shader. */
inline std::pair<std::string,std::string> rocket_material_shader(bool es,bool modern) {
    const std::string prefix=es?"#version 100\nprecision mediump float;\n":modern?"#version 130\n":"#version 120\n";
    std::string vs=prefix+(modern?"in vec3 aPosition;in vec4 aUV;in vec4 aColor;out vec4 vColor;out vec4 vUV;":"attribute vec3 aPosition;attribute vec4 aUV;attribute vec4 aColor;varying vec4 vColor;varying vec4 vUV;");
    vs+="uniform mat4 uMVP;void main(){gl_Position=uMVP*vec4(aPosition,1.0);vColor=aColor;vUV=aUV;}";
    std::string fs=prefix+(modern?"in vec4 vColor;in vec4 vUV;out vec4 outputColor;":"varying vec4 vColor;varying vec4 vUV;");
    const char *sample=modern?"texture":"texture2D";
    fs+="uniform vec4 uCaps;uniform bool uMetal;uniform int uMaterial;uniform sampler2D uMetalTexture;void main(){if(uCaps.x>0.5 && mod(floor(gl_FragCoord.x)+floor(gl_FragCoord.y),2.0)>0.5)discard;vec4 color=vColor;if(uMetal){vec2 uv=clamp(vUV.zw,0.0,1.0);uv=vec2((uv.x*63.0+0.5)/64.0,(uv.y*31.0+0.5)/64.0);vec3 shade=";
    fs+=sample;fs+="(uMetalTexture,uv).rgb;vec3 light=";fs+=sample;
    fs+="(uMetalTexture,uv+vec2(0.0,0.5)).rgb;color.rgb=clamp(shade*0.5+light,0.0,1.0);}";
    fs+="else if(uMaterial>0){vec3 diffuse=";fs+=sample;
    fs+="(uMetalTexture,vUV.xy).rgb;float paint=smoothstep(0.5,0.9,diffuse.r);vec3 base=diffuse;float reflection=0.045;if(uMaterial==2){base=mix(diffuse*0.65,vec3(0.025,0.36,0.95),paint);reflection=mix(0.30,0.08,paint);}vec2 n=vUV.zw*2.0-1.0;float rim=pow(1.0-sqrt(max(0.0,1.0-dot(n,n))),5.0);vec3 sky=mix(vec3(0.025,0.03,0.04),vec3(0.45,0.60,0.80),smoothstep(0.38,0.70,vUV.z));float glint=pow(max(0.0,1.0-length(vUV.zw-vec2(0.62,0.32))*2.8),5.0);color.rgb=clamp(base*vColor.rgb+sky*reflection*(0.35+rim)+vec3(glint)*reflection,0.0,1.0);color.a=1.0;}";
    // Soft crossed ribbons, with a hot center and orange taper. No texture assets
    // or UE3 particle code; bounded simulation phase freezes with the car pose.
    fs+="if(uMaterial<0){float x=vUV.x;float ripple=0.88+0.12*sin(x*32.0-vUV.z*18.8495559+vUV.w);float width=(0.38+0.62*sin(x*3.14159265))*pow(1.0-x,0.4)*ripple;float radial=abs(vUV.y)/max(width,0.001);float glow=1.0-smoothstep(0.15,1.0,radial);float core=(1.0-smoothstep(0.0,0.42,radial))*(1.0-x);color.rgb=mix(vColor.rgb,vec3(1.0,0.88,0.5),core);color.a=glow*(1.0-smoothstep(0.58,1.0,x))*0.42;if(color.a<0.003)discard;}";
    fs+=(modern?"outputColor=color;}":"gl_FragColor=color;}");
    return {vs,fs};
}
