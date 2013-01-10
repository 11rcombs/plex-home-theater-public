/*
 *  Copyright (C) 2011 Plex, Inc.
 *
 *  Created on: Apr 10, 2011
 *      Author: Elan Feingold
 */

#pragma once

#include "GUISettings.h"
#include "NetworkServiceAdvertiser.h"
#include "CocoaUtils.h"
#include "PlexUtils.h"
#include "GUIInfoManager.h"

/////////////////////////////////////////////////////////////////////////////
class PlexNetworkServiceAdvertiser : public NetworkServiceAdvertiser
{
 public:
  
  /// Constructor.
  PlexNetworkServiceAdvertiser(boost::asio::io_service& ioService)
    : NetworkServiceAdvertiser(ioService, NS_BROADCAST_ADDR, NS_PLEX_MEDIA_CLIENT_PORT) {}
  
 protected:
  
  /// For subclasses to fill in.
  virtual void createReply(map<string, string>& headers) 
  {
    headers["Name"] = PlexUtils::GetHostName();
    headers["Port"] = g_guiSettings.GetString("services.webserverport");
    headers["Version"] = g_infoManager.GetVersion();
    headers["Product"] = "Plex Media Center (frodo)";
    headers["Protocol"] = "xbmcjson";
  }
  
  /// For subclasses to fill in.
  virtual string getType()
  {
    return "plex/media-player";
  }
  
  virtual string getResourceIdentifier() { return g_guiSettings.GetString("system.uuid");  }
  virtual string getBody() { return ""; }
};
