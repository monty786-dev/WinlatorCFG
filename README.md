# Winlator Configuration (modern winecfg replacement)

A single-file C (Win32) program that replaces Wine's `winecfg.exe`, with a modern look
(visual styles, gradient header, tabs) and extra Winlator-focused features.

Tabs: Applications · Graphics · Libraries · Audio / Drives · Winlator Tweaks · About / Update

New vs. stock winecfg: DXVK / VKD3D-Proton one-click presets, renderer + video memory + CSMT,
virtual desktop & DPI, mouse-capture / borderless / focus fixes, crash-dialog and Mono/Gecko
prompt suppression, quick tools, **self-update from GitHub Releases**, and `/install` mode.

## Build on GitHub (no PC tools needed)
1. Create a new GitHub repo, upload ALL files in this folder (keep `.github/workflows/build.yml`).
2. Edit `winecfg.c`: set `UPDATE_REPO` to `your-user/your-repo` and `APP_VERSION`.
3. Commit. Open the **Actions** tab -> "Build winecfg.exe" -> open the run -> download artifact **winecfg**
   (contains `winecfg-x64.exe` and `winecfg-x86.exe`).
4. To publish a release (needed for the in-app updater): Releases -> create tag `v1.0.0`
   (or `git tag v1.0.0 && git push --tags`). The workflow attaches both .exe files automatically.

## Install in Winlator
1. Copy the .exe files to your phone (Downloads).
2. In Winlator: container settings -> open the container's drive C: (or use File Manager) and place
   `winecfg-x64.exe` anywhere, e.g. `C:\`.
3. Start it from Winlator (Run -> choose file) with argument `/install`
   (or open it and press **Install as default winecfg**). This backs up the original to
   `system32\winecfg.exe.orig` and replaces `system32\winecfg.exe`.
   For 32-bit prefixes/WoW64, also run `winecfg-x86.exe /install` under `C:\windows\syswow64` (32-bit wine).
4. Winlator's "Wine Configuration" now opens the new interface. `winecfg-x64.exe /uninstall` restores the original.

## Local build
`sudo apt install gcc-mingw-w64 binutils-mingw-w64 && make`
