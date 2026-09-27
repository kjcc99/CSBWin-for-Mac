cmake_minimum_required(VERSION 3.16)
project(CSBwin CXX)

# SDL2 build of CSBwin for macOS (and other POSIX systems).
# The Windows build still uses CSBWin.sln.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(SDL2 REQUIRED CONFIG)

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

add_executable(CSBwin ${GAME_SOURCES} ${PLATFORM_SOURCES})
target_include_directories(CSBwin PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(CSBwin PRIVATE _LINUX SDL20)
target_compile_options(CSBwin PRIVATE
   -fsigned-char
   -Wno-everything)
target_link_libraries(CSBwin PRIVATE SDL2::SDL2)
if(APPLE)
   # The game keeps #pragma pack(1) structs (inherited from the 68000 original)
   # containing statically initialized pointers at unaligned offsets.  arm64
   # handles unaligned loads fine, but chained fixups require aligned pointers.
   target_link_options(CSBwin PRIVATE -Wl,-no_fixup_chains)
endif()

# Like CSBwin.exe on Windows, the program looks for config.txt, hcsb.dat etc.
# in its own folder, so place a copy next to the game data.
add_custom_command(TARGET CSBwin POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:CSBwin> ${CMAKE_SOURCE_DIR}/Game/CSBwin)
