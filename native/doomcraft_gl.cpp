// Direct OpenGL compositor for the existing Minecraft frame protocol. GPL-3.0-or-later.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>
#include <cmath>
#include <array>
#include "doomcraft.h"
#include "gl_system.h"
#include "gl_renderbuffers.h"
#include "gl_postprocessstate.h"
#include "v_video.h"
#include "c_console.h"
#include "menustate.h"
#include "printf.h"

namespace OpenGLRenderer {
static HANDLE mapping=nullptr;
static const uint8_t* memory=nullptr;
static constexpr uint64_t stride=uint64_t(3840)*2160*12;
static constexpr uint64_t total=4096+3*stride;
static uint64_t counter=0;
static bool prepared=false;
static int pid=0,w=0,h=0,flags=0;
static float nearMC=.05f,farMC=1024;
static double capturedPosition[3]={};
static float capturedYaw=0,capturedPitch=0,capturedFov=70;
static GLuint program=0,tex[3]={},vao=0;
static bool reportedFrame=false;
static int textureW=0,textureH=0;
static std::vector<uint8_t> snapshot;
static std::chrono::steady_clock::time_point received;
template<class T> static T read(size_t at) { T v; std::memcpy(&v,memory+at,sizeof v); return v; }
static bool acquire() {
    if(!mapping) {
        mapping=OpenFileMappingW(FILE_MAP_READ,FALSE,L"Local\\DoomCraftFrame");
        if(!mapping) return false;
        memory=(const uint8_t*)MapViewOfFile(mapping,FILE_MAP_READ,0,0,(SIZE_T)total);
        if(!memory) { CloseHandle(mapping);mapping=nullptr;return false; }
    }
    if(read<uint32_t>(0)!=0x5450434D || read<int>(4)!=1 || read<int>(8)!=4096 || read<int>(12)!=3 || read<uint64_t>(16)!=stride) return false;
    int currentPid=read<int>(44);
    if(pid!=currentPid) {pid=currentPid;counter=0;}
    uint64_t next=read<uint64_t>(32);
    if(next==counter) return w>0 && std::chrono::steady_clock::now()-received<std::chrono::seconds(2);
    int slot=read<int>(40); if(slot<0||slot>=3) return false;
    size_t desc=256+slot*128;
    uint64_t seq=read<uint64_t>(desc);
    if(seq&1) return w>0 && std::chrono::steady_clock::now()-received<std::chrono::seconds(2);
    int nw=read<int>(desc+24),nh=read<int>(desc+28);
    int newFlags=read<int>(desc+44); float newNear=read<float>(desc+32),newFar=read<float>(desc+36);
    double newPosition[3]={read<double>(desc+48),read<double>(desc+56),read<double>(desc+64)};
    float newYaw=read<float>(desc+72),newPitch=read<float>(desc+76),newFov=read<float>(desc+40);
    if(nw<=0||nw>3840||nh<=0||nh>2160) return false;
    size_t n=size_t(nw)*nh*4;
    snapshot.resize(n*3);
    std::memcpy(snapshot.data(),memory+4096+slot*stride,n*3);
    MemoryBarrier();
    // A newer publication does not invalidate this coherent slot. Keep the previous texture on a rare write race.
    if(seq!=read<uint64_t>(desc)) return w>0 && std::chrono::steady_clock::now()-received<std::chrono::seconds(2);
    w=nw;h=nh; flags=newFlags; nearMC=newNear;farMC=newFar;
    std::memcpy(capturedPosition,newPosition,sizeof newPosition);capturedYaw=newYaw;capturedPitch=newPitch;capturedFov=newFov;
    counter=next;received=std::chrono::steady_clock::now();
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    bool resize=textureW!=w || textureH!=h;
    for(int i=0;i<3;i++) {
        glActiveTexture(GL_TEXTURE0+i+2);glBindTexture(GL_TEXTURE_2D,tex[i]);
        if(resize)glTexImage2D(GL_TEXTURE_2D,0,i==1?GL_R32F:GL_RGBA8,w,h,0,i==1?GL_RED:GL_RGBA,i==1?GL_FLOAT:GL_UNSIGNED_BYTE,snapshot.data()+i*n);
        else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,i==1?GL_RED:GL_RGBA,i==1?GL_FLOAT:GL_UNSIGNED_BYTE,snapshot.data()+i*n);
    }
    textureW=w;textureH=h;
    if(!reportedFrame) {Printf("DoomCraft: acquired Minecraft frame %dx%d, flags %d\n",w,h,flags);}
    return true;
}
static GLuint shader(GLenum kind,const char* source) {
    GLuint s=glCreateShader(kind);glShaderSource(s,1,&source,nullptr);glCompileShader(s);
    GLint okay=0;glGetShaderiv(s,GL_COMPILE_STATUS,&okay);
    if(!okay) { char log[4096]={};glGetShaderInfoLog(s,sizeof log,nullptr,log);Printf("DoomCraft shader error: %s\n",log);glDeleteShader(s);return 0; }
    return s;
}
static bool init() {
    const char* vertex=R"(#version 330 core
out vec2 uv;
void main(){ vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2); uv=p; gl_Position=vec4(p*2.0-1.0,0,1); }
)";
    const char* fragment=R"(#version 330 core
in vec2 uv; out vec4 color;
uniform sampler2D worldColor,worldDepth,overlayColor,doomDepth;
uniform vec2 mcClip,doomClip,depthScale;
uniform mat3 cameraRotation;
uniform vec3 cameraTranslation;
uniform vec2 oldScale,newScale;
uniform int frameFlags;
float distanceAt(float d,vec2 clip){return clip.x*clip.y/(clip.y-d*(clip.y-clip.x));}
vec2 textureUV(vec2 p){if((frameFlags&2)==0)p.y=1.0-p.y;return p;}
float mcDistance(vec2 p){
 float d=texture(worldDepth,textureUV(p)).r;
 if((frameFlags&4)!=0)d=1.0-d;
 if((frameFlags&1)==0)d=d*0.5+0.5;
 return distanceAt(d,mcClip);
}
bool sampleAt(vec3 p,out vec2 q,out float gap){
 if(p.z<=0.001)return false;
 q=p.xy/p.z/oldScale*0.5+0.5;
 if(any(lessThan(q,vec2(0)))||any(greaterThan(q,vec2(1))))return false;
 float d=mcDistance(q); gap=p.z-d;
 return texture(worldColor,textureUV(q)).a>0.001 && d<mcClip.y*0.999;
}
// Adapted from the reviewed RDR bridge's depth fixed point and crossing test. The hand/HUD stays in screen space.
bool reproject(vec3 ray,float limit,out vec2 q,out float z){
 vec3 r=cameraRotation*ray,T=cameraTranslation;
 q=vec2(0);z=1e9;
 if(r.z>0.001){
  vec2 seed=r.xy/r.z/oldScale*0.5+0.5;
  if(all(greaterThanEqual(seed,vec2(0)))&&all(lessThanEqual(seed,vec2(1)))){
   float depth=mcDistance(seed);float t=(depth-T.z)/r.z;
   for(int i=0;i<7;i++){
    if(t<0.02||t>limit)break;
    float gap;if(!sampleAt(r*t+T,q,gap))break;
    if(abs(gap)<0.012+depth*0.0005){z=t;return true;}
    depth=mcDistance(q);t=(depth-T.z)/r.z;
   }
  }
 }
 if(dot(T,T)<1e-10)return false;
 float previous=0.02;
 for(int i=0;i<24;i++){
  float t=0.02*pow(max(limit,0.021)/0.02,float(i)/23.0),gap;
  if(!sampleAt(r*t+T,q,gap)||gap<0.0){previous=t;continue;}
  float lo=previous,hi=t;
  for(int j=0;j<7;j++){
   float mid=(lo+hi)*0.5,g;vec2 at;
   if(sampleAt(r*mid+T,at,g)&&g>=0.0)hi=mid;else lo=mid;
  }
  if(sampleAt(r*hi+T,q,gap)&&abs(gap)<0.03){z=hi;return true;}
  previous=t;
 }
 return false;
}
void main(){
 vec4 overlay=texture(overlayColor,textureUV(uv)),world=vec4(0);
 float host=distanceAt(texture(doomDepth,uv*depthScale).r,doomClip);
 vec2 q;float depth;
 if(reproject(vec3((uv*2.0-1.0)*newScale,1.0),min(host+0.03,400.0),q,depth) && depth<=host+0.025)world=texture(worldColor,textureUV(q));
 color=overlay+world*(1.0-overlay.a);
}
)";
    GLuint v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,fragment);
    if(!v||!f)return false;
    program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glLinkProgram(program);
    glDeleteShader(v);glDeleteShader(f);GLint okay=0;glGetProgramiv(program,GL_LINK_STATUS,&okay);if(!okay)return false;
    glGenVertexArrays(1,&vao);glGenTextures(3,tex);
    for(int i=0;i<3;i++) {glActiveTexture(GL_TEXTURE0+i+2);glBindTexture(GL_TEXTURE_2D,tex[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    Printf("DoomCraft: OpenGL compositor initialized\n");return true;
}
bool DoomCraftPrepareView(DoomCraftView& view) {
    prepared=false;
    if(!DoomCraftActive() || menuactive!=MENU_Off)return false;
    FGLPostProcessState saved;saved.SaveTextureBindings(6);
    GLint oldVao=0,oldUnpack=0;glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&oldUnpack);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    if(!program && !init()){glBindBuffer(GL_PIXEL_UNPACK_BUFFER,oldUnpack);return false;}
    bool okay=acquire();
    glBindVertexArray(oldVao);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,oldUnpack);
    if(!okay)return false;
    view={capturedPosition[0],capturedPosition[1],capturedPosition[2],capturedYaw,capturedPitch,capturedFov};
    auto desired=DoomCraftCurrentView();
    // Never show the previous map or a pre-teleport image while the guest streams its new region.
    if(std::hypot(view.x-desired.x,view.z-desired.z)>16 || std::abs(view.y-desired.y)>16)return false;
    prepared=true;return true;
}
uint64_t DoomCraftShownFrame(){return prepared?counter:0;}
using Axis=std::array<double,3>;
static double dot(const Axis& a,const Axis& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static std::array<Axis,3> basis(double yaw,double pitch){
    double y=yaw*3.141592653589793/180,p=pitch*3.141592653589793/180;
    return {Axis{-std::cos(y),0,-std::sin(y)},Axis{-std::sin(y)*std::sin(p),std::cos(p),std::cos(y)*std::sin(p)},Axis{-std::sin(y)*std::cos(p),-std::sin(p),std::cos(y)*std::cos(p)}};
}
void DoomCraftComposite(FGLRenderBuffers* buffers) {
    if(!DoomCraftActive() || menuactive!=MENU_Off)return;
    if(!reportedFrame)while(glGetError()!=GL_NO_ERROR){}
    FGLPostProcessState saved; saved.SaveTextureBindings(6);
    if(!program && !init())return;
    GLint oldVao=0,oldUnpack=0; glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&oldUnpack);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
    bool cull=glIsEnabled(GL_CULL_FACE),stencil=glIsEnabled(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
    if(!reportedFrame)Printf("DoomCraft: GL setup status 0x%x\n",glGetError());
    if(prepared) {
        if(!reportedFrame)Printf("DoomCraft: GL upload status 0x%x\n",glGetError());
        buffers->BindSceneDepthTexture(5);
        for(int i=0;i<3;i++){glActiveTexture(GL_TEXTURE0+i+2);glBindTexture(GL_TEXTURE_2D,tex[i]);}
        glUseProgram(program);
        glUniform1i(glGetUniformLocation(program,"worldColor"),2);glUniform1i(glGetUniformLocation(program,"worldDepth"),3);
        glUniform1i(glGetUniformLocation(program,"overlayColor"),4);glUniform1i(glGetUniformLocation(program,"doomDepth"),5);
        glUniform2f(glGetUniformLocation(program,"mcClip"),nearMC,farMC);
        glUniform2f(glGetUniformLocation(program,"doomClip"),screen->GetZNear()/32,screen->GetZFar()/32);
        glUniform2f(glGetUniformLocation(program,"depthScale"),screen->mSceneViewport.width/(float)buffers->GetWidth(),screen->mSceneViewport.height/(float)buffers->GetHeight());
        glUniform1i(glGetUniformLocation(program,"frameFlags"),flags);
        DoomCraftView current{capturedPosition[0],capturedPosition[1],capturedPosition[2],capturedYaw,capturedPitch,capturedFov};auto oldBasis=basis(capturedYaw,capturedPitch),newBasis=basis(current.yaw,current.pitch);
        GLfloat rotation[9];for(int col=0;col<3;col++)for(int row=0;row<3;row++)rotation[col*3+row]=(float)dot(oldBasis[row],newBasis[col]);
        Axis delta{current.x-capturedPosition[0],current.y-capturedPosition[1],current.z-capturedPosition[2]};
        glUniformMatrix3fv(glGetUniformLocation(program,"cameraRotation"),1,GL_FALSE,rotation);
        glUniform3f(glGetUniformLocation(program,"cameraTranslation"),(float)dot(oldBasis[0],delta),(float)dot(oldBasis[1],delta),(float)dot(oldBasis[2],delta));
        float oldTan=(float)std::tan(capturedFov*3.141592653589793/360),newTan=(float)std::tan(current.fov*3.141592653589793/360);
        glUniform2f(glGetUniformLocation(program,"oldScale"),oldTan*w/h,oldTan);
        glUniform2f(glGetUniformLocation(program,"newScale"),newTan*screen->mSceneViewport.width/screen->mSceneViewport.height,newTan);
        if(!reportedFrame)Printf("DoomCraft: GL uniforms status 0x%x\n",glGetError());
        glEnable(GL_BLEND);glBlendEquation(GL_FUNC_ADD);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);
        if(!reportedFrame){Printf("DoomCraft: first composite GL status 0x%x\n",glGetError());reportedFrame=true;}
    }
    if(cull)glEnable(GL_CULL_FACE);if(stencil)glEnable(GL_STENCIL_TEST);
    glBindVertexArray(oldVao);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,oldUnpack);
}
}
