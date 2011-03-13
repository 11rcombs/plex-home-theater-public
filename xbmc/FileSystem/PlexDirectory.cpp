/*
 * PlexDirectory.cpp
 *
 *  Created on: Oct 4, 2008
 *      Author: Elan Feingold
 */
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include "stdafx.h"
#include "Album.h"
#include "CocoaUtilsPlus.h"
#include "CocoaUtils.h"
#include "File.h"
#include "PlexDirectory.h"
#include "DirectoryCache.h"
#include "Util.h"
#include "FileCurl.h"
#include "utils/HttpHeader.h"
#include "VideoInfoTag.h"
#include "MusicInfoTag.h"
#include "GUIWindowManager.h"
#include "GUIDialogProgress.h"
#include "URL.h"
#include "FileItem.h"
#include "GUIViewState.h"
#include "GUIDialogOK.h"

using namespace std;
using namespace XFILE;
using namespace DIRECTORY;

#define MASTER_PLEX_MEDIA_SERVER "http://localhost:32400"
#define MAX_THUMBNAIL_AGE (3600*24*2)
#define MAX_FANART_AGE    (3600*24*7)

///////////////////////////////////////////////////////////////////////////////////////////////////
CPlexDirectory::CPlexDirectory(bool parseResults)
  : m_bStop(false)
  , m_bSuccess(true)
  , m_bParseResults(parseResults)
  , m_dirCacheType(DIR_CACHE_ALWAYS)
{
  m_timeout = 300;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
CPlexDirectory::~CPlexDirectory()
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////
bool CPlexDirectory::GetDirectory(const CStdString& strPath, CFileItemList &items)
{
  CStdString strRoot = strPath;
  if (CUtil::HasSlashAtEnd(strRoot) && strRoot != "plex://")
    strRoot.Delete(strRoot.size() - 1);

  strRoot.Replace(" ", "%20");

  // Start the download thread running.
  printf("PlexDirectory::GetDirectory(%s)\n", strRoot.c_str());
  m_url = strRoot;
  CThread::Create(false, 0);

  // Now display progress, look for cancel.
  CGUIDialogProgress* dlgProgress = 0;

  int time = GetTickCount();

  while (m_downloadEvent.WaitMSec(100) == false)
  {
    // If enough time has passed, display the dialog.
    if (GetTickCount() - time > 1000 && m_allowPrompting == true)
    {
      dlgProgress = (CGUIDialogProgress*)m_gWindowManager.GetWindow(WINDOW_DIALOG_PROGRESS);
      if (dlgProgress)
      {
        dlgProgress->ShowProgressBar(false);
        dlgProgress->SetHeading(40203);
        dlgProgress->SetLine(0, 40204);
        dlgProgress->SetLine(1, "");
        dlgProgress->SetLine(2, "");
        dlgProgress->StartModal();
      }
    }

    if (dlgProgress)
    {
      dlgProgress->Progress();
      if (dlgProgress->IsCanceled())
      {
        items.m_wasListingCancelled = true;
        m_http.Cancel();
        StopThread();
      }
    }
  }

  if (dlgProgress)
    dlgProgress->Close();

  // Wait for the thread to exit.
  WaitForThreadExit(INFINITE);

  // See if we suceeded.
  if (m_bSuccess == false)
    return false;

  // See if we're supposed to parse the results or not.
  if (m_bParseResults == false)
    return true;

  // Parse returned xml.
  TiXmlDocument doc;
  doc.Parse(m_data.c_str());

  TiXmlElement* root = doc.RootElement();
  if (root == 0)
  {
    CLog::Log(LOGERROR, "%s - Unable to parse XML\n%s", __FUNCTION__, m_data.c_str());
    return false;
  }

  // Get the fanart.
  const char* fanart = root->Attribute("art");
  string strFanart;
  if (fanart && strlen(fanart) > 0)
    strFanart = ProcessUrl(strPath, fanart, false);

  // Walk the parsed tree.
  string strFileLabel = "%N - %T";
  string strSecondFileLabel = "%D";
  string strDirLabel = "%B";
  string strSecondDirLabel = "%Y";

  Parse(m_url, root, items, strFileLabel, strSecondFileLabel, strDirLabel, strSecondDirLabel);

  // Check if any restrictions should be applied
  bool disableFanart = false;

  if (g_advancedSettings.m_bEnableViewRestrictions)
  {
    // Disable fanart
    const char* strDisableFanart = root->Attribute("disableFanart");
    if (strDisableFanart && strcmp(strDisableFanart, "1") == 0)
      disableFanart = true;

    // Disabled view modes
    const char* disabledViewModes = root->Attribute("disabledViewModes");
    if (disabledViewModes && strlen(disabledViewModes) > 0)
      items.SetDisabledViewModes(disabledViewModes);
  }

  // Set the window titles
  const char* title1 = root->Attribute("title1");
  const char* title2 = root->Attribute("title2");

  if (title1 && strlen(title1) > 0)
    items.SetFirstTitle(title1);
  if (title2 && strlen(title2) > 0)
    items.SetSecondTitle(title2);

  // Get container summary.
  const char* summary = root->Attribute("summary");
  if (summary)
    items.SetProperty("description", summary);

  // Get color values
  const char* communityRatingColor = root->Attribute("ratingColor");

  const char* httpCookies = root->Attribute("httpCookies");
  const char* userAgent = root->Attribute("userAgent");

  const char* pluginIdentifier = root->Attribute("identifier");
  if (pluginIdentifier)
    items.SetProperty("identifier", pluginIdentifier);

  // Set fanart on items if they don't have their own, or if individual item fanart
  // is disabled. Also set HTTP & rating info
  //
  int firstProvider = -1;
  for (int i=0; i<items.Size(); i++)
  {
    CFileItemPtr pItem = items[i];

    // See if this is a provider.
    if (pItem->GetProperty("provider") == "1")
    {
      items.AddProvider(pItem);

      if (firstProvider == -1)
        firstProvider = i;
    }

    // Fall back to directory fanart?
    if ((strFanart.size() > 0 && pItem->GetQuickFanart().size() == 0) || disableFanart)
    {
      pItem->SetQuickFanart(strFanart);

      if (strFanart.find("32400/:/resources") != -1)
        pItem->SetProperty("fanart_fallback", "1");
    }

    // Save the fallback fanart in case we need it while loading the real one.
    if (strFanart.size() > 0)
    {
      // Only do this if we have it cached already.
      if (CFile::Exists(CFileItem::GetCachedPlexMediaServerFanart(strFanart)))
        pItem->SetProperty("fanart_image_fallback", CFileItem::GetCachedPlexMediaServerFanart(strFanart));
    }

    // Fall back to directory thumb?
    if (strThumb.size() > 0 && pItem->GetThumbnailImage().size() == 0)
      pItem->SetThumbnailImage(strThumb);

    // Make sure sort label is lower case.
    string sortLabel = pItem->GetLabel();
    boost::to_lower(sortLabel);
    pItem->SetSortLabel(sortLabel);

    // See if there's a cookie property to set.
    if (httpCookies)
      pItem->SetProperty("httpCookies", httpCookies);

    if (userAgent)
      pItem->SetProperty("userAgent", userAgent);

    if (communityRatingColor)
      pItem->SetProperty("communityRatingColor", communityRatingColor);

    if (pluginIdentifier)
      pItem->SetProperty("pluginIdentifier", pluginIdentifier);
  }

  // If we had providers, whack them.
  if (firstProvider != -1)
  {
    for (int i=items.Size()-1; i>=firstProvider; i--)
      items.Remove(i);
  }

  // Set fanart on directory.
  if (strFanart.size() > 0)
    items.SetQuickFanart(strFanart);

  // Set the view mode.
  const char* viewmode = root->Attribute("viewmode");
  if (viewmode && strlen(viewmode) > 0)
  {
    CGUIViewState* viewState = CGUIViewState::GetViewState(0, items);
    viewState->SaveViewAsControl(atoi(viewmode));
  }

  // Override labels.
  const char* fileLabel = root->Attribute("filelabel");
  if (fileLabel && strlen(fileLabel) > 0)
    strFileLabel = fileLabel;

  const char* dirLabel = root->Attribute("dirlabel");
  if (dirLabel && strlen(dirLabel) > 0)
    strDirLabel = dirLabel;

  // Add the sort method.
  items.AddSortMethod(SORT_METHOD_NONE, 552, LABEL_MASKS(strFileLabel, strSecondFileLabel, strDirLabel, strSecondDirLabel));

  // Set the content label.
  const char* content = root->Attribute("content");
  if (content && strlen(content) > 0)
  {
    items.SetContent(content);
  }

  // Check for dialog message attributes
  CStdString strMessage = "";
  const char* header = root->Attribute("header");
  if (header && strlen(header) > 0)
  {
    const char* message = root->Attribute("message");
    if (message && strlen(message) > 0)
      strMessage = message;

    items.m_displayMessage = true;
    items.m_displayMessageTitle = header;
    items.m_displayMessageContents = root->Attribute("message");

    // Don't cache these.
    m_dirCacheType = DIR_CACHE_NEVER;
  }

  // See if this directory replaces the parent.
  const char* replace = root->Attribute("replaceParent");
  if (replace && strcmp(replace, "1") == 0)
    items.SetReplaceListing(true);

  // See if we're saving this into the history or not.
  const char* noHistory = root->Attribute("noHistory");
    if (noHistory && strcmp(noHistory, "1") == 0)
      items.SetSaveInHistory(false);

  // See if we're not supposed to cache this directory.
  const char* noCache = root->Attribute("nocache");
  if (noCache && strcmp(noCache, "1") == 0)
    m_dirCacheType = DIR_CACHE_NEVER;

  return true;
}

class PlexMediaNode
{
 public:
   static PlexMediaNode* Create(const string& name);

   CFileItemPtr BuildFileItem(const CURL& url, TiXmlElement& el)
   {
     CFileItemPtr pItem(new CFileItem());
     pItem->m_bIsFolder = true;

     const char* key = el.Attribute("key");
     if (key == 0 || strlen(key) == 0)
       return CFileItemPtr();

     string src = key;
     CStdString parentPath;
     url.GetURL(parentPath);

     // Compute the new path.
     pItem->m_strPath = CPlexDirectory::ProcessUrl(parentPath, src, true);

     // Let subclass finish.
     DoBuildFileItem(pItem, string(parentPath), el);

     // Parent path.
     if (el.Attribute("parentKey"))
       pItem->SetProperty("parentPath", CPlexDirectory::ProcessUrl(parentPath, el.Attribute("parentKey"), true));

     // Sort label.
     if (el.Attribute("titleSort"))
       pItem->SetSortLabel(el.Attribute("titleSort"));
     else
       pItem->SetSortLabel(pItem->GetLabel());

     // Set the key.
     pItem->SetProperty("unprocessedKey", key);
     pItem->SetProperty("key", CPlexDirectory::ProcessUrl(parentPath, key, false));

     // Source title.
     SetProperty(pItem, el, "sourceTitle");

     // Date.
     SetProperty(pItem, el, "subtitle");

     // Ancestry.
     SetProperty(pItem, el, "parentTitle");
     SetProperty(pItem, el, "grandparentTitle");

     // Bitrate.
     const char* bitrate = el.Attribute("bitrate");
     if (bitrate && strlen(bitrate) > 0)
       pItem->m_iBitrate = boost::lexical_cast<int>(bitrate);

     // View offset.
     SetProperty(pItem, el, "viewOffset");

     const char* label2;
     label2 = el.Attribute("infolabel");
     if (label2 && strlen(label2) > 0)
     {
       pItem->SetLabel2(label2);
       pItem->SetLabelPreformated(true);
     }
     label2 = el.Attribute("infoLabel");
     if (label2 && strlen(label2) > 0)
     {
       pItem->SetLabel2(label2);
       pItem->SetLabelPreformated(true);
     }

     // The type of the media.
     if (el.Attribute("type"))
     {
       pItem->SetProperty("mediaType::" + string(el.Attribute("type")), "1");
       pItem->SetProperty("type", el.Attribute("type"));
       pItem->SetProperty("typeNumber", TypeStringToNumber(el.Attribute("type")));
     }

     try
     {
       // Thumb.
       const char* thumb = el.Attribute("thumb");
       if (thumb && strlen(thumb) > 0)
       {
         string strThumb = CPlexDirectory::ProcessUrl(parentPath, thumb, false);

         // See if the item is too old.
         string cachedFile(CFileItem::GetCachedPlexMediaServerThumb(strThumb));
         if (CFile::Age(cachedFile) > MAX_THUMBNAIL_AGE)
         {
           printf("Whacked thumbnail for being too old [%s]\n", strThumb.c_str());
           CFile::Delete(cachedFile);
         }

         // Set the thumbnail URL.
         pItem->SetThumbnailImage(strThumb);
       }

       // Fanart.
       const char* fanart = el.Attribute("art");
       if (fanart && strlen(fanart) > 0)
       {
         string strFanart = CPlexDirectory::ProcessUrl(parentPath, fanart, false);

         // See if the item is too old.
         string cachedFile(CFileItem::GetCachedPlexMediaServerFanart(strFanart));
         if (CFile::Age(cachedFile) > MAX_FANART_AGE)
         {
           printf("Whacked fanart for being too old [%s]\n", strFanart.c_str());
           CFile::Delete(cachedFile);
         }

         // Set the fanart.
         pItem->SetQuickFanart(strFanart);
       }
     }
     catch (...)
     {
       printf("ERROR: Exception setting directory thumbnail.\n");
     }

     // Make sure we have the trailing slash.
     if (pItem->m_bIsFolder == true && pItem->m_strPath[pItem->m_strPath.size()-1] != '/')
       pItem->m_strPath += "/";

     return pItem;
   }

   virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el) = 0;
   virtual void ComputeLabels(const string& strPath, string& strFileLabel, string& strSecondFileLabel, string& strDirLabel, string& strSecondDirLabel)
   {
     strFileLabel = "%N - %T";
     strDirLabel = "%B";
     strSecondDirLabel = "%Y";

     if (strPath.find("/Albums") != -1)
       strDirLabel = "%B - %A";
     else if (strPath.find("/Recently%20Played/By%20Artist") != -1 || strPath.find("/Most%20Played/By%20Artist") != -1)
       strDirLabel = "%B";
     else if (strPath.find("/Decades/") != -1 || strPath.find("/Recently%20Added") != -1 || strPath.find("/Most%20Played") != -1 || strPath.find("/Recently%20Played/") != -1 || strPath.find("/Genre/") != -1)
       strDirLabel = "%A - %B";
   }

   string GetLabel(const TiXmlElement& el)
   {
     // FIXME: We weren't consistent, so no we need to accept multiple ones until
     // we release this and update the framework.
     //
     if (el.Attribute("title"))
       return el.Attribute("title");
     else if (el.Attribute("label"))
       return el.Attribute("label");
     else if (el.Attribute("name"))
       return el.Attribute("name");
     else if (el.Attribute("track"))
       return el.Attribute("track");

     return "";
   }

   void SetProperty(const CFileItemPtr& item, const TiXmlElement& el, const string& attrName, const string& propertyName="")
   {
     string propName = attrName;
     if (propertyName.size() > 0)
       propName = propertyName;

     const char* pVal = el.Attribute(attrName.c_str());
     if (pVal && *pVal != 0)
       item->SetProperty(propName, pVal);
   }

   void SetValue(const TiXmlElement& el, CStdString& value, const char* name)
   {
     const char* pVal = el.Attribute(name);
     if (pVal && *pVal != 0)
       value = pVal;
   }

   void SetValue(const TiXmlElement& el, int& value, const char* name)
   {
     const char* pVal = el.Attribute(name);
     if (pVal && *pVal != 0)
       value = boost::lexical_cast<int>(pVal);
   }

   void SetValue(const TiXmlElement& el, const TiXmlElement& parentEl, CStdString& value, const char* name)
   {
     const char* val = el.Attribute(name);
     const char* valParent = parentEl.Attribute(name);

     if (val && *val != 0)
       value = val;
     else if (valParent && *valParent)
       value = valParent;
   }

   void SetValue(const TiXmlElement& el, const TiXmlElement& parentEl, int& value, const char* name)
   {
     const char* val = el.Attribute(name);
     const char* valParent = parentEl.Attribute(name);

     if (val && *val != 0)
       value = boost::lexical_cast<int>(val);
     else if (valParent && *valParent)
       value = boost::lexical_cast<int>(valParent);
   }

   void SetPropertyValue(const TiXmlElement& el, CFileItemPtr& item, const string& propName, const char* attrName)
   {
     const char* pVal = el.Attribute(attrName);
     if (pVal && *pVal != 0)
       item->SetProperty(propName, pVal);
   }

   string BuildDurationString(const string& duration)
   {
     if (duration.size() > 0)
     {
       int seconds = boost::lexical_cast<int>(duration)/1000;
       int hours = seconds/3600;
       int minutes = (seconds / 60) % 60;
       seconds = seconds % 60;

       CStdString std;
       if (hours > 0)
         std.Format("%d:%02d:%02d", hours, minutes, seconds);
       else
         std.Format("%d:%02d", minutes, seconds);

       return std;
     }

     return "";
   }

 protected:

   int TypeStringToNumber(const CStdString& type)
   {
     if (type == "show")
       return PLEX_METADATA_SHOW;
     else if (type == "episode")
       return PLEX_METADATA_EPISODE;
     else if (type == "movie")
       return PLEX_METADATA_MOVIE;
     else if (type == "artist")
       return PLEX_METADATA_ARTIST;
     else if (type == "album")
       return PLEX_METADATA_ALBUM;
     else if (type == "track")
       return PLEX_METADATA_TRACK;
     else if (type == "clip")
       return PLEX_METADATA_CLIP;
     else if (type == "person")
       return PLEX_METADATA_PERSON;

     return -1;
   }
};

class PlexMediaNodeLibrary : public PlexMediaNode
{
 public:

  string ComputeMediaUrl(const string& parentPath, TiXmlElement* media, string& localPath)
  {
    string ret;

    vector<string> urls;
    vector<string> localPaths;

    // Collect the URLs.
    for (TiXmlElement* part = media->FirstChildElement(); part; part=part->NextSiblingElement())
    {
      if (part->Attribute("key"))
      {
        string url = CPlexDirectory::ProcessUrl(parentPath, part->Attribute("key"), false);
        urls.push_back(url);

        CStdString path = part->Attribute("file");
        CUtil::UrlDecode(path);
        localPaths.push_back(path);
      }
    }

    if (urls.size() > 0)
    {
      // See if we need a stack or not.
      ret = urls[0];
      localPath = localPaths[0];

      if (urls.size() > 1 && boost::iequals(CUtil::GetExtension(urls[0]), ".ifo") == false)
      {
        ret = localPath = "stack://";

        for (int i=0; i<urls.size(); i++)
        {
          ret += urls[i];
          localPath += localPaths[i];

          if (i < urls.size()-1)
          {
            ret += " , ";
            localPath += " , ";
          }
        }
      }
    }

    return ret;
  }

  void DoBuildTrackItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    TiXmlElement* parent = (TiXmlElement* )el.Parent();
    CSong song;

    // Artist, album, year.
    SetValue(el, *parent, song.strArtist, "grandparentTitle");
    SetValue(el, *parent, song.strAlbum, "parentTitle");
    SetValue(el, *parent, song.iYear, "parentYear");

    song.strTitle = GetLabel(el);

    // Duration.
    SetValue(el, song.iDuration, "duration");
    song.iDuration /= 1000;

    // Track number.
    SetValue(el, song.iTrack, "index");

    // Media tags.
    const char* pRoot = parent->Attribute("mediaTagPrefix");
    const char* pVersion = parent->Attribute("mediaTagVersion");

    vector<CFileItemPtr> mediaItems;
    for (TiXmlElement* media = el.FirstChildElement(); media; media=media->NextSiblingElement())
    {
      if (media->ValueStr() == "Media")
      {
        // Replace the item.
        CFileItemPtr newItem(new CFileItem(song));
        newItem->m_strPath = pItem->m_strPath;
        pItem = newItem;

        // Check for indirect.
        if (media->Attribute("indirect") && strcmp(media->Attribute("indirect"), "1") == 0)
          pItem->SetProperty("indirect", 1);

        const char* bitrate = el.Attribute("bitrate");
        if (bitrate && strlen(bitrate) > 0)
        {
          pItem->m_dwSize = boost::lexical_cast<int>(bitrate);
          pItem->GetMusicInfoTag()->SetAlbum(bitrate);
        }

        // Summary.
        SetPropertyValue(*parent, pItem, "description", "summary");

        // Path to the track.
        string localPath;
        string url = ComputeMediaUrl(parentPath, media, localPath);
        pItem->m_strPath = url;
        song.strFileName = pItem->m_strPath;

        if (pRoot && pVersion)
        {
          string url = CPlexDirectory::ProcessUrl(parentPath, pRoot, false);
          CacheMediaThumb(pItem, media, url, "audioChannels", pVersion);
          CacheMediaThumb(pItem, media, url, "audioCodec", pVersion);
          CacheMediaThumb(pItem, &el, url, "contentRating", pVersion);
          CacheMediaThumb(pItem, &el, url, "studio", pVersion);
        }

        // We're done.
        break;
      }
    }

    pItem->m_bIsFolder = false;

    // Summary.
    SetPropertyValue(el, pItem, "description", "summary");
  }

  virtual void DoBuildVideoFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    string type = el.Attribute("type");

    // Top level data.
    CVideoInfoTag videoInfo;
    SetValue(el, videoInfo.m_strTitle, "title");
    SetValue(el, videoInfo.m_strOriginalTitle, "originalTitle");
    videoInfo.m_strPlot = videoInfo.m_strPlotOutline = el.Attribute("summary");
    SetValue(el, videoInfo.m_iYear, "year");

    // Show, season.
    SetValue(el, *(TiXmlElement* )el.Parent(), videoInfo.m_strShowTitle, "grandparentTitle");

    // Indexes.
    if (type == "episode")
    {
      SetValue(el, videoInfo.m_iEpisode, "index");
      SetValue(el, *((TiXmlElement* )el.Parent()), videoInfo.m_iSeason, "parentIndex");
    }
    else if (type == "show")
    {
      videoInfo.m_strShowTitle = videoInfo.m_strTitle;
    }

    // Tagline.
    SetValue(el, videoInfo.m_strTagLine, "tagline");

    // Views.
    int viewCount = 0;
    SetValue(el, viewCount, "viewCount");

    vector<CFileItemPtr> mediaItems;
    for (TiXmlElement* media = el.FirstChildElement(); media; media=media->NextSiblingElement())
    {
      if (media->ValueStr() == "Media")
      {
        // Create a new file item.
        CVideoInfoTag theVideoInfo = videoInfo;

        // Compute the URL.
        string localPath;
        string url = ComputeMediaUrl(parentPath, media, localPath);
        videoInfo.m_strFile = url;
        videoInfo.m_strFileNameAndPath = url;

        // Duration.
        const char* pDuration = el.Attribute("duration");
        if (pDuration && strlen(pDuration) > 0)
          theVideoInfo.m_strRuntime = BuildDurationString(pDuration);

        // Viewed.
        theVideoInfo.m_playCount = viewCount;

        // Create the file item.
        CFileItemPtr theMediaItem(new CFileItem(theVideoInfo));

        // Check for indirect.
        if (media->Attribute("indirect") && strcmp(media->Attribute("indirect"), "1") == 0)
        {
          pItem->SetProperty("indirect", 1);
          theMediaItem->SetProperty("indirect", 1);
        }

        // If it's not an STRM file then save the local path.
        if (CUtil::GetExtension(localPath) != ".strm")
          theMediaItem->SetProperty("localPath", localPath);

        theMediaItem->m_bIsFolder = false;
        theMediaItem->m_strPath = url;

        // See if the item has been played or is in progress.
        if (el.Attribute("viewOffset") != 0 && strlen(el.Attribute("viewOffset")) > 0)
          theMediaItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_IN_PROGRESS);
        else
          theMediaItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, viewCount > 0);

        // Bitrate.
        SetValue(*media, theMediaItem->m_iBitrate, "bitrate");

        // Build the URLs for the flags.
        TiXmlElement* parent = (TiXmlElement* )el.Parent();
        const char* pRoot = parent->Attribute("mediaTagPrefix");
        const char* pVersion = parent->Attribute("mediaTagVersion");
        if (pRoot && pVersion)
        {
          string url = CPlexDirectory::ProcessUrl(parentPath, pRoot, false);
          CacheMediaThumb(theMediaItem, media, url, "aspectRatio", pVersion);
          CacheMediaThumb(theMediaItem, media, url, "audioChannels", pVersion);
          CacheMediaThumb(theMediaItem, media, url, "audioCodec", pVersion);
          CacheMediaThumb(theMediaItem, media, url, "videoCodec", pVersion);
          CacheMediaThumb(theMediaItem, media, url, "videoResolution", pVersion);
          CacheMediaThumb(theMediaItem, media, url, "videoFrameRate", pVersion);

          // From the metadata item.
          if (el.Attribute("contentRating"))
            CacheMediaThumb(theMediaItem, &el, url, "contentRating", pVersion);
          else
            CacheMediaThumb(theMediaItem, parent, url, "grandparentContentRating", pVersion, "contentRating");

          if (el.Attribute("studio"))
            CacheMediaThumb(theMediaItem, &el, url, "studio", pVersion);
          else
            CacheMediaThumb(theMediaItem, parent, url, "grandparentStudio", pVersion, "studio");
        }

        // But we add each one to the list.
        mediaItems.push_back(theMediaItem);
      }
    }

    pItem = mediaItems[0];
    pItem->m_mediaItems = mediaItems;
  }

  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    string type = el.Attribute("type");

    // Check for track.
    if (type == "track")
      DoBuildTrackItem(pItem, parentPath, el);
    else
      DoBuildVideoFileItem(pItem, parentPath, el);
  }

>>>>>>> 5628c23... First take on universal search within Plex.
};

class PlexMediaDirectory : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->SetLabel(GetLabel(el));
    CVideoInfoTag tag;
    tag.m_strTitle = pItem->GetLabel();

    // Summary.
    const char* summary = el.Attribute("summary");
    if (summary)
    {
      pItem->SetProperty("description", summary);
      tag.m_strPlot = tag.m_strPlotOutline = summary;
    }

    CFileItemPtr newItem(new CFileItem(tag));
    newItem->m_bIsFolder = true;
    newItem->m_strPath = pItem->m_strPath;
    newItem->SetProperty("description", pItem->GetProperty("description"));
    pItem = newItem;

    // Watch/unwatched.
    if (el.Attribute("leafCount") && el.Attribute("viewedLeafCount"))
    {
      int count = boost::lexical_cast<int>(el.Attribute("leafCount"));
      int watchedCount = boost::lexical_cast<int>(el.Attribute("viewedLeafCount"));
      pItem->SetEpisodeData(count, watchedCount);

      pItem->GetVideoInfoTag()->m_iEpisode = count;
      pItem->GetVideoInfoTag()->m_playCount = (count == watchedCount) ? 1 : 0;
      pItem->SetOverlayImage(CGUIListItem::ICON_OVERLAY_UNWATCHED, pItem->GetVideoInfoTag()->m_playCount > 0);
    }

    // Music.
    if (type == "artist")
    {
      pItem->SetProperty("artist", pItem->GetLabel());
    }
    else if (type == "album")
    {
      if (el.Attribute("parentTitle"))
        pItem->SetProperty("artist", el.Attribute("parentTitle"));

      pItem->SetProperty("album", pItem->GetLabel());
      if (el.Attribute("year"))
        pItem->SetLabel2(el.Attribute("year"));
    }
    else if (type == "track")
    {
      if (el.Attribute("index"))
        pItem->SetProperty("index", el.Attribute("index"));
    }

    // Check for special directories.
    const char* search = el.Attribute("search");
    const char* prompt = el.Attribute("prompt");
    const char* settings = el.Attribute("settings");

    // Check for search directory.
    if (search && strlen(search) > 0 && prompt)
    {
      string strSearch = search;
      if (strSearch == "1")
      {
        pItem->m_bIsSearchDir = true;
        pItem->m_strSearchPrompt = prompt;
      }
    }

    // Check for popup menus.
    const char* popup = el.Attribute("popup");
    if (popup && strlen(popup) > 0)
    {
      string strPopup = popup;
      if (strPopup == "1")
        pItem->m_bIsPopupMenuItem = true;
    }

    // Check for preferences.
    if (settings && strlen(settings) > 0)
    {
      string strSettings = settings;
      if (strSettings == "1")
        pItem->m_bIsSettingsDir = true;
    }
  }
};

class PlexMediaObject : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el) {}
};

class PlexMediaArtist : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->SetLabel(el.Attribute("artist"));
  }
};

class PlexMediaAlbum : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    CAlbum album;

    album.strLabel = GetLabel(el);
    //album.idAlbum = boost::lexical_cast<int>(el.Attribute("key"));
    album.strAlbum = el.Attribute("album");
    album.strArtist = el.Attribute("artist");
    album.strGenre = el.Attribute("genre");
    album.iYear = boost::lexical_cast<int>(el.Attribute("year"));

    CFileItemPtr newItem(new CFileItem(pItem->m_strPath, album));
    pItem = newItem;
  }
};

class PlexMediaPodcast : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    CAlbum album;

    album.strLabel = el.Attribute("title");
    album.idAlbum = boost::lexical_cast<int>(el.Attribute("key"));
    album.strAlbum = el.Attribute("title");

    if (strlen(el.Attribute("year")) > 0)
      album.iYear = boost::lexical_cast<int>(el.Attribute("year"));

    CFileItemPtr newItem(new CFileItem(pItem->m_strPath, album));
    pItem = newItem;
  }
};

class PlexMediaPlaylist : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->SetLabel(el.Attribute("title"));
  }
};

class PlexMediaGenre : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->SetLabel(el.Attribute("genre"));
  }
};

class PlexMediaProvider : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->m_strPath = CPlexDirectory::ProcessUrl(parentPath, el.Attribute("key"), false);
    pItem->SetLabel(el.Attribute("title"));
    pItem->SetProperty("type", el.Attribute("type"));
    pItem->SetProperty("provider", "1");
  }
};

class PlexMediaVideo : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->m_bIsFolder = false;

    CURL parentURL(parentPath);
    CVideoInfoTag videoInfo;
    videoInfo.m_strTitle = el.Attribute("title");
    videoInfo.m_strPlot = videoInfo.m_strPlotOutline = el.Attribute("summary");

    const char* rating = el.Attribute("rating");
    if (rating && strlen(rating) > 0)
      videoInfo.m_fRating = atof(rating);

    const char* year = el.Attribute("year");
    if (year)
      videoInfo.m_iYear = boost::lexical_cast<int>(year);

    const char* pDuration = el.Attribute("duration");
    if (pDuration && strlen(pDuration) > 0)
    {
      string duration = pDuration;
      int seconds = boost::lexical_cast<int>(duration)/1000;
      int hours = seconds/3600;
      int minutes = (seconds / 60) % 60;
      seconds = seconds % 60;

      CStdString std;
      if (hours > 0)
        std.Format("%d:%02d:%02d", hours, minutes, seconds);
      else
        std.Format(":%d:%02d", minutes, seconds);

      videoInfo.m_strRuntime = std;
    }

#if 0
    // NOT YET, messes up display in file mode.

    // TV show information.
    const char* season = el.Attribute("season");
    if (season)
      videoInfo.m_iSeason = boost::lexical_cast<int>(season);

    const char* episode = el.Attribute("episode");
    if (episode)
      videoInfo.m_iEpisode = boost::lexical_cast<int>(episode);

    const char* show = el.Attribute("show");
    if (show)
      videoInfo.m_strShowTitle = show;
#endif

    // Path to the track itself.
    CURL url2(pItem->m_strPath);
    if (url2.GetProtocol() == "plex" && url2.GetFileName().Find("video/:/") == -1)
    {
      url2.SetProtocol("http");
      url2.SetPort(32400);
      url2.GetURL(pItem->m_strPath);
    }

    videoInfo.m_strFile = pItem->m_strPath;
    videoInfo.m_strFileNameAndPath = pItem->m_strPath;

    CFileItemPtr newItem(new CFileItem(videoInfo));
    newItem->m_bIsFolder = false;
    newItem->m_strPath = pItem->m_strPath;
    pItem = newItem;

    // Support for specifying RTMP play paths.
    const char* pPlayPath = el.Attribute("rtmpPlayPath");
    if (pPlayPath && strlen(pPlayPath) > 0)
      pItem->SetProperty("PlayPath", pPlayPath);
  }

  virtual void ComputeLabels(const string& strPath, string& strFileLabel, string& strSecondFileLabel, string& strDirLabel, string& strSecondDirLabel)
  {
    strDirLabel = "%B";
    strFileLabel = "%K";
    strSecondDirLabel = "%D";
  }
};

class PlexMediaTrack : public PlexMediaNode
{
 public:
  PlexMediaTrack()
    : m_hasBitrate(false)
    , m_hasDuration(false) {}

 private:
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->m_bIsFolder = false;

    CSong song;
    song.strTitle = GetLabel(el);
    song.strArtist = el.Attribute("artist");
    song.strAlbum = el.Attribute("album");
    song.strFileName = pItem->m_strPath;

    // Old-school.
    const char* totalTime = el.Attribute("totalTime");
    if (totalTime && strlen(totalTime) > 0)
    {
      song.iDuration = boost::lexical_cast<int>(totalTime)/1000;
      m_hasDuration = true;
    }

    // Standard.
    totalTime = el.Attribute("duration");
    if (totalTime && strlen(totalTime) > 0)
      song.iDuration = boost::lexical_cast<int>(totalTime)/1000;

    const char* trackNumber = el.Attribute("index");
    if (trackNumber && strlen(trackNumber) > 0)
      song.iTrack = boost::lexical_cast<int>(trackNumber);

    // Replace the item.
    CFileItemPtr newItem(new CFileItem(song));
    newItem->m_strPath = pItem->m_strPath;
    pItem = newItem;

    const char* bitrate = el.Attribute("bitrate");
    if (bitrate && strlen(bitrate) > 0)
    {
      pItem->m_dwSize = boost::lexical_cast<int>(bitrate);
      pItem->GetMusicInfoTag()->SetAlbum(bitrate);
      m_hasBitrate = true;
    }

    // Path to the track.
    pItem->m_strPath = CPlexDirectory::ProcessUrl(parentPath, el.Attribute("key"), false);

    // Summary.
    const char* summary = el.Attribute("summary");
    if  (summary)
      pItem->SetProperty("description", summary);
  }

  virtual void ComputeLabels(const string& strPath, string& strFileLabel, string& strSecondFileLabel, string& strDirLabel, string& strSecondDirLabel)
  {
    strDirLabel = "%B";

    // Use bitrate if it's available and duration is not.
    if (m_hasBitrate == true && m_hasDuration == false)
      strSecondFileLabel = "%B kbps";

    if (strPath.find("/Artists/") != -1 || strPath.find("/Genre/") != -1 || strPath.find("/Albums/") != -1 ||
        strPath.find("/Recently%20Played/By%20Album/") != -1 || strPath.find("Recently%20Played/By%20Artist") != -1 ||
        strPath.find("/Recently%20Added/") != -1)
      strFileLabel = "%N - %T";
    else if (strPath.find("/Compilations/") != -1)
      strFileLabel = "%N - %A - %T";
    else if (strPath.find("/Tracks/") != -1)
      strFileLabel = "%T - %A";
    else if (strPath.find("/Podcasts/") != -1)
      strFileLabel = "%T";
    else
      strFileLabel = "%A - %T";
  }

 private:

  bool m_hasBitrate;
  bool m_hasDuration;
};

class PlexMediaRoll : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->SetLabel(GetLabel(el));
  }
};

class PlexMediaPhotoAlbum : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->SetLabel(GetLabel(el));
  }
};

class PlexMediaPhotoKeyword : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->SetLabel(GetLabel(el));
  }
};

class PlexMediaPhoto : public PlexMediaNode
{
  virtual void DoBuildFileItem(CFileItemPtr& pItem, const string& parentPath, TiXmlElement& el)
  {
    pItem->m_bIsFolder = false;
    pItem->SetLabel(GetLabel(el));

    // Summary.
    const char* summary = el.Attribute("summary");
    if  (summary)
      pItem->SetProperty("description", summary);

    // Path to the photo.
    pItem->m_strPath = CPlexDirectory::ProcessUrl(parentPath, el.Attribute("key"), false);
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
PlexMediaNode* PlexMediaNode::Create(const string& name)
{
  // FIXME: Move to using factory pattern.
  if (name == "Directory")
    return new PlexMediaDirectory();
  else if (name == "Artist")
    return new PlexMediaArtist();
  else if (name == "Album")
    return new PlexMediaAlbum();
  else if (name == "Playlist")
    return new PlexMediaPlaylist();
  else if (name == "Podcast")
    return new PlexMediaPodcast();
  else if (name == "Track")
    return new PlexMediaTrack();
  else if (name == "Genre")
    return new PlexMediaGenre();
  else if (name == "Roll")
    return new PlexMediaRoll();
  else if (name == "Photo")
    return new PlexMediaPhoto();
  else if (name == "PhotoAlbum")
    return new PlexMediaPhotoAlbum();
  else if (name == "PhotoKeyword")
    return new PlexMediaPhotoKeyword();
  else if (name == "Video")
    return new PlexMediaVideo();
  else if (name == "Provider")
    return new PlexMediaProvider();
  else
    printf("ERROR: Unknown class [%s]\n", name.c_str());

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CPlexDirectory::Parse(const CURL& url, TiXmlElement* root, CFileItemList &items, string& strFileLabel, string& strSecondFileLabel, string& strDirLabel, string& strSecondDirLabel)
{
  PlexMediaNode* mediaNode = 0;

  for (TiXmlElement* element = root->FirstChildElement(); element; element=element->NextSiblingElement())
  {
    mediaNode = PlexMediaNode::Create(element->Value());
    if (mediaNode != 0)
    {
      CFileItemPtr item = mediaNode->BuildFileItem(url, *element);
      if (item)
        items.Add(item);
    }
  }

  if (mediaNode != 0)
  {
    CStdString strURL;
    url.GetURL(strURL);
    mediaNode->ComputeLabels(strURL, strFileLabel, strSecondFileLabel, strDirLabel, strSecondDirLabel);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CPlexDirectory::Process()
{
  CURL url(m_url);
  CStdString protocol = url.GetProtocol();
  url.SetProtocol("http");
  url.SetPort(32400);

  // Set request headers.
  m_http.SetRequestHeader("X-Plex-Version", Cocoa_GetAppVersion());
  m_http.SetRequestHeader("X-Plex-Language", Cocoa_GetLanguage());

  m_http.SetTimeout(m_timeout);
  if (m_http.Open(url, false) == false)
  {
    CLog::Log(LOGERROR, "%s - Unable to get Plex Media Server directory", __FUNCTION__);
    m_bSuccess = false;
    m_downloadEvent.Set();
    return;
  }

  // Restore protocol.
  url.SetProtocol(protocol);

  CStdString content = m_http.GetContent();
  if (content.Equals("text/xml;charset=utf-8") == false && content.Equals("application/xml") == false)
  {
    CLog::Log(LOGERROR, "%s - Invalid content type %s", __FUNCTION__, content.c_str());
    m_bSuccess = false;
    m_downloadEvent.Set();
  }
  else
  {
    int size_read = 0;
    int size_total = (int)m_http.GetLength();
    int data_size = 0;

    m_data.reserve(size_total);
    printf("Content-Length was %d bytes\n", size_total);

    // Read response from server into string buffer.
    char buffer[4096];
    while (m_bStop == false && (size_read = m_http.Read(buffer, sizeof(buffer)-1)) > 0)
    {
      buffer[size_read] = 0;
      m_data += buffer;
      data_size += size_read;
    }

    // If we didn't get it all, we failed.
    if (m_data.size() != size_total)
      m_bSuccess = false;
  }

  m_http.Close();
  m_downloadEvent.Set();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CPlexDirectory::OnExit()
{
  m_bStop = true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CPlexDirectory::StopThread()
{
  m_bStop = true;
  CThread::StopThread();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
string CPlexDirectory::ProcessUrl(const string& parent, const string& url, bool isDirectory)
{
  CURL theURL(parent);

  // Files use plain HTTP.
  if (isDirectory == false)
  {
    theURL.SetProtocol("http");
    theURL.SetPort(32400);
  }

  if (url.find("://") != -1)
  {
    // It's got its own protocol, so leave it alone.
    return url;
  }
  else if (url.find("/") == 0)
  {
    // Absolute off parent.
    theURL.SetFileName(url.substr(1));
  }
  else
  {
    // Relative off parent.
    string path = theURL.GetFileName();
    if (path[path.size() -1] != '/')
      path += '/';

    theURL.SetFileName(path + url);
  }

  CStdString newURL;
  theURL.GetURL(newURL);
  return newURL;
}

