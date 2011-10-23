/*!
\file GUIFontManager.h
\brief
*/

#ifndef GUILIB_FONTMANAGER_H
#define GUILIB_FONTMANAGER_H

#pragma once

/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
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

#include "GraphicContext.h"
#include "IMsgTargetCallback.h"

// Forward
class CGUIFont;
class CGUIFontTTFBase;
class TiXmlDocument;
class TiXmlNode;

struct OrigFontInfo
{
   int size;
   float aspect;
   CStdString fontFilePath;
   CStdString fileName;
   CStdString variant;
   RESOLUTION sourceRes;
   bool preserveAspect;
   bool border;
};

/*!
 \ingroup textures
 \brief
 */
class GUIFontManager : public IMsgTargetCallback
{
public:
  GUIFontManager(void);
  virtual ~GUIFontManager(void);

  virtual bool OnMessage(CGUIMessage &message);

  void Unload(const CStdString& strFontName);
  void LoadFonts(const CStdString& strFontSet);
  CGUIFont* LoadTTF(const CStdString& strFontName, const CStdString& strFilename, color_t textColor, color_t shadowColor, const int iSize, int iStyle, bool border = false, float lineSpacing = 1.0f, float aspect = 1.0f, RESOLUTION res = RES_INVALID, bool preserveAspect = false, const CStdString& variant = "");
  CGUIFont* GetFont(const CStdString& strFontName, bool fallback = true);
  std::vector<std::string> GetSystemFontNames();

  /*! \brief return a default font
   \param border whether the font should be a font with an outline
   \return the font.  NULL if no default font can be found.
   */
  CGUIFont* GetDefaultFont(bool border = false);

  void Clear();
  void FreeFontFile(CGUIFontTTFBase *pFont);

  bool IsFontSetUnicode() { return m_fontsetUnicode; }
  bool IsFontSetUnicode(const CStdString& strFontSet);
  bool GetFirstFontSetUnicode(CStdString& strFontSet);

protected:
  void RescaleFontSizeAndAspect(float *size, float *aspect, RESOLUTION sourceRes, bool preserveAspect) const;
  void ReloadTTFFonts();
  void LoadFonts(const TiXmlNode* fontNode);
  CGUIFontTTFBase* GetFontFile(const CStdString& strFontFile);
  bool OpenFontFile(TiXmlDocument& xmlDoc);
  bool FindSystemFontPath(const CStdString& strFilename, CStdString *fontPath);
  bool GetFontAlias(const CStdString& strFontName, const CStdString& strVariant, CStdString& strAlias, int& aliasStyle);

  std::map<std::string, std::pair<std::string, int> > m_fontAliasMap;
  std::vector<CGUIFont*> m_vecFonts;
  std::vector<CGUIFontTTFBase*> m_vecFontFiles;
  std::vector<OrigFontInfo> m_vecFontInfo;
  bool m_fontsetUnicode;
  RESOLUTION m_skinResolution;
  bool m_canReload;
};

/*!
 \ingroup textures
 \brief
 */
extern GUIFontManager g_fontManager;
#endif