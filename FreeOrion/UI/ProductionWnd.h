// -*- C++ -*-
#ifndef _ProductionWnd_h_
#define _ProductionWnd_h_

#include "CUIWnd.h"
#include "../universe/Enums.h"

#include <GG/ListBox.h>

class CUIListBox;
class ProductionInfoPanel;
class BuildDesignatorWnd;

/** Contains a BuildDesignatorWnd, some stats on the empire-wide production queue, and the queue itself. */
class ProductionWnd : public CUIWnd
{
public:
    /** \name Structors */ //@{
    ProductionWnd(int w, int h);
    //@}

    /** \name Mutators */ //@{
    virtual GG::Pt ClientUpperLeft() const;
    virtual GG::Pt ClientLowerRight() const;
    virtual bool   InWindow(const GG::Pt& pt) const;
    virtual bool   InClient(const GG::Pt& pt) const;
    virtual void   Render();

    void Reset();
    void CenterOnBuild(int queue_idx);
    void SelectSystem(int system);
    void QueueItemMoved(int row_idx, GG::ListBox::Row* row);
    void Sanitize();
    //@}

private:
    void UpdateQueue();
    void ResetInfoPanel();
    void AddBuildToQueueSlot(BuildType build_type, const std::string& name, int number, int location);
    void ChangeBuildQuantitySlot(int queue_idx, int quantity);
    void QueueItemDeletedSlot(int row_idx, GG::ListBox::Row* row);
    void QueueItemClickedSlot(int row_idx, GG::ListBox::Row* row, const GG::Pt& pt);
    void QueueItemDoubleClickedSlot(int row_idx, GG::ListBox::Row* row);

    ProductionInfoPanel* m_production_info_panel;
    CUIListBox*          m_queue_lb;
    BuildDesignatorWnd*  m_build_designator_wnd;
};

inline std::string ProductionWndRevision()
{return "$Id$";}

#endif // _ProductionWnd_h_
