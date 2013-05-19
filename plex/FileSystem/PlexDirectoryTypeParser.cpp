//
//  PlexDirectoryTypeParser.cpp
//  Plex
//
//  Created by Tobias Hieta <tobias@plexapp.com> on 2013-04-08.
//  Copyright 2013 Plex Inc. All rights reserved.
//

#include "PlexDirectoryTypeParser.h"
#include "video/VideoInfoTag.h"
#include "plex/PlexTypes.h"

#include "PlexDirectoryTypeParserVideo.h"
#include "PlexDirectoryTypeParserMusic.h"

static CPlexDirectoryTypeParserBase* videoParser = new CPlexDirectoryTypeParserVideo;
static CPlexDirectoryTypeParserBase* albumParser = new CPlexDirectoryTypeParserAlbum;
static CPlexDirectoryTypeParserBase* baseParser = new CPlexDirectoryTypeParserBase;

CPlexDirectoryTypeParserBase*
CPlexDirectoryTypeParserBase::GetDirectoryTypeParser(EPlexDirectoryType type)
{
  if (type == PLEX_DIR_TYPE_MOVIE ||
      type == PLEX_DIR_TYPE_SHOW ||
      type == PLEX_DIR_TYPE_EPISODE ||
      type == PLEX_DIR_TYPE_SEASON)
    return videoParser;

  else if (type == PLEX_DIR_TYPE_ALBUM)
    return albumParser;

  return baseParser;
}
