//
//  GUIWindowMediaFilterView.h
//  Plex
//
//  Created by Tobias Hieta <tobias@plexapp.com> on 2012-11-19.
//  Copyright 2012 Plex Inc. All rights reserved.
//

#ifndef GUIWINDOWMEDIAFILTERVIEW_H
#define GUIWINDOWMEDIAFILTERVIEW_H

#include "video/windows/GUIWindowVideoNav.h"
#include "guilib/GUIButtonControl.h"
#include "guilib/GUIRadioButtonControl.h"
#include "StringUtils.h"


#define FILTER_LIST     19000
#define FILTER_BUTTON   19001
#define FILTER_RADIO_BUTTON 19002
#define FILTER_SPIN_CONTROL 19003

#define SORT_LIST       19010
#define SORT_RADIO_BUTTON 19011

#define FILTER_BUTTONS_START -100
#define SORT_BUTTONS_START -200

class CPlexFilter
{
  public:
    CPlexFilter(const CStdString& filterString, const CStdString& filterName, const CStdString& filterType, const CStdString& key)
      : m_filterName(filterName)
      , m_filterType(filterType)
      , m_filterString(filterString)
      , m_key(key)
    {
      m_filterControl = NULL;
    }

    CGUIButtonControl* NewFilterControl(CGUIButtonControl *parent, int id)
    {
      if (IsBooleanType())
        m_filterControl = new CGUIRadioButtonControl(*(CGUIRadioButtonControl*)parent);
      else
        m_filterControl = new CGUIButtonControl(*parent);

      m_filterControl->SetLabel(m_filterString);
      m_filterControl->AllocResources();
      m_filterControl->SetID(id);
      m_filterControl->SetVisible(true);

      return m_filterControl;
    }

    CGUIButtonControl* GetFilterControl() const { return m_filterControl; }

    bool IsStringType() const { return m_filterType == "string"; }
    bool IsIntegerType() const { return m_filterType == "integer"; }
    bool IsBooleanType() const { return m_filterType == "boolean"; }
    bool IsActive() const { return !m_currentValue.empty(); }

    CStdString GetFilterName() const { return m_filterName; }

    int GetControlID() const
    {
      if (m_filterControl)
        return m_filterControl->GetID();

      return 0;
    }

    CStdString GetFilterValue() const
    {
      CStdString filterStr;
      if (IsBooleanType())
      {
        CGUIRadioButtonControl* radio = (CGUIRadioButtonControl*)m_filterControl;
        if (radio->IsSelected())
          filterStr.Format("%s=1", m_filterName);
      }
      else
      {
        CStdString values = StringUtils::Join(m_currentValue, ",");
        filterStr.Format("%s=%s", m_filterName, values);
      }

      return filterStr;
    }

  private:
    CGUIButtonControl* m_filterControl;
    CStdString m_filterName;
    CStdString m_filterString;
    CStdString m_filterType;
    std::vector<std::string> m_currentValue;
    CStdString m_key;
};

typedef boost::shared_ptr<CPlexFilter> CPlexFilterPtr;

class CGUIWindowMediaFilterView : public CGUIWindowVideoNav
{
  public:
    bool OnMessage(CGUIMessage &message);
  protected:
    bool Update(const CStdString &strDirectory, bool updateFilterPath);
    void BuildFilters(const CStdString &url, int type);
    bool FetchFilterSortList(const CStdString& url, const CStdString& filterSort, int type, CFileItemList& list);

  private:
    std::map<CStdString, CPlexFilterPtr> m_filters;
    std::map<CStdString, CPlexFilterPtr> m_sorts;
    CStdString m_baseUrl;
    std::vector<std::string> m_appliedFilters;
};

#endif // GUIWINDOWMEDIAFILTERVIEW_H
