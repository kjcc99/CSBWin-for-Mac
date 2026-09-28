cmake_minimum_required(VERSION 3.16)

# SDL2 build of CSBwin for macOS (and other POSIX systems).
# The Windows build still uses CSBWin.sln.
#
# -DCSBWIN_APP=ON builds a self-contained CSBwin.app instead of a bare
# executable: SDL2 is built from source and linked statically, so the app
# runs on any Apple Silicon Mac with macOS 11 or later.
option(CSBWIN_APP "Build a self-contained CSBwin.app (macOS)" OFF)
if(CSBWIN_APP)
   set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0 CACHE STRING "")
   set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "")
endif()

project(CSBwin C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if(CSBWIN_APP)
   # Keep the builder's folder names (e.g. /Users/<name>) out of the app,
   # which would otherwise contain them in assert messages and debug info.
   add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=. -ffile-prefix-map=${CMAKE_BINARY_DIR}=build)
   include(FetchContent)
   set(SDL_SHARED OFF CACHE BOOL "" FORCE)
   set(SDL_STATIC ON CACHE BOOL "" FORCE)
   set(SDL_TEST OFF CACHE BOOL "" FORCE)
   FetchContent_Declare(SDL2
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-2.32.10
      GIT_SHALLOW TRUE)
   FetchContent_MakeAvailable(SDL2)
   set(SDL_LIBRARY SDL2::SDL2-static)
else()
   find_package(SDL2 REQUIRED CONFIG)
   set(SDL_LIBRARY SDL2::SDL2)
endif()

set(GAME_SOURCES
   AsciiDump.cpp Attack.cpp Bitmaps.cpp Chaos.cpp Character.cpp
   Code11f52.cpp Code13ea4.cpp Code17818.cpp Code1f9e6.cpp Code222ea.cpp
   Code390e.cpp Code51a4.cpp Codea59a.cpp CSBCode.cpp CSBUI.cpp data.cpp
   DSA.cpp Graphics.cpp Hint.cpp Magic.cpp md5C.cpp Menu.cpp Monster.cpp
   Mouse.cpp MoveObject.cpp NewBugs.cpp OldBugs.cpp RC4.cpp Recording.cpp
   SaveGame.cpp SmartDiscard.cpp Sound.cpp Statistics.cpp Stdafx.cpp
   system.cpp Timer.cpp Translation.cpp utility.cpp VBL.cpp Viewport.cpp
   WinScreen.cpp)

set(PLATFORM_SOURCES
   sdl/SDLMain.cpp
   sdl/SDLUI.cpp
   sdl/SDLSound.cpp)
if(APPLE)
   # The menu bar.  Compiled as CXX: clang sees Objective-C++ from the .mm
   # extension, and enabling OBJCXX would pass C++ flags to SDL's .m files.
   set_source_files_properties(sdl/MacMenu.mm PROPERTIES LANGUAGE CXX)
   list(APPEND PLATFORM_SOURCES sdl/MacMenu.mm)
endif()

add_executable(CSBwin ${GAME_SOURCES} ${PLATFORM_SOURCES})
target_include_directories(CSBwin PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(CSBwin PRIVATE _LINUX SDL20)
target_compile_options(CSBwin PRIVATE
   -fsigned-char
   -Wno-everything)
target_link_libraries(CSBwin PRIVATE ${SDL_LIBRARY})
if(APPLE)
   target_link_libraries(CSBwin PRIVATE "-framework Cocoa")
   # The game keeps #pragma pack(1) structs (inherited from the 68000 original)
   # containing statically initialized pointers at unaligned offsets.  arm64
   # handles unaligned loads fine, but chained fixups require aligned pointers.
   target_link_options(CSBwin PRIVATE -Wl,-no_fixup_chains)
endif()

if(CSBWIN_APP)
   # Only the game data that ships in the repository; saved games in the
   # developer's Game folder stay out of the app.
   set(GAME_DATA
      config.txt hcsb.dat hcsb.hct mini.dat
      DM/dungeon.dat DM/Dungeon-Kid.dat DM/graphics.dat DM/Playfile.replay
      CSB/Dungeon.dat CSB/csbgame2.dat CSB/graphics.dat CSB/Playfile.replay)
   foreach(file ${GAME_DATA})
      get_filename_component(folder ${file} DIRECTORY)
      set_source_files_properties(Game/${file} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/${folder})
      target_sources(CSBwin PRIVATE Game/${file})
   endforeach()
   set_source_files_properties(macos/CSBwin.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
   target_sources(CSBwin PRIVATE macos/CSBwin.icns)
   set_target_properties(CSBwin PROPERTIES
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/macos/Info.plist.in)
   # The app opens with a SwiftUI launcher, which runs the game.
   # (CMake only compiles Swift with the Ninja and Xcode generators.)
   set(LAUNCHER ${CMAKE_BINARY_DIR}/CSBwinLauncher)
   add_custom_command(OUTPUT ${LAUNCHER}
      COMMAND xcrun swiftc -O -target arm64-apple-macos${CMAKE_OSX_DEPLOYMENT_TARGET}
              -parse-as-library ${CMAKE_SOURCE_DIR}/macos/Launcher.swift -o ${LAUNCHER}
      DEPENDS ${CMAKE_SOURCE_DIR}/macos/Launcher.swift)
   # Apple Silicon refuses to run code without at least an ad-hoc signature.
   # Sign on every build, since changed resources are copied without relinking.
   add_custom_target(app ALL
      COMMAND ${CMAKE_COMMAND} -E copy ${LAUNCHER} $<TARGET_BUNDLE_CONTENT_DIR:CSBwin>/MacOS/
      COMMAND codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:CSBwin>
      DEPENDS CSBwin ${LAUNCHER})
else()
   # Like CSBwin.exe on Windows, the program looks for config.txt, hcsb.dat etc.
   # in its own folder, so place a copy next to the game data.
   add_custom_command(TARGET CSBwin POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:CSBwin> ${CMAKE_SOURCE_DIR}/Game/CSBwin)
endif()
