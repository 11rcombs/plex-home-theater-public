//
//  AdvancedSettingsController.m
//  Plex
//
//  Created by James Clarke on 05/03/2009.
//  Copyright 2009 Plex. All rights reserved.
//

#import "AdvancedSettingsController.h"

#define ADVSETTINGS_FILE @"~/Library/Application Support/Plex/userdata/advancedsettings.xml"

@implementation AdvancedSettingsController

id g_advancedSettingsController;

void setEnabledFromXML(NSXMLDocument* xmlDoc, NSButton* control, NSString* xpath, BOOL invert)
{
  NSArray* nodes;
  nodes = [xmlDoc nodesForXPath:xpath error:nil];
  if ([nodes count] > 0)
  {
    if ((([[(NSXMLElement*)[nodes objectAtIndex:0] stringValue] isEqualToString:@"true"]) && (invert == NO)) ||
      (([[(NSXMLElement*)[nodes objectAtIndex:0] stringValue] isEqualToString:@"false"]) && (invert == YES)))
    {
      // NSLog(@"Enabling control for %@", xpath);
      [control setState:NSOnState];
    }
    else
    {
      // NSLog(@"Disabling control for %@", xpath);
      [control setState:NSOffState];
    }
  }
}

void enabledFromControl(NSXMLElement* xmlElement, NSButton* control, NSString* nodeName, BOOL invert)
{
  NSString* strValue;
  if ((([control state] == NSOnState) && (invert == NO)) || (([control state] == NSOffState) && (invert == YES)))
    strValue = @"true";
  else
    strValue = @"false";
  
  for (NSXMLElement* child in [xmlElement children])
    if ([[child name] isEqualToString:nodeName])
    {
      [child setStringValue:strValue];
      return;
    }
  [xmlElement addChild:[NSXMLElement elementWithName:nodeName stringValue:strValue]];
}

void setStringFromXML(NSXMLDocument* xmlDoc, NSTextField* control, NSString* xpath)
{
  NSArray* nodes;
  nodes = [xmlDoc nodesForXPath:xpath error:nil];
  if ([nodes count] > 0)
  {
    NSString* strValue = [(NSXMLElement*)[nodes objectAtIndex:0] stringValue];
    //NSLog(@"String value for control %@ is %@", xpath, strValue);
    [control setStringValue:strValue];
  }
  else
  {
    //NSLog(@"No string value set for control %@", xpath);
    [control setStringValue:@""];
  }
}

void stringFromControl(NSXMLElement* xmlElement, NSTextField* control, NSString* nodeName)
{
  for (NSXMLElement* child in [xmlElement children])
    if ([[child name] isEqualToString:nodeName])
    {
      [child setStringValue:[control stringValue]];
      return;
    }
  [xmlElement addChild:[NSXMLElement elementWithName:nodeName stringValue:[control stringValue]]];
}

void setPopupFromXML(NSXMLDocument* xmlDoc, NSPopUpButton* control, NSString* xpath)
{
  NSArray* nodes;
  nodes = [xmlDoc nodesForXPath:xpath error:nil];
  if ([nodes count] > 0)
  {
    @try
    {
      [control selectItemWithTag:[[(NSXMLElement*)[nodes objectAtIndex:0] stringValue] intValue]]; 
    }
    @catch (id e) {}
  }
}

void tagFromControl(NSXMLElement* xmlElement, NSPopUpButton* control, NSString* nodeName)
{
  NSString* strValue = [NSString stringWithFormat:@"%d", [[control selectedItem] tag]];
  for (NSXMLElement* child in [xmlElement children])
    if ([[child name] isEqualToString:nodeName])
    {
      [child setStringValue:strValue];
      return;
    }
  [xmlElement addChild:[NSXMLElement elementWithName:nodeName stringValue:strValue]];
}

NSXMLElement* rootElement(NSXMLDocument* xmlDoc, NSString* nodeName)
{
  NSArray* nodes;
  nodes = [xmlDoc nodesForXPath:[NSString stringWithFormat:@"./advancedsettings/%@", nodeName] error:nil];
  if ([nodes count] == 0)
  {
    NSLog(@"Creating node %@", nodeName);
    NSXMLElement* el = [NSXMLElement elementWithName:nodeName];
    [xmlDoc addChild:el];
    return el;
  }
  else
  {
    NSLog(@"Returning existing node %@", nodeName);
    return [nodes objectAtIndex:0];
  }
}

+ (AdvancedSettingsController*)sharedInstance
{
  return (AdvancedSettingsController*)g_advancedSettingsController;
}

- (void)awakeFromNib
{
  g_advancedSettingsController = (id)self;
  m_settingChanged = NO;
  m_shouldClose = NO;
  [self loadSettings];
}

- (void)windowWillClose:(NSNotification *)notification
{
  [NSApp stopModal];
}

- (BOOL)windowShouldClose:(id)theWindow
{
  if (m_settingChanged == YES)
  {
    NSBeginAlertSheet(@"There are unsaved changes to your settings.",
                      @"Save",
                      @"Cancel",
                      @"Discard",
                      window,
                      self,
                      @selector(sheetDidEndShouldSave:returnCode:contextInfo:), 
                      nil,
                      nil,
                      @"Do you want to save these changes?");
    return NO;
  }
  
  if (m_shouldClose == YES || m_settingChanged == NO)
  {
    m_shouldClose = NO;
    return YES;
  }
  return NO;
}

- (void)sheetDidEndShouldSave: (NSWindow *)sheet
                   returnCode: (int)returnCode
                  contextInfo: (void *)contextInfo
{
  if (returnCode == NSAlertDefaultReturn)
  {
    [self saveSettings];
    m_shouldClose = YES;
    [window close];
    [self relaunchPlex];
  }
  else if (returnCode == NSAlertOtherReturn)
  {
    [self loadSettings];
    m_settingChanged = NO;
    m_shouldClose = YES;
    [window close];    
  }
}

-(IBAction)showWindow:(id)sender
{
  [[NSApplication sharedApplication] runModalForWindow:window];
}

-(BOOL)windowIsVisible
{
  return [window isVisible];
}

-(IBAction)settingChanged:(id)sender
{
  NSLog(@"Setting changed");
  m_settingChanged = YES;
}

-(IBAction)restoreDefaults:(id)sender
{
  NSBeginAlertSheet(@"Are you sure you want to restore the default settings?",
                    @"Restore",
                    nil,
                    @"Cancel",
                    window,
                    self,
                    @selector(sheetDidEndShouldRestoreDefaults:returnCode:contextInfo:), 
                    nil,
                    nil,
                    @"Any changes you've made to the advanced settings will be lost. This will not affect the rest of your Plex settings.");
}

- (void)sheetDidEndShouldRestoreDefaults: (NSWindow *)sheet
                              returnCode: (int)returnCode
                             contextInfo: (void *)contextInfo
{
  if (returnCode == NSAlertDefaultReturn)
  {
    [[NSFileManager defaultManager] removeFileAtPath:[ADVSETTINGS_FILE stringByExpandingTildeInPath] handler:nil];
    m_settingChanged = NO;
    m_shouldClose = YES;
    [window close];
    [self relaunchPlex];
  }
}

-(void)loadSettings
{
  // Load the advancedsettings.xml file
  NSXMLDocument* xmlDoc;
  NSError* err = nil;
  NSURL* furl = [NSURL fileURLWithPath:[ADVSETTINGS_FILE stringByExpandingTildeInPath]];
  if (!furl) {
    NSLog(@"Can't create an URL from file.");
    return;
  }
  xmlDoc = [[NSXMLDocument alloc] initWithContentsOfURL:furl
                                                options:(NSXMLNodePreserveWhitespace|NSXMLNodePreserveCDATA)
                                                  error:&err];
  if (xmlDoc == nil) {
    xmlDoc = [[NSXMLDocument alloc] initWithContentsOfURL:furl
                                                  options:NSXMLDocumentTidyXML
                                                    error:&err];
  }
  if (xmlDoc == nil)  {
    if (err) {
      NSLog(@"%@", err);
    }
    return;
  }
  
  if (err) {
    NSLog(@"%@", err);
    return;
  }
  
  // Query the XML document and set GUI control states
  setEnabledFromXML(xmlDoc, debugLogging, @"./advancedsettings/system/debuglogging", NO);
  setEnabledFromXML(xmlDoc, opticalMedia, @"./advancedsettings/enableopticalmedia", NO);
  setEnabledFromXML(xmlDoc, trueFullscreen, @"./advancedsettings/fakefullscreen", YES);
  setEnabledFromXML(xmlDoc, cleanOnUpdate, @"./advancedsettings/videolibrary/cleanonupdate", NO);
  setEnabledFromXML(xmlDoc, fileDeletion, @"./advancedsettings/filelists/allowfiledeletion", NO);
  setEnabledFromXML(xmlDoc, showExtensions, @"./advancedsettings/filelists/hideextensions", YES);
  setEnabledFromXML(xmlDoc, showAddSource, @"./advancedsettings/filelists/disableaddsourcebuttons", YES);
  setEnabledFromXML(xmlDoc, ignoreSortTokens, @"./advancedsettings/filelists/ignorethewhensorting", NO);
  setEnabledFromXML(xmlDoc, vizOnPlay, @"./advancedsettings/visualizeronplay", NO);
  setStringFromXML(xmlDoc, httpProxyUsername, @"./advancedsettings/network/httpproxyusername");
  setStringFromXML(xmlDoc, httpProxyPassword, @"./advancedsettings/network/httpproxypassword");
  setStringFromXML(xmlDoc, timeToViz, @"./advancedsettings/secondstovisualizer");
  setPopupFromXML(xmlDoc, scalingAlgorithm, @"./advancedsettings/videoplayer/upscalingalgorithm");
  setPopupFromXML(xmlDoc, flattenTVShows, @"./advancedsettings/videolibrary/flattentvshows");
  
  [xmlDoc release];
}

-(void)saveSettings
{
  // Load the advancedsettings.xml file
  NSXMLElement* root = (NSXMLElement *)[NSXMLNode elementWithName:@"advancedsettings"];
  NSXMLDocument* xmlDoc;
  NSError* err = nil;
  NSURL* furl = [NSURL fileURLWithPath:[ADVSETTINGS_FILE stringByExpandingTildeInPath]];
  if (!furl) {
    NSLog(@"Can't create an URL from file.");
    return;
  }
  xmlDoc = [[NSXMLDocument alloc] initWithContentsOfURL:furl
                                                options:(NSXMLNodePreserveWhitespace|NSXMLNodePreserveCDATA)
                                                  error:&err];
  if (xmlDoc == nil) {
    xmlDoc = [[NSXMLDocument alloc] initWithContentsOfURL:furl
                                                  options:NSXMLDocumentTidyXML
                                                    error:&err];
  }
  if (xmlDoc == nil)  {
    if (err) {
      NSLog(@"%@", err);
    }
    xmlDoc = [[NSXMLDocument alloc] initWithRootElement:root];
  }
  
  if (err) {
    NSLog(@"%@", err);
    xmlDoc = [[NSXMLDocument alloc] initWithRootElement:root];
  }

  root = [xmlDoc rootElement];
  
  NSXMLElement* system = rootElement(xmlDoc, @"system");
  NSXMLElement* network = rootElement(xmlDoc, @"network");
  NSXMLElement* filelists = rootElement(xmlDoc, @"filelists");
  NSXMLElement* videolibrary = rootElement(xmlDoc, @"videolibrary");
  NSXMLElement* videoplayer = rootElement(xmlDoc, @"videoplayer");
  enabledFromControl(system, debugLogging, @"debuglogging", NO);
  enabledFromControl(root, opticalMedia, @"enableopticalmedia", NO);
  enabledFromControl(root, trueFullscreen, @"fakefullscreen", YES);
  enabledFromControl(videolibrary, cleanOnUpdate, @"cleanonupdate", NO);
  enabledFromControl(filelists, fileDeletion, @"allowfiledeletion", NO); 
  enabledFromControl(filelists, showExtensions, @"hidefileextensions", YES);
  enabledFromControl(filelists, showAddSource, @"disableaddsourcebuttons", YES);
  enabledFromControl(filelists, ignoreSortTokens, @"ignorethewhensorting", NO);
  enabledFromControl(root, vizOnPlay, @"visualizeronplay", NO);
  stringFromControl(network, httpProxyUsername, @"httpproxyusername");
  stringFromControl(network, httpProxyPassword, @"httpproxypassword");
  stringFromControl(root, timeToViz, @"secondstovisualizer");
  tagFromControl(videoplayer, scalingAlgorithm, @"upscalingalgorithm");
  tagFromControl(videolibrary, flattenTVShows, @"flattentvshows");
  /*
  



  */
  //NSLog([xmlDoc XMLStringWithOptions:NSXMLNodePrettyPrint]);
  NSData* xmlData = [xmlDoc XMLDataWithOptions:NSXMLNodePrettyPrint];
  if (![xmlData writeToFile:[ADVSETTINGS_FILE stringByExpandingTildeInPath] atomically:YES]) {
    NSBeep();
    NSLog(@"Could not write document out...");
  }
  
  [xmlDoc release];
  
  m_settingChanged = NO;
}

-(void)relaunchPlex
{
  if (NSRunAlertPanel(@"Plex needs to be relaunched to apply your changes.", @"Do you want to relaunch Plex now?", @"Relaunch now", @"Later", nil) == NSAlertDefaultReturn)
  {
    NSString* pathToRelaunch = [[NSBundle mainBundle] bundlePath];
    NSString* relaunchPath = [[NSBundle mainBundle] pathForResource:@"relaunch" ofType:nil];
    [NSTask launchedTaskWithLaunchPath:relaunchPath arguments:[NSArray arrayWithObjects:pathToRelaunch, [NSString stringWithFormat:@"%d", [[NSProcessInfo processInfo] processIdentifier]], nil]];
    
    [NSApp terminate:self];
  }
}

@end
