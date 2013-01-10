//
//  BackgroundMusicPlayer.h
//  Plex
//
//  Created by Jamie Kirkpatrick on 29/01/2011.
//  Copyright 2011 Kirk Consulting Limited. All rights reserved.
//

#pragma once

#include <boost/shared_ptr.hpp>

#include "cores/IPlayer.h"
#include "StdString.h"
#include "PlexTypes.h"
#include "GlobalsHandling.h"

//
// Utility class for playing background theme music.
//
class BackgroundMusicPlayer : public IPlayerCallback, IAudioCallback
{
public:
   // Convenience method to send a background theme change message.
  static void SendThemeChangeMessage(const CStdString& theme = CStdString());
  
  // Constructor.
  BackgroundMusicPlayer();

  // Destructor
  virtual ~BackgroundMusicPlayer();
  
  // Set the currently active theme id.
  void SetTheme(const CStdString& theme);
  
  // Play the currently selected theme music if there is any.
  void PlayCurrentTheme();
  void FadeOutAndDie();
  
private:
  
  // Player callbacks.
  void OnPlayBackEnded(){};
  void OnPlayBackStarted(){};
  void OnPlayBackStopped(){};
  void OnQueueNextItem(){};
  
  // Audio callbacks
  void OnInitialize(int iChannels, int iSamplesPerSec, int iBitsPerSample);
  void OnAudioData(const float* pAudioData, int iAudioDataLength) {};
  
  // Member variables.
  int m_globalVolume;
  CStdString m_theme;
  IPlayer *m_player;
};

XBMC_GLOBAL_REF(BackgroundMusicPlayer, g_backgroundMusicPlayer);
#define g_backgroundMusicPlayer XBMC_GLOBAL_USE(BackgroundMusicPlayer)

