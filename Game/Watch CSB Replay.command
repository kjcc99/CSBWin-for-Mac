#!/bin/sh
# macOS counterpart of "Watch CSB Replay.bat" (double-click in Finder)
cd "$(dirname "$0")" && exec ./CSBwin directory=CSB play=Playfile.replay
