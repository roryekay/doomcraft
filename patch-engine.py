"""Apply the explicit DoomCraft extension to the pinned local GZDoom source."""
from pathlib import Path
import shutil
ROOT=Path(__file__).resolve().parent
ENGINE=ROOT/'local/gzdoom-source'

def replace(path,old,new):
    p=ENGINE/path
    text=p.read_text(encoding='utf8')
    if new in text:return
    if text.count(old)!=1:raise RuntimeError(f'Unexpected upstream hook: {path}')
    p.write_text(text.replace(old,new),encoding='utf8')

target=ENGINE/'src/doomcraft'
target.mkdir(exist_ok=True)
for p in (ROOT/'native').glob('*'):
    if p.suffix in ('.cpp','.h'):shutil.copy2(p,target/p.name)
if 'doomcraft/doomcraft.cpp' not in (ENGINE/'src/CMakeLists.txt').read_text():replace(Path('src/CMakeLists.txt'),'add_executable( zdoom WIN32 MACOSX_BUNDLE ${GAME_SOURCES} )','add_executable( zdoom WIN32 MACOSX_BUNDLE ${GAME_SOURCES} doomcraft/doomcraft.cpp doomcraft/doomcraft_gl.cpp doomcraft/ws.cpp )\ntarget_link_libraries(zdoom ws2_32)')
replace(Path('src/CMakeLists.txt'),'doomcraft/doomcraft_gl.cpp doomcraft/ws.cpp','doomcraft/doomcraft_gl.cpp doomcraft/doomcraft_boss.cpp doomcraft/ws.cpp')
p=ENGINE/'src/g_game.cpp';s=p.read_text(encoding='utf8')
p.write_text(s.replace('void G_Ticker ()\n{\n\tDoomCraftTick();','void G_Ticker ()\n{'),encoding='utf8')
replace(Path('src/g_game.cpp'),'\tprimaryLevel->localEventManager->PostUiTick();\n}','\tprimaryLevel->localEventManager->PostUiTick();\n\tDoomCraftTick();\n}')
replace(Path('src/g_game.cpp'),'#include "doomstat.h"','#include "doomstat.h"\n#include "doomcraft/doomcraft.h"')
if 'DoomCraftCamera(mainvp' not in (ENGINE/'src/rendering/hwrenderer/hw_entrypoint.cpp').read_text():replace(Path('src/rendering/hwrenderer/hw_entrypoint.cpp'),'\tR_SetupFrame(mainvp, r_viewwindow, camera);','\tR_SetupFrame(mainvp, r_viewwindow, camera);\n\tif(mainview && toscreen) DoomCraftCamera(mainvp, fov, ratio, fovratio);')
replace(Path('src/rendering/hwrenderer/hw_entrypoint.cpp'),'if(mainview && toscreen) DoomCraftCamera(mainvp, fov, ratio, fovratio);','if(mainview && toscreen) { DoomCraftCamera(mainvp, fov, ratio, fovratio); DoomCraftSyncView(mainvp); }')
p=ENGINE/'src/rendering/hwrenderer/hw_entrypoint.cpp';s=p.read_text();
if '#include "doomcraft/doomcraft.h"' not in s:p.write_text('#include "doomcraft/doomcraft.h"\n'+s,encoding='utf8')
replace(Path('src/common/rendering/gl/gl_postprocess.cpp'),'\tRenderScreenQuad();\n}','\tRenderScreenQuad();\n\tDoomCraftComposite(mBuffers);\n}')
p=ENGINE/'src/common/rendering/gl/gl_postprocess.cpp';s=p.read_text();
if '#include "doomcraft/doomcraft.h"' not in s:p.write_text('#include "doomcraft/doomcraft.h"\n'+s,encoding='utf8')
# Always expose scene depth, including when ambient occlusion is turned off.
replace(Path('src/common/rendering/gl/gl_renderbuffers.cpp'),'bool needsSceneTextures = (gl_ssao != 0);','bool needsSceneTextures = true; // DoomCraft requires the scene depth texture.')
print('Applied DoomCraft camera, gameplay and OpenGL hooks to GZDoom 4.14.2.')
p=ENGINE/'src/d_main.cpp';s=p.read_text()
if '#include "doomcraft/doomcraft.h"' not in s:p.write_text('#include "doomcraft/doomcraft.h"\n'+s,encoding='utf8')
replace(Path('src/d_main.cpp'),'bool System_WantGuiCapture()\n{','bool System_WantGuiCapture()\n{\n\tif(DoomCraftScreen())return true;')
replace(Path('src/d_main.cpp'),'bool System_WantNativeMouse()\n{','bool System_WantNativeMouse()\n{\n\tif(DoomCraftScreen())return true;')
replace(Path('src/d_main.cpp'),'static bool System_CaptureModeInGame()\n{','static bool System_CaptureModeInGame()\n{\n\tif(DoomCraftScreen())return false;')
replace(Path('src/d_main.cpp'),'static bool System_DispatchEvent(event_t* ev)\n{','static bool System_DispatchEvent(event_t* ev)\n{\n\tif(DoomCraftEvent(ev))return true;')
p=ENGINE/'src/common/platform/win32/i_input.cpp';s=p.read_text()
if '#include "../../../doomcraft/doomcraft.h"' not in s:p.write_text('#include "../../../doomcraft/doomcraft.h"\n'+s,encoding='utf8')
replace(Path('src/common/platform/win32/i_input.cpp'),'if (GUICapture && GUIWndProcHook(hWnd, message, wParam, lParam, &result))','''if (!GUICapture && (message==WM_KEYDOWN || message==WM_KEYUP || message==WM_SYSKEYDOWN || message==WM_SYSKEYUP) && DoomCraftNativeKey((int)wParam,message==WM_KEYDOWN || message==WM_SYSKEYDOWN,(lParam & 0x40000000)!=0)) return 0;
	if (GUICapture && GUIWndProcHook(hWnd, message, wParam, lParam, &result))''')
replace(Path('src/playsim/p_user.cpp'),'#include "doomstat.h"','#include "doomstat.h"\n#include "doomcraft/doomcraft.h"')
replace(Path('src/playsim/p_interaction.cpp'),'#include "doomstat.h"','#include "doomstat.h"\n#include "doomcraft/doomcraft.h"')
p=ENGINE/'src/playsim/p_interaction.cpp'
s=p.read_text(encoding='utf8')
p.write_text(''.join(line for line in s.splitlines(keepends=True) if 'damage=DoomCraftArmorDamage(target,damage);' not in line and 'damage=DoomCraftTotemDamage(target,damage,' not in line),encoding='utf8')
replace(Path('src/playsim/p_interaction.cpp'),'\t\tbool compat_voodoo_zombie = target->Level->i_compatflags2 & COMPATF2_VOODOO_ZOMBIES;','''\t\tif (!(flags & (DMG_FORCED | DMG_NO_ARMOR)) && !telefragDamage)damage=DoomCraftArmorDamage(target,damage);
\t\tdamage=DoomCraftTotemDamage(target,damage,(flags & DMG_FORCED)!=0 || telefragDamage);
\t\tbool compat_voodoo_zombie = target->Level->i_compatflags2 & COMPATF2_VOODOO_ZOMBIES;''')
import re
p=ENGINE/'src/playsim/p_user.cpp';s=p.read_text()
s=re.sub(r'\n    if\(player==&players\[consoleplayer\] && DoomCraftMovement\(\).*?    // DoomCraft player hook end\n', '\n', s, flags=re.S) if '// DoomCraft player hook end' in s else s
s=re.sub(r'\n    if\(player==&players\[consoleplayer\] && DoomCraftMovement\(\).*?else if\(player==&players\[consoleplayer\]\) player->mo->flags &= ~MF_NOGRAVITY;\n', '\n', s, flags=re.S)
p.write_text(s,encoding='utf8')
replace(Path('src/playsim/p_user.cpp'),'ticcmd_t *cmd = &player->cmd;','''ticcmd_t *cmd = &player->cmd;
    if(player==&players[consoleplayer] && DoomCraftMovement() && player->playerstate==PST_LIVE) {
        cmd->ucmd.forwardmove=cmd->ucmd.sidemove=cmd->ucmd.upmove=0;
        cmd->ucmd.buttons &= ~BT_JUMP;
        player->mo->flags |= MF_NOGRAVITY;
        player->mo->Vel=DVector3(0,0,0);
        player->mo->radius=9.6;
    } else if(player==&players[consoleplayer]) {
        player->mo->flags &= ~MF_NOGRAVITY;
        if(DoomCraftRespawnRequested()) cmd->ucmd.buttons |= BT_USE;
    }
    // DoomCraft player hook end
''')
