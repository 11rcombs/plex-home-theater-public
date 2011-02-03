//
//  GUIDialogPlexPluginSettings.h
//  Plex
//
//  Created by Jamie Kirkpatrick on 03/02/2011.
//  Copyright 2011 Kirk Consulting Limited. All rights reserved.
//

#pragma once

#include "tinyXML/tinyxml.h"
#include "URL.h"
#include "GUIDialogBoxBase.h"

class CPlexPluginSettings
{
public:
  void Set(const CStdString& key, const CStdString& value);
  CStdString Get(const CStdString& key);
  
  bool Load(TiXmlElement* root);
  bool Save(const CStdString& strPath);
  
  TiXmlElement* GetPluginRoot();
  
private: 
  TiXmlDocument   m_userXmlDoc;
  TiXmlDocument   m_pluginXmlDoc;
};

class CGUIDialogPlexPluginSettings : public CGUIDialogBoxBase
{
public:
  CGUIDialogPlexPluginSettings(CPlexPluginSettings& settings, const CStdString& heading);
  virtual bool OnMessage(CGUIMessage& message);
  static void ShowAndGetInput(const CStdString& path, const CStdString& compositeXml);
  
private:
  void CreateControls();
  void FreeControls();
  void EnableControls();
  void SetDefaults();
  bool GetCondition(const CStdString &condition, const int controlId);

  bool SaveSettings(void);
  bool ShowVirtualKeyboard(int iControl);
  bool TranslateSingleString(const CStdString &strCondition, std::vector<CStdString> &enableVec);
  CPlexPluginSettings& m_settings;
  CStdString m_strHeading;
  bool m_okSelected; 
};
