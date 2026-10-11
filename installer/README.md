# Windows installer

Inno Setup supplies the wizard, per-user installation, shortcuts and uninstaller.
Our PowerShell glue detects Steam libraries/IWADs, verifies pinned downloads,
installs official Fabric profile metadata/libraries and configures an isolated
Minecraft instance. The official Minecraft Launcher handles licensed client and
Java downloads and login. No Python, Gradle, compiler or standalone JDK is needed
by an end user. Missing games must be installed by their owner; setup supports
manual path selection and links to the official Minecraft download.

## Reuse review

Reviewed jrsoftware/issrc (5,698 stars, checked October 10, 2026), Inno Setup
6.4.3, tag commit fdc1d79e2443a4faa2698b8119da92662e1cbc57. Reviewed the
license, compiler build entry point, basic file/shortcut example and relevant
installation/source paths. Its permissive Inno Setup license is retained in
licenses/Inno-Setup.txt. No concerning behavior found in the reviewed paths;
this was a targeted review, not a complete audit.

The portable compiler was obtained from the Tools.InnoSetup 6.4.3 NuGet
redistribution. Package SHA-256:
9d67a2b1155ca5bc825475ceb907aa357c5feb42e474cfd5347eddeef5ad92a5.
ISCC.exe and ISCmplr.dll have valid Pyrsys B.V. Authenticode signatures.
ISCC.exe SHA-256: 8a430b74fd1ecef5651f514a62b5341b0c65e1a53ffe5ba33cc26cf57e67029f.
ISCmplr.dll SHA-256: 2c230c3e99bc42215221b2c847356fbbe9eae33a15a842be429e94fd26e7c436.
The package is a third-party redistribution, not an independent reproducible
verification of every compiler component. It is used only to build this setup.

Fabric installation follows the official FabricMC/fabric-installer
ClientInstaller flow: official profile JSON and Maven libraries. The pinned
profile and all artifact URLs/SHA-256 values are checked into this directory.
GZDoom DLLs and sound banks are fetched from the official g4.14.2 Windows release;
the stock archive hash records the reviewed downloaded artifact rather than an
independent upstream checksum. The release contains our patched engine, bridge,
engine resources and corresponding patched GZDoom source archive. Proprietary
IWADs/Minecraft files and personal data are excluded using explicit file lists.

## Build

Build the native engine and bridge using the root instructions first. Stage with:

```powershell
python installer/prepare.py --runtime local/runtime --guest mc/build/libs/passthrough-0.1.0-doom.jar --engine-source local/gzdoom-source
powershell -NoProfile -ExecutionPolicy Bypass -File installer/test-setup.ps1
ISCC.exe installer/doomcraft.iss
```

Artifacts appear in local/release. prepare.py replaces only local/installer-payload
with an explicit clean staging tree. The setup test uses a fresh TEMP directory
and fictitious launcher metadata; it does not change the player's active profile.
For extraction-only smoke tests run the built EXE with /VERYSILENT /SMOKETEST=1
and an explicit temporary /DIR. This suppresses shortcut and uninstall registry
creation; silent mode skips game setup. Never use silent smoke mode for user
installation. The normal wizard runs game setup at Finish.

Setup has been tested with Windows PowerShell 5.1, a secondary Steam library,
IWAD header validation, fresh/repeated Fabric downloads, launcher metadata
preservation/backups, keybind import and option preservation on reinstall.
It has not been tested on a completely fresh Windows machine without Minecraft.
