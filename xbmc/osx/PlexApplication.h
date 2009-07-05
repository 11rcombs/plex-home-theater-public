//
//  PlexApplication.h
//  Plex
//
//  Created by Elan Feingold on 7/16/2008.
//  Copyright 2008 Blue Mandrill Design. All rights reserved.
//
//
#import <Cocoa/Cocoa.h>
#include <SDL/SDL.h>

#define PLEX_MESSAGE SDL_USEREVENT

@interface PlexApplication : NSApplication {
  IBOutlet NSWindow* aboutWindow;
}
+ (PlexApplication*)sharedInstance;
- (void)terminate:(id)sender;
- (IBAction)quit:(id)sender;
- (IBAction)fullScreenToggle:(id)sender;
- (IBAction)floatOnTopToggle:(id)sender;
- (IBAction)moveToPreviousScreen:(id)sender;
- (IBAction)moveToNextScreen:(id)sender;
- (void)finishLaunching;
- (BOOL)isAboutWindowVisible;
@end
