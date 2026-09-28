# CSBwin for Mac

A native macOS app for **Dungeon Master** and **Chaos Strikes Back**, built on
[CSBwin](https://github.com/BeipDev/CSBWin), the faithful PC recreation of the
Atari ST originals.

The game data ships inside the app, so there is nothing else to download.

## Features

- **One app, three games:** Dungeon Master, Chaos Strikes Back and the small
  Kid Dungeon, plus the recorded replays of both games.
- **A launcher** with Play, Settings and Controls tabs:
  - **Settings:** game speed, volume, window size and starting in full screen.
  - **Controls:** rebind the adventuring keys (movement, attacks, runes,
    spell casting) with a primary and an alternate key for each. Choosing a
    key that's already in use takes it away from the other action and tells
    you which one.
- **A Mac menu bar in the game** with the options of the Windows version's
  menu: speed, volume, DM Rules, recording and playback, items remaining,
  statistics, aspect ratio and full screen.
- Sharp scaling on Retina displays, a 4:3 or original 16:10 aspect ratio, and
  borderless full screen.
- Saves and settings are kept in `~/Library/Application Support/CSBwin`, so
  they survive replacing the app. **Settings › Show in Finder** opens that folder.

## Requirements

- A Mac with Apple Silicon (M1 or later)
- macOS 11 Big Sur or later

## Opening the app the first time

The app isn't notarized by Apple, so macOS blocks it the first time you open
a copy that was downloaded or sent to you:

1. Open **CSBwin.app**. When macOS says it can't be opened, click **Done**.
2. Open **System Settings › Privacy & Security**, scroll down, and click
   **Open Anyway** next to the message about CSBwin.
3. Confirm with **Open Anyway** and your password.

After that it opens normally. An app you built yourself isn't blocked.

## Building

You need the Xcode Command Line Tools (`xcode-select --install`) and
[CMake](https://cmake.org) 3.16 or later, e.g. `brew install cmake`.

```sh
cmake -S . -B build-app -DCSBWIN_APP=ON
cmake --build build-app
open build-app/CSBwin.app
```

The first build downloads SDL2 and builds it from source, so it takes a few
minutes. SDL is linked into the app, which then runs on any Apple Silicon Mac
without anything else installed.

### Without the app

For development there's also a bare build, which uses Homebrew's SDL2 and
puts the program next to the game data in `Game/`:

```sh
brew install sdl2 cmake
cmake -S . -B build
cmake --build build
cd Game && ./CSBwin directory=DM
```

The `.command` files in `Game/` start the games and replays this way. The
command-line options are the same as on Windows (`directory=`, `dungeon=`,
`play=`, `speed=`, `size=full`, `record`, …); `./CSBwin --help` lists them.

## Playing

| Keys | |
|---|---|
| ⌘1 … ⌘7 | Game speed, Glacial … Quick as a Bunny |
| ⌘E | Extra ticks |
| ⌘I | Statistics |
| ⌘A | 4:3 aspect ratio |
| ⌘F or F11 | Full screen |
| ⌘/ | Help |
| ⌘Q | Quit |
| Control-click | Right click |

The keys for moving and fighting are in the launcher's **Controls** tab.

Starting a new Chaos Strikes Back game takes a few steps (choose champions in
the prison, then make a new adventure in the Utility); the **Play** tab lists them.

## How it's built

- `sdl/`: the platform layer (window, input, sound, dialogs and the Mac menu
  bar), the SDL2 counterpart of `CSBwin.cpp`, the Windows version's platform code.
- `macos/`: the SwiftUI launcher, the icon and `Info.plist`.
- `CMakeLists.txt`: the Mac build. The Windows build still uses `CSBWin.sln`.

The launcher is the app's main program. It starts the game as a separate
process and writes the key bindings to `config.txt` in the user's folder,
which the game reads instead of the one inside the app.

## Credits

- **Dungeon Master** and **Chaos Strikes Back** by FTL Games.
- **CSBwin** by Paul R. Stevens, maintained at
  [BeipDev/CSBWin](https://github.com/BeipDev/CSBWin).
- Built with [SDL2](https://www.libsdl.org).

This repository is a fork of BeipDev/CSBWin with the macOS port added. The
original README, with the Linux notes, is in [`README`](README).

## License

The files this port adds (`sdl/`, `macos/`, `CMakeLists.txt`, this README and
the `.command` files in `Game/`) are under the MIT License; see
[`LICENSE`](LICENSE). CSBwin itself and the game data belong to their authors
and aren't covered by it.
