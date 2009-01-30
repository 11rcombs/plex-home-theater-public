#pragma once

/*
 *      Copyright (C) 2008-2009 Plex
 *      http://www.plexapp.com
 * 
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "IPlayer.h"
#include "utils/Thread.h"
#include "../dlgcache.h"
 
class CPlexMediaServerPlayer : public IPlayer, public CThread
{
public:
  
  CPlexMediaServerPlayer(IPlayerCallback& callback);
  virtual ~CPlexMediaServerPlayer();
  virtual void RegisterAudioCallback(IAudioCallback* pCallback) {}
  virtual void UnRegisterAudioCallback()                        {}
  virtual bool OpenFile(const CFileItem& file, const CPlayerOptions &options);
  virtual bool CloseFile();
  virtual bool IsPlaying() const;
  virtual void Pause();
  virtual bool IsPaused() const;
  virtual bool HasVideo() const;
  virtual bool HasAudio() const;
  virtual void ToggleOSD() {}
  virtual void SwitchToNextLanguage() {}
  virtual void ToggleSubtitles() {}
  virtual void ToggleFrameDrop() {}
  virtual bool CanSeek();
  virtual void Seek(bool bPlus, bool bLargeStep);
  virtual void SeekPercentage(float iPercent);
  virtual float GetPercentage();
  virtual void SetVolume(long nVolume); 
  virtual void SetDynamicRangeCompression(long drc) {}
  virtual void SetContrast(bool bPlus) {}
  virtual void SetBrightness(bool bPlus) {}
  virtual void SetHue(bool bPlus) {}
  virtual void SetSaturation(bool bPlus) {}
  virtual void GetAudioInfo(CStdString& strAudioInfo);
  virtual void GetVideoInfo(CStdString& strVideoInfo);
  virtual void GetGeneralInfo( CStdString& strVideoInfo);
  virtual void Update(bool bPauseDrawing)                       {}
  virtual void GetVideoRect(RECT& SrcRect, RECT& DestRect)      {}
  virtual void GetVideoAspectRatio(float& fAR)                  {}
  virtual void SwitchToNextAudioLanguage() {}
  virtual bool CanRecord() { return false; }
  virtual bool IsRecording() { return false; }
  virtual bool Record(bool bOnOff) { return false; }
  virtual void SetAVDelay(float fValue = 0.0f);
  virtual float GetAVDelay();
  void Render();

  virtual void SetSubTitleDelay(float fValue = 0.0f) {}
  virtual float GetSubTitleDelay() { return 0.0f; }

  virtual void SeekTime(__int64 iTime);
  virtual __int64 GetTime();
  virtual int GetTotalTime();
  virtual void ToFFRW(int iSpeed);
  virtual void DoAudioWork() {}
  
  virtual CStdString GetPlayerState() { return ""; }
  virtual bool SetPlayerState(CStdString state) { return true; }
  
private:

  void OnPlaybackEnded();
  void OnPlaybackStarted();
  void OnNewFrame(); 
  void OnPaused();
  void OnResumed();
  void OnProgress(int nPct); 
  
  virtual void Process();

  bool m_paused;
  bool m_playing;
  
  __int64 m_clock;
  int     m_pct;
  DWORD m_lastTime;
  int m_speed;
  
  int          m_totalTime;
  int          m_width;
  int          m_height;
  int          m_cropTop;
  int          m_cropBottom;
  int          m_cropLeft;
  int          m_cropRight;
  
  CDlgCache   *m_pDlgCache;
};
