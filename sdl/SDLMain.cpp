// SDLMain.cpp : SDL2 entry point, window and event loop.
// This is the SDL counterpart of CSBwin.cpp (Win32) and plays the same role:
// it turns platform events into CSB_UI_MESSAGEs for CSBUI().

#include "stdafx.h"
#include "UI.h"
#include "Dispatch.h"
#include "CSB.h"
#include "Data.h"
#include "resource.h"
#include <unistd.h>
#ifdef __APPLE__
#include "MacMenu.h"
#endif

void display();
void ForceScreenDraw();
void Cleanup(bool programTermination);
void SDLSound_Initialize();
void SDLSound_Shutdown();

extern bool BeginRecordOK;
extern bool ItemsRemainingOK;
extern bool PlayfileIsOpen();
extern bool RecordMenuOption;
extern i32 NoSpeedLimit;
extern i32 GameMode;
extern unsigned char *encipheredDataFile;
extern bool simpleEncipher;
extern RECT g_rcClient;

i32 trace = -1;
CSB_UI_MESSAGE csbMessage;
bool overlappingText = false;
ui32 TImER = 0;
char szCSBVersion[MAX_LOADSTRING] = "CSB for Windows/Linux Version 15.9";

const char *helpMessage =
    "CSBwin looks in three places for files:\n"
    " 1) The directory given by directory=<dir>\n"
    " 2) The parent of that directory\n"
    " 3) The directory containing the CSBwin program\n"
    "      Searched in order 1, 2, 3\n\n"
    "Keyboard shortcuts (Mac):\n"
    "  Cmd+F or F11   Toggle fullscreen\n"
    "  Cmd+A          Toggle 4x3 aspect ratio\n"
    "  Cmd+1 .. Cmd+7 Game speed (Glacial .. Quick as a Bunny)\n"
    "  Cmd+E          Toggle extra ticks\n"
    "  Cmd+I          Statistics\n"
    "  Cmd+/          This help\n"
    "  Cmd+Q          Quit";

i32 WindowWidth = 960;
i32 WindowHeight = 0;
i32 WindowX = 0;
i32 WindowY = 0;
bool fullscreenRequested = false;

POINT g_aspectRatio{320, 240};

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture *g_texture;
static bool g_paintPending = true;
static bool g_quit = false;
static bool g_cursorIsShowing = true;

void MTRACE(const char *msg)
{
   if(trace < 0)
      return;
   FILE *f = GETFILE(trace);
   fputs(msg, f);
   fflush(f);
}

// Pass a message to the game.  The game asks us to quit by returning
// something other than UI_STATUS_NORMAL.
static void SendToGame(MTYPE type, i32 p1 = 0, i32 p2 = 0)
{
   if(g_quit)
      return;
   csbMessage.type = type;
   csbMessage.p1 = p1;
   csbMessage.p2 = p2;
   if(CSBUI(&csbMessage) != UI_STATUS_NORMAL)
      g_quit = true;
}

static void SetOption(i32 option, i32 value = 0)
{
   SendToGame(UIM_SETOPTION, option, value);
}

SDL_Window *GameWindow()
{
   return g_window;
}

void UI_Invalidate(bool /*erase*/)
{
   g_paintPending = true;
}

void LIN_Invalidate()
{
   g_paintPending = true;
}

// The largest rectangle with the given aspect ratio that fits centered in rcArea.
static RECT TouchFromInside(const RECT &rcArea, POINT size)
{
   i32 areaWidth = rcArea.right - rcArea.left;
   i32 areaHeight = rcArea.bottom - rcArea.top;
   float ratio = std::min(areaWidth / float(size.x), areaHeight / float(size.y));
   i32 width = i32(size.x * ratio);
   i32 height = i32(size.y * ratio);
   i32 left = rcArea.left + (areaWidth - width) / 2;
   i32 top = rcArea.top + (areaHeight - height) / 2;
   return RECT{left, top, left + width, top + height};
}

static void UpdateClientRect()
{
   int width, height;
   SDL_GetRendererOutputSize(g_renderer, &width, &height);
   g_rcClient = TouchFromInside(RECT{0, 0, width, height}, g_aspectRatio);
}

// Window coordinates (points) to the 320x200 Atari screen.
static POINT WindowToAtari(i32 x, i32 y)
{
   int windowWidth, windowHeight, pixelWidth, pixelHeight;
   SDL_GetWindowSize(g_window, &windowWidth, &windowHeight);
   SDL_GetRendererOutputSize(g_renderer, &pixelWidth, &pixelHeight);
   float px = x * float(pixelWidth) / std::max(windowWidth, 1);
   float py = y * float(pixelHeight) / std::max(windowHeight, 1);
   i32 clientWidth = std::max(g_rcClient.right - g_rcClient.left, 1);
   i32 clientHeight = std::max(g_rcClient.bottom - g_rcClient.top, 1);
   return POINT{i32((px - g_rcClient.left) * g_rcAtari.right / clientWidth),
                i32((py - g_rcClient.top) * g_rcAtari.bottom / clientHeight)};
}

static bool IsInsideAtariScreen(POINT point)
{
   return point.x >= 0 && point.y >= 0 && point.x < g_rcAtari.right && point.y < g_rcAtari.bottom;
}

// The mouse position on the Atari screen.  Mouse events are not used for this
// because sdl2-compat reports their coordinates in pixels on HiDPI displays,
// while SDL_GetMouseState reports points.
static POINT MouseToAtari()
{
   int mouseX, mouseY;
   SDL_GetMouseState(&mouseX, &mouseY);
   return WindowToAtari(mouseX, mouseY);
}

void UI_GetCursorPos(i32 *x, i32 *y)
{
   POINT point = MouseToAtari();
   *x = point.x;
   *y = point.y;
}

static void RenderWindow()
{
   SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
   SDL_RenderClear(g_renderer);
   SDL_Rect dst{g_rcClient.left, g_rcClient.top, g_rcClient.right - g_rcClient.left, g_rcClient.bottom - g_rcClient.top};
   SDL_RenderCopy(g_renderer, g_texture, nullptr, &dst);
   SDL_RenderPresent(g_renderer);
}

// Called by display() (WinScreen.cpp) whenever the 320x200 image changed.
void PresentScreen(const ui32 *bitmap)
{
   SDL_UpdateTexture(g_texture, nullptr, bitmap, g_rcAtari.right * sizeof(ui32));
   RenderWindow();
}

static void ToggleFullscreen()
{
   bool fullscreen = (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
   SDL_SetWindowFullscreen(g_window, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

static void ToggleAspectRatio()
{
   g_aspectRatio = (g_aspectRatio.y == 240) ? POINT{320, 200} : POINT{320, 240};
   UpdateClientRect();
   RenderWindow();
}

static void ShowCursorIfNeeded(bool inside)
{
   // During play the game draws its own cursor, so hide ours over the game area.
   bool wantCursor = !(GameMode == 1 && inside);
   if(wantCursor != g_cursorIsShowing)
   {
      SDL_ShowCursor(wantCursor ? SDL_ENABLE : SDL_DISABLE);
      g_cursorIsShowing = wantCursor;
   }
}

// Translate an SDL key to the Win32 virtual key code used by config.txt (mscan).
static i32 VirtualKey(SDL_Keycode key, SDL_Scancode scancode)
{
   if(scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
      return 'A' + (scancode - SDL_SCANCODE_A);
   if(scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
      return '1' + (scancode - SDL_SCANCODE_1);
   if(scancode == SDL_SCANCODE_0)
      return '0';
   if(scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12)
      return 0x70 + (scancode - SDL_SCANCODE_F1);
   if(scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9)
      return 0x61 + (scancode - SDL_SCANCODE_KP_1);
   switch(scancode)
   {
      case SDL_SCANCODE_KP_0: return 0x60;
      case SDL_SCANCODE_BACKSPACE: return 0x08;
      case SDL_SCANCODE_TAB: return 0x09;
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_KP_ENTER: return 0x0d;
      case SDL_SCANCODE_LSHIFT:
      case SDL_SCANCODE_RSHIFT: return 0x10;
      case SDL_SCANCODE_LCTRL:
      case SDL_SCANCODE_RCTRL: return 0x11;
      case SDL_SCANCODE_LALT:
      case SDL_SCANCODE_RALT: return 0x12;
      case SDL_SCANCODE_ESCAPE: return 0x1b;
      case SDL_SCANCODE_SPACE: return 0x20;
      case SDL_SCANCODE_PAGEUP: return 0x21;
      case SDL_SCANCODE_PAGEDOWN: return 0x22;
      case SDL_SCANCODE_END: return 0x23;
      case SDL_SCANCODE_HOME: return 0x24;
      case SDL_SCANCODE_LEFT: return 0x25;
      case SDL_SCANCODE_UP: return 0x26;
      case SDL_SCANCODE_RIGHT: return 0x27;
      case SDL_SCANCODE_DOWN: return 0x28;
      case SDL_SCANCODE_INSERT: return 0x2d;
      case SDL_SCANCODE_DELETE: return 0x2e;
      case SDL_SCANCODE_SEMICOLON: return 0xba;
      case SDL_SCANCODE_EQUALS: return 0xbb;
      case SDL_SCANCODE_COMMA: return 0xbc;
      case SDL_SCANCODE_MINUS: return 0xbd;
      case SDL_SCANCODE_PERIOD: return 0xbe;
      case SDL_SCANCODE_SLASH: return 0xbf;
      case SDL_SCANCODE_GRAVE: return 0xc0;
      case SDL_SCANCODE_LEFTBRACKET: return 0xdb;
      case SDL_SCANCODE_BACKSLASH: return 0xdc;
      case SDL_SCANCODE_RIGHTBRACKET: return 0xdd;
      case SDL_SCANCODE_APOSTROPHE: return 0xde;
      default: return key & 0xff;
   }
}

// Translate an SDL scancode to the PC (set 1) keyboard scan code used by config.txt (scan).
// Like Windows, the extended-key flag is dropped, so the arrow keys share codes with the keypad.
static i32 PCScanCode(SDL_Scancode scancode)
{
   static const std::unordered_map<int, i32> table{
       {SDL_SCANCODE_ESCAPE, 0x01}, {SDL_SCANCODE_1, 0x02}, {SDL_SCANCODE_2, 0x03}, {SDL_SCANCODE_3, 0x04},
       {SDL_SCANCODE_4, 0x05}, {SDL_SCANCODE_5, 0x06}, {SDL_SCANCODE_6, 0x07}, {SDL_SCANCODE_7, 0x08},
       {SDL_SCANCODE_8, 0x09}, {SDL_SCANCODE_9, 0x0a}, {SDL_SCANCODE_0, 0x0b}, {SDL_SCANCODE_MINUS, 0x0c},
       {SDL_SCANCODE_EQUALS, 0x0d}, {SDL_SCANCODE_BACKSPACE, 0x0e}, {SDL_SCANCODE_TAB, 0x0f},
       {SDL_SCANCODE_Q, 0x10}, {SDL_SCANCODE_W, 0x11}, {SDL_SCANCODE_E, 0x12}, {SDL_SCANCODE_R, 0x13},
       {SDL_SCANCODE_T, 0x14}, {SDL_SCANCODE_Y, 0x15}, {SDL_SCANCODE_U, 0x16}, {SDL_SCANCODE_I, 0x17},
       {SDL_SCANCODE_O, 0x18}, {SDL_SCANCODE_P, 0x19}, {SDL_SCANCODE_LEFTBRACKET, 0x1a},
       {SDL_SCANCODE_RIGHTBRACKET, 0x1b}, {SDL_SCANCODE_RETURN, 0x1c}, {SDL_SCANCODE_KP_ENTER, 0x1c},
       {SDL_SCANCODE_LCTRL, 0x1d}, {SDL_SCANCODE_RCTRL, 0x1d}, {SDL_SCANCODE_A, 0x1e}, {SDL_SCANCODE_S, 0x1f},
       {SDL_SCANCODE_D, 0x20}, {SDL_SCANCODE_F, 0x21}, {SDL_SCANCODE_G, 0x22}, {SDL_SCANCODE_H, 0x23},
       {SDL_SCANCODE_J, 0x24}, {SDL_SCANCODE_K, 0x25}, {SDL_SCANCODE_L, 0x26}, {SDL_SCANCODE_SEMICOLON, 0x27},
       {SDL_SCANCODE_APOSTROPHE, 0x28}, {SDL_SCANCODE_GRAVE, 0x29}, {SDL_SCANCODE_LSHIFT, 0x2a},
       {SDL_SCANCODE_BACKSLASH, 0x2b}, {SDL_SCANCODE_Z, 0x2c}, {SDL_SCANCODE_X, 0x2d}, {SDL_SCANCODE_C, 0x2e},
       {SDL_SCANCODE_V, 0x2f}, {SDL_SCANCODE_B, 0x30}, {SDL_SCANCODE_N, 0x31}, {SDL_SCANCODE_M, 0x32},
       {SDL_SCANCODE_COMMA, 0x33}, {SDL_SCANCODE_PERIOD, 0x34}, {SDL_SCANCODE_SLASH, 0x35},
       {SDL_SCANCODE_KP_DIVIDE, 0x35}, {SDL_SCANCODE_RSHIFT, 0x36}, {SDL_SCANCODE_KP_MULTIPLY, 0x37},
       {SDL_SCANCODE_LALT, 0x38}, {SDL_SCANCODE_RALT, 0x38}, {SDL_SCANCODE_SPACE, 0x39},
       {SDL_SCANCODE_CAPSLOCK, 0x3a}, {SDL_SCANCODE_F1, 0x3b}, {SDL_SCANCODE_F2, 0x3c}, {SDL_SCANCODE_F3, 0x3d},
       {SDL_SCANCODE_F4, 0x3e}, {SDL_SCANCODE_F5, 0x3f}, {SDL_SCANCODE_F6, 0x40}, {SDL_SCANCODE_F7, 0x41},
       {SDL_SCANCODE_F8, 0x42}, {SDL_SCANCODE_F9, 0x43}, {SDL_SCANCODE_F10, 0x44},
       {SDL_SCANCODE_NUMLOCKCLEAR, 0x45}, {SDL_SCANCODE_SCROLLLOCK, 0x46}, {SDL_SCANCODE_KP_7, 0x47},
       {SDL_SCANCODE_HOME, 0x47}, {SDL_SCANCODE_KP_8, 0x48}, {SDL_SCANCODE_UP, 0x48}, {SDL_SCANCODE_KP_9, 0x49},
       {SDL_SCANCODE_PAGEUP, 0x49}, {SDL_SCANCODE_KP_MINUS, 0x4a}, {SDL_SCANCODE_KP_4, 0x4b},
       {SDL_SCANCODE_LEFT, 0x4b}, {SDL_SCANCODE_KP_5, 0x4c}, {SDL_SCANCODE_KP_6, 0x4d}, {SDL_SCANCODE_RIGHT, 0x4d},
       {SDL_SCANCODE_KP_PLUS, 0x4e}, {SDL_SCANCODE_KP_1, 0x4f}, {SDL_SCANCODE_END, 0x4f}, {SDL_SCANCODE_KP_2, 0x50},
       {SDL_SCANCODE_DOWN, 0x50}, {SDL_SCANCODE_KP_3, 0x51}, {SDL_SCANCODE_PAGEDOWN, 0x51},
       {SDL_SCANCODE_KP_0, 0x52}, {SDL_SCANCODE_INSERT, 0x52}, {SDL_SCANCODE_KP_PERIOD, 0x53},
       {SDL_SCANCODE_DELETE, 0x53}, {SDL_SCANCODE_F11, 0x57}, {SDL_SCANCODE_F12, 0x58},
   };
   auto it = table.find(scancode);
   return it == table.end() ? 0 : it->second;
}

// Returns true if the key was one of our own (Cmd) shortcuts.
static bool HandleShortcut(const SDL_KeyboardEvent &key)
{
   if(key.keysym.scancode == SDL_SCANCODE_F11)
   {
      ToggleFullscreen();
      return true;
   }
   if(!(key.keysym.mod & KMOD_GUI))
      return false;
#ifndef __APPLE__
   switch(key.keysym.scancode)
   {
      case SDL_SCANCODE_F: ToggleFullscreen(); break;
      case SDL_SCANCODE_A: ToggleAspectRatio(); break;
      case SDL_SCANCODE_1: SetOption(OPT_CLOCK, SPEED_GLACIAL); break;
      case SDL_SCANCODE_2: SetOption(OPT_CLOCK, SPEED_MOLASSES); break;
      case SDL_SCANCODE_3: SetOption(OPT_CLOCK, SPEED_VERYSLOW); break;
      case SDL_SCANCODE_4: SetOption(OPT_CLOCK, SPEED_SLOW); break;
      case SDL_SCANCODE_5: SetOption(OPT_CLOCK, SPEED_NORMAL); break;
      case SDL_SCANCODE_6: SetOption(OPT_CLOCK, SPEED_FAST); break;
      case SDL_SCANCODE_7: SetOption(OPT_CLOCK, SPEED_QUICK); break;
      case SDL_SCANCODE_E: SetOption(OPT_EXTRATICKS); break;
      case SDL_SCANCODE_I: SendToGame(UIM_Statistics); break;
      case SDL_SCANCODE_SLASH: UI_MessageBox(helpMessage, "Help", MESSAGE_OK); break;
      case SDL_SCANCODE_Q: g_quit = true; break;
      default: break;
   }
#endif
   // On the Mac these are the menus' key equivalents, which the menu bar handles.
   return true; // Never pass Cmd-combinations on to the game
}

#ifdef __APPLE__
static bool ItemsRemainingAvailable()
{
   return ItemsRemainingOK && (encipheredDataFile == NULL) && !simpleEncipher;
}

// The same states as WM_INITMENUPOPUP in CSBwin.cpp.
bool MacMenu_ItemState(int id, bool &checked)
{
   switch(id)
   {
      case IDC_ItemsRemaining:
      case IDC_NonCSBItemsRemaining: return ItemsRemainingAvailable();
      case IDM_DMRULES: checked = DM_rules; return true;
      case IDC_Record: checked = RecordMenuOption; return BeginRecordOK;
      case IDC_Playback: checked = PlayfileIsOpen(); return BeginRecordOK;
      case IDC_QuickPlay: checked = NoSpeedLimit != 0; return PlayfileIsOpen();
      case IDM_Glacial: checked = gameSpeed == SPEED_GLACIAL; return true;
      case IDM_Molasses: checked = gameSpeed == SPEED_MOLASSES; return true;
      case IDM_VerySlow: checked = gameSpeed == SPEED_VERYSLOW; return true;
      case IDM_Slow: checked = gameSpeed == SPEED_SLOW; return true;
      case IDM_Normal: checked = gameSpeed == SPEED_NORMAL; return true;
      case IDM_Fast: checked = gameSpeed == SPEED_FAST; return true;
      case IDM_Quick: checked = gameSpeed == SPEED_QUICK; return true;
      case IDM_ExtraTicks: checked = extraTicks; return true;
      case IDM_PlayerClock: checked = playerClock; return true;
      case IDM_VOLUME_FULL: checked = gameVolume == VOLUME_FULL; return true;
      case IDM_VOLUME_HALF: checked = gameVolume == VOLUME_HALF; return true;
      case IDM_VOLUME_QUARTER: checked = gameVolume == VOLUME_QUARTER; return true;
      case IDM_VOLUME_EIGHTH: checked = gameVolume == VOLUME_EIGHTH; return true;
      case IDM_VOLUME_OFF: checked = gameVolume == VOLUME_OFF; return true;
      case ID_4X3ASPECTRATIO: checked = g_aspectRatio.y == 240; return true;
      case IDM_FULLSCREEN: checked = (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0; return true;
      default: return true;
   }
}

// The same actions as WM_COMMAND in CSBwin.cpp.
static void HandleMenuCommand(int id)
{
   switch(id)
   {
      case IDM_Statistics: SendToGame(UIM_Statistics); break;
      case IDC_ItemsRemaining: SetOption(OPT_ITEMSREMAINING); break;
      case IDC_NonCSBItemsRemaining: SetOption(OPT_NONCSBITEMSREMAINING); break;
      case IDM_DMRULES: SetOption(OPT_DMRULES); break;
      case IDC_Record: SetOption(OPT_RECORD, RecordMenuOption ? 0 : 1); break;
      case IDC_Playback: SetOption(OPT_PLAYBACK, PlayfileIsOpen() ? 0 : 1); break;
      case IDC_QuickPlay:
         if(PlayfileIsOpen())
            SetOption(OPT_QUICKPLAY, NoSpeedLimit != 0 ? 0 : 1);
         break;
      case IDM_Glacial: SetOption(OPT_CLOCK, SPEED_GLACIAL); break;
      case IDM_Molasses: SetOption(OPT_CLOCK, SPEED_MOLASSES); break;
      case IDM_VerySlow: SetOption(OPT_CLOCK, SPEED_VERYSLOW); break;
      case IDM_Slow: SetOption(OPT_CLOCK, SPEED_SLOW); break;
      case IDM_Normal: SetOption(OPT_CLOCK, SPEED_NORMAL); break;
      case IDM_Fast: SetOption(OPT_CLOCK, SPEED_FAST); break;
      case IDM_Quick: SetOption(OPT_CLOCK, SPEED_QUICK); break;
      case IDM_ExtraTicks: SetOption(OPT_EXTRATICKS); break;
      case IDM_PlayerClock: SetOption(OPT_PLAYERCLOCK); break;
      case IDM_VOLUME_FULL: SetOption(OPT_VOLUME, VOLUME_FULL); break;
      case IDM_VOLUME_HALF: SetOption(OPT_VOLUME, VOLUME_HALF); break;
      case IDM_VOLUME_QUARTER: SetOption(OPT_VOLUME, VOLUME_QUARTER); break;
      case IDM_VOLUME_EIGHTH: SetOption(OPT_VOLUME, VOLUME_EIGHTH); break;
      case IDM_VOLUME_OFF: SetOption(OPT_VOLUME, VOLUME_OFF); break;
      case ID_4X3ASPECTRATIO: ToggleAspectRatio(); break;
      case IDM_FULLSCREEN: ToggleFullscreen(); break;
      case IDM_HELP: UI_MessageBox(helpMessage, "Help", MESSAGE_OK); break;
   }
}
#endif

static void HandleKeyDown(const SDL_KeyboardEvent &key)
{
   if(HandleShortcut(key))
      return;
   SDL_Scancode scancode = key.keysym.scancode;
   SendToGame(UIM_KEYDOWN, VirtualKey(key.keysym.sym, scancode), PCScanCode(scancode));

   // Win32 delivers WM_CHAR for these; printable characters arrive via SDL_TEXTINPUT.
   i32 ch = 0;
   switch(scancode)
   {
      case SDL_SCANCODE_RETURN:
      case SDL_SCANCODE_KP_ENTER: ch = 0x0d; break;
      case SDL_SCANCODE_BACKSPACE: ch = 0x08; break;
      case SDL_SCANCODE_ESCAPE: ch = 0x1b; break;
      case SDL_SCANCODE_TAB: ch = 0x09; break;
      default:
         if((key.keysym.mod & KMOD_CTRL) && scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
            ch = 1 + (scancode - SDL_SCANCODE_A); // Control characters, e.g. Ctrl-C == 3
         break;
   }
   if(ch)
      SendToGame(UIM_CHAR, ch);
}

static void HandleEvent(const SDL_Event &event)
{
#ifdef __APPLE__
   if(event.type == MacMenu_EventType())
   {
      HandleMenuCommand(event.user.code);
      return;
   }
#endif
   switch(event.type)
   {
      case SDL_QUIT:
         g_quit = true;
         break;
      case SDL_WINDOWEVENT:
         switch(event.window.event)
         {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
               UpdateClientRect();
               SendToGame(UIM_REDRAW_ENTIRE_SCREEN);
               RenderWindow();
               break;
            case SDL_WINDOWEVENT_EXPOSED:
               RenderWindow();
               break;
            case SDL_WINDOWEVENT_LEAVE:
               ShowCursorIfNeeded(false);
               break;
         }
         break;
      case SDL_MOUSEMOTION:
         ShowCursorIfNeeded(IsInsideAtariScreen(MouseToAtari()));
         break;
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
         bool down = event.type == SDL_MOUSEBUTTONDOWN;
         POINT point = MouseToAtari();
         // Ctrl-click acts as a right click for one-button trackpads.
         bool right = event.button.button == SDL_BUTTON_RIGHT ||
                      (event.button.button == SDL_BUTTON_LEFT && (SDL_GetModState() & KMOD_CTRL));
         if(event.button.button != SDL_BUTTON_LEFT && event.button.button != SDL_BUTTON_RIGHT)
            break;
         MTYPE type = right ? (down ? UIM_RIGHT_BUTTON_DOWN : UIM_RIGHT_BUTTON_UP)
                            : (down ? UIM_LEFT_BUTTON_DOWN : UIM_LEFT_BUTTON_UP);
         SendToGame(type, point.x, point.y);
         break;
      }
      case SDL_KEYDOWN:
         HandleKeyDown(event.key);
         break;
      case SDL_TEXTINPUT:
         for(const char *p = event.text.text; *p; p++)
         {
            if((unsigned char)*p < 0x80)
               SendToGame(UIM_CHAR, *p);
         }
         break;
   }
}

// Parse arguments of the form key=value, the same syntax as the Windows version,
// e.g.  CSBwin directory=DM play=Playfile.replay
static void ProcessCommandLine(int argc, char *argv[])
{
   for(int i = 1; i < argc; i++)
   {
      if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
      {
         printf("%s\n\nUsage: CSBwin [directory=<dir>] [dungeon=<file>] [play=<file>]\n"
                "              [speed=<glacial|molasses|veryslow|slow|normal|fast|quick>]\n"
                "              [volume=<full|half|quarter|eighth|off>]\n"
                "              [size=full] [width=<pixels>] [record] [norecord]\n\n%s\n",
                szCSBVersion, helpMessage);
         exit(0);
      }
      char key[201], value[201];
      const char *equals = strchr(argv[i], '=');
      size_t keyLength = equals ? size_t(equals - argv[i]) : strlen(argv[i]);
      snprintf(key, sizeof(key), "%.*s", int(keyLength), argv[i]);
      snprintf(value, sizeof(value), "%s", equals ? equals + 1 : "");
      _strupr(key);
      if(key[0] != 0)
         UI_ProcessOption(key, value);
   }
}

int main(int argc, char *argv[])
{
   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
   SDL_SetHint(SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK, "0");
   if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
   {
      fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
      return 1;
   }
#ifdef __APPLE__
   MacMenu_Create();
#endif

   // Like the Windows version, the folder containing the program is the last place searched for files.
   if(char *basePath = SDL_GetBasePath())
   {
      g_root = basePath;
      SDL_free(basePath);
   }
   // Inside CSBwin.app the game data is in Contents/Resources, which may be
   // read-only.  Saves and settings go in ~/Library/Application Support/CSBwin.
   static const char bundleSuffix[] = ".app/Contents/Resources/";
   if(g_root.size() > strlen(bundleSuffix) &&
      g_root.compare(g_root.size() - strlen(bundleSuffix), std::string::npos, bundleSuffix) == 0)
   {
      if(char *prefPath = SDL_GetPrefPath("", "CSBwin"))
      {
         g_userRoot = prefPath;
         SDL_free(prefPath);
         chdir(g_userRoot.c_str()); // For the odd debug file opened with a bare name
         UI_ProcessOption((char *)"DIRECTORY", (char *)"DM"); // Until the launcher picks a game
      }
   }
   versionSignature = Signature(szCSBVersion);
   ProcessCommandLine(argc, argv);

   i32 height = WindowHeight ? WindowHeight : WindowWidth * 240 / 320;
   g_window = SDL_CreateWindow("Chaos Strikes Back",
                               WindowX ? WindowX : SDL_WINDOWPOS_CENTERED,
                               WindowY ? WindowY : SDL_WINDOWPOS_CENTERED,
                               WindowWidth, height,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
   if(!g_window)
   {
      fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
      return 1;
   }
   SDL_SetWindowMinimumSize(g_window, 320, 240);
   g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
   if(!g_renderer)
      g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
   g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
                                 g_rcAtari.right, g_rcAtari.bottom);
   UpdateClientRect();
   SDL_RaiseWindow(g_window); // When started by the launcher in CSBwin.app
   if(fullscreenRequested)
      ToggleFullscreen();

   speedTable[SPEED_GLACIAL].vblPerTick = 1000;
   speedTable[SPEED_MOLASSES].vblPerTick = 55;
   speedTable[SPEED_VERYSLOW].vblPerTick = 33;
   speedTable[SPEED_SLOW].vblPerTick = 22;
   speedTable[SPEED_NORMAL].vblPerTick = 15;
   speedTable[SPEED_FAST].vblPerTick = 11;
   speedTable[SPEED_QUICK].vblPerTick = 7;

   volumeTable[VOLUME_FULL].attenuation = 0;
   volumeTable[VOLUME_HALF].attenuation = 6;
   volumeTable[VOLUME_QUARTER].attenuation = 12;
   volumeTable[VOLUME_EIGHTH].attenuation = 18;
   volumeTable[VOLUME_OFF].attenuation = 100;

   volumeTable[VOLUME_FULL].divisor = 1;
   volumeTable[VOLUME_HALF].divisor = 2;
   volumeTable[VOLUME_QUARTER].divisor = 4;
   volumeTable[VOLUME_EIGHTH].divisor = 8;
   volumeTable[VOLUME_OFF].divisor = 65535;

   SDLSound_Initialize();
   SDL_StartTextInput();

   SendToGame(UIM_INITIALIZE);

   // The Windows version runs the game from a 10ms WM_TIMER and repaints on WM_PAINT.
   const Uint32 timerInterval = TImER ? TImER : 10;
   Uint32 nextTimer = SDL_GetTicks() + timerInterval;
   while(!g_quit)
   {
      Uint32 now = SDL_GetTicks();
      SDL_Event event;
      int timeout = SDL_TICKS_PASSED(now, nextTimer) ? 0 : int(nextTimer - now);
      if(SDL_WaitEventTimeout(&event, timeout))
      {
         HandleEvent(event);
         while(!g_quit && SDL_PollEvent(&event))
            HandleEvent(event);
      }

      now = SDL_GetTicks();
      if(SDL_TICKS_PASSED(now, nextTimer))
      {
         nextTimer += timerInterval;
         if(SDL_TICKS_PASSED(now, nextTimer))
            nextTimer = now + timerInterval; // Don't try to catch up after a stall
         SendToGame(UIM_TIMER);
      }
      if(g_paintPending)
      {
         g_paintPending = false;
         SendToGame(UIM_PAINT);
      }
   }

   if(trace >= 0)
      CLOSE(trace);
   Cleanup(true); // Program termination
   UI_CheckMemoryLeaks();
   SDLSound_Shutdown();
   SDL_DestroyTexture(g_texture);
   SDL_DestroyRenderer(g_renderer);
   SDL_DestroyWindow(g_window);
   SDL_Quit();
   return 0;
}
