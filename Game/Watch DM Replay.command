#!/bin/sh
# macOS counterpart of "Watch DM Replay.bat" (double-click in Finder)
cd "$(dirname "$0")" && exec ./CSBwin directory=DM play=Playfile.replay
