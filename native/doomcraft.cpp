// DoomCraft adapter for GZDoom 4.14.2. GPL-3.0-or-later.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <map>
#include <tuple>
#include <set>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "doomcraft.h"
#include "ws.h"
#include "actor.h"
#include "d_player.h"
#include "doomstat.h"
#include "g_levellocals.h"
#include "g_level.h"
#include "p_local.h"
#include "p_trace.h"
#include "m_random.h"
#include "r_utility.h"
#include "v_video.h"
#include "menustate.h"
#include "d_event.h"
#include "keydef.h"
// Keep the bridge parser's templates separate from the engine serializer's differently configured RapidJSON.
#define RAPIDJSON_NAMESPACE doomcraft_json
#define RAPIDJSON_48BITPOINTER_OPTIMIZATION 0
#define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#define RAPIDJSON_HAS_CXX11_RANGE_FOR 1
#define RAPIDJSON_PARSE_DEFAULT_FLAGS kParseFullPrecisionFlag
#include "rapidjson/document.h"
#include "c_cvars.h"
#include "c_dispatch.h"
extern uint8_t globalfreeze, globalchangefreeze;

CVAR(Bool, doomcraft_enabled, true, CVAR_ARCHIVE)
CVAR(Bool, doomcraft_debug, false, CVAR_ARCHIVE)
static DoomCraftView currentView,displayedView;
DoomCraftView DoomCraftCurrentView(){return currentView;}
static WsClient link;
static bool started=false;
static unsigned long long frame=0;
static int generation=-1, tick=0;
static std::string mapName;
static std::set<std::tuple<int,int,int>> blocks;
static bool heldAttack=false, heldUse=false;
static bool previous[256]={};
static bool nativeHeld[256]={};
static constexpr double scale=32.0;
static double mapOffset=0;
static FLevelLocals* observedLevel=nullptr;
static int observedTime=-1;
struct FireworkTrack {DVector3 position;bool done=false;};
static std::map<int,FireworkTrack> fireworks;
static unsigned explosions=0;
static unsigned positionSerial=0;
static bool creative=false, spectator=false;
static int minecraftArmor=0;
static bool minecraftTotem=false,reportedTotem=false,totemAwaitEmpty=false;
static unsigned totemSerial=GetTickCount()&0x3fffffff,totemPending=0;
static int totemDeadline=0,totemGrace=0;
static unsigned absorptionSession=(GetTickCount()&0x3fffffff)+1;
static unsigned long long absorptionSpent=0;
static int minecraftAbsorption=0;
static DVector3 acceptedPosition;
static bool havePosition=false, heldSwap=false;
static int movementInput=0;
static double minecraftEye=1.62;
static int testInput=0,testInputTicks=0;
static int screenKind=0;
static bool restartRequested=false,wasDead=false,restartWasDown=false;
static std::map<std::string,int> inputBindings={{"forward",'W'},{"back",'S'},{"left",'A'},{"right",'D'},{"jump",VK_SPACE},{"sneak",VK_LSHIFT},{"sprint",VK_LCONTROL},{"attack",VK_LBUTTON},{"use",VK_RBUTTON},{"swap",'F'},{"inventory",'E'},{"drop",'Q'},{"pick",VK_MBUTTON},{"chat",'T'},{"command",VK_OEM_2}};
static std::map<std::string,bool> actionHeld;
static FRandom fireworkDamage("DoomCraftFirework");
static void trace(const std::string& message) { if(doomcraft_debug)if(auto f=std::fopen("doomcraft-debug.log","a")){std::fputs(message.c_str(),f);std::fputs("\n",f);std::fclose(f);} }
bool DoomCraftActive() { return doomcraft_enabled && gamestate==GS_LEVEL && players[consoleplayer].mo; }
bool DoomCraftMovement() { return DoomCraftActive() && link.connected(); }
static void send(const std::string& s) { link.send(s); }
static void reportAbsorptionDamage(){send("{\"t\":\"absorb_damage\",\"session\":"+std::to_string(absorptionSession)+",\"spent\":"+std::to_string(absorptionSpent)+"}");}
static void key(const char*,bool,bool&);
static bool focused() { DWORD pid=0; GetWindowThreadProcessId(GetForegroundWindow(),&pid); return pid==GetCurrentProcessId(); }
static bool down(int k) { return focused() && (GetAsyncKeyState(k)&0x8000); }
static bool press(int k) { bool d=down(k), edge=d&&!previous[k]; previous[k]=d; return edge; }
static bool actionDown(const std::string& name){auto i=inputBindings.find(name);if(i==inputBindings.end()&&name.size()==7&&name.rfind("hotbar",0)==0)return down('1'+name.back()-'0');return i!=inputBindings.end() && i->second>0 && down(i->second);}
static bool restartKey(int vk){
    if(vk==VK_RETURN||vk=='G')return true;
    for(const char* action:{"attack","use","jump"}){auto i=inputBindings.find(action);if(i!=inputBindings.end()&&i->second==vk)return true;}
    return false;
}
static void requestRestart(){
    restartRequested=true;
    send("{\"t\":\"key\",\"k\":\"escape\",\"down\":true}");
}
bool DoomCraftRespawnRequested(){return DoomCraftMovement()&&restartRequested&&players[consoleplayer].playerstate==PST_DEAD;}
int DoomCraftArmorDamage(AActor* target,int damage){
    if(!DoomCraftMovement() || target!=players[consoleplayer].mo || damage<=0)return damage;
    // Bridge balance: each worn Minecraft armor point absorbs 4%, capped at 80%.
    // Apply before native health subtraction, so lethal attacks are protected too.
    return std::max(1,(damage*(25-minecraftArmor)+24)/25);
}
int DoomCraftTotemDamage(AActor* target,int damage,bool bypass){
    if(bypass || !DoomCraftMovement() || target!=players[consoleplayer].mo || target->health<=0 || damage<=0)return damage;
    if(totemPending || tick<totemGrace)return 0;
    int absorbed=std::min(damage,minecraftAbsorption);
    if(absorbed>0){minecraftAbsorption-=absorbed;damage-=absorbed;absorptionSpent+=absorbed;reportAbsorptionDamage();}
    if(damage<players[consoleplayer].health || !minecraftTotem)return damage;
    minecraftTotem=false;totemPending=++totemSerial;totemDeadline=tick+35;
    send("{\"t\":\"totem\",\"id\":"+std::to_string(totemPending)+"}");
    Printf(PRINT_LOG,"DoomCraft: reserved held totem %u before lethal damage\n",totemPending);
    // Reserve the rescue while Minecraft's server consumes the actual held item.
    if(players[consoleplayer].health<5)players[consoleplayer].health=target->health=5;
    return std::max(0,players[consoleplayer].health-5);
}
static int bindingKey(std::string s){
    if(s=="key.mouse.left")return VK_LBUTTON;if(s=="key.mouse.right")return VK_RBUTTON;if(s=="key.mouse.middle")return VK_MBUTTON;if(s=="key.mouse.4")return VK_XBUTTON1;if(s=="key.mouse.5")return VK_XBUTTON2;
    const std::string prefix="key.keyboard.";if(s.rfind(prefix,0)!=0)return 0;s=s.substr(prefix.size());
    if(s.size()==1){char c=s[0];if(c>='a'&&c<='z')return c-'a'+'A';if(c>='0'&&c<='9')return c;}
    if(s.size()>1&&s[0]=='f'){int n=std::atoi(s.c_str()+1);if(n>=1&&n<=24)return VK_F1+n-1;}
    static const std::map<std::string,int> special={{"space",VK_SPACE},{"left.shift",VK_LSHIFT},{"right.shift",VK_RSHIFT},{"left.control",VK_LCONTROL},{"right.control",VK_RCONTROL},{"left.alt",VK_LMENU},{"right.alt",VK_RMENU},{"tab",VK_TAB},{"enter",VK_RETURN},{"escape",VK_ESCAPE},{"backspace",VK_BACK},{"delete",VK_DELETE},{"up",VK_UP},{"down",VK_DOWN},{"left",VK_LEFT},{"right",VK_RIGHT},{"slash",VK_OEM_2},{"backslash",VK_OEM_5},{"semicolon",VK_OEM_1},{"apostrophe",VK_OEM_7},{"comma",VK_OEM_COMMA},{"period",VK_OEM_PERIOD},{"minus",VK_OEM_MINUS},{"equal",VK_OEM_PLUS},{"left.bracket",VK_OEM_4},{"right.bracket",VK_OEM_6}};
    auto i=special.find(s);return i==special.end()?0:i->second;
}
static int guiScan(int k){
    if(k>='a'&&k<='z')k=k-'a'+'A';if(k>='A'&&k<='Z')return k-'A'+4;if(k>='1'&&k<='9')return k-'1'+30;if(k=='0')return 39;
    if(k>=GK_F1&&k<=GK_F12)return 58+k-GK_F1;
    switch(k){case GK_ESCAPE:return 41;case GK_RETURN:return 40;case GK_TAB:return 43;case GK_BACKSPACE:return 42;case GK_DEL:return 76;case GK_LEFT:return 80;case GK_RIGHT:return 79;case GK_UP:return 82;case GK_DOWN:return 81;case GK_HOME:return 74;case GK_END:return 77;case GK_PGUP:return 75;case GK_PGDN:return 78;case ' ':return 44;case '-':return 45;case '=':return 46;case '[':return 47;case ']':return 48;case '\\':return 49;case ';':return 51;case '\'':return 52;case '`':return 53;case ',':return 54;case '.':return 55;case '/':return 56;}return 0;
}
bool DoomCraftNativeKey(int vk,bool pressed,bool repeat){
    if(vk>0 && vk<256)nativeHeld[vk]=pressed;
    if(DoomCraftMovement() && players[consoleplayer].mo->health<=0 && menuactive==MENU_Off && focused() && restartKey(vk)){
        if(pressed&&!repeat)requestRestart();return true;
    }
    if(!DoomCraftMovement() || screenKind!=0 || menuactive!=MENU_Off || !focused())return false;
    bool handled=false;
    for(const char* action:{"swap","inventory","drop","pick","chat","command","attack","use"}) {
        auto i=inputBindings.find(action);if(i!=inputBindings.end()&&i->second==vk){
            if(std::string(action)=="attack")key(action,pressed,heldAttack);
            else if(std::string(action)=="use")key(action,pressed,heldUse);
            else key(action,pressed,actionHeld[action]);handled=true;
        }
    }
    for(int n=0;n<9;n++){std::string name="hotbar"+std::to_string(n);auto i=inputBindings.find(name);int binding=i==inputBindings.end()?'1'+n:i->second;if(vk==binding){if(pressed&&!repeat)send("{\"t\":\"slot\",\"n\":"+std::to_string(n)+"}");actionHeld[name]=pressed;handled=true;}}
    if(vk==VK_F4&&(nativeHeld[VK_F3]||down(VK_F3))){if(pressed&&!repeat)send("{\"t\":\"mode_toggle\"}");previous[VK_F4]=pressed;return true;}
    if(vk==VK_F9){if(pressed&&!repeat)send("{\"t\":\"equip_elytra\"}");previous[VK_F9]=pressed;return true;}
    return handled;
}
bool DoomCraftScreen(){return DoomCraftMovement() && screenKind!=0 && menuactive==MENU_Off;}
bool DoomCraftEvent(event_t* ev){
    if(DoomCraftMovement() && players[consoleplayer].mo->health<=0 && menuactive==MENU_Off && focused()){
        if(ev->type==EV_KeyDown && (ev->data1==KEY_MOUSE1 || ev->data1==KEY_MOUSE2)){requestRestart();return true;}
        if(ev->type==EV_GUI_Event&&(ev->subtype==EV_GUI_LButtonDown||ev->subtype==EV_GUI_RButtonDown)){requestRestart();return true;}
        if(ev->type==EV_GUI_Event && ev->subtype==EV_GUI_KeyDown){
            int vk=ev->data1==GK_RETURN?VK_RETURN:ev->data1;
            if(vk>='a' && vk<='z')vk=vk-'a'+'A';
            if(restartKey(vk)){requestRestart();return true;}
        }
        if(ev->type==EV_KeyDown&&ev->data1>0&&ev->data1<256){int sc=ev->data1;if(sc&128)sc=(sc&127)|0xe000;if(restartKey(MapVirtualKeyW(sc,MAPVK_VSC_TO_VK_EX))){requestRestart();return true;}}
    }
    if(!DoomCraftScreen()) {
        if(DoomCraftMovement() && menuactive==MENU_Off && focused() && (ev->type==EV_KeyDown||ev->type==EV_KeyUp) && ev->data1>0 && ev->data1<256) {
            int scan=ev->data1;if(scan&128)scan=(scan&127)|0xe000;
            int vk=MapVirtualKeyW(scan,MAPVK_VSC_TO_VK_EX);bool pressed=ev->type==EV_KeyDown,handled=false;
            for(const char* action:{"swap","inventory","drop","pick","chat","command","attack","use"}) {
                auto i=inputBindings.find(action);if(i!=inputBindings.end()&&i->second==vk){
                    if(std::string(action)=="attack")key(action,pressed,heldAttack);
                    else if(std::string(action)=="use")key(action,pressed,heldUse);
                    else key(action,pressed,actionHeld[action]);handled=true;
                }
            }
            for(int n=0;n<9;n++){std::string name="hotbar"+std::to_string(n);auto i=inputBindings.find(name);int binding=i==inputBindings.end()?'1'+n:i->second;if(vk==binding){if(pressed)send("{\"t\":\"slot\",\"n\":"+std::to_string(n)+"}");actionHeld[name]=pressed;handled=true;}}
            if(handled)return true;
        }
        return false;
    }
    if(ev->type!=EV_GUI_Event)return ev->type==EV_Mouse||ev->type==EV_KeyDown||ev->type==EV_KeyUp;
    int mods=((ev->data3&GKM_SHIFT)?3:0)|((ev->data3&GKM_CTRL)?192:0)|((ev->data3&GKM_ALT)?768:0);
    char b[256];
    if(ev->subtype==EV_GUI_Char){std::snprintf(b,sizeof b,"{\"t\":\"chr\",\"c\":%d}",ev->data1);send(b);}
    else if(ev->subtype==EV_GUI_KeyDown||ev->subtype==EV_GUI_KeyUp||ev->subtype==EV_GUI_KeyRepeat){
        int sc=guiScan(ev->data1),code=ev->data1;if(code>='A'&&code<='Z')code=code-'A'+'a';
        if(sc){std::snprintf(b,sizeof b,"{\"t\":\"rkey\",\"sc\":%d,\"kc\":%d,\"a\":%d,\"m\":%d}",sc,code,ev->subtype==EV_GUI_KeyUp?0:ev->subtype==EV_GUI_KeyRepeat?2:1,mods);send(b);}
    }else if(ev->subtype==EV_GUI_WheelUp||ev->subtype==EV_GUI_WheelDown)send(std::string("{\"t\":\"wheel\",\"d\":")+(ev->subtype==EV_GUI_WheelUp?"1}":"-1}"));
    else {
        std::snprintf(b,sizeof b,"{\"t\":\"cursor\",\"x\":%.6f,\"y\":%.6f}",double(ev->data1)/screen->GetWidth(),double(ev->data2)/screen->GetHeight());send(b);
        int button=ev->subtype==EV_GUI_LButtonDown||ev->subtype==EV_GUI_LButtonUp?1:ev->subtype==EV_GUI_RButtonDown||ev->subtype==EV_GUI_RButtonUp?3:ev->subtype==EV_GUI_MButtonDown||ev->subtype==EV_GUI_MButtonUp?2:0;
        if(button){bool pressed=ev->subtype==EV_GUI_LButtonDown||ev->subtype==EV_GUI_RButtonDown||ev->subtype==EV_GUI_MButtonDown;std::snprintf(b,sizeof b,"{\"t\":\"mbtn\",\"b\":%d,\"m\":%d,\"down\":%s}",button,mods,pressed?"true":"false");send(b);}
    }
    return true;
}
static void key(const char* name, bool d, bool& was) {
    if (d!=was) { send(std::string("{\"t\":\"key\",\"k\":\"")+name+"\",\"down\":"+(d?"true":"false")+"}"); was=d; }
}
static DVector3 nativePos(const doomcraft_json::Value& p) { return DVector3((p[0].GetDouble()-mapOffset)*scale,-p[2].GetDouble()*scale,(p[1].GetDouble()-64)*scale); }
static bool triple(const doomcraft_json::Value& p){return p.IsArray() && p.Size()==3 && p[0].IsNumber() && p[1].IsNumber() && p[2].IsNumber() && std::isfinite(p[0].GetDouble()) && std::isfinite(p[1].GetDouble()) && std::isfinite(p[2].GetDouble());}
static void correctPosition(AActor* player) {
    char b[256]; std::snprintf(b,sizeof b,"{\"t\":\"pset\",\"id\":%u,\"pos\":[%.6f,%.6f,%.6f]}",++positionSerial,player->X()/scale+mapOffset,player->Z()/scale+64,-player->Y()/scale);send(b);
    acceptedPosition=player->Pos();havePosition=true;
}
static void blast(AActor* player,const DVector3& position,bool tnt,AActor* direct=nullptr){
    if((position-player->Pos()).Length()>16384)return;
    auto spot=Spawn(player->Level,PClass::FindActor(tnt?"DoomCraftTNTExplosion":"DoomCraftFireworkExplosion"),position,NO_REPLACE);
    if(!spot)return;
    spot->target=player;spot->flags&=~MF_MISSILE;
    if(direct && !(direct->ObjectFlags&OF_EuthanizeMe))P_DamageMobj(direct,spot,player,20*(1+(fireworkDamage()&7)),FName("Explosion"));
    spot->SetState(spot->FindState(NAME_Death));++explosions;
    Printf(PRINT_LOG,"DoomCraft: %s native explosion %u\n",tnt?"TNT":"firework",explosions);
}
static void projectiles(AActor* player,const doomcraft_json::Value& list){
    if(!list.IsArray())return;
    std::set<int> seen;
    for(auto& a:list.GetArray()){
        if(!a.IsArray() || a.Size()<5 || !a[0].IsInt() || !a[1].IsString() || !a[2].IsNumber() || !a[3].IsNumber() || !a[4].IsNumber())continue;
        std::string kind=a[1].GetString();bool rocket=kind=="firework",pearl=kind=="pearl",arrow=kind.rfind("arrow",0)==0,trident=kind.rfind("trident",0)==0,potion=kind.rfind("potion:",0)==0;
        if(!rocket&&!pearl&&!arrow&&!trident&&!potion)continue;
        int id=a[0].GetInt();seen.insert(id);
        DVector3 end((a[2].GetDouble()-mapOffset)*scale,-a[4].GetDouble()*scale,(a[3].GetDouble()-64)*scale);
        if(!std::isfinite(end.X)||!std::isfinite(end.Y)||!std::isfinite(end.Z)|| (end-player->Pos()).Length()>16384)continue;
        if(fireworks.size()>256)break;
        DVector3 start=DVector3(currentView.x-mapOffset,-currentView.z,currentView.y-64)*scale;
        if(a.Size()>=8&&a[5].IsNumber()&&a[6].IsNumber()&&a[7].IsNumber())start=DVector3((a[5].GetDouble()-mapOffset)*scale,-a[7].GetDouble()*scale,(a[6].GetDouble()-64)*scale);
        auto [it,created]=fireworks.emplace(id,FireworkTrack{start,false});
        auto& track=it->second;auto delta=end-track.position;double length=delta.Length();
        if(!track.done && length>0.001 && length<4096){
            FTraceResults hit{};
            if(Trace(track.position,player->Level->PointInSector(track.position),delta/length,length,MF_SHOOTABLE,ML_BLOCKING|ML_BLOCKEVERYTHING,player,hit,TRACE_NoSky|TRACE_PortalRestrict)){
                if(rocket)blast(player,hit.HitPos,false,hit.Actor);
                else if(hit.Actor&&(arrow||trident)) {
                    int damage=trident?80:40;
                    if(a.Size()>8&&a[8].IsNumber())damage=std::clamp((int)std::ceil(a[8].GetDouble()*10),1,1000);
                    P_DamageMobj(hit.Actor,player,player,damage,FName(trident?"Melee":"Normal"));
                }
                track.done=true;
                Printf(PRINT_LOG,"DoomCraft: %s hit %s\n",kind.c_str(),hit.Actor?hit.Actor->GetClass()->TypeName.GetChars():"world");
                DVector3 contact=hit.HitPos;
                if(pearl)contact-=delta/length*(player->radius+2);
                char b[256];std::snprintf(b,sizeof b,"{\"t\":\"projhit\",\"id\":%d,\"pos\":[%.5f,%.5f,%.5f],\"stick\":%s}",id,contact.X/scale+mapOffset,contact.Z/scale+64,-contact.Y/scale,(arrow||trident)&&!hit.Actor?"true":"false");send(b);
            }
        }
        track.position=end;
    }
    for(auto it=fireworks.begin();it!=fireworks.end();)if(!seen.count(it->first))it=fireworks.erase(it);else++it;
}
static void applyBlocks(const doomcraft_json::Value& a, bool add) {
    trace(add?"apply set":"apply clear");
    if (!a.IsArray() || a.Size()%3) return;
    auto player=players[consoleplayer].mo;
    static bool reported=false;
    if(add && a.Size() && !reported) {Printf("DoomCraft: block message %u values; class available %d\n",a.Size(),PClass::FindActor("DoomCraftBlock")!=nullptr); reported=true;}
    for (unsigned i=0;i<a.Size() && i<3*4096;i+=3) {
        if(!a[i].IsInt() || !a[i+1].IsInt() || !a[i+2].IsInt()) continue;
        int x=a[i].GetInt(),y=a[i+1].GetInt(),z=a[i+2].GetInt();
        auto id=std::make_tuple(x,y,z); auto it=blocks.find(id);
        if (!add) {
            if(it!=blocks.end()) {
                // Actors belong to the engine GC. Resolve live actors instead of retaining raw pointers across ticks/save loads.
                TThinkerIterator<AActor> scan(player->Level,FName("DoomCraftBlock"));
                while(auto actor=scan.Next()) {
                    int ax=(int)std::floor(actor->X()/scale+mapOffset),ay=(int)std::floor(actor->Z()/scale+64),az=(int)std::floor(-actor->Y()/scale);
                    if(std::make_tuple(ax,ay,az)==id)actor->Destroy();
                }
                blocks.erase(it);
            }
            continue;
        }
        if (it!=blocks.end() || blocks.size()>=4096) continue;
        // Bound collision proxies to the player's neighborhood; sync restores them on reconnect.
        DVector3 pos((x-mapOffset+0.5)*scale,-(z+0.5)*scale,(y-64)*scale);
        if ((pos-player->Pos()).Length()>4096) continue;
        if(std::abs(pos.X-player->X())<player->radius+16 && std::abs(pos.Y-player->Y())<player->radius+16 && pos.Z<player->Top() && pos.Z+32>player->Z()) {
            // Minecraft's avatar is slimmer than Doom's pawn: reject placement intersecting the authoritative host body.
            send("{\"t\":\"cmd\",\"c\":\"setblock "+std::to_string(x)+" "+std::to_string(y)+" "+std::to_string(z)+" air\"}");
            continue;
        }
        auto cls=PClass::FindActor("DoomCraftBlock");
        if (cls) {  auto b=Spawn(player->Level,cls,pos,NO_REPLACE); if(b && !(b->ObjectFlags & OF_EuthanizeMe)) blocks.insert(id);  }
    }
}
static void geometry(AActor* player,bool force=false) {
    static std::string previousMap;static int previousX=0,previousZ=0;
    static double previousFloor=0,previousCeiling=0;
    auto sector=player->Level->PointInSector(player->Pos());
    double pf=sector->floorplane.ZatPoint(player->Pos()),pc=sector->ceilingplane.ZatPoint(player->Pos());
    int cx=(int)std::floor(player->X()/scale), cz=(int)std::floor(-player->Y()/scale);
    previousMap=mapName;previousX=cx;previousZ=cz;previousFloor=pf;previousCeiling=pc;
    std::set<std::tuple<int,int,int,int>> columns;
    std::string collision="{\"t\":\"hc\",\"b\":["; bool firstBox=true;
    auto box=[&](double x0,double y0,double z0,double x1,double y1,double z1){ char b[256];std::snprintf(b,sizeof b,"%s%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",firstBox?"":",",x0+mapOffset,y0,z0,x1+mapOffset,y1,z1);collision+=b;firstBox=false; };
    // Local floor/ceiling approximation. Native movement remains authoritative.
    for(int x=cx-8;x<=cx+8;x++) for(int z=cz-8;z<=cz+8;z++) {
        auto sec=player->Level->PointInSector(DVector2((x+.5)*scale,-(z+.5)*scale));
        int floor=(int)std::ceil(sec->floorplane.ZatPoint((x+.5)*scale,-(z+.5)*scale)/scale+64)-1;
        int ceil=(int)std::floor(sec->ceilingplane.ZatPoint((x+.5)*scale,-(z+.5)*scale)/scale+64);
        columns.emplace(x,z,floor,floor);
        columns.emplace(x,z,ceil,ceil);
        double fy=sec->floorplane.ZatPoint((x+.5)*scale,-(z+.5)*scale)/scale+64;
        double cy=sec->ceilingplane.ZatPoint((x+.5)*scale,-(z+.5)*scale)/scale+64;
        box(x,fy-1,z,x+1,fy,z+1);box(x,cy,z,x+1,cy+1,z+1);
    }
    // Blocking linedefs supply voxel walls near the player.
    for(auto& l:player->Level->lines) {
        if(l.backsector && !(l.flags & ML_BLOCKING)) continue;
        double x0=l.v1->fX()/scale, z0=-l.v1->fY()/scale;
        double x1=l.v2->fX()/scale, z1=-l.v2->fY()/scale;
        if(std::max(x0,x1)<cx-8 || std::min(x0,x1)>cx+9 || std::max(z0,z1)<cz-8 || std::min(z0,z1)>cz+9) continue;
        int steps=(int)std::ceil(std::max(std::abs(x1-x0),std::abs(z1-z0))*4);
        steps=std::min(steps,4096);
        for(int i=0;i<=steps;i++) {
            double t=steps?double(i)/steps:0;
            int x=(int)std::floor(x0+(x1-x0)*t),z=(int)std::floor(z0+(z1-z0)*t);
            if(std::abs(x-cx)>8 || std::abs(z-cz)>8) continue;
            int y=(int)std::floor(player->Z()/scale+64);
            columns.emplace(x,z,y-2,y+5);
            double wx=x0+(x1-x0)*t,wz=z0+(z1-z0)*t;
            box(wx-.08,y-2,wz-.08,wx+.08,y+5,wz+.08);
        }
    }
    std::string s="{\"t\":\"ground\",\"c\":["; bool first=true;
    for(auto c:columns) { char b[96]; std::snprintf(b,sizeof b,"%s%d,%d,%d,%d",first?"":",",std::get<0>(c)+(int)mapOffset,std::get<1>(c),std::get<2>(c),std::get<3>(c)); s+=b; first=false; }
    static std::string previousCollision,previousColumns;
    collision+="]}";s+="]}";
    // Recheck neighboring sectors so opening doors/platforms update even while standing still.
    if(!force && collision==previousCollision && s==previousColumns)return;
    previousCollision=collision;previousColumns=s;
    send("{\"t\":\"clear\"}"); send(s);send(collision);
}
void DoomCraftTick() {
    if(!started) { link.start("127.0.0.1",25601); started=true; }
    if(press(VK_F7)) doomcraft_enabled=!doomcraft_enabled;
    if(!DoomCraftActive()) { key("attack",false,heldAttack); key("use",false,heldUse); return; }
    auto player=players[consoleplayer].mo;
    bool dead=player->health<=0 || players[consoleplayer].playerstate==PST_DEAD;
    bool restartDown=actionDown("attack") || actionDown("use") || actionDown("jump") || down(VK_RETURN) || down('G');
    if(dead) {
        if(!wasDead) {
            restartWasDown=restartDown;
            Printf("DoomCraft: click, Jump, Enter, or G to restart after death\n");
        } else if(menuactive==MENU_Off && focused() && restartDown && !restartWasDown)requestRestart();
        restartWasDown=restartDown;
    } else if(wasDead) {
        generation=-1;havePosition=false;restartRequested=false;minecraftEye=1.62;
        Printf("DoomCraft: respawn movement reset\n");
    }
    wasDead=dead;
    if(DoomCraftBossTick(player)){
        // A fresh serial discards guest movement captured in the level's starting room.
        generation=-1;havePosition=false;
    }
    if(creative||spectator)players[consoleplayer].cheats|=CF_GODMODE;else players[consoleplayer].cheats&=~CF_GODMODE;
    // Use square world units so Doom geometry and Minecraft cubes share the same projection.
    player->Level->info->pixelstretch=1;player->Level->pixelstretch=1;
    std::string current=player->Level->MapName.GetChars();
    if(current!=mapName || player->Level!=observedLevel || player->Level->time<observedTime) {
        totemPending=0;totemGrace=0;
        blocks.clear();fireworks.clear(); mapName=current; generation=-1;havePosition=false;
        int episode=0,map=0;
        if(std::sscanf(current.c_str(),"E%dM%d",&episode,&map)==2) mapOffset=((episode-1)*9+map-1)*4096;
        else if(std::sscanf(current.c_str(),"MAP%d",&map)==1) mapOffset=(100+map-1)*4096;
        send("{\"t\":\"clear\"}");
    }
    observedLevel=player->Level;observedTime=player->Level->time;
    if(tick%70==0) {
        blocks.clear();
        TThinkerIterator<AActor> scan(player->Level,FName("DoomCraftBlock"));
        while(auto actor=scan.Next())blocks.emplace((int)std::floor(actor->X()/scale+mapOffset),(int)std::floor(actor->Z()/scale+64),(int)std::floor(-actor->Y()/scale));
    }
    if(!link.connected()) { heldAttack=heldUse=false; return; }
    if(link.generation()!=generation) {
        generation=link.generation();
        send("{\"t\":\"view\",\"w\":1280,\"h\":720}");
        geometry(player,true);
        correctPosition(player);
        Printf("DoomCraft: connected to Minecraft on port 25601\n");
    }
    if(++tick%18==0) geometry(player);
    if(tick%20==0)reportAbsorptionDamage();
    if(totemPending && tick>=totemDeadline){totemPending=0;minecraftTotem=false;P_DamageMobj(player,nullptr,nullptr,10000,FName("None"),DMG_FORCED);}
    if(tick%70==0) send("{\"t\":\"blocksync\",\"r\":48}");
    bool controls=menuactive==MENU_Off && focused();
    bool gameControls=controls && screenKind==0 && !dead;
    if(havePosition && (player->Pos()-acceptedPosition).Length()>4)correctPosition(player);
    movementInput=gameControls && player->health>0 ? (actionDown("forward")?1:0)|(actionDown("back")?2:0)|(actionDown("left")?4:0)|(actionDown("right")?8:0)|(actionDown("jump")?16:0)|(actionDown("sneak")?32:0)|(actionDown("sprint")?64:0) : 0;
    if(testInputTicks>0) {--testInputTicks;if(controls && !movementInput)movementInput=testInput;}
    if(press(VK_F9)&&gameControls)send("{\"t\":\"equip_elytra\"}");
    if(press(VK_F4)&&gameControls&&down(VK_F3))send("{\"t\":\"mode_toggle\"}");
    key("attack",gameControls&&actionDown("attack"),heldAttack);
    key("use",gameControls&&actionDown("use"),heldUse);
    for(const char* action:{"swap","inventory","drop","pick","chat","command"})key(action,gameControls&&actionDown(action),actionHeld[action]);
    for(int i=0;i<9;i++) {std::string name="hotbar"+std::to_string(i);bool pressed=gameControls&&actionDown(name);if(pressed&&!actionHeld[name])send("{\"t\":\"slot\",\"n\":"+std::to_string(i)+"}");actionHeld[name]=pressed;}
    std::string message;
    for(int count=0;count<128 && link.poll(message);count++) {
        trace("receive "+message.substr(0,200));
        doomcraft_json::Document m; m.Parse(message.c_str());
        trace(std::string("parsed error ")+std::to_string(m.HasParseError())+" object "+std::to_string(m.IsObject()));
        if(m.HasParseError() || !m.IsObject() || !m.HasMember("t") || !m["t"].IsString()) continue;
        std::string type=m["t"].GetString();
        trace("type "+type);
        if(type=="screen" && m.HasMember("kind") && m["kind"].IsInt())screenKind=std::clamp(m["kind"].GetInt(),0,3);
        if(type=="bindings" && m.HasMember("b") && m["b"].IsObject()) {
            for(auto i=m["b"].MemberBegin();i!=m["b"].MemberEnd();++i)if(i->value.IsString())inputBindings[i->name.GetString()]=bindingKey(i->value.GetString());
        }
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && std::string(m["op"].GetString())=="input" && m.HasMember("bits") && m["bits"].IsInt()) {
            testInput=m["bits"].GetInt()&127;testInputTicks=m.HasMember("ticks") && m["ticks"].IsInt()?std::clamp(m["ticks"].GetInt(),0,105):1;
        }
        if(type=="absorption" && m.HasMember("session") && m["session"].IsUint() && m["session"].GetUint()==absorptionSession && m.HasMember("spent") && m["spent"].IsUint64() && m["spent"].GetUint64()<=absorptionSpent && m.HasMember("amount") && m["amount"].IsNumber()) {
            double amount=m["amount"].GetDouble();
            if(std::isfinite(amount))minecraftAbsorption=(int)std::max(0.0,std::min(10000.0,std::round(amount*5)-(absorptionSpent-m["spent"].GetUint64())));
        }
        if(type=="pstate") {
            reportedTotem=m.HasMember("tot") && m["tot"].IsInt() && m["tot"].GetInt()!=0;
            if(!reportedTotem)totemAwaitEmpty=false;
            if(!totemPending && !totemAwaitEmpty)minecraftTotem=reportedTotem;
            minecraftArmor=m.HasMember("arm") && m["arm"].IsInt()?std::clamp(m["arm"].GetInt(),0,20):0;
            creative=m.HasMember("creative") && m["creative"].IsBool() && m["creative"].GetBool();
            spectator=m.HasMember("spec") && m["spec"].IsInt() && m["spec"].GetInt()!=0;
            if(creative||spectator)players[consoleplayer].cheats|=CF_GODMODE;else players[consoleplayer].cheats&=~CF_GODMODE;
        }
        if(type=="totem_ack" && m.HasMember("id") && m["id"].IsUint() && m["id"].GetUint()==totemPending && totemPending){
            bool used=m.HasMember("used") && m["used"].IsBool() && m["used"].GetBool();
            totemPending=0;
            if(used){
                minecraftTotem=m.HasMember("remaining") && m["remaining"].IsBool() && m["remaining"].GetBool();
                totemAwaitEmpty=!minecraftTotem && reportedTotem;
                totemGrace=tick+18;
                Printf(PRINT_LOG,"DoomCraft: totem consumed; death prevented\n");
            }else{minecraftTotem=false;P_DamageMobj(player,nullptr,nullptr,10000,FName("None"),DMG_FORCED);}
        }
        if(type=="mcpos" && m.HasMember("pos") && triple(m["pos"]) && m.HasMember("ps") && m["ps"].IsInt() && m["ps"].GetInt()==(int)positionSerial && player->health>0) {
            if(m.HasMember("height") && m["height"].IsNumber())player->Height=std::clamp(m["height"].GetDouble(),0.6,1.8)*scale;
            if(m.HasMember("eye") && m["eye"].IsNumber())minecraftEye=std::clamp(m["eye"].GetDouble(),0.4,1.62);
            auto target=nativePos(m["pos"]);
            auto requested=target;
            if((target-player->Pos()).Length()>1024) {correctPosition(player);continue;}
            auto old=player->Pos();
            auto sec=player->Level->PointInSector(target);
            if(!spectator)target.Z=std::max(sec->floorplane.ZatPoint(target),std::min(sec->ceilingplane.ZatPoint(target)-player->Height,target.Z));
            bool blocked=false;
            if(spectator)player->SetOrigin(target,false);
            else {
                int steps=std::max(1,(int)std::ceil((target-old).Length()/8));
                for(int step=1;step<=steps;step++) {
                    auto at=old+(target-old)*(double(step)/steps);double zBefore=player->Z();player->SetZ(at.Z);
                    if(!P_TryMove(player,DVector2(at.X,at.Y),2)) {player->SetZ(zBefore);blocked=true;break;}
                }
                if(blocked)correctPosition(player);
            }
            // During map streaming the guest may briefly lack its floor/ceiling chunks.
            // Correct clamped height instead of letting the captured camera fall through Doom's floor.
            if(std::abs(player->Z()-requested.Z)>0.5)correctPosition(player);
            acceptedPosition=player->Pos();havePosition=true;player->Vel=DVector3(0,0,0);
        }
        if(type=="pteleport" && m.HasMember("pos") && triple(m["pos"])) {
            auto target=nativePos(m["pos"]);
            if((target-player->Pos()).Length()<16384) {
                auto sec=player->Level->PointInSector(target);
                target.Z=std::max(sec->floorplane.ZatPoint(target),std::min(sec->ceilingplane.ZatPoint(target)-player->Height,target.Z));
                auto old=player->Z();player->SetZ(target.Z);
                if(!P_TeleportMove(player,target,false))player->SetZ(old);
                correctPosition(player);geometry(player,true);
            }
        }
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && std::string(m["op"].GetString())=="boss" && m.HasMember("map") && m["map"].IsString() && m.HasMember("actor") && m["actor"].IsString()) {
            DoomCraftBossRequest(m["map"].GetString(),m["actor"].GetString(),m.HasMember("index") && m["index"].IsInt()?m["index"].GetInt():0,m.HasMember("skill") && m["skill"].IsInt()?m["skill"].GetInt():2);
            break;
        }
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && std::string(m["op"].GetString())=="resupply") { AddCommandString("doomcraft_resupply"); }
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && std::string(m["op"].GetString())=="boss_menu") { AddCommandString("doomcraft_boss_menu"); }
        if(type=="proj" && m.HasMember("p"))projectiles(player,m["p"]);
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && std::string(m["op"].GetString())=="freeze" && m.HasMember("on") && m["on"].IsBool()) {
            globalchangefreeze=globalfreeze != (m["on"].GetBool()?1:0) ? 1:0;
        }
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && !netgame){
            std::string op=m["op"].GetString();
            if(op=="respawn"&&dead)requestRestart();
            if(op=="death_test"&&!dead)P_DamageMobj(player,nullptr,nullptr,10000,FName("None"),DMG_FORCED);
            if(op=="damage_test"&&!dead && m.HasMember("damage") && m["damage"].IsInt())P_DamageMobj(player,nullptr,nullptr,std::clamp(m["damage"].GetInt(),1,500),FName("Normal"));
        }
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && std::string(m["op"].GetString())=="view" && m.HasMember("yaw") && m["yaw"].IsNumber() && m.HasMember("pitch") && m["pitch"].IsNumber()) {
            if(std::isfinite(m["yaw"].GetDouble()) && std::isfinite(m["pitch"].GetDouble())) {
                player->Angles.Yaw=DAngle::fromDeg(m["yaw"].GetDouble());player->Angles.Pitch=DAngle::fromDeg(std::max(-80.0,std::min(80.0,m["pitch"].GetDouble())));
            }
        }
        if(type=="director" && m.HasMember("op") && m["op"].IsString() && std::string(m["op"].GetString())=="collision_probe" && m.HasMember("p") && m["p"].IsArray() && m["p"].Size()==3 && m["p"][0].IsNumber() && m["p"][1].IsNumber() && m["p"][2].IsNumber()) {
            auto point=nativePos(m["p"]);
            if((point-player->Pos()).Length()<4096)send(std::string("{\"t\":\"gtainfo\",\"op\":\"collision_probe\",\"clear\":")+(P_CheckPosition(player,DVector2(point.X,point.Y))?"true":"false")+"}");
        }
        if(type=="blocks") { if(m.HasMember("clear")) applyBlocks(m["clear"],false); if(m.HasMember("set")) applyBlocks(m["set"],true); }
        if(type=="melee" && m.HasMember("s") && m["s"].IsNumber()) {
            int damage=(int)std::round(35*std::max(0.2,m["s"].GetDouble()));
            trace("melee start");
            P_LineAttack(player,player->Angles.Yaw,3*scale,player->Angles.Pitch,damage,NAME_Melee,FName("BulletPuff"));
            trace("melee done");
        }
        if(type=="pheal" && player->health>0 && players[consoleplayer].playerstate==PST_LIVE && m.HasMember("d") && m["d"].IsNumber()) {
            player->health=std::min(100,player->health+(int)std::round(m["d"].GetDouble()*5)); players[consoleplayer].health=player->health;
        }
        if(type=="explosion" && m.HasMember("pos") && triple(m["pos"]) && m.HasMember("r") && m["r"].IsNumber()) {
            bool firework=m.HasMember("src") && m["src"].IsString() && std::string(m["src"].GetString())=="firework";
            int id=m.HasMember("id") && m["id"].IsInt()?m["id"].GetInt():-1;
            auto it=fireworks.find(id);
            if(!firework || it==fireworks.end() || !it->second.done)blast(player,nativePos(m["pos"]),!firework);
            if(it!=fireworks.end())it->second.done=true;
        }
    }
    players[consoleplayer].viewheight=minecraftEye*scale;players[consoleplayer].deltaviewheight=0;
}
void DoomCraftCamera(const FRenderViewpoint& vp, float fov, float ratio, float fovRatio) {
    if(!DoomCraftActive() || !link.connected()) return;
    auto p=players[consoleplayer].mo;
    DVector3 eye=vp.Pos;if(p->health>0)eye.Z=p->Z()+minecraftEye*scale;
    double yaw=-90.0-vp.Angles.Yaw.Degrees();
    double vertical=2*180/3.141592653589793*std::atan(std::tan(fov*3.141592653589793/360)/fovRatio);
    currentView={eye.X/scale+mapOffset,eye.Z/scale+64,-eye.Y/scale,(float)yaw,(float)vp.Angles.Pitch.Degrees(),(float)vertical};
    char b[1024];
    FTraceResults hit{};
    double cp=vp.Angles.Pitch.Cos();
    DVector3 direction(cp*vp.Angles.Yaw.Cos(),cp*vp.Angles.Yaw.Sin(),-vp.Angles.Pitch.Sin());
    std::string surface;
    if(Trace(eye,p->Level->PointInSector(eye),direction,5*scale,0,ML_BLOCKING|ML_BLOCKEVERYTHING,p,hit,TRACE_NoSky|TRACE_PortalRestrict)) {
        DVector3 normal(0,0,0);
        if(hit.HitType==TRACE_HitFloor)normal=hit.Sector->floorplane.Normal();
        else if(hit.HitType==TRACE_HitCeiling)normal=hit.Sector->ceilingplane.Normal();
        else if(hit.HitType==TRACE_HitWall && hit.Line) {
            double dx=hit.Line->v2->fX()-hit.Line->v1->fX(),dy=hit.Line->v2->fY()-hit.Line->v1->fY();
            double length=std::hypot(dx,dy); if(length>0)normal=DVector3(-dy/length,dx/length,0);
        }
        if((normal|direction)>0)normal=-normal;
        if(normal.LengthSquared()>0.1) {
            std::snprintf(b,sizeof b,",\"ghOn\":true,\"gh\":[%.6f,%.6f,%.6f,%.6f,%.6f,%.6f]",hit.HitPos.X/scale+mapOffset,hit.HitPos.Z/scale+64,-hit.HitPos.Y/scale,normal.X,normal.Z,-normal.Y);
            surface=b;
        }
    }
    std::snprintf(b,sizeof b,"{\"t\":\"cam\",\"f\":%llu,\"p\":[%.6f,%.6f,%.6f],\"r\":[%.5f,%.5f,0],\"fov\":%.5f,\"fp\":true,\"pl\":[%.6f,%.6f,%.6f],\"h\":%.5f,\"hp\":[%d,100],\"ar\":%d,\"ctl\":%s,\"dead\":%s}",
        ++frame,eye.X/scale+mapOffset,eye.Z/scale+64,-eye.Y/scale,yaw,vp.Angles.Pitch.Degrees(),vertical,
        p->X()/scale+mapOffset,p->Z()/scale+64,-p->Y()/scale,yaw,players[consoleplayer].playerstate==PST_DEAD?0:std::max(0,p->health),0,
        menuactive==MENU_Off && focused()?"true":"false",p->health<=0?"true":"false");
    std::string camera=b; camera.pop_back();
    send(camera+surface+",\"walk\":"+(p->health>0?"true":"false")+",\"in\":"+std::to_string(movementInput)+"}");
    if(frame%60==0) {
        std::snprintf(b,sizeof b,"{\"t\":\"gtastate\",\"host\":\"doom\",\"map\":\"%s\",\"pos\":[%.4f,%.4f,%.4f],\"camera\":[%.4f,%.4f,%.4f],\"yaw\":%.4f,\"pitch\":%.4f,\"fov\":%.4f,\"blocks\":%zu,\"health\":%d,\"explosions\":%u,\"controls\":%s,\"shownFrame\":%llu,\"rendered\":[%.6f,%.6f,%.6f,%.5f,%.5f]}",
            mapName.c_str(),p->X()/scale+mapOffset,p->Z()/scale+64,-p->Y()/scale,
            vp.Pos.X/scale+mapOffset,vp.Pos.Z/scale+64,-vp.Pos.Y/scale,yaw,vp.Angles.Pitch.Degrees(),vertical,blocks.size(),p->health,explosions,menuactive==MENU_Off && focused()?"true":"false",(unsigned long long)OpenGLRenderer::DoomCraftShownFrame(),displayedView.x,displayedView.y,displayedView.z,displayedView.yaw,displayedView.pitch); std::string state=b; state.pop_back(); state+=",\"bosses\":["; bool first=true; for(const char* name:{"Cyberdemon","SpiderMastermind","BaronOfHell","BossBrain"}) { TThinkerIterator<AActor> scan(p->Level,FName(name)); int index=0; while(auto boss=scan.Next()){ char info[256];std::snprintf(info,sizeof info,"%s{\"actor\":\"%s\",\"index\":%d,\"health\":%d,\"pos\":[%.4f,%.4f,%.4f]}",first?"":",",name,index++,boss->health,boss->X()/scale+mapOffset,boss->Z()/scale+64,-boss->Y()/scale);state+=info;first=false;} } send(state+"]}");
    }
}
CCMD(doomcraft_next) { if(DoomCraftActive())send("{\"t\":\"scroll\",\"d\":1}"); }
CCMD(doomcraft_prev) { if(DoomCraftActive())send("{\"t\":\"scroll\",\"d\":-1}"); }

CCMD(doomcraft_resupply) {
    if(!DoomCraftActive())return;
    for(const char* command:{
        "item replace entity @p hotbar.2 with minecraft:crossbow[charged_projectiles=[{id:\"minecraft:firework_rocket\",count:1}],enchantments={quick_charge:3}]",
        "item replace entity @p hotbar.4 with minecraft:tnt 64",
        "item replace entity @p hotbar.5 with minecraft:flint_and_steel",
        "item replace entity @p hotbar.8 with minecraft:firework_rocket 64",
        "item replace entity @p weapon.offhand with minecraft:firework_rocket 64"}) {
        std::string escaped; for(char c:std::string(command)){if(c=='"'||c=='\\')escaped+='\\';escaped+=c;}
        send("{\"t\":\"cmd\",\"c\":\""+escaped+"\"}");
    }
    Printf("DoomCraft: TNT and firework crossbow refilled.\n");
}

void DoomCraftSyncView(FRenderViewpoint& vp){
    DoomCraftView shown;if(!OpenGLRenderer::DoomCraftPrepareView(shown))return;
    displayedView=shown;
    DVector3 position((shown.x-mapOffset)*scale,-shown.z*scale,(shown.y-64)*scale);
    static std::string reported; if(reported!=mapName) {
        auto destination=vp.ViewLevel->PointInSector(position);
        Printf(PRINT_LOG,"DoomCraft camera %s offset %.0f old %.6f %.6f %.6f new %.6f %.6f %.6f sectors %d %d floor %.2f ceil %.2f\n",mapName.c_str(),mapOffset,vp.Pos.X,vp.Pos.Y,vp.Pos.Z,position.X,position.Y,position.Z,vp.sector->Index(),destination->Index(),destination->floorplane.ZatPoint(position),destination->ceilingplane.ZatPoint(position));reported=mapName;
    }
    auto delta=position-vp.Pos;vp.Pos=position;vp.Path[0]+=delta;vp.Path[1]+=delta;
    vp.sector=vp.ViewLevel->PointInSector(position);
    vp.Angles.Yaw=DAngle::fromDeg(-90-shown.yaw);vp.Angles.Pitch=DAngle::fromDeg(shown.pitch);vp.Angles.Roll=DAngle::fromDeg(0);
    vp.SetViewAngle(r_viewwindow);vp.HWAngles.Pitch=FAngle::fromDeg(shown.pitch);vp.HWAngles.Roll=FAngle::fromDeg(0);
}
