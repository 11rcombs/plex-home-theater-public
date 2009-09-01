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

#include "stdafx.h"
#include "DirectoryHistory.h"
#include "Util.h"

using namespace std;

CDirectoryHistory::CDirectoryHistory()
{
  m_strNull = "";
}

CDirectoryHistory::~CDirectoryHistory()
{}

void CDirectoryHistory::RemoveSelectedItem(const CStdString& strDirectory)
{
  CStdString strDir = strDirectory;
  strDir.ToLower();
  while (CUtil::HasSlashAtEnd(strDir) )
    strDir = strDir.Left(strDir.size() - 1);

  vector<CHistoryItem>::iterator Iter;
  for (Iter = m_vecHistory.begin();Iter != m_vecHistory.end(); Iter++)
  {
    if ( strDir == Iter->m_strDirectory)
    {
      m_vecHistory.erase(Iter);
      return ;
    }
  }
}

void CDirectoryHistory::SetSelectedItem(const CStdString& strSelectedItem, const CStdString& strDirectory, int selectedIndex)
{
  if (strSelectedItem.size() == 0) return ;
  // if (strDirectory.size()==0) return;
  CStdString strDir = strDirectory;
  strDir.ToLower();
  while (CUtil::HasSlashAtEnd(strDir) )
  {
    strDir = strDir.Left(strDir.size() - 1);
  }

  CStdString strItem = strSelectedItem;
  while (CUtil::HasSlashAtEnd(strItem) )
  {
    strItem = strItem.Left(strItem.size() - 1);
  }


  for (int i = 0; i < (int)m_vecHistory.size(); ++i)
  {
    CHistoryItem& item = m_vecHistory[i];
    if ( strDir == item.m_strDirectory)
    {
      item.m_strItem = strItem;
      item.m_index = selectedIndex;
      return ;
    }
  }

  CHistoryItem item;
  item.m_strItem = strItem;
  item.m_strDirectory = strDir;
  item.m_index = selectedIndex;
  m_vecHistory.push_back(item);
}

const CStdString& CDirectoryHistory::GetSelectedItem(const CStdString& strDirectory) const
{
  CStdString strDir = strDirectory;
  strDir.ToLower();
  while (CUtil::HasSlashAtEnd(strDir) )
  {
    strDir = strDir.Left(strDir.size() - 1);
  }
  for (int i = 0; i < (int)m_vecHistory.size(); ++i)
  {
    const CHistoryItem& item = m_vecHistory[i];
    if ( strDir == item.m_strDirectory)
    {

      return item.m_strItem;
    }
  }
  return m_strNull;
}

int CDirectoryHistory::GetSelectedIndex(const CStdString& strDirectory) const
{
  CStdString strDir = strDirectory;
  strDir.ToLower();
  while (CUtil::HasSlashAtEnd(strDir) )
  {
    strDir = strDir.Left(strDir.size() - 1);
  }
  for (int i = 0; i < (int)m_vecHistory.size(); ++i)
  {
    const CHistoryItem& item = m_vecHistory[i];
    if (strDir == item.m_strDirectory)
      return item.m_index;
  }
  return -1;
}

void CDirectoryHistory::AddPath(const CStdString& strPath)
{
  if ((m_vecPathHistory.size() == 0) || m_vecPathHistory.back() != strPath)
  {
    m_vecPathHistory.push_back(strPath);
  }
}

void CDirectoryHistory::AddPathFront(const CStdString& strPath)
{
  m_vecPathHistory.insert(m_vecPathHistory.begin(), strPath);
}

CStdString CDirectoryHistory::GetParentPath()
{
  CStdString strParent;
  if (m_vecPathHistory.size() > 0)
  {
    strParent = m_vecPathHistory.back();
  }

  return strParent;
}

CStdString CDirectoryHistory::RemoveParentPath()
{
  CStdString strParent;
  if (m_vecPathHistory.size() > 0)
  {
    strParent = m_vecPathHistory.back();
    m_vecPathHistory.pop_back();
  }

  return strParent;
}

void CDirectoryHistory::ClearPathHistory()
{
  m_vecPathHistory.clear();
}

void CDirectoryHistory::DumpPathHistory()
{
  // debug log
  CStdString strTemp;
  printf("Current m_vecPathHistory:\n");
  for (int i = 0; i < (int)m_vecPathHistory.size(); ++i)
  {
    strTemp.Format("%02i.[%s]", i, m_vecPathHistory[i]);
    printf(" * %s\n", strTemp.c_str());
  }
}
