# Wild Hearts Lag Fix

Wild Hearts notoriously lags very heavily on PC due to a poorly optimized port.

After having done some profiling, we found that the issue primarily stems from windows priority boost 
causing issues across the game threads.

In short by simply adding some small fixes on top of windows' default `dxgi.dll` we have managed to
drastically smooth out frame-time stutters/increase fps (without touching any game code at all).

Originally discovered by Angrevol.

## Install

1. Download **`dxgi.dll`** from the [latest release](../../releases/latest).
2. Put it in the folder that holds the game's `.exe`, next to it. For a default Steam
   install that is:

   ```
   C:\Program Files (x86)\Steam\steamapps\common\WILD HEARTS\00_game\target_origin\ex
   ```

   (Otherwise in Steam, right-click **WILD HEARTS → Manage → Browse local
   files**, then look for the folder with the wild hearts `.exe`)
3. Launch the game and it should automatically use the new `dxgi.dll` with the changes.

**To uninstall:** delete `dxgi.dll` from that folder.

## What The .dll Does

This dll:

- runs the main game process at **High** priority
- runs the render thread at *Highest* priority
- **disables** dynamic priority boosting for every thread except the render thread

It changes no game files and doesn't require admin rights on your pc.

## Building from source

For those who want to build the dll yourself, we've included build scripts for windows powrshell/wsl/linux.

Source lives in `src/`, with build output ending up in `build/`. 

You need [LLVM](https://releases.llvm.org/download.html) (`clang-cl`, `lld-link`, `llvm-dlltool`).

**Windows** — PowerShell:

```powershell
.\build.ps1
```

**Linux or WSL:**

```sh
./build.sh      # then choose 1 for Linux, 2 for WSL
```

Both produce `build/dxgi.dll`. Pass `-LLVMPath` / `LLVM=` if your toolchain is not in
the usual place.
