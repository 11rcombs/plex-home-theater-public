//
//  BackgroundMusicPlayer.h
//  Plex
//
//  Created by James Clarke on 08/09/2008.
//

#import <Cocoa/Cocoa.h>
#import <QTKit/QTKit.h>


@interface BackgroundMusicPlayer : NSObject {
  BOOL isThemeMusicEnabled;
  BOOL isThemeDownloadingEnabled;
  BOOL isPlaying;
  
  float volume;
  int volumeFadeLevel;
  int targetVolumeFade;
  int fadeIncrement;
  int globalVolumeAsPercent;
  
  
  NSString *mainMusicPath;
  NSString *themeMusicPath;
  NSArray *mainMusicNames;
  NSMutableDictionary *themeMusicRequests;
  
  NSString *currentId;
  
  QTMovie *mainMusic, *themeMusic;
}

+ (BackgroundMusicPlayer *)sharedInstance;

- (BOOL)themeMusicEnabled;
- (void)setThemeMusicEnabled:(BOOL)enabled;
- (void)setThemeDownloadsEnabled:(BOOL)enabled;

- (void)checkForThemeWithId:(NSString*)tvShowId;

- (BOOL)isPlaying;

- (void)startMusic;
- (void)stopMusic;
- (void)loadNextTrack;
- (void)setThemeMusicId:(NSString*)newId;

- (void)updateMusicVolume;
- (float)volume;
- (void)setVolume:(float)newVolume;
- (void)setGlobalVolumeAsPercent:(int)newGlobalVolumeAsPercent;
- (void)fadeToTheme:(BOOL)toTheme;
- (void)adjustVolumeFadeLevel;

@end
