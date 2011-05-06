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

#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/lexical_cast.hpp>

#include <list>
#include <vector>

#include "FileSystem/File.h"
#include "FileItem.h"
#include "GUIBaseContainer.h"
#include "GUIStaticItem.h"
#include "GUIWindowHome.h"
#include "GUIDialogTimer.h"
#include "GUIDialogYesNo.h"
#include "GUIWindowManager.h"
#include "GUIUserMessages.h"
#include "AlarmClock.h"
#include "Key.h"

#include "PlexContentWorker.h"
#include "PlexDirectory.h"
#include "PlexSourceScanner.h"

using namespace std;
using namespace XFILE;
using namespace boost;

#define MAIN_MENU         300 // THIS WAS 300 for Plex skin.
#define POWER_MENU        407

#define QUIT_ITEM          111
#define SLEEP_ITEM         112
#define SHUTDOWN_ITEM      113
#define SLEEP_DISPLAY_ITEM 114

#define SLIDESHOW_MULTIIMAGE 10101

CGUIWindowHome::CGUIWindowHome(void) : CGUIWindow(WINDOW_HOME, "Home.xml")
  , m_lastSelectedItem(-1)
  , m_lastSelectedID(-1)
  , m_pendingSelectID(-1)
  , m_selectedContainerID(-1)
  , m_selectedItem(-1)
{
  // Create the worker. We're not going to destroy it because whacking it on exit can cause problems.
  m_workerManager = new PlexContentWorkerManager();
}

CGUIWindowHome::~CGUIWindowHome(void)
{
}

bool CGUIWindowHome::OnAction(const CAction &action)
{
  if (action.GetID() == ACTION_PREVIOUS_MENU && GetFocusedControlID() > 9000)
  {
    CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), 300);
    OnMessage(msg);

    return true;
  }
  
  if (action.GetID() == ACTION_CONTEXT_MENU)
  {
    return OnPopupMenu();
  }
  else if (action.GetID() == ACTION_PREVIOUS_MENU || action.GetID() == ACTION_PARENT_DIR)
  {
    CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), 300);
    OnMessage(msg);
    
    return true;
  }
  
  bool ret = CGUIWindow::OnAction(action);
  
  // See what's focused.
  if (GetFocusedControl() && GetFocusedControl()->GetID() == MAIN_MENU)
  {
    CGUIBaseContainer* pControl = (CGUIBaseContainer*)GetFocusedControl();
    if (pControl)
    {
      CGUIListItemPtr pItem = pControl->GetListItem(pControl->GetSelectedItem());
      int itemId = pControl->GetSelectedItemID();
      if (itemId != m_lastSelectedID)
      {
        // Hide lists.
        HideAllLists();
        
        // OK, let's load it after a delay.
        m_pendingSelectID = itemId;
        m_lastSelectedID = -1;
        m_contentLoadTimer.StartZero();
      }
    }
  }
  
  return ret;
}

void CGUIWindowHome::UpdateContentForSelectedItem(int itemID)
{
  // Hide lists.
  HideAllLists();
  
  if (m_lastSelectedID == itemID)
  {
    // Bind the lists.
    BOOST_FOREACH(int_list_pair pair, m_contentLists)
    {
      int controlID = pair.first;
      CFileItemListPtr list = pair.second.list;
    
      CGUIBaseContainer* control = (CGUIBaseContainer* )GetControl(controlID);
      if (control && list->Size() > 0)
      {
        // Bind the list.
        CGUIMessage msg(GUI_MSG_LABEL_BIND, MAIN_MENU, controlID, 0, 0, list.get());
        OnMessage(msg);
        
        // Make sure it's visible.
        SET_CONTROL_VISIBLE(controlID);
        SET_CONTROL_VISIBLE(controlID-1000);
        
        // Load thumbs.
        m_contentLists[controlID].loader->Load(*list.get());
      }
    }
  }
  else
  {
    // Clear old lists.
    m_contentLists.clear();
    
    // Cancel any pending requests.
    m_workerManager->cancelPending();
    
    // Depending on what's selected, get the appropriate content.
    if (itemID >= 1000)
    {
      // A library section.
      string sectionUrl = m_idToSectionUrlMap[itemID];
      int typeID = m_idToSectionTypeMap[itemID];

      if (typeID == PLEX_METADATA_MIXED)
      {
        // Queue.
        m_contentLists[CONTENT_LIST_QUEUE] = Group(kVIDEO_LOADER);
        m_workerManager->enqueue(WINDOW_HOME, sectionUrl, CONTENT_LIST_QUEUE);
      }
      else
      {
        // Recently added.
        m_contentLists[CONTENT_LIST_RECENTLY_ADDED] = Group(typeID == PLEX_METADATA_ALBUM ? kMUSIC_LOADER : kVIDEO_LOADER);
        m_workerManager->enqueue(WINDOW_HOME, sectionUrl + "/recentlyAdded", CONTENT_LIST_RECENTLY_ADDED);

        if (typeID == PLEX_METADATA_SHOW || typeID == PLEX_METADATA_MOVIE)
        {
          // On deck.
          m_contentLists[CONTENT_LIST_ON_DECK] = Group(kVIDEO_LOADER);
          m_workerManager->enqueue(WINDOW_HOME, sectionUrl + "/onDeck", CONTENT_LIST_ON_DECK);
        }

        // Asynchronously fetch the fanart for the section.
        m_workerManager->enqueue(WINDOW_HOME, sectionUrl + "/arts", CONTENT_LIST_FANART);
      }
    }
    else
    {
      SET_CONTROL_HIDDEN(SLIDESHOW_MULTIIMAGE);
    }
  
    // Remember what the last one was.
    m_lastSelectedID = itemID;
  }
}

bool CGUIWindowHome::OnPopupMenu()
{
  if (!GetFocusedControl())
    return false;
  
  int controlId = GetFocusedControl()->GetID();
  if (controlId == MAIN_MENU || controlId == POWER_MENU)
  {
    CGUIBaseContainer* pControl = (CGUIBaseContainer*)GetFocusedControl();
    CGUIListItemPtr pItem = pControl->GetListItem(pControl->GetSelectedItem());
    int itemId = pControl->GetSelectedItemID();
    
    float menuOffset = 0;
    int iHeading, iInfo, iAlreadySetMsg;
    CStdString sAlarmName, sAction;
    
    if (controlId == POWER_MENU) menuOffset = 180;
    
    switch (itemId) 
    {
      case QUIT_ITEM:
        iHeading = 40300;
        iInfo = 40310;
        iAlreadySetMsg = 40320;
        sAlarmName = "plex_quit_timer";
        sAction = "quit";
        break;
      case SLEEP_ITEM:       
        iHeading = 40301;
        iInfo = 40311;
        iAlreadySetMsg = 40321;
        sAlarmName = "plex_sleep_timer";
        sAction = "sleepsystem";
        break;
      case SHUTDOWN_ITEM:
        iHeading = 40302;
        iInfo = 40312;
        iAlreadySetMsg = 40322;
        sAlarmName = "plex_shutdown_timer";
        sAction = "shutdownsystem";
        break;
      case SLEEP_DISPLAY_ITEM:
        iHeading = 40303;
        iInfo = 40312;
        iAlreadySetMsg = 40323;
        sAlarmName = "plex_sleep_display_timer";
        sAction = "sleepdisplay";
      default:
        return false;
        break;
    }
    
    // Check to see if any timers already exist
    if (!CheckTimer("plex_quit_timer", sAlarmName, 40325, 40315, iAlreadySetMsg) ||
        !CheckTimer("plex_sleep_timer", sAlarmName, 40325, 40316, iAlreadySetMsg) ||
        !CheckTimer("plex_shutdown_timer", sAlarmName, 40325, 40317, iAlreadySetMsg) ||
        !CheckTimer("plex_sleep_display_timer", sAlarmName, 40325, 40318, iAlreadySetMsg))
      return false;
    
    
    int iTime;
    if (g_alarmClock.HasAlarm(sAlarmName))
    {
      iTime = (int)(g_alarmClock.GetRemaining(sAlarmName)/60);
      iHeading += 5; // Change the title to "Change" not "Set".
    }
    else
    {
      iTime = 0;
    }
    
    iTime = CGUIDialogTimer::ShowAndGetInput(iHeading, iInfo, iTime);
    
    // Dialog cancelled
    if (iTime == -1) return false;
    
    // If the alarm's already been set, cancel it
    if (g_alarmClock.HasAlarm(sAlarmName))
      g_alarmClock.Stop(sAlarmName, false);
    
    // Start a new alarm
    if (iTime > 0)
      g_alarmClock.Start(sAlarmName, iTime*60, sAction, false);
    
    // Focus the main menu again
    CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), MAIN_MENU);
    if(OwningCriticalSection(g_graphicsContext))
      CGUIWindow::OnMessage(msg);
    else
      g_windowManager.SendThreadMessage(msg, GetID());
    
    return true;
    
    if (pControl->GetSelectedItemID() == 1)
    {
      g_alarmClock.Start ("plex_quit_timer", 5, "ShutDown", false);      
    }
  }
  return false;
}

bool CGUIWindowHome::CheckTimer(const CStdString& strExisting, const CStdString& strNew, int title, int line1, int line2)
{
  bool bReturn;
  if (g_alarmClock.HasAlarm(strExisting) && strExisting != strNew)
  {
    if (CGUIDialogYesNo::ShowAndGetInput(title, line1, line2, 0, bReturn) == false)
    {
      return false;
    }
    else
    {
      g_alarmClock.Stop(strExisting, false);  
      return true;
    }
  }
  else
    return true;
}

typedef pair<string, HostSourcesPtr> string_sources_pair;

static bool compare(CFileItemPtr first, CFileItemPtr second)
{
  return first->GetLabel() <= second->GetLabel();
}

bool CGUIWindowHome::OnMessage(CGUIMessage& message)
{
  if (message.GetMessage() ==  GUI_MSG_WINDOW_DEINIT)
  {
    CGUIBaseContainer* control = (CGUIBaseContainer* )GetControl(MAIN_MENU);
    if (control == 0)
      control = (CGUIBaseContainer* )GetControl(300);
    
    if (control)
      m_lastSelectedItem = control->GetSelectedItem();
    
    // Cancel pending tasks and hide.
    m_workerManager->cancelPending();
    HideAllLists();
  }

  bool ret = CGUIWindow::OnMessage(message);
  
  switch (message.GetMessage())
  {
  case GUI_MSG_WINDOW_INIT:
  {
    if (m_lastSelectedID != -1)
      UpdateContentForSelectedItem(m_lastSelectedID);
    else
      HideAllLists();
  }
    
  case GUI_MSG_WINDOW_RESET:
  case GUI_MSG_UPDATE_MAIN_MENU:
  {
    // This will be our new list.
    vector<CGUIListItemPtr> newList;
    
    // Get the old list.
    CGUIBaseContainer* control = (CGUIBaseContainer* )GetControl(MAIN_MENU);
    if (control == 0)
      control = (CGUIBaseContainer* )GetControl(300);
    
    if (control)
    {
      vector<CGUIListItemPtr>& oldList = control->GetStaticItems();
      
      // First collect all the real items.
      BOOST_FOREACH(CGUIListItemPtr item, oldList)
      {
        if (item->HasProperty("plex") == false)
          newList.push_back(item);
      }
      
      // Now collect all the added items.
      CPlexSourceScanner::Lock();
      
      map<string, int> nameCounts;
      map<string, HostSourcesPtr>& map = CPlexSourceScanner::GetMap();
      list<CFileItemPtr> newItems;
      BOOST_FOREACH(string_sources_pair nameSource, map)
      {
        for (int i=0; i<nameSource.second->librarySections.Size(); i++)
        {
          newItems.push_back(nameSource.second->librarySections[i]);
          CStdString sectionName = nameSource.second->librarySections[i]->GetLabel();
          ++nameCounts[sectionName.ToLower()];
        }
      }

      CPlexSourceScanner::Unlock();
      
      // Now sort them according to name.
      newItems.sort(compare);
      
      // Clear the maps.
      m_idToSectionUrlMap.clear();
      m_idToSectionTypeMap.clear();

      // Now add the new ones.
      int id = 1000;
      BOOST_FOREACH(CFileItemPtr item, newItems)
      {
        CFileItemPtr newItem = CFileItemPtr(new CGUIStaticItem());
        newItem->SetLabel(item->GetLabel());
        newItem->SetProperty("plex", "1");
        
        CStdString sectionName = item->GetLabel();
        if (nameCounts[sectionName.ToLower()] > 1)
          newItem->SetLabel2(item->GetLabel2());

        // Save the map from ID to library section ID.
        m_idToSectionUrlMap[id] = item->GetProperty("key");
        m_idToSectionTypeMap[id] = item->GetPropertyInt("typeNumber");
        
        if (item->GetProperty("type") == "artist")
          newItem->m_strPath = "XBMC.ActivateWindow(MyMusicFiles," + item->m_strPath + ",return)";
        else
          newItem->m_strPath = "XBMC.ActivateWindow(MyVideoFiles," + item->m_strPath + ",return)";
        
        newItem->m_idepth = 0;
        newItem->SetQuickFanart(item->GetQuickFanart());
        newItem->m_iprogramCount = id++;

        // Load and set fanart.
        newItem->CacheLocalFanart();
        if (CFile::Exists(newItem->GetCachedProgramFanart()))
          newItem->SetProperty("fanart_image", newItem->GetCachedProgramFanart());

        newList.push_back(newItem);
      }

      // Replace 'em.
      control->GetStaticItems().clear();
      control->GetStaticItems().assign(newList.begin(), newList.end());
      
      // See if we have a selected item and restore it if we do.
      if (m_lastSelectedItem >= 0)
      {
        CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), control->GetID(), m_lastSelectedItem+1, 0);
        g_windowManager.SendThreadMessage(msg);
      }

      // Additionally, if we have a selected item on the right hand side, restore it.
      if (m_selectedItem != -1)
      {
        // Select group.
        CGUIMessage msg(GUI_MSG_SETFOCUS, GetID(), m_selectedContainerID);
        g_windowManager.SendThreadMessage(msg);
        
        // Select item.
        CGUIMessage msg2(GUI_MSG_ITEM_SELECT, GetID(), m_selectedContainerID, m_selectedItem);
        g_windowManager.SendThreadMessage(msg2);
      }
    }
    
    m_selectedContainerID = -1;
    m_selectedItem = -1;
  }
  break;
  
  case GUI_MSG_SEARCH_HELPER_COMPLETE:
  {
    PlexContentWorkerPtr worker = m_workerManager->find(message.GetParam1());
    if (worker)
    {
      CFileItemListPtr results = worker->getResults();
      int controlID = message.GetParam2();
      printf("Processing results from worker: %d (context: %d).\n", worker->getID(), controlID);

      // Copy the items across.
      if (m_contentLists.find(controlID) != m_contentLists.end())
      {
        m_contentLists[controlID].list = results;
        
        CGUIBaseContainer* control = (CGUIBaseContainer* )GetControl(controlID);
        if (control && results->Size() > 0)
        {
          // Bind the list.
          CGUIMessage msg(GUI_MSG_LABEL_BIND, MAIN_MENU, controlID, 0, 0, m_contentLists[controlID].list.get());
          OnMessage(msg);
          
          // Make sure it's visible.
          SET_CONTROL_VISIBLE(controlID);
          SET_CONTROL_VISIBLE(controlID-1000);
          
          // Load thumbs.
          m_contentLists[controlID].loader->Load(*m_contentLists[controlID].list.get());
        }
      }
      else if (controlID == CONTENT_LIST_FANART)
      {
        // Send the right slideshow information over to the multiimage.
        CGUIMessage msg(GUI_MSG_LABEL_BIND, GetID(), SLIDESHOW_MULTIIMAGE, 0, 0, results.get());
        OnMessage(msg);
        
        // Make it visible.
        SET_CONTROL_VISIBLE(SLIDESHOW_MULTIIMAGE);
      }
      
      m_workerManager->destroy(worker->getID());
    }
  }
  break;
  
  case GUI_MSG_CLICKED:
  {
    int iControl = message.GetSenderId();
    PlayFileFromContainer(GetControl(iControl));
  }
  }
  
  return ret;
}

void CGUIWindowHome::SaveStateBeforePlay(CGUIBaseContainer* container)
{
  // Save state.
  m_selectedContainerID = container->GetID();
  m_selectedItem = container->GetSelectedItem();
}

void CGUIWindowHome::HideAllLists()
{
  // Hide lists.
  short lists[] = {CONTENT_LIST_ON_DECK, CONTENT_LIST_RECENTLY_ACCESSED, CONTENT_LIST_RECENTLY_ADDED, CONTENT_LIST_QUEUE};
  BOOST_FOREACH(int id, lists)
  {
    SET_CONTROL_HIDDEN(id);
    SET_CONTROL_HIDDEN(id-1000);
  }
  
  SET_CONTROL_HIDDEN(SLIDESHOW_MULTIIMAGE);
}

void CGUIWindowHome::Render()
{
  if (m_pendingSelectID != -1 && m_contentLoadTimer.IsRunning() && m_contentLoadTimer.GetElapsedMilliseconds() > 500)
  {
    UpdateContentForSelectedItem(m_pendingSelectID);
    m_pendingSelectID = -1;
    m_contentLoadTimer.Stop();
  }
  
  CGUIWindow::Render();
}
