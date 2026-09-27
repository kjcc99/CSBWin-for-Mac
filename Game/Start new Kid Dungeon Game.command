#!/bin/sh
# macOS counterpart of "Start new Kid Dungeon Game.bat" (double-click in Finder)
cd "$(dirname "$0")" && exec ./CSBwin directory=DM dungeon=dungeon-kid.dat
