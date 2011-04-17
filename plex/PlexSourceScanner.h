#pragma once
/*
 *  PlexSourceScanner.h
 *  Plex
 *
 *  Created by Elan Feingold & James Clarke on 13/11/2009.
 *  Copyright 2009 Plex Development Team. All rights reserved.
 *
 */

#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "CriticalSection.h"
#include "FileItem.h"
#include "SingleLock.h"
#include "Settings.h"
#include "Thread.h"

class HostSources
{
 public:

  VECSOURCES    videoSources;
  VECSOURCES    musicSources;
  VECSOURCES    pictureSources;
  CFileItemList librarySections;
};

typedef boost::shared_ptr<HostSources> HostSourcesPtr;
typedef std::pair<std::string, HostSourcesPtr> StringSourcesPair;

////////////////////////////////////////////
class CPlexSourceScanner : public CThread
{
public:
  
  virtual void Process();
  
  static void ScanHost(const std::string& uuid, const std::string& host, const std::string& hostLabel, const std::string& url);
  static void RemoveHost(const std::string& uuid);
  
  static void MergeSourcesForWindow(int windowId);
  
  static void Lock() { ::EnterCriticalSection(g_lock); }
  static std::map<std::string, HostSourcesPtr>& GetMap() { return g_hostSourcesMap; }
  static void Unlock() { ::LeaveCriticalSection(g_lock); }

  static int GetActiveScannerCount() { return g_activeScannerCount; } 
  
  static void AutodetectPlexSources(CStdString strPlexPath, VECSOURCES& dstSources, CStdString strLabel = "", bool onlyShared = false);
  static void RemovePlexSources(CStdString strPlexPath, VECSOURCES& dstSources);
  
protected:
  
  static void MergeSource(VECSOURCES& sources, VECSOURCES& remoteSources);
  static void CheckForRemovedSources(VECSOURCES& sources, int windowId);
  
  CPlexSourceScanner(const std::string& uuid, const std::string& host, const std::string& hostLabel, const std::string& url)
  : m_uuid(uuid)
  , m_host(host)
  , m_hostLabel(hostLabel)
  , m_url(url)
  {
    Create(true);
  }
  
  virtual ~CPlexSourceScanner() {}
  
private:
  
  std::string m_uuid;
  std::string m_host;
  std::string m_hostLabel;
  std::string m_url;
  
  static std::map<std::string, HostSourcesPtr> g_hostSourcesMap;
  static CCriticalSection g_lock;
  static int g_activeScannerCount;
};
