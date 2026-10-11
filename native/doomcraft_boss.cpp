// Local arena travel for the original Doom/Doom II IWADs. GPL-3.0-or-later.
#include "doomcraft.h"
#include "actor.h"
#include "doomstat.h"
#include "g_level.h"
#include "g_game.h"
#include "d_event.h"
#include "g_levellocals.h"
#include "p_local.h"
#include "p_setup.h"
#include "p_trace.h"
#include "c_dispatch.h"
#include "menu.h"
#include "gi.h"
#include <cstdlib>
#include <algorithm>
#include "printf.h"
#include <string>
#include <cmath>
static std::string destination,kind;
static int bossIndex=0;
static bool waitingForLoad=false;
void DoomCraftBossRequest(const char* map,const char* actor,int index,int skill){
    if(netgame || !P_CheckMapData(map)){Printf("DoomCraft: this boss destination is not in the current game. Use its desktop launcher.\n");return;}
    if(std::string(actor)!="Cyberdemon" && std::string(actor)!="SpiderMastermind" && std::string(actor)!="BaronOfHell" && std::string(actor)!="BossBrain")return;
    destination=map;kind=actor;bossIndex=std::max(0,std::min(64,index));
    waitingForLoad=true;
    M_ClearMenus();G_DeferedInitNew(map,std::max(0,std::min(3,skill)));
}
static bool land(AActor* player,const DVector2& point){
    auto sector=player->Level->PointInSector(point);
    double floor=sector->floorplane.ZatPoint(point),ceiling=sector->ceilingplane.ZatPoint(point);
    if(ceiling-floor<player->Height)return false;
    double oldZ=player->Z();player->SetZ(floor);
    bool okay=P_CheckPosition(player,point);player->SetZ(oldZ);
    if(!okay || !P_TeleportMove(player,DVector3(point.X,point.Y,floor),false))return false;
    player->Vel=DVector3(0,0,0);player->ClearInterpolation();return true;
}
bool DoomCraftBossTick(AActor* player){
    // Deferred new games take two ticks. Never spend a same-map request on the old level.
    if(destination.empty() || gameaction==ga_newgame || gameaction==ga_newgame2 || destination!=player->Level->MapName.GetChars())return false;
    if(waitingForLoad){
        if(player->Level->time>=5)return false;
        waitingForLoad=false;
    }
    if(player->Level->time<5 || player->health<=0)return false;
    TThinkerIterator<AActor> bosses(player->Level,FName(kind.c_str()));AActor* boss=nullptr;
    for(int i=0;i<=bossIndex;i++){boss=bosses.Next();if(!boss)break;}
    if(!boss){Printf("DoomCraft: boss unavailable on this difficulty.\n");destination.clear();return false;}
    bool moved=false;
    if(kind=="BossBrain"){
        // MAP30's real teleport landing leads into the arena; the brain is hidden behind the face.
        TThinkerIterator<AActor> spots(player->Level,FName("TeleportDest"));
        while(auto spot=spots.Next())if((spot->Pos()-boss->Pos()).Length()<4096 && land(player,spot->Pos().XY())){moved=true;break;}
    }else{
        for(double radius:{128.0,160.0,256.0,384.0,512.0,768.0,96.0}){
            for(int i=0;i<16 && !moved;i++){
                double a=i*6.283185307179586/16;
                DVector2 point(boss->X()+std::cos(a)*radius,boss->Y()+std::sin(a)*radius);
                auto sec=player->Level->PointInSector(point);
                DVector3 eye(point.X,point.Y,sec->floorplane.ZatPoint(point)+41);
                auto toward=boss->PosAtZ(boss->Center())-eye;double length=toward.Length();FTraceResults trace{};
                if(length>0 && Trace(eye,sec,toward/length,length+boss->radius,MF_SHOOTABLE,ML_BLOCKING|ML_BLOCKEVERYTHING,player,trace,TRACE_NoSky) && trace.Actor==boss)moved=land(player,point);
            }
            if(moved)break;
        }
    }
    if(!moved && kind!="BossBrain") {
        for(double radius:{96.0,128.0,160.0,256.0,384.0}) {
            for(int i=0;i<32 && !moved;i++) {
                double a=i*6.283185307179586/32;
                moved=land(player,DVector2(boss->X()+std::cos(a)*radius,boss->Y()+std::sin(a)*radius));
            }
            if(moved)break;
        }
    }
    if(moved){player->Angles.Yaw=DAngle::fromRad(std::atan2(boss->Y()-player->Y(),boss->X()-player->X()));player->Angles.Pitch=DAngle::fromRad(-std::atan2(boss->Center()-(player->Z()+41),std::hypot(boss->X()-player->X(),boss->Y()-player->Y())));Printf("DoomCraft: arrived at %s in %s\n",kind.c_str(),destination.c_str());}
    else Printf("DoomCraft: no clear arena landing found; loaded the boss level at its start.\n");
    destination.clear();
    return moved;
}
CCMD(doomcraft_boss){if(argv.argc()>=5)DoomCraftBossRequest(argv[1],argv[2],std::atoi(argv[3]),std::atoi(argv[4]));}
CCMD(doomcraft_boss_menu){M_StartControlPanel(true);M_SetMenu(FName((gameinfo.flags&GI_MAPxx)?"DoomCraftDoomIIBosses":"DoomCraftDoomBosses"));}
