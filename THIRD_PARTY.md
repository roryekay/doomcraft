# Attribution and licenses

The native GZDoom adapter and engine patch are GPL-3.0-or-later (LICENSE).
GZDoom is fetched separately from ZDoom/gzdoom at g4.14.2,
commit 99aa489d09015a95bb78df2b30ede29f328cc874. Preserve its bundled notices
when distributing a built engine. The source tree has no game binaries; the
Windows release bundles the patched GZDoom engine/resources with corresponding
source, and downloads upstream sound dependencies with their notices.

The Minecraft passthrough foundation is by Rehan and universal-modder
contributors, adapted through AZP3001/rdr2-passthrough commit
37c3db58ffc5a196123c0788feaba75fcaf6694e. Its Fabric descriptor explicitly
declares MIT. Preserve licenses/MIT-passthrough.txt. AZP3001's repository has
no separate repository-wide LICENSE; the MIT declaration applies to the
Minecraft mod, and is not used to license unrelated assets from that repo.

The native ws.cpp/ws.h transport matches the MIT originals in
rehan-remade/universal-modder at commit
550651675153070cce68066e07c3c8f95df86d08, under
examples/minecraft-gta5-passthrough/gta/src (before our payload limit and
shutdown fixes). That upstream LICENSE is included unchanged.

The Gradle wrapper is from Gradle, Apache-2.0. Fabric Loader, Fabric API,
Fabric Loom and Java-WebSocket retain their upstream licenses and notices.
Minecraft, Doom and Doom II are separate runtime dependencies. Their game
data, assets, authentication data and executables are not included.

Windows setup uses Inno Setup 6.4.3 by Jordan Russell and Martijn Laan under
its permissive license (licenses/Inno-Setup.txt). See installer/README.md for
the pinned compiler, provenance checks and targeted reuse review.
