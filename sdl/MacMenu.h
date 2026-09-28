// MacMenu.h : the macOS menu bar, the counterpart of the Windows menu in CSBwin.rc.
#pragma once

#include <SDL.h>

// Menu items not in the Windows menu (the others use the ids in resource.h).
enum
{
   IDM_FULLSCREEN = 33000
};

// MacMenu.mm: adds our menus to the one SDL creates.  Call after SDL_Init.
// Choosing an item posts an SDL event of type MacMenu_EventType() with the
// item's id in user.code, so the game sees it from the event loop and never
// from inside a message box's modal loop.
void MacMenu_Create();
Uint32 MacMenu_EventType();

// SDLMain.cpp: returns whether the item is enabled and sets 'checked'.
bool MacMenu_ItemState(int id, bool &checked);
