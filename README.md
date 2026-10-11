# DoomCraft

Real Minecraft Java gameplay inside classic Doom and Doom II on Windows.
Minecraft runs alongside a patched GZDoom engine: Minecraft renders your
hands, inventory, blocks and HUD; Doom supplies its maps, enemies and combat.

This is the source snapshot of the locally tested October 10, 2026 prototype.
It requires building the engine and Minecraft bridge. There is no packaged
one-click download yet. You must own Minecraft Java and supply Doom/Doom II
IWADs. No game files, worlds, account data or authentication files are included.

## Features

- Minecraft movement, hotbar bindings, inventory, offhand, Creative flight and elytra.
- Building and mining alongside Doom enemies and maps.
- Bow, spear, pearl, TNT and explosive crossbow integration.
- Splash and lingering potions stop at Doom geometry; Minecraft blocks retain vanilla collision.
- Boss travel for Doom and Doom II with fresh arena reloads after death.
- Minecraft armor reduces Doom damage. Held totems prevent ordinary lethal hits.
- Golden apple, enchanted apple and totem absorption absorbs damage before red hearts and expires with Minecraft's effects.
- Separate local instances, configurations and desktop launchers for both Doom games.

See [CONTROLS.md](CONTROLS.md) for controls and gameplay limits.

## Build and setup

Use Windows x64, Visual Studio 2022 C++ tools and Windows SDK, Git, Python 3,
CMake 3.31 or newer, and JDK 25. Install Minecraft Java 26.3 through the official
Launcher, then Fabric Loader 0.19.5 with the [official Fabric installer](https://fabricmc.net/use/installer/).
Download [Fabric API](https://modrinth.com/mod/fabric-api) version 0.161.0+26.3.

Run these commands from this repository in a Developer PowerShell for VS2022:

```powershell
New-Item -ItemType Directory -Force local
git clone --branch g4.14.2 https://github.com/ZDoom/gzdoom.git local/gzdoom-source
git -C local/gzdoom-source checkout 99aa489d09015a95bb78df2b30ede29f328cc874
python patch-engine.py
cmake -S local/gzdoom-source -B local/gzdoom-build -G "Visual Studio 17 2022" -A x64
cmake --build local/gzdoom-build --config Release --parallel
.\mc\gradlew.bat -p mc build
```

Obtain the Windows x64 portable resources for **GZDoom 4.14.2** from its
[official release](https://github.com/ZDoom/gzdoom/releases/tag/g4.14.2).
Extract them so `local/gzdoom-stock/gzdoom.exe` exists. `install-engine.py`
copies the stock runtime dependencies/resources and replaces the executable
with your custom build. The engine's build may need its usual dependency
configuration for your VS installation; consult upstream GZDoom build docs.

```powershell
python install-engine.py
python setup-minecraft.py --fabric-api "C:\Downloads\fabric-api-0.161.0+26.3.jar"
.\create-shortcuts.ps1
```

Open Minecraft Launcher, select **Minecraft x Doom**, click Play and leave it
running. Then use a desktop shortcut, or:

```powershell
.\launch.ps1 -Game Doom -TestMap -WadPath "D:\Games\DOOM.WAD"
.\launch.ps1 -Game DoomII -TestMap -WadPath "D:\Games\DOOM2.WAD"
```

For shortcuts with a non-default Steam path, set user environment variables
`DOOMCRAFT_DOOM_WAD` and `DOOMCRAFT_DOOM2_WAD` to your IWAD files. Otherwise the
launcher checks Steam's default Ultimate Doom installation. Doom II has its
own configuration and saves. Only one GZDoom session should run at a time.

## Verified scope and limitations

Built and played with GZDoom 4.14.2, Minecraft 26.3, Fabric Loader 0.19.5,
Fabric API 0.161.0+26.3, Java 25, Loom 1.18.3 and Gradle 9.7.1. Live checks
covered movement, inventory, opposite-direction Lunge, bows/pearls, boss
retry, death/restart, armor, held totems and temporary absorption. The final
potion fix was checked with splash and lingering bottles against a Doom wall
and a splash bottle against a placed glass block.

Two running games add input latency. Voxel wall proxies are approximate,
while Doom resolves host collisions. Full campaign/save-load stability and
multiplayer are untested. Armor uses a bridge-specific reduction; full
Minecraft enchantment, shield, potion effects on Doom monsters and item
parity are not implemented. A prior Doom II session intermittently rendered
a black host scene; complete Doom II rendering stability is not established.

## License and credits

Native engine integration: **GPL-3.0-or-later**, see [LICENSE](LICENSE).
Minecraft passthrough foundation and shared transport: **MIT**, see
[THIRD_PARTY.md](THIRD_PARTY.md) and [the preserved MIT notice](licenses/MIT-passthrough.txt).
Credits to Rehan/universal-modder, AZP3001's passthrough port, GZDoom's
contributors, Fabric and the DoomCraft contributors. Minecraft and Doom
assets retain their owners' rights and are separate runtime dependencies.
