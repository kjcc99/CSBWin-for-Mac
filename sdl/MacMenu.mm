// MacMenu.mm : the macOS menu bar.
// SDL creates the application and Window menus; we add the Game, Speed, Volume,
// View and Help menus of the Windows version (see CSBwin.rc).

#import <Cocoa/Cocoa.h>
#include "MacMenu.h"
#include "resource.h"

static Uint32 g_menuEventType = (Uint32)-1;

@interface MenuTarget : NSObject <NSMenuItemValidation>
@end

@implementation MenuTarget
- (void)choose:(NSMenuItem *)item
{
   SDL_Event event = {};
   event.type = g_menuEventType;
   event.user.code = (Sint32)item.tag;
   SDL_PushEvent(&event);
}

- (BOOL)validateMenuItem:(NSMenuItem *)item
{
   bool checked = false;
   bool enabled = MacMenu_ItemState((int)item.tag, checked);
   item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
   return enabled;
}
@end

static MenuTarget *g_target;

static void AddItem(NSMenu *menu, NSString *title, int id, NSString *key = @"")
{
   NSMenuItem *item = [menu addItemWithTitle:title action:@selector(choose:) keyEquivalent:key];
   item.target = g_target;
   item.tag = id;
}

static NSMenu *AddMenu(NSString *title)
{
   NSMenu *menu = [[NSMenu alloc] initWithTitle:title];
   NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
   item.submenu = menu;
   NSMenu *mainMenu = NSApp.mainMenu;
   // Before SDL's Window menu, which is last
   [mainMenu insertItem:item atIndex:mainMenu.numberOfItems - 1];
   return menu;
}

void MacMenu_Create()
{
   if(NSApp.mainMenu == nil)
      return;
   g_menuEventType = SDL_RegisterEvents(1);
   g_target = [[MenuTarget alloc] init];

   NSMenu *game = AddMenu(@"Game");
   AddItem(game, @"Statistics", IDM_Statistics, @"i");
   AddItem(game, @"Items Remaining (CSB Challenge)", IDC_ItemsRemaining);
   AddItem(game, @"Non-CSB Items Remaining", IDC_NonCSBItemsRemaining);
   [game addItem:[NSMenuItem separatorItem]];
   AddItem(game, @"DM Rules", IDM_DMRULES);
   [game addItem:[NSMenuItem separatorItem]];
   AddItem(game, @"Record", IDC_Record);
   AddItem(game, @"Playback", IDC_Playback);
   AddItem(game, @"QuickPlay", IDC_QuickPlay);

   NSMenu *speed = AddMenu(@"Speed");
   AddItem(speed, @"Glacial", IDM_Glacial, @"1");
   AddItem(speed, @"Molasses", IDM_Molasses, @"2");
   AddItem(speed, @"Very Slow", IDM_VerySlow, @"3");
   AddItem(speed, @"Slow", IDM_Slow, @"4");
   AddItem(speed, @"Normal", IDM_Normal, @"5");
   AddItem(speed, @"Fast", IDM_Fast, @"6");
   AddItem(speed, @"Quick as a Bunny", IDM_Quick, @"7");
   [speed addItem:[NSMenuItem separatorItem]];
   AddItem(speed, @"Extra Ticks", IDM_ExtraTicks, @"e");
   AddItem(speed, @"Player Clock", IDM_PlayerClock);

   NSMenu *volume = AddMenu(@"Volume");
   AddItem(volume, @"Full", IDM_VOLUME_FULL);
   AddItem(volume, @"Half", IDM_VOLUME_HALF);
   AddItem(volume, @"Quarter", IDM_VOLUME_QUARTER);
   AddItem(volume, @"Eighth", IDM_VOLUME_EIGHTH);
   AddItem(volume, @"Off", IDM_VOLUME_OFF);

   NSMenu *view = AddMenu(@"View");
   AddItem(view, @"4x3 Aspect Ratio", ID_4X3ASPECTRATIO, @"a");
   AddItem(view, @"Full Screen", IDM_FULLSCREEN, @"f");

   // After the Window menu, where macOS expects Help
   NSMenu *help = [[NSMenu alloc] initWithTitle:@"Help"];
   NSMenuItem *helpItem = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
   helpItem.submenu = help;
   [NSApp.mainMenu addItem:helpItem];
   NSApp.helpMenu = help;
   AddItem(help, @"CSBwin Help", IDM_HELP, @"/");
}

Uint32 MacMenu_EventType()
{
   return g_menuEventType;
}
