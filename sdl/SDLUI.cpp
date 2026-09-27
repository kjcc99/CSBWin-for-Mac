// SDLUI.cpp : SDL implementations of the UI_* services that CSBUI.cpp
// implements with Win32 calls on Windows.

#include "stdafx.h"
#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"

#include <chrono>

SDL_Window *GameWindow(); // SDLMain.cpp

i32 UI_MessageBox(const char *msg, const char *title, i32 flags)
{
   if(title == NULL)
      title = "Error";

   SDL_MessageBoxButtonData okButtons[] = {
       {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, MESSAGE_IDOK, "OK"},
   };
   SDL_MessageBoxButtonData yesNoButtons[] = {
       {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, MESSAGE_IDNO, "No"},
       {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, MESSAGE_IDYES, "Yes"},
   };
   bool yesNo = (flags & MESSAGE_YESNO) != 0;

   SDL_MessageBoxData data{};
   data.flags = (flags & MESSAGE_ICONERROR) ? SDL_MESSAGEBOX_ERROR : (flags & MESSAGE_ICONWARNING) ? SDL_MESSAGEBOX_WARNING : SDL_MESSAGEBOX_INFORMATION;
   data.window = GameWindow();
   data.title = title;
   data.message = msg;
   data.numbuttons = yesNo ? 2 : 1;
   data.buttons = yesNo ? yesNoButtons : okButtons;

   int wasShowing = SDL_ShowCursor(SDL_QUERY);
   SDL_ShowCursor(SDL_ENABLE);
   i32 mask = UI_DisableAllMessages();
   int buttonId = yesNo ? MESSAGE_IDNO : MESSAGE_IDOK;
   if(SDL_ShowMessageBox(&data, &buttonId) < 0)
      fprintf(stderr, "%s: %s\n", title, msg);
   UI_EnableMessages(mask);
   SDL_ShowCursor(wasShowing);
   return buttonId;
}

// Used for the game information text and listings such as Items Remaining.
// There is no edit box here, so the text is shown read-only and returned unchanged.
i32 EditDialog::DoModal()
{
   const char *text = m_initialText ? m_initialText : "";
   std::string display;
   for(const char *p = text; *p; p++)
   {
      if(*p != '\r')
         display += *p;
   }
   printf("%s\n", display.c_str());
   UI_MessageBox(display.c_str(), "CSBwin", MESSAGE_OK);
   m_finalText = (char *)UI_malloc(i32(strlen(text) + 1), MALLOC012);
   if(m_finalText != NULL)
      strcpy(m_finalText, text);
   return 0;
}

i64 UI_GetSystemTime()
{
   // Cumulative milliseconds
   using namespace std::chrono;
   return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
