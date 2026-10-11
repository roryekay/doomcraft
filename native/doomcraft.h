// DoomCraft local prototype. Custom code: GPL-3.0-or-later when linked with GZDoom.
#pragma once
#include <cstdint>
struct FRenderViewpoint;
class AActor;
struct event_t;
void DoomCraftTick();
void DoomCraftCamera(const FRenderViewpoint&, float fov, float ratio, float fovRatio);
bool DoomCraftActive();
bool DoomCraftMovement();
bool DoomCraftScreen();
bool DoomCraftEvent(event_t*);
bool DoomCraftNativeKey(int,bool,bool);
bool DoomCraftRespawnRequested();
int DoomCraftArmorDamage(AActor*,int);
int DoomCraftTotemDamage(AActor*,int,bool);
struct DoomCraftView { double x=0,y=0,z=0; float yaw=0,pitch=0,fov=70; };
DoomCraftView DoomCraftCurrentView();
void DoomCraftSyncView(FRenderViewpoint&);
bool DoomCraftBossTick(AActor*);
void DoomCraftBossRequest(const char*,const char*,int,int);
namespace OpenGLRenderer {
class FGLRenderBuffers;
void DoomCraftComposite(FGLRenderBuffers* buffers);
bool DoomCraftPrepareView(DoomCraftView&);
uint64_t DoomCraftShownFrame();
}
