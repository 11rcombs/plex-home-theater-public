//
//  PlexDirectory.cpp
//  Plex
//
//  Created by Tobias Hieta <tobias@plexapp.com> on 2013-04-05.
//  Copyright 2013 Plex Inc. All rights reserved.
//

#include "PlexDirectory.h"
#include "filesystem/FileFactory.h"
#include "File.h"
#include "XBMCTinyXML.h"
#include "utils/log.h"
#include "JobManager.h"

#include "PlexAttributeParser.h"
#include "PlexDirectoryTypeParser.h"

#include "video/VideoInfoTag.h"

#include <boost/assign/list_of.hpp>
#include <boost/bimap.hpp>
#include <boost/foreach.hpp>
#include <map>

using namespace XFILE;

/* IDirectory Interface */
bool
CPlexDirectory::GetDirectory(const CStdString& strPath, CFileItemList& fileItems)
{
  m_url = CURL(strPath);

  CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory %s", strPath.c_str());

  if (boost::ends_with(m_url.GetFileName(), "/children/"))
  {
    CURL augmentUrl = m_url;
    CStdString newFile = m_url.GetFileName();
    boost::replace_last(newFile, "/children", "");
    augmentUrl.SetFileName(newFile);
    AddAugmentation(augmentUrl);
  }

  CXBMCTinyXML doc;

  if (!doc.LoadFile(strPath))
  {
    CLog::Log(LOGERROR, "CPlexDirectory::GetDirectory failed to parse XML from %s", strPath.c_str());
    CancelAugmentations();
    return false;
  }

  if (!ReadMediaContainer(doc.RootElement(), fileItems))
  {
    CLog::Log(LOGERROR, "CPlexDirectory::GetDirectory failed to read root MediaContainer from %s", strPath.c_str());
    CancelAugmentations();
    return false;
  }

  if (m_isAugmented)
    DoAugmentation(fileItems);

  CLog::Log(LOGDEBUG, "CPlexDirectory::GetDirectory returning a directory with %d items with content %s", fileItems.Size(), fileItems.GetContent().c_str());

  return true;
}

void
CPlexDirectory::CancelDirectory()
{
  CancelAugmentations();
}

typedef boost::bimap<EPlexDirectoryType, CStdString> DirectoryTypeMap;
static DirectoryTypeMap g_typeMap = boost::assign::list_of<DirectoryTypeMap::relation>
                                    (PLEX_DIR_TYPE_UNKNOWN, "unknown")
                                    (PLEX_DIR_TYPE_MOVIE, "movie")
                                    (PLEX_DIR_TYPE_SHOW, "show")
                                    (PLEX_DIR_TYPE_SEASON, "season")
                                    (PLEX_DIR_TYPE_EPISODE, "episode")
                                    (PLEX_DIR_TYPE_ARTIST, "artist")
                                    (PLEX_DIR_TYPE_ALBUM, "album")
                                    (PLEX_DIR_TYPE_TRACK, "track")
                                    (PLEX_DIR_TYPE_PHOTO, "photo")
                                    (PLEX_DIR_TYPE_VIDEO, "video")
                                    (PLEX_DIR_TYPE_DIRECTORY, "directory")
                                    (PLEX_DIR_TYPE_SECTION, "section")
                                    (PLEX_DIR_TYPE_SERVER, "server")
                                    (PLEX_DIR_TYPE_DEVICE, "device")
                                    (PLEX_DIR_TYPE_SYNCITEM, "syncitem")
                                    (PLEX_DIR_TYPE_MEDIASETTINGS, "mediasettings")
                                    (PLEX_DIR_TYPE_POLICY, "policy")
                                    (PLEX_DIR_TYPE_LOCATION, "location")
                                    (PLEX_DIR_TYPE_MEDIA, "media")
                                    (PLEX_DIR_TYPE_PART, "part")
                                    (PLEX_DIR_TYPE_SYNCITEMS, "syncitems")
                                    (PLEX_DIR_TYPE_STREAM, "stream")
                                    (PLEX_DIR_TYPE_STATUS, "status")
                                    (PLEX_DIR_TYPE_TRANSCODEJOB, "transcodejob")
                                    (PLEX_DIR_TYPE_TRANSCODESESSION, "transcodesession")
                                    (PLEX_DIR_TYPE_PROVIDER, "provider")
                                    (PLEX_DIR_TYPE_CLIP, "clip")
                                    (PLEX_DIR_TYPE_PLAYLIST, "playlist")
                                    (PLEX_DIR_TYPE_CHANNEL, "channel")
                                    (PLEX_DIR_TYPE_SECONDARY, "secondary")
                                    (PLEX_DIR_TYPE_GENRE, "genre")
                                    (PLEX_DIR_TYPE_ROLE, "role")
                                    (PLEX_DIR_TYPE_WRITER, "writer")
                                    (PLEX_DIR_TYPE_PRODUCER, "producer")
                                    (PLEX_DIR_TYPE_COUNTRY, "country")
                                    (PLEX_DIR_TYPE_DIRECTOR, "director")
                                    ;


CPlexAttributeParserBase *g_parserInt = new CPlexAttributeParserInt;
CPlexAttributeParserBase *g_parserBool = new CPlexAttributeParserBool;
CPlexAttributeParserBase *g_parserKey = new CPlexAttributeParserKey;
CPlexAttributeParserBase *g_parserMediaUrl = new CPlexAttributeParserMediaUrl;
CPlexAttributeParserBase *g_parserType = new CPlexAttributeParserType;
CPlexAttributeParserBase *g_parserLabel = new CPlexAttributeParserLabel;
CPlexAttributeParserBase *g_parserMediaFlag = new CPlexAttributeParserMediaFlag;
CPlexAttributeParserBase *g_parserDateTime = new CPlexAttributeParserDateTime;

typedef std::map<CStdString, CPlexAttributeParserBase*> AttributeMap;
typedef std::pair<CStdString, CPlexAttributeParserBase*> AttributePair;
static AttributeMap g_attributeMap = boost::assign::list_of<AttributePair>
                                     ("size", g_parserInt)
                                     ("channels", g_parserInt)
                                     ("createdAt", g_parserInt)
                                     ("updatedAt", g_parserInt)
                                     ("leafCount", g_parserInt)
                                     ("viewedLeafCount", g_parserInt)
                                     ("ratingKey", g_parserInt)
                                     ("bitrate", g_parserInt)
                                     ("duration", g_parserInt)
                                     ("librarySectionID", g_parserInt)
                                     ("streamType", g_parserInt)
                                     ("index", g_parserInt)
                                     ("channels", g_parserInt)
                                     ("bitrate", g_parserInt)
                                     ("samplingRate", g_parserInt)
                                     ("dialogNorm", g_parserInt)


                                     ("filters", g_parserBool)
                                     ("refreshing", g_parserBool)
                                     ("allowSync", g_parserBool)
                                     ("secondary", g_parserBool)
                                     ("search", g_parserBool)
                                     ("selected", g_parserBool)

                                     ("key", g_parserKey)
                                     ("theme", g_parserKey)
                                     ("parentKey", g_parserKey)
                                     ("grandparentKey", g_parserKey)

                                     ("thumb", g_parserMediaUrl)
                                     ("art", g_parserMediaUrl)
                                     ("poster", g_parserMediaUrl)
                                     ("banner", g_parserMediaUrl)
                                     ("parentThumb", g_parserMediaUrl)
                                     ("grandparentThumb", g_parserMediaUrl)

                                     /* Media flags */
                                     ("aspectRatio", g_parserMediaFlag)
                                     ("audioChannels", g_parserMediaFlag)
                                     ("audioCodec", g_parserMediaFlag)
                                     ("videoCodec", g_parserMediaFlag)
                                     ("videoResolution", g_parserMediaFlag)
                                     ("videoFrameRate", g_parserMediaFlag)
                                     ("contentRating", g_parserMediaFlag)
                                     ("grandparentContentRating", g_parserMediaFlag)
                                     ("studio", g_parserMediaFlag)
                                     ("grandparentStudio", g_parserMediaFlag)

                                     ("type", g_parserType)
                                     ("content", g_parserType)

                                     ("title", g_parserLabel)

                                     ("originallyAvailableAt", g_parserDateTime)
                                     ;

static CPlexAttributeParserBase* g_defaultAttr = new CPlexAttributeParserBase;

void
CPlexDirectory::CopyAttributes(TiXmlElement* el, CFileItem& item, const CURL &url)
{
  TiXmlAttribute *attr = el->FirstAttribute();

  while (attr)
  {
    CStdString key = attr->NameTStr();
    CStdString valStr = CStdString(attr->Value());

    if (g_attributeMap.find(key) != g_attributeMap.end())
    {
      CPlexAttributeParserBase* attr = g_attributeMap[key];
      attr->Process(url, key, valStr, item);
    }
    else
    {
      g_defaultAttr->Process(url, key, valStr, item);
    }

    attr = attr->Next();
  }
}

CFileItemPtr
CPlexDirectory::NewPlexElement(TiXmlElement *element, CFileItem &parentItem, const CURL &baseUrl)
{
  CFileItemPtr newItem = CFileItemPtr(new CFileItem);

  /* Make sure this is set before running copyattributes so that
   * mediaflag urls can be calculated correctly */
  if (parentItem.HasProperties())
  {
    if (parentItem.HasProperty("mediaTagPrefix"))
      newItem->SetProperty("mediaTagPrefix", parentItem.GetProperty("mediaTagPrefix").asString());
    if (parentItem.HasProperty("mediaTagVersion"))
      newItem->SetProperty("mediaTagVersion", parentItem.GetProperty("mediaTagVersion").asString());
  }

  CPlexDirectory::CopyAttributes(element, *newItem, baseUrl);

  if (newItem->GetPlexDirectoryType() == PLEX_DIR_TYPE_UNKNOWN)
  {
    /* no type attribute, let's try to use the name of the XML element */
    CPlexAttributeParserType t;
    t.Process(baseUrl, "type", element->ValueStr(), *newItem);
  }

  if (newItem->HasProperty("key"))
    newItem->SetPath(newItem->GetProperty("key").asString());

  newItem->SetProperty("plex", true);
  newItem->SetProperty("plexserver", baseUrl.GetHostName());

  return newItem;
}

void
CPlexDirectory::ReadChildren(TiXmlElement* root, CFileItemList& container)
{

  CPlexDirectoryTypeParserBase *directoryParser = NULL;
  EPlexDirectoryType lastType = PLEX_DIR_TYPE_UNKNOWN;

  for (TiXmlElement *element = root->FirstChildElement(); element; element = element->NextSiblingElement())
  {
    CFileItemPtr item = CPlexDirectory::NewPlexElement(element, container, m_url);

    EPlexDirectoryType currentType = item->GetPlexDirectoryType();

    if (currentType != lastType || !directoryParser)
    {
      directoryParser = CPlexDirectoryTypeParserBase::GetDirectoryTypeParser(currentType);
      lastType = currentType;
    }

    directoryParser->Process(*item, container, element);

    container.Add(item);
  }
}

bool
CPlexDirectory::ReadMediaContainer(TiXmlElement* root, CFileItemList& mediaContainer)
{
  if (root->ValueStr() != "MediaContainer")
  {
    CLog::Log(LOGWARNING, "CPlexDirectory::ReadMediaContainer got XML document without mediaContainer as root at %s", m_url.Get().c_str());
    return false;
  }

  /* common attributes */
  mediaContainer.SetPath(m_url.Get());
  mediaContainer.SetProperty("plex", true);
  mediaContainer.SetProperty("plexserver", m_url.GetHostName());

  CPlexDirectory::CopyAttributes(root, mediaContainer, m_url);
  ReadChildren(root, mediaContainer);

  /* We just use the first item Type, it might be wrong and we should maybe have a look... */
  if (mediaContainer.GetPlexDirectoryType() == PLEX_DIR_TYPE_UNKNOWN && mediaContainer.Size() > 0)
  {
    /* When loading a season the first element can be "All Episodes" and that is just a directory
     * without a type attribute. So let's skip that. */
    if (boost::ends_with(mediaContainer.Get(0)->GetProperty("unprocessed_key").asString(), "/allLeaves") &&
        mediaContainer.Size() > 1)
      mediaContainer.SetPlexDirectoryType(mediaContainer.Get(1)->GetPlexDirectoryType());
    else
      mediaContainer.SetPlexDirectoryType(mediaContainer.Get(0)->GetPlexDirectoryType());
  }

  /* now we need to set content to something that XBMC expects */
  CStdString content = CPlexDirectory::GetContentFromType(mediaContainer.GetPlexDirectoryType());
  if (!content.empty())
  {
    CLog::Log(LOGDEBUG, "CPlexDirectory::ReadMediaContainer setting content = %s", content.c_str());
    mediaContainer.SetContent(content);
  }

  return true;
}


EPlexDirectoryType
CPlexDirectory::GetDirectoryType(const CStdString &typeStr)
{
  DirectoryTypeMap::right_const_iterator it = g_typeMap.right.find(typeStr);
  if (it != g_typeMap.right.end())
    return it->second;
  return PLEX_DIR_TYPE_UNKNOWN;
}

CStdString
CPlexDirectory::GetDirectoryTypeString(EPlexDirectoryType typeNr)
{
  DirectoryTypeMap::left_const_iterator it = g_typeMap.left.find(typeNr);
  if (it != g_typeMap.left.end())
    return it->second;

  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
CStdString CPlexDirectory::GetContentFromType(EPlexDirectoryType typeNr)
{
  CStdString content;

  switch(typeNr)
  {
    case PLEX_DIR_TYPE_MOVIE:
      content = "movies";
      break;
    case PLEX_DIR_TYPE_SHOW:
      content = "tvshows";
      break;
    case PLEX_DIR_TYPE_SEASON:
      content = "seasons";
      break;
    case PLEX_DIR_TYPE_EPISODE:
      content = "episodes";
      break;
    case PLEX_DIR_TYPE_ARTIST:
      content = "artists";
      break;
    case PLEX_DIR_TYPE_ALBUM:
      content = "albums";
      break;
    case PLEX_DIR_TYPE_TRACK:
      content = "songs";
      break;
    case PLEX_DIR_TYPE_SECONDARY:
      content = "secondary";
      break;
    default:
      CLog::Log(LOGDEBUG, "CPlexDirectory::GetContentFromType oopes, no Content for Type %s", CPlexDirectory::GetDirectoryTypeString(typeNr).c_str());
      break;
  }

  return content;
}

////////////////////////////////////////////////////////////////////////////////
void CPlexDirectory::DoAugmentation(CFileItemList &fileItems)
{
  /* Wait for the agumentation to return for 5 seconds */
  CLog::Log(LOGDEBUG, "CPlexDirectory::DoAugmentation waiting for augmentation to download...");
  if (m_augmentationEvent.WaitMSec(5 * 1000))
  {
    CLog::Log(LOGDEBUG, "CPlexDirectory::DoAugmentation got it...");
    BOOST_FOREACH(CFileItemListPtr augList, m_augmentationItems)
    {
      if (augList->Size() > 0)
      {
        CFileItemPtr augItem = augList->Get(0);
        if ((fileItems.GetPlexDirectoryType() == PLEX_DIR_TYPE_SEASON ||
             fileItems.GetPlexDirectoryType() == PLEX_DIR_TYPE_EPISODE) &&
            augItem->GetPlexDirectoryType() == PLEX_DIR_TYPE_SHOW)
        {
          /* Augmentation of seasons works like this:
         * We load metadata/XX/children as our main url
         * then we load metadata/XX as augmentation, then we
         * augment our main CFileItem with information from the
         * first subitem from the augmentation URL.
         */

          std::pair<CStdString, CVariant> p;
          BOOST_FOREACH(p, augItem->m_mapProperties)
          {
            /* we only insert the properties if they are not available */
            if (fileItems.m_mapProperties.find(p.first) == fileItems.m_mapProperties.end())
            {
              fileItems.m_mapProperties[p.first] = p.second;
            }
          }

          fileItems.AppendArt(augItem->GetArt());

          CVideoInfoTag* infoTag = fileItems.GetVideoInfoTag();
          CVideoInfoTag* infoTag2 = augItem->GetVideoInfoTag();
          infoTag->m_genre.insert(infoTag->m_genre.end(), infoTag2->m_genre.begin(), infoTag2->m_genre.end());
        }
      }
    }
  }
  else
    CLog::Log(LOGWARNING, "CPlexDirectory::DoAugmentation failed to get augmentation URL");
}


////////////////////////////////////////////////////////////////////////////////
void CPlexDirectory::OnJobComplete(unsigned int jobID, bool success, CJob *job)
{
  CLog::Log(LOGDEBUG, "CPlexDirectory::OnJobComplete Augmentationjob complete...");

  CSingleLock lk(m_augmentationLock);

  if (success)
  {
    CPlexDirectoryFetchJob *fjob = static_cast<CPlexDirectoryFetchJob*>(job);
    m_augmentationItems.push_back(fjob->m_items);

    /* Fire off some more augmentation events if needed */
    CFileItemListPtr list = fjob->m_items;
    if (list->GetPlexDirectoryType() == PLEX_DIR_TYPE_SEASON &&
        list->Size() > 0 &&
        list->Get(0)->HasProperty("parentKey"))
    {
      lk.unlock();
      AddAugmentation(CURL(list->Get(0)->GetProperty("parentKey").asString()));
      lk.lock();
    }
  }

  m_augmentationJobs.erase(std::remove(m_augmentationJobs.begin(), m_augmentationJobs.end(), jobID), m_augmentationJobs.end());

  if (m_augmentationJobs.size() == 0)
    m_augmentationEvent.Set();
}


////////////////////////////////////////////////////////////////////////////////
bool CPlexDirectory::CPlexDirectoryFetchJob::DoWork()
{
  CPlexDirectory dir;

  m_items = CFileItemListPtr(new CFileItemList);
  return dir.GetDirectory(m_url.Get(), *m_items);
}
