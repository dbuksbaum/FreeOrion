// -*- C++ -*-
#ifndef _CUISpin_h_
#define _CUISpin_h_

#ifndef _CUIControls_h_
#include "CUIControls.h"
#endif

#ifndef _GG_Spin_h_
#include <GG/Spin.h>
#endif

#ifndef _HumanClientApp_h_
#include "../client/human/HumanClientApp.h"
#endif

#ifndef _OptionsDB_h_
#include "../util/OptionsDB.h"
#endif


namespace detail {
#ifdef _MSC_VER
    void PlayValueChangedSound(...);
#else
    template <class T>
    void PlayValueChangedSound(T);
#endif
}


/** a FreeOrion Spin control */
template <class T> class CUISpin : public GG::Spin<T>
{
public:
    typedef typename GG::Spin<T>::ValueType ValueType;

    /** \name Structors */ //@{
    CUISpin(int x, int y, int w, T value, T step, T min, T max, bool edits) :
        GG::Spin<T>(x, y, w, value, step, min, max, edits, GG::GUI::GetGUI()->GetFont(ClientUI::FONT, ClientUI::PTS), ClientUI::CTRL_BORDER_COLOR, 
                    ClientUI::TEXT_COLOR, GG::CLR_ZERO)
    {
#ifdef _MSC_VER
        GG::Connect(ValueChangedSignal, &detail::PlayValueChangedSound, -1);
#else
        GG::Connect(GG::Spin<T>::ValueChangedSignal, &detail::PlayValueChangedSound<T>, -1);
#endif
        if (GG::Spin<T>::GetEdit())
            GG::Spin<T>::GetEdit()->SetHiliteColor(ClientUI::EDIT_HILITE_COLOR);
    }

    /** \name Mutators */ //@{
    virtual void Render()
    {
        GG::Clr color_to_use = this->Disabled() ? DisabledColor(this->Color()) : this->Color();
        GG::Clr int_color_to_use = this->Disabled() ? DisabledColor(this->InteriorColor()) : this->InteriorColor();
        GG::Pt ul = this->UpperLeft(), lr = this->LowerRight();
        FlatRectangle(ul.x, ul.y, lr.x, lr.y, int_color_to_use, color_to_use, 1);
        if (!this->EditsAllowed()) {
            this->GetEdit()->OffsetMove(this->UpperLeft());
            this->GetEdit()->Render();
            this->GetEdit()->OffsetMove(-this->UpperLeft());
        }
        this->UpButton()->OffsetMove(this->UpperLeft());
        this->DownButton()->OffsetMove(this->UpperLeft());
        this->UpButton()->Render();
        this->DownButton()->Render();
        this->UpButton()->OffsetMove(-this->UpperLeft());
        this->DownButton()->OffsetMove(-this->UpperLeft());
    }

    virtual void SizeMove(const GG::Pt& ul, const GG::Pt& lr)
    {
        GG::Wnd::SizeMove(ul, lr);
        const int BORDER_THICK = 1;
        const int BUTTON_MARGIN = 3;
        const int BN_HEIGHT = (this->Height() - 3 * BUTTON_MARGIN) / 2;
        const int BN_WIDTH = BN_HEIGHT;
        const int BN_X_POS = this->Width() - BN_WIDTH - BORDER_THICK;
        this->GetEdit()->SizeMove(GG::Pt(0, 0), GG::Pt(BN_X_POS, this->Height()));
        this->UpButton()->SizeMove(GG::Pt(BN_X_POS, BORDER_THICK),
                                   GG::Pt(BN_X_POS + BN_WIDTH, BUTTON_MARGIN + BN_HEIGHT));
        this->DownButton()->SizeMove(GG::Pt(BN_X_POS, BUTTON_MARGIN + BN_HEIGHT + BUTTON_MARGIN),
                                     GG::Pt(BN_X_POS + BN_WIDTH, BUTTON_MARGIN + BN_HEIGHT + BUTTON_MARGIN + BN_HEIGHT));
    }
    //@}

    /** returns a std::string representing this CUISpin's exact type, including the type of its data, to aid with automatic
        XML saving and loading*/
    static std::string XMLTypeName()
    {
        std::string retval = "CUISpin_";
        retval += typeid(ValueType).name();
        retval += "_";
        return retval;
    }
};

namespace detail {
#ifdef _MSC_VER
    inline void PlayValueChangedSound(...)
    {
        std::string sound_dir = GetOptionsDB().Get<std::string>("settings-dir");
        if (!sound_dir.empty() && sound_dir[sound_dir.size() - 1] != '/')
            sound_dir += '/';
        sound_dir += "data/sound/";
        if (GetOptionsDB().Get<bool>("UI.sound.enabled"))
            HumanClientApp::GetApp()->PlaySound(sound_dir + GetOptionsDB().Get<std::string>("UI.sound.button-click"));
    }
#else
    template <class T>
    inline void PlayValueChangedSound(T)
    {
        std::string sound_dir = GetOptionsDB().template Get<std::string>("settings-dir");
        if (!sound_dir.empty() && sound_dir[sound_dir.size() - 1] != '/')
            sound_dir += '/';
        sound_dir += "data/sound/";
        if (GetOptionsDB().template Get<bool>("UI.sound.enabled"))
            HumanClientApp::GetApp()->PlaySound(sound_dir + GetOptionsDB().template Get<std::string>("UI.sound.button-click"));
    }
#endif
}

inline std::pair<std::string, std::string> CUISpinRevision()
{return std::pair<std::string, std::string>("$RCSfile$", "$Revision$");}

#endif // _CUISpin_h_
