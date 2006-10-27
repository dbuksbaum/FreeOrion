#include "MapWnd.h"

#include "ClientUI.h"
#include "CUIControls.h"
#include "../universe/Fleet.h"
#include "FleetButton.h"
#include "FleetWnd.h"
#include "../client/human/HumanClientApp.h"
#include "InGameMenu.h"
#include "../network/Message.h"
#include "../util/MultiplayerCommon.h"
#include "../util/OptionsDB.h"
#include "../universe/Planet.h"
#include "../universe/Predicates.h"
#include "../util/Random.h"
#include "ProductionWnd.h"
#include "ResearchWnd.h"
#include "SidePanel.h"
#include "SitRepPanel.h"
#include "../universe/System.h"
#include "SystemIcon.h"
#include "../universe/Universe.h"
#include "../universe/UniverseObject.h"
#include "TurnProgressWnd.h"
#include "../Empire/Empire.h"

#include <GG/DrawUtil.h>
#include <GG/MultiEdit.h>

#include <vector>
#include <deque>

#define TEST_BROWSE_INFO 0
#if TEST_BROWSE_INFO
#include <GG/BrowseInfoWnd.h>
class BrowseFoo : public GG::TextBoxBrowseInfoWnd
{
public:
    BrowseFoo() :
        TextBoxBrowseInfoWnd(200, ClientUI::Font(), 12, GG::CLR_SHADOW, GG::CLR_WHITE, GG::CLR_WHITE)
        {}

    void Update(int mode, const GG::Wnd* target)
        { SetText("mode=" + boost::lexical_cast<std::string>(mode) + " wnd=" + target->WindowText()); }
};
#endif

namespace {
    const double ZOOM_STEP_SIZE = 1.25;
    const int MIN_NEBULAE = 3; // this min and max are for a 1000.0-width galaxy
    const int MAX_NEBULAE = 6;
    const int END_TURN_BTN_WIDTH = 60;
    const int SITREP_PANEL_WIDTH = 400;
    const int SITREP_PANEL_HEIGHT = 300;
    const int MIN_SYSTEM_NAME_SIZE = 10;
    const int LAYOUT_MARGIN = 5;
    const int CHAT_WIDTH = 400;
    const int CHAT_HEIGHT = 400;
    const int CHAT_EDIT_HEIGHT = 30;
    int g_chat_display_show_time = 0;
    std::deque<std::string> g_chat_edit_history;
    int g_history_position = 0; // the current edit contents are in history position 0
    void AddOptions(OptionsDB& db)
    {
        db.Add("UI.chat-hide-interval", "OPTIONS_DB_UI_CHAT_HIDE_INTERVAL", 10, RangedValidator<int>(0, 3600));
        db.Add("UI.chat-edit-history", "OPTIONS_DB_UI_CHAT_EDIT_HISTORY", 50, RangedValidator<int>(0, 1000));
    }
    bool temp_bool = RegisterOptions(&AddOptions);

#ifndef FREEORION_RELEASE
    bool RequestRegressionTestDump()
    {
        ClientNetworkCore& network_core = HumanClientApp::GetApp()->NetworkCore();
        Message msg(Message::DEBUG, HumanClientApp::GetApp()->PlayerID(), -1, Message::CORE, "EffectsRegressionTest");
        network_core.SendMessage(msg);
        return true;
    }
#endif
}


////////////////////////////////////////////////
// MapWnd::StarlaneData
////////////////////////////////////////////////
/* Note that an order is imposed on the two systems the starlane spans.  The "source" system is the one with the lower pointer.
   This is so StarlaneDatas can be stored in a std::set and duplicates will not be a problem. */
struct MapWnd::StarlaneData
{
    StarlaneData() {}
    StarlaneData(const System* src_system, const System* dst_system) : 
        src(std::min(src_system, dst_system)), 
        dst(std::max(src_system, dst_system)) 
        {}
    bool operator<(const StarlaneData& rhs) const 
    {
        if (src != rhs.src) 
            return src < rhs.src; 
        if (dst != rhs.dst) 
            return dst < rhs.dst;
        return false;
    }

    const System* Src() const {return src;}
    const System* Dst() const {return dst;}

private:
    const System* src;
    const System* dst;
};


////////////////////////////////////////////////////////////
// MapWndPopup
////////////////////////////////////////////////////////////

MapWndPopup::MapWndPopup( const std::string& t, int x, int y, int h, int w, Uint32 flags ):
    CUIWnd( t, x, y, h, w, flags )
{
    // register with map wnd
    ClientUI::GetClientUI()->GetMapWnd()->RegisterPopup( this );
}

MapWndPopup::~MapWndPopup( )
{
    // remove from map wnd
    ClientUI::GetClientUI()->GetMapWnd()->RemovePopup( this );
}

void MapWndPopup::CloseClicked()
{
    CUIWnd::CloseClicked();
    delete this;
}

void MapWndPopup::Close( )
{
    // close window as though it's been clicked closed by the user.
    CloseClicked( );
}

////////////////////////////////////////////////
// MapWnd
////////////////////////////////////////////////
// static(s)
const int MapWnd::NUM_BACKGROUNDS = 3;
double    MapWnd::s_min_scale_factor = 0.35;
double    MapWnd::s_max_scale_factor = 8.0;
const int MapWnd::SIDE_PANEL_WIDTH = 300;
int       MapWnd::s_nebula_size = 192;

MapWnd::MapWnd() :
    GG::Wnd(-GG::GUI::GetGUI()->AppWidth(), -GG::GUI::GetGUI()->AppHeight(),
            static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor + GG::GUI::GetGUI()->AppWidth() * 1.5), 
            static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor + GG::GUI::GetGUI()->AppHeight() * 1.5), 
            GG::CLICKABLE | GG::DRAGABLE),
    m_disabled_accels_list(),
    m_backgrounds(NUM_BACKGROUNDS),
    m_bg_scroll_rate(NUM_BACKGROUNDS),
    m_bg_position_X(NUM_BACKGROUNDS),
    m_bg_position_Y(NUM_BACKGROUNDS),
    m_zoom_factor(1.0),
    m_drag_offset(-1, -1),
    m_dragged(false),
    m_current_owned_system(UniverseObject::INVALID_OBJECT_ID),
    m_current_fleet(UniverseObject::INVALID_OBJECT_ID),
    m_in_production_view_mode(false)
{
    SetText("MapWnd");

    Connect(GetUniverse().UniverseObjectDeleteSignal, &MapWnd::UniverseObjectDeleted, this);

    // toolbar
    m_toolbar = new CUIToolBar(0,0,GG::GUI::GetGUI()->AppWidth(),30);
    GG::GUI::GetGUI()->Register(m_toolbar);
    m_toolbar->Hide();

    // system-view side panel
    m_side_panel = new SidePanel(GG::GUI::GetGUI()->AppWidth() - SIDE_PANEL_WIDTH, m_toolbar->LowerRight().y, SIDE_PANEL_WIDTH, GG::GUI::GetGUI()->AppHeight());
    AttachChild(m_side_panel);
    GG::Connect(m_side_panel->SystemSelectedSignal, &MapWnd::SelectSystem, this); // sidepanel requests system selection change -> select it

    m_sitrep_panel = new SitRepPanel( (GG::GUI::GetGUI()->AppWidth()-SITREP_PANEL_WIDTH)/2, (GG::GUI::GetGUI()->AppHeight()-SITREP_PANEL_HEIGHT)/2, SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT );
    AttachChild(m_sitrep_panel);

    m_research_wnd = new ResearchWnd(GG::GUI::GetGUI()->AppWidth(), GG::GUI::GetGUI()->AppHeight() - m_toolbar->Height());
    m_research_wnd->MoveTo(GG::Pt(0, m_toolbar->Height()));
    GG::GUI::GetGUI()->Register(m_research_wnd);
    m_research_wnd->Hide();

    m_production_wnd = new ProductionWnd(GG::GUI::GetGUI()->AppWidth(), GG::GUI::GetGUI()->AppHeight() - m_toolbar->Height());
    m_production_wnd->MoveTo(GG::Pt(0, m_toolbar->Height()));
    GG::GUI::GetGUI()->Register(m_production_wnd);
    m_production_wnd->Hide();
    GG::Connect(m_production_wnd->SystemSelectedSignal, &MapWnd::SelectSystem, this); // productionwnd requests system selection change -> select it

    // turn button
    m_turn_update = new CUITurnButton(LAYOUT_MARGIN, LAYOUT_MARGIN, END_TURN_BTN_WIDTH, "" );
    m_toolbar->AttachChild(m_turn_update);
    GG::Connect(m_turn_update->ClickedSignal, &MapWnd::TurnBtnClicked, this);

    boost::shared_ptr<GG::Font> font = GG::GUI::GetGUI()->GetFont(ClientUI::Font(), ClientUI::Pts());
    const int BUTTON_TOTAL_MARGIN = 8;

    int button_width = font->TextExtent(UserString("MAP_BTN_MENU")).x + BUTTON_TOTAL_MARGIN;
    m_btn_menu = new CUIButton(m_toolbar->LowerRight().x-LAYOUT_MARGIN-button_width, LAYOUT_MARGIN, button_width, UserString("MAP_BTN_MENU") );
    m_toolbar->AttachChild(m_btn_menu);
    GG::Connect(m_btn_menu->ClickedSignal, &MapWnd::MenuBtnClicked, this);

    button_width = font->TextExtent(UserString("MAP_BTN_PRODUCTION")).x + BUTTON_TOTAL_MARGIN;
    m_btn_production = new CUIButton(m_btn_menu->UpperLeft().x-LAYOUT_MARGIN-button_width, LAYOUT_MARGIN, button_width, UserString("MAP_BTN_PRODUCTION") );
    m_toolbar->AttachChild(m_btn_production);
    GG::Connect(m_btn_production->ClickedSignal, &MapWnd::ProductionBtnClicked, this);

    button_width = font->TextExtent(UserString("MAP_BTN_RESEARCH")).x + BUTTON_TOTAL_MARGIN;
    m_btn_research = new CUIButton(m_btn_production->UpperLeft().x-LAYOUT_MARGIN-button_width, LAYOUT_MARGIN, button_width, UserString("MAP_BTN_RESEARCH") );
    m_toolbar->AttachChild(m_btn_research);
    GG::Connect(m_btn_research->ClickedSignal, &MapWnd::ResearchBtnClicked, this);

    button_width = font->TextExtent(UserString("MAP_BTN_SITREP")).x + BUTTON_TOTAL_MARGIN;
    m_btn_siterep = new CUIButton(m_btn_research->UpperLeft().x-LAYOUT_MARGIN-button_width, LAYOUT_MARGIN, button_width, UserString("MAP_BTN_SITREP") );
    m_toolbar->AttachChild(m_btn_siterep);
    GG::Connect(m_btn_siterep->ClickedSignal, &MapWnd::SitRepBtnClicked, this);

    // resources
    const int ICON_DUAL_WIDTH = 110;
    const int ICON_WIDTH = ICON_DUAL_WIDTH - 30;
    m_population = new StatisticIcon(m_btn_siterep->UpperLeft().x-LAYOUT_MARGIN-ICON_DUAL_WIDTH,LAYOUT_MARGIN,ICON_DUAL_WIDTH,m_turn_update->Height(),
                                     (ClientUI::ArtDir() / "icons" / "pop.png").native_file_string(),
                                     GG::CLR_WHITE,0,0,3,2,false,false,false,true);
    m_population->SetPositiveColor(GG::CLR_GREEN); m_population->SetNegativeColor(GG::CLR_RED);
    m_toolbar->AttachChild(m_population);
   
    m_industry = new StatisticIcon(m_population->UpperLeft().x-LAYOUT_MARGIN-ICON_WIDTH,LAYOUT_MARGIN,ICON_WIDTH,m_turn_update->Height(),
                                   (ClientUI::ArtDir() / "icons" / "industry.png").native_file_string(),
                                   GG::CLR_WHITE,0,2,false,false);
    m_toolbar->AttachChild(m_industry);

    m_research = new StatisticIcon(m_industry->UpperLeft().x-LAYOUT_MARGIN-ICON_WIDTH,LAYOUT_MARGIN,ICON_WIDTH,m_turn_update->Height(),
                                   (ClientUI::ArtDir() / "icons" / "research.png").native_file_string(),
                                   GG::CLR_WHITE,0,5,true,false);
    m_toolbar->AttachChild(m_research);

    m_trade = new StatisticIcon(m_research->UpperLeft().x-LAYOUT_MARGIN-ICON_DUAL_WIDTH,LAYOUT_MARGIN,ICON_DUAL_WIDTH,m_turn_update->Height(),
                                (ClientUI::ArtDir() / "icons" / "trade.png").native_file_string(),
                                GG::CLR_WHITE,0,0,3,2,false,false,false,true);
    m_trade->SetPositiveColor(GG::CLR_GREEN); m_trade->SetNegativeColor(GG::CLR_RED);
    m_toolbar->AttachChild(m_trade);

    m_mineral = new StatisticIcon(m_trade->UpperLeft().x-LAYOUT_MARGIN-ICON_DUAL_WIDTH,LAYOUT_MARGIN,ICON_DUAL_WIDTH,m_turn_update->Height(),
                                  (ClientUI::ArtDir() / "icons" / "mining.png").native_file_string(),
                                  GG::CLR_WHITE,0,0,2,2,false,false,false,true);
    m_mineral->SetPositiveColor(GG::CLR_GREEN); m_mineral->SetNegativeColor(GG::CLR_RED);
    m_toolbar->AttachChild(m_mineral);

    m_food = new StatisticIcon(m_mineral->UpperLeft().x-LAYOUT_MARGIN-ICON_DUAL_WIDTH,LAYOUT_MARGIN,ICON_DUAL_WIDTH,m_turn_update->Height(),
                               (ClientUI::ArtDir() / "icons" / "farming.png").native_file_string(),
                               GG::CLR_WHITE,0,0,3,2,false,false,false,true);
    m_food->SetPositiveColor(GG::CLR_GREEN); m_food->SetNegativeColor(GG::CLR_RED);
    m_toolbar->AttachChild(m_food);

    // chat display and chat input box
    m_chat_display = new GG::MultiEdit(LAYOUT_MARGIN, m_turn_update->LowerRight().y + LAYOUT_MARGIN, CHAT_WIDTH, CHAT_HEIGHT, "", GG::GUI::GetGUI()->GetFont(ClientUI::Font(), ClientUI::Pts()), GG::CLR_ZERO, 
                                       GG::TF_WORDBREAK | GG::MultiEdit::READ_ONLY | GG::MultiEdit::TERMINAL_STYLE | GG::MultiEdit::INTEGRAL_HEIGHT | GG::MultiEdit::NO_VSCROLL, 
                                       ClientUI::TextColor(), GG::CLR_ZERO, 0);
    AttachChild(m_chat_display);
    m_chat_display->SetMaxLinesOfHistory(100);
    m_chat_display->Hide();

    m_chat_edit = new CUIEdit(LAYOUT_MARGIN, GG::GUI::GetGUI()->AppHeight() - CHAT_EDIT_HEIGHT - LAYOUT_MARGIN, CHAT_WIDTH, "", 
                              GG::GUI::GetGUI()->GetFont(ClientUI::Font(), ClientUI::Pts()), ClientUI::CtrlBorderColor(), ClientUI::TextColor(), GG::CLR_ZERO);
    AttachChild(m_chat_edit);
    m_chat_edit->Hide();
    EnableAlphaNumAccels();

    m_menu_showing = false;

    //set up background images
    m_backgrounds[0] = ClientUI::GetTexture(ClientUI::ArtDir() / "starfield1.png");
    m_bg_position_X[0] = 10.0;
    m_bg_position_Y[0] = 10.0;
    m_bg_scroll_rate[0] = 0.125;

    m_backgrounds[1] = ClientUI::GetTexture(ClientUI::ArtDir() / "starfield2.png");
    m_bg_position_X[1] = 10.0;
    m_bg_position_Y[1] = 10.0;
    m_bg_scroll_rate[1] = 0.25;

    m_backgrounds[2] = ClientUI::GetTexture(ClientUI::ArtDir() / "starfield3.png");
    m_bg_position_X[2] = 10.0;
    m_bg_position_Y[2] = 10.0;
    m_bg_scroll_rate[2] = 0.5;

    // connect keyboard accelerators
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_ESCAPE, 0), &MapWnd::ReturnToMap, this);

    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_RETURN, 0), &MapWnd::OpenChatWindow, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_ENTER, 0), &MapWnd::OpenChatWindow, this);

    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_RETURN, GG::GGKMOD_CTRL), &MapWnd::EndTurn, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_ENTER, GG::GGKMOD_CTRL), &MapWnd::EndTurn, this);

    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F2, 0), &MapWnd::ToggleSitRep, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F3, 0), &MapWnd::ToggleResearch, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F4, 0), &MapWnd::ToggleProduction, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_F10, 0), &MapWnd::ShowMenu, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_s, 0), &MapWnd::CloseSystemView, this);

    // Keys for zooming
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_e, 0), &MapWnd::KeyboardZoomIn, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_PLUS, 0), &MapWnd::KeyboardZoomIn, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_r, 0), &MapWnd::KeyboardZoomOut, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_KP_MINUS, 0), &MapWnd::KeyboardZoomOut, this);

    // Keys for showing systems
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_d, 0), &MapWnd::ZoomToHomeSystem, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_x, 0), &MapWnd::ZoomToPrevOwnedSystem, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_c, 0), &MapWnd::ZoomToNextOwnedSystem, this);

    // Keys for showing fleets
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_f, 0), &MapWnd::ZoomToPrevIdleFleet, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_g, 0), &MapWnd::ZoomToNextIdleFleet, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_v, 0), &MapWnd::ZoomToPrevFleet, this);
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_b, 0), &MapWnd::ZoomToNextFleet, this);

#ifndef FREEORION_RELEASE
    // special development-only key combo that dumps ValueRef, Condition, and Effect regression tests using the current
    // Universe
    GG::Connect(GG::GUI::GetGUI()->AcceleratorSignal(GG::GGK_r, GG::GGKMOD_CTRL), &RequestRegressionTestDump);
#endif

    g_chat_edit_history.push_front("");

#if TEST_BROWSE_INFO
    boost::shared_ptr<GG::BrowseInfoWnd> browser_wnd(new BrowseFoo());
    GG::Wnd::SetDefaultBrowseInfoWnd(browser_wnd);
#endif
}

MapWnd::~MapWnd()
{
    delete m_toolbar;
    delete m_research_wnd;
    delete m_production_wnd;
    RemoveAccelerators();
}

GG::Pt MapWnd::ClientUpperLeft() const
{
    return UpperLeft() + GG::Pt(GG::GUI::GetGUI()->AppWidth(), GG::GUI::GetGUI()->AppHeight());
}

XMLElement MapWnd::SaveGameData() const
{
    XMLElement retval("MapWnd");
    retval.AppendChild(XMLElement("upper_left_x", boost::lexical_cast<std::string>(UpperLeft().x)));
    retval.AppendChild(XMLElement("upper_left_y", boost::lexical_cast<std::string>(UpperLeft().y)));
    retval.AppendChild(XMLElement("m_zoom_factor", boost::lexical_cast<std::string>(m_zoom_factor)));
    retval.AppendChild(XMLElement("m_nebulae"));
    for (unsigned int i = 0; i < m_nebulae.size(); ++i) {
        retval.LastChild().AppendChild(XMLElement("nebula" + boost::lexical_cast<std::string>(i)));
        retval.LastChild().LastChild().AppendChild(XMLElement("filename", m_nebulae[i]->Filename()));
        retval.LastChild().LastChild().AppendChild(XMLElement("position_x", boost::lexical_cast<std::string>(m_nebula_centers[i].x)));
        retval.LastChild().LastChild().AppendChild(XMLElement("position_y", boost::lexical_cast<std::string>(m_nebula_centers[i].y)));
    }
    return retval;
}

bool MapWnd::InProductionViewMode() const
{
    return m_in_production_view_mode;
}

void MapWnd::Render()
{
    RenderBackgrounds();
    RenderStarlanes();
    RenderFleetMovementLines();

    int interval = GetOptionsDB().Get<int>("UI.chat-hide-interval");
    if (!m_chat_edit->Visible() && g_chat_display_show_time && interval && 
        (interval < (GG::GUI::GetGUI()->Ticks() - g_chat_display_show_time) / 1000)) {
        m_chat_display->Hide();
        g_chat_display_show_time = 0;
    }
}

void MapWnd::KeyPress (GG::Key key, Uint32 key_mods)
{
    switch (key) {
    case GG::GGK_TAB: { // auto-complete current chat edit word
        if (m_chat_edit->Visible()) {
            std::string text = m_chat_edit->WindowText();
            std::pair<int, int> cursor_pos = m_chat_edit->CursorPosn();
            if (cursor_pos.first == cursor_pos.second && 0 < cursor_pos.first && cursor_pos.first <= static_cast<int>(text.size())) {
                std::string::size_type word_start = text.substr(0, cursor_pos.first).find_last_of(" :");
                if (word_start == std::string::npos)
                    word_start = 0;
                else
                    ++word_start;
                std::string partial_word = text.substr(word_start, cursor_pos.first - word_start);
                if (partial_word == "")
                    return;
                std::set<std::string> names;
                // add player and empire names
                for (EmpireManager::const_iterator it = Empires().begin(); it != Empires().end(); ++it) {
                    names.insert(it->second->Name());
                    names.insert(it->second->PlayerName());
                }
                // add system names
                std::vector<System*> systems = GetUniverse().FindObjects<System>();
                for (unsigned int i = 0; i < systems.size(); ++i) {
                    if (systems[i]->Name() != "")
                        names.insert(systems[i]->Name());
                }

                if (names.find(partial_word) != names.end()) { // if there's an exact match, just add a space
                    text.insert(cursor_pos.first, " ");
                    m_chat_edit->SetText(text);
                    m_chat_edit->SelectRange(cursor_pos.first + 1, cursor_pos.first + 1);
                } else { // no exact match; look for possible completions
                    // find the range of strings in names that is at least partially matched by partial_word
                    std::set<std::string>::iterator lower_bound = names.lower_bound(partial_word);
                    std::set<std::string>::iterator upper_bound = lower_bound;
                    while (upper_bound != names.end() && upper_bound->find(partial_word) == 0)
                        ++upper_bound;
                    if (lower_bound == upper_bound)
                        return;

                    // find the common portion of the strings in (upper_bound, lower_bound)
                    unsigned int common_end = partial_word.size();
                    for ( ; common_end < lower_bound->size(); ++common_end) {
                        std::set<std::string>::iterator it = lower_bound;
                        char ch = (*it++)[common_end];
                        bool match = true;
                        for ( ; it != upper_bound; ++it) {
                            if ((*it)[common_end] != ch) {
                                match = false;
                                break;
                            }
                        }
                        if (!match)
                            break;
                    }
                    unsigned int chars_to_add = common_end - partial_word.size();
                    bool full_completion = common_end == lower_bound->size();
                    text.insert(cursor_pos.first, lower_bound->substr(partial_word.size(), chars_to_add) + (full_completion ? " " : ""));
                    m_chat_edit->SetText(text);
                    int move_cursor_to = cursor_pos.first + chars_to_add + (full_completion ? 1 : 0);
                    m_chat_edit->SelectRange(move_cursor_to, move_cursor_to);
                }
            }
        }
        break;
    }
    case GG::GGK_RETURN:
    case GG::GGK_KP_ENTER: { // send chat message
        if (m_chat_edit->Visible()) {
            std::string edit_text = m_chat_edit->WindowText();
            if (edit_text != "") {
                if (g_chat_edit_history.size() == 1 || g_chat_edit_history[1] != edit_text) {
                    g_chat_edit_history[0] = edit_text;
                    g_chat_edit_history.push_front("");
                } else {
                    g_chat_edit_history[0] = "";
                }
                while (GetOptionsDB().Get<int>("UI.chat-edit-history") < static_cast<int>(g_chat_edit_history.size()) + 1)
                    g_chat_edit_history.pop_back();
                g_history_position = 0;
                HumanClientApp::GetApp()->NetworkCore().SendMessage(ChatMessage(HumanClientApp::GetApp()->PlayerID(), edit_text));
            }
            m_chat_edit->Clear();
            m_chat_edit->Hide();
            EnableAlphaNumAccels();

            GG::GUI::GetGUI()->SetFocusWnd(this);
            g_chat_display_show_time = GG::GUI::GetGUI()->Ticks();
        }
        break;
    }

    case GG::GGK_UP: {
        if (m_chat_edit->Visible() && g_history_position < static_cast<int>(g_chat_edit_history.size()) - 1) {
            g_chat_edit_history[g_history_position] = m_chat_edit->WindowText();
            ++g_history_position;
            m_chat_edit->SetText(g_chat_edit_history[g_history_position]);
        }
        break;
    }

    case GG::GGK_DOWN: {
        if (m_chat_edit->Visible() && 0 < g_history_position) {
            g_chat_edit_history[g_history_position] = m_chat_edit->WindowText();
            --g_history_position;
            m_chat_edit->SetText(g_chat_edit_history[g_history_position]);
        }
        break;
    }

    default:
        break;
    }
}

void MapWnd::LButtonDown (const GG::Pt &pt, Uint32 keys)
{
    m_drag_offset = pt - ClientUpperLeft();
}

void MapWnd::LDrag (const GG::Pt &pt, const GG::Pt &move, Uint32 keys)
{
    GG::Pt move_to_pt = pt - m_drag_offset;
    CorrectMapPosition(move_to_pt);
    GG::Pt final_move = move_to_pt - ClientUpperLeft();
    m_side_panel->OffsetMove(-final_move);
    m_chat_display->OffsetMove(-final_move);
    m_chat_edit->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);

    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::GUI::GetGUI()->AppWidth(), GG::GUI::GetGUI()->AppHeight()));
    m_dragged = true;
}

void MapWnd::LButtonUp (const GG::Pt &pt, Uint32 keys)
{
    m_drag_offset = GG::Pt(-1, -1);
    m_dragged = false;
}

void MapWnd::LClick (const GG::Pt &pt, Uint32 keys)
{
    m_drag_offset = GG::Pt(-1, -1);
    if (!m_dragged && !m_in_production_view_mode) {
        SelectSystem(UniverseObject::INVALID_OBJECT_ID);
        m_side_panel->Hide();
    }
    m_dragged = false;
}

void MapWnd::RClick(const GG::Pt& pt, Uint32 keys)
{
    // Attempt to close open fleet windows (if any are open and this is allowed), then attempt to close the SidePanel (if open);
    // if these fail, go ahead with the context-sensitive popup menu . Note that this enforces a one-close-per-click policy.

    if (GetOptionsDB().Get<bool>("UI.window-quickclose")) {
        if (FleetWnd::CloseAllFleetWnds())
            return;

        if (m_side_panel->Visible()) {
            m_side_panel->Hide();
            return;
        }
    }
}

void MapWnd::MouseWheel(const GG::Pt& pt, int move, Uint32 keys)
{
    if (move)
        Zoom(move);
}

void MapWnd::InitTurn(int turn_number)
{
    SetAccelerators();

    Universe& universe = GetUniverse();

    Resize(GG::Pt(static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor + GG::GUI::GetGUI()->AppWidth() * 1.5),
                  static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor + GG::GUI::GetGUI()->AppHeight() * 1.5)));

    // set up nebulae on the first turn
    if (m_nebulae.empty()) {
        // chosen so that the density of nebulae will be about MIN_NEBULAE to MAX_NEBULAE for a 1000.0-width galaxy
        const double DENSITY_SCALE_FACTOR = (Universe::UniverseWidth() * Universe::UniverseWidth()) / (1000.0 * 1000.0);
        int num_nebulae = RandSmallInt(static_cast<int>(MIN_NEBULAE * DENSITY_SCALE_FACTOR), 
                                       static_cast<int>(MAX_NEBULAE * DENSITY_SCALE_FACTOR));
        m_nebulae.resize(num_nebulae);
        m_nebula_centers.resize(num_nebulae);
        SmallIntDistType universe_placement = SmallIntDist(0, static_cast<int>(Universe::UniverseWidth()));
        for (int i = 0; i < num_nebulae; ++i) {
            m_nebulae[i] = ClientUI::GetClientUI()->GetRandomTexture(ClientUI::ArtDir(), "nebula");
            m_nebula_centers[i] = GG::Pt(universe_placement(), universe_placement());
        }
    }

    // this gets cleared here instead of with the movement line stuff because that would clear some movement lines that come from the SystemIcons below
    m_fleet_lines.clear();
    m_projected_fleet_lines = MovementLineData();

    // systems and starlanes
    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        DeleteChild(it->second);
    }
    m_system_icons.clear();
    m_starlanes.clear();

    std::vector<System*> systems = universe.FindObjects<System>();
    for (unsigned int i = 0; i < systems.size(); ++i) {
        // system
        SystemIcon* icon = new SystemIcon(systems[i]->ID(), m_zoom_factor);
        m_system_icons[systems[i]->ID()] = icon;
        icon->InstallEventFilter(this);
        AttachChild(icon);
        icon->Refresh();
        GG::Connect(icon->LeftClickedSignal, &MapWnd::SystemLeftClicked, this);
        GG::Connect(icon->RightClickedSignal, &MapWnd::SystemRightClicked, this);
        GG::Connect(icon->LeftDoubleClickedSignal, &MapWnd::SystemDoubleClicked, this);
        GG::Connect(icon->MouseEnteringSignal, &MapWnd::MouseEnteringSystem, this);
        GG::Connect(icon->MouseLeavingSignal, &MapWnd::MouseLeavingSystem, this);

        // system's starlanes
        for (System::lane_iterator it = systems[i]->begin_lanes(); it != systems[i]->end_lanes(); ++it) {
            if (!it->second) {
                System* dest_system = universe.Object<System>(it->first);
                m_starlanes.insert(StarlaneData(systems[i], dest_system));
            }
        }
    }        

    // fleets not in systems
    for (unsigned int i = 0; i < m_moving_fleet_buttons.size(); ++i) {
        DeleteChild(m_moving_fleet_buttons[i]);
    }
    m_moving_fleet_buttons.clear();

    Universe::ObjectVec fleets = universe.FindObjects(MovingFleetVisitor());
    typedef std::multimap<std::pair<double, double>, UniverseObject*> SortedFleetMap;
    SortedFleetMap position_sorted_fleets;
    for (unsigned int i = 0; i < fleets.size(); ++i) {
        position_sorted_fleets.insert(std::make_pair(std::make_pair(fleets[i]->X(), fleets[i]->Y()), fleets[i]));
    }

    SortedFleetMap::iterator it = position_sorted_fleets.begin();
    SortedFleetMap::iterator end_it = position_sorted_fleets.end();
    while (it != end_it) {
        SortedFleetMap::iterator local_end_it = position_sorted_fleets.upper_bound(it->first);
        std::map<int, std::vector<int> > IDs_by_empire_color;
        for (; it != local_end_it; ++it) {
            IDs_by_empire_color[*it->second->Owners().begin()].push_back(it->second->ID());
        }
        for (std::map<int, std::vector<int> >::iterator ID_it = IDs_by_empire_color.begin(); ID_it != IDs_by_empire_color.end(); ++ID_it) {
            FleetButton* fb = new FleetButton(Empires().Lookup(ID_it->first)->Color(), ID_it->second, m_zoom_factor);
            m_moving_fleet_buttons.push_back(fb);
            AttachChild(fb);
            SetFleetMovement(fb);
        }
    }

    MoveChildUp(m_side_panel);
    if (m_side_panel->SystemID() == UniverseObject::INVALID_OBJECT_ID)
        m_side_panel->Hide();

    // set turn button to current turn
    m_turn_update->SetText( UserString("MAP_BTN_TURN_UPDATE") + boost::lexical_cast<std::string>(turn_number ) );    
    MoveChildUp( m_turn_update );

    MoveChildUp(m_sitrep_panel);
    // are there any sitreps to show?
    Empire* empire = HumanClientApp::GetApp()->Empires().Lookup(HumanClientApp::GetApp()->EmpireID());
    assert(empire);
    m_sitrep_panel->Update();
    // HACK! The first time this SitRepPanel gets an update, the report row(s) are misaligned.  I have no idea why, and
    // I am sick of dealing with it, so I'm forcing a resize in order to force it to behave.
    m_sitrep_panel->Resize(m_sitrep_panel->Size());
    if (empire->NumSitRepEntries())
        m_sitrep_panel->Show();
    else
        m_sitrep_panel->Hide();

    m_research_wnd->Hide();
    m_production_wnd->Hide();
    m_in_production_view_mode = false;

    m_chat_edit->Hide();
    EnableAlphaNumAccels();


    if (m_zoom_factor * ClientUI::Pts() < MIN_SYSTEM_NAME_SIZE)
        HideSystemNames();
    else
        ShowSystemNames();

    // center the map on player's home system at the start of the game (if we're at the default start position, the odds are very good that this is a fresh game)
    if (ClientUpperLeft() == GG::Pt()) {
        int capitol_id = empire->CapitolID();
        UniverseObject *obj = universe.Object(capitol_id);
        if (obj) {
            CenterOnMapCoord(obj->X(), obj->Y());
        } else {
            // default to centred on whole universe if there is no capitol
            CenterOnMapCoord(Universe::UniverseWidth() / 2, Universe::UniverseWidth() / 2);
        }
    }

    GG::Connect(empire->GetFoodResPool().ChangedSignal, &MapWnd::RefreshFoodResourceIndicator, this, 0);
    GG::Connect(empire->GetMineralResPool().ChangedSignal, &MapWnd::RefreshMineralsResourceIndicator, this, 0);
    GG::Connect(empire->GetTradeResPool().ChangedSignal, &MapWnd::RefreshTradeResourceIndicator, this, 0);
    GG::Connect(empire->GetResearchResPool().ChangedSignal, &MapWnd::RefreshResearchResourceIndicator, this, 0);
    GG::Connect(empire->GetIndustryResPool().ChangedSignal, &MapWnd::RefreshIndustryResourceIndicator, this, 0);

    GG::Connect(empire->GetPopulationPool().ChangedSignal, &MapWnd::RefreshPopulationIndicator, this, 1);

    empire->UpdateResourcePool();

    m_toolbar->Show();
}

void MapWnd::RestoreFromSaveData(const XMLElement& elem)
{
    m_zoom_factor = boost::lexical_cast<double>(elem.Child("m_zoom_factor").Text());

    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        const System& system = it->second->GetSystem();
        GG::Pt icon_ul(static_cast<int>((system.X() - ClientUI::SystemIconSize() / 2) * m_zoom_factor), 
                       static_cast<int>((system.Y() - ClientUI::SystemIconSize() / 2) * m_zoom_factor));
        it->second->SizeMove(icon_ul, 
                             GG::Pt(static_cast<int>(icon_ul.x + ClientUI::SystemIconSize() * m_zoom_factor + 0.5), 
                                    static_cast<int>(icon_ul.y + ClientUI::SystemIconSize() * m_zoom_factor + 0.5)));
    }

    for (unsigned int i = 0; i < m_moving_fleet_buttons.size(); ++i) {
        Fleet* fleet = *m_moving_fleet_buttons[i]->Fleets().begin();
        double x = fleet->X();
        double y = fleet->Y();
        GG::Pt button_ul(static_cast<int>((x - ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() / 2) * m_zoom_factor), 
                         static_cast<int>((y - ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() / 2) * m_zoom_factor));
        m_moving_fleet_buttons[i]->SizeMove(button_ul, 
                                            GG::Pt(static_cast<int>(button_ul.x + ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() * m_zoom_factor + 0.5), 
                                                   static_cast<int>(button_ul.y + ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() * m_zoom_factor + 0.5)));
    }

    GG::Pt ul = UpperLeft();
    GG::Pt map_move =
        GG::Pt(boost::lexical_cast<int>(elem.Child("upper_left_x").Text()),
               boost::lexical_cast<int>(elem.Child("upper_left_y").Text())) -
        ul;
    OffsetMove(map_move);
    MoveBackgrounds(map_move);
    m_side_panel->OffsetMove(-map_move);
    m_chat_display->OffsetMove(-map_move);
    m_chat_edit->OffsetMove(-map_move);
    m_sitrep_panel->OffsetMove(-map_move);

    // this correction ensures that zooming in doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);
    GG::Pt final_move = move_to_pt - ul;
    m_side_panel->OffsetMove(-final_move);
    m_chat_display->OffsetMove(-final_move);
    m_chat_edit->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);

    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::GUI::GetGUI()->AppWidth(), GG::GUI::GetGUI()->AppHeight()));

    m_nebulae.clear();
    m_nebula_centers.clear();
    const XMLElement& nebulae = elem.Child("m_nebulae");
    for (int i = 0; i < nebulae.NumChildren(); ++i) {
        const XMLElement& curr_nebula = nebulae.Child(i);
        m_nebulae.push_back(GG::GUI::GetGUI()->GetTexture(curr_nebula.Child("filename").Text()));
        m_nebula_centers.push_back(GG::Pt(boost::lexical_cast<int>(curr_nebula.Child("position_x").Text()),
                                          boost::lexical_cast<int>(curr_nebula.Child("position_y").Text())));
    }
}

void MapWnd::ShowSystemNames()
{
    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        it->second->ShowName();
    }
}

void MapWnd::HideSystemNames()
{
    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        it->second->HideName();
    }
}

void MapWnd::HandlePlayerChatMessage(const std::string& msg)
{
    *m_chat_display += msg;
    m_chat_display->Show();
    g_chat_display_show_time = GG::GUI::GetGUI()->Ticks();
}

void MapWnd::CenterOnMapCoord(double x, double y)
{
    GG::Pt ul = ClientUpperLeft();
    double current_x = (GG::GUI::GetGUI()->AppWidth() / 2 - ul.x) / m_zoom_factor;
    double current_y = (GG::GUI::GetGUI()->AppHeight() / 2 - ul.y) / m_zoom_factor;
    GG::Pt map_move = GG::Pt(static_cast<int>((current_x - x) * m_zoom_factor), 
                             static_cast<int>((current_y - y) * m_zoom_factor));
    OffsetMove(map_move);
    MoveBackgrounds(map_move);
    m_side_panel->OffsetMove(-map_move);
    m_chat_display->OffsetMove(-map_move);
    m_chat_edit->OffsetMove(-map_move);
    m_sitrep_panel->OffsetMove(-map_move);

    // this correction ensures that the centering doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);
    GG::Pt final_move = move_to_pt - ul;
    m_side_panel->OffsetMove(-final_move);
    m_chat_display->OffsetMove(-final_move);
    m_chat_edit->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);

    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::GUI::GetGUI()->AppWidth(), GG::GUI::GetGUI()->AppHeight()));
}

void MapWnd::CenterOnSystem(int systemID)
{
    if (System* system = GetUniverse().Object<System>(systemID))
        CenterOnSystem(system);
}

void MapWnd::CenterOnFleet(int fleetID)
{
    if (Fleet* fleet = GetUniverse().Object<Fleet>(fleetID))
        CenterOnFleet(fleet);
}

void MapWnd::ShowTech(const std::string& tech_name)
{
    if (!m_research_wnd->Visible())
        ToggleResearch();
    m_research_wnd->CenterOnTech(tech_name);
}

void MapWnd::CenterOnSystem(System* system)
{
    CenterOnMapCoord(system->X(), system->Y());
}

void MapWnd::CenterOnFleet(Fleet* fleet)
{
    CenterOnMapCoord(fleet->X(), fleet->Y());
}

void MapWnd::SelectSystem(int system_id)
{
    // remove selection indicator from previously selected system
    int prev_system_id = m_side_panel->SystemID();
    if (prev_system_id != UniverseObject::INVALID_OBJECT_ID)
        m_system_icons[prev_system_id]->SetSelected(false);
    
    // place indicator on newly selected system
    if (system_id != UniverseObject::INVALID_OBJECT_ID)
        m_system_icons[system_id]->SetSelected(true);

    // show selected system in sidepanel(s)
    if (m_in_production_view_mode) {
        if (system_id != m_side_panel->SystemID()) {
            // only set selected system if newly selected system is different from before, otherwise planet rotation phase resets
            m_side_panel->SetSystem(system_id);
            m_production_wnd->SelectSystem(system_id);            
        }
        m_side_panel->Hide();   // only show ProductionWnd's sidepanel when ProductionWnd is open
    } else {    
        if (!m_side_panel->Visible() || system_id != m_side_panel->SystemID()) {
            m_side_panel->SetSystem(system_id);
            
            // if selected an invalid system, hide sidepanel
            if (system_id == UniverseObject::INVALID_OBJECT_ID) {
                m_side_panel->Hide();
            } else {
                m_side_panel->Show();
            }
        }
    }
}

void MapWnd::SelectFleet(int fleet_id)
{
    if (Fleet* fleet = GetUniverse().Object<Fleet>(fleet_id))
        SelectFleet(fleet);
}

void MapWnd::SelectFleet(Fleet* fleet)
{
    if (System* system = fleet->GetSystem()) {
        for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
            if (&it->second->GetSystem() == system) {
                it->second->ClickFleetButton(fleet);
                break;
            }
        }
    } else {
        for (unsigned int i = 0; i < m_moving_fleet_buttons.size(); ++i) {
            if (std::find(m_moving_fleet_buttons[i]->Fleets().begin(), m_moving_fleet_buttons[i]->Fleets().end(), fleet) != m_moving_fleet_buttons[i]->Fleets().end()) {
                m_moving_fleet_buttons[i]->LClick(GG::Pt(), 0);
                break;
            }
        }
    }
}

void MapWnd::SetFleetMovement(FleetButton* fleet_button)
{
    std::set<int> destinations;
    for (unsigned int i = 0; i < fleet_button->Fleets().size(); ++i) {
        Fleet* fleet = fleet_button->Fleets()[i];
        GG::Pt cl_ul = ClientUpperLeft();
        if (fleet->FinalDestinationID() != UniverseObject::INVALID_OBJECT_ID &&
            fleet->FinalDestinationID() != fleet->SystemID() &&
            destinations.find(fleet->FinalDestinationID()) == destinations.end()) {
            destinations.insert(fleet->FinalDestinationID());
            GG::Pt sz = fleet_button->Size();
            GG::Pt fleet_center = fleet_button->UpperLeft() + GG::Pt(sz.x / 2, sz.y / 2);
            m_fleet_lines[fleet] = MovementLineData((fleet_center - cl_ul).x / m_zoom_factor,
                                                    (fleet_center - cl_ul).y / m_zoom_factor,
                                                    fleet->TravelRoute());
        } else {
            m_fleet_lines.erase(fleet);
        }
    }
}

void MapWnd::SetFleetMovement(Fleet* fleet)
{
    GG::Pt ul = ClientUpperLeft();
    std::map<Fleet*, MovementLineData>::iterator it = m_fleet_lines.find(fleet);
    if (it != m_fleet_lines.end()) {
        m_fleet_lines[fleet].destinations = fleet->TravelRoute();
    }
}

void MapWnd::SetProjectedFleetMovement(Fleet* fleet, const std::list<System*>& travel_route)
{
    if (!fleet || travel_route.empty() ||
        travel_route.size() == 2 && travel_route.front() == travel_route.back()) {
        m_projected_fleet_lines = MovementLineData();
    } else {
        std::list<System*> route = travel_route;
        if (route.front() == fleet->GetSystem())
            route.pop_front();
        std::map<Fleet*, MovementLineData>::iterator it = m_fleet_lines.find(fleet);
        GG::Clr line_color = Empires().Lookup(*fleet->Owners().begin())->Color();
        if (it != m_fleet_lines.end()) {
            m_projected_fleet_lines = m_fleet_lines[fleet];
            m_projected_fleet_lines.destinations = route;
            m_projected_fleet_lines.color = line_color;
        } else {
            GG::Pt cl_ul = ClientUpperLeft();
            const FleetButton* fleet_button = m_system_icons[fleet->SystemID()]->GetFleetButton(fleet);
            GG::Pt sz = fleet_button->Size();
            GG::Pt fleet_center = fleet_button->UpperLeft() + GG::Pt(sz.x / 2, sz.y / 2);
            m_projected_fleet_lines = MovementLineData((fleet_center - cl_ul).x / m_zoom_factor,
                                                       (fleet_center - cl_ul).y / m_zoom_factor,
                                                       route, line_color);
        }
    }
}

bool MapWnd::EventFilter(GG::Wnd* w, const GG::Wnd::Event& event)
{
    if (event.Type() == GG::Wnd::Event::RClick && !FleetWnd::FleetWndsOpen()) {
        // Attempt to close the SidePanel (if open); if this fails, just let Wnd w handle it.  
        // Note that this enforces a one-close-per-click policy.

        if (GetOptionsDB().Get<bool>("UI.window-quickclose")) {
            if (m_side_panel->Visible()) {
                m_side_panel->Hide();
                return true;
            }
        }
    }
    return false;
}

void MapWnd::Zoom(int delta)
{
    GG::Pt ul = ClientUpperLeft();
    double ul_x = ul.x, ul_y = ul.y;
    double center_x = GG::GUI::GetGUI()->AppWidth() / 2.0, center_y = GG::GUI::GetGUI()->AppHeight() / 2.0;
    double ul_offset_x = ul_x - center_x, ul_offset_y = ul_y - center_y;
    if (delta > 0) {
        if (m_zoom_factor * ZOOM_STEP_SIZE < s_max_scale_factor) {
            ul_offset_x *= ZOOM_STEP_SIZE;
            ul_offset_y *= ZOOM_STEP_SIZE;
            m_zoom_factor *= ZOOM_STEP_SIZE;
        } else {
            ul_offset_x *= s_max_scale_factor / m_zoom_factor;
            ul_offset_y *= s_max_scale_factor / m_zoom_factor;
            m_zoom_factor = s_max_scale_factor;
        }
    } else if (delta < 0) {
        if (s_min_scale_factor < m_zoom_factor / ZOOM_STEP_SIZE) {
            ul_offset_x /= ZOOM_STEP_SIZE;
            ul_offset_y /= ZOOM_STEP_SIZE;
            m_zoom_factor /= ZOOM_STEP_SIZE;
        } else {
            ul_offset_x *= s_min_scale_factor / m_zoom_factor;
            ul_offset_y *= s_min_scale_factor / m_zoom_factor;
            m_zoom_factor = s_min_scale_factor;
        }
    } else {
        return; // If delta == 0, no change
    }

    if (m_zoom_factor * ClientUI::Pts() < MIN_SYSTEM_NAME_SIZE)
        HideSystemNames();
    else
        ShowSystemNames();

    for (std::map<int, SystemIcon*>::iterator it = m_system_icons.begin(); it != m_system_icons.end(); ++it) {
        const System& system = it->second->GetSystem();
        GG::Pt icon_ul(static_cast<int>((system.X() - ClientUI::SystemIconSize() / 2.0) * m_zoom_factor), 
                       static_cast<int>((system.Y() - ClientUI::SystemIconSize() / 2.0) * m_zoom_factor));
        it->second->SizeMove(icon_ul,
                             GG::Pt(static_cast<int>(icon_ul.x + ClientUI::SystemIconSize() * m_zoom_factor), 
                                    static_cast<int>(icon_ul.y + ClientUI::SystemIconSize() * m_zoom_factor)));
    }

    for (unsigned int i = 0; i < m_moving_fleet_buttons.size(); ++i) {
        Fleet* fleet = *m_moving_fleet_buttons[i]->Fleets().begin();
        double x = fleet->X();
        double y = fleet->Y();
        GG::Pt button_ul(static_cast<int>((x - ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() / 2.0) * m_zoom_factor), 
                         static_cast<int>((y - ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() / 2.0) * m_zoom_factor));
        m_moving_fleet_buttons[i]->SizeMove(button_ul,
                                            GG::Pt(static_cast<int>(button_ul.x + ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() * m_zoom_factor), 
                                                   static_cast<int>(button_ul.y + ClientUI::SystemIconSize() * ClientUI::FleetButtonSize() * m_zoom_factor)));
    }

    GG::Pt map_move(static_cast<int>((center_x + ul_offset_x) - ul_x),
                    static_cast<int>((center_y + ul_offset_y) - ul_y));
    OffsetMove(map_move);
    MoveBackgrounds(map_move);
    m_side_panel->OffsetMove(-map_move);
    m_chat_display->OffsetMove(-map_move);
    m_chat_edit->OffsetMove(-map_move);
    m_sitrep_panel->OffsetMove(-map_move);

    // this correction ensures that zooming in doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);
    GG::Pt final_move = move_to_pt - ul;
    m_side_panel->OffsetMove(-final_move);
    m_chat_display->OffsetMove(-final_move);
    m_chat_edit->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);

    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::GUI::GetGUI()->AppWidth(), GG::GUI::GetGUI()->AppHeight()));
}

void MapWnd::RenderBackgrounds()
{
    double x, y;
    glColor3d(1.0, 1.0, 1.0);
    for (int i = 0; i < NUM_BACKGROUNDS; ++i) {
        int bg_width = m_backgrounds[i]->Width();
        int bg_height = m_backgrounds[i]->Height();
        int app_width = GG::GUI::GetGUI()->AppWidth();
        int app_height = GG::GUI::GetGUI()->AppHeight();
        x = std::fmod(m_bg_position_X[i], bg_width);
        while (x < app_width + bg_width) {
            y = std::fmod(m_bg_position_Y[i], bg_height);
            while (y < app_height + bg_height) {
                m_backgrounds[i]->OrthoBlit(static_cast<int>(x - bg_width), static_cast<int>(y - bg_height), false);
                y += m_backgrounds[i]->Height();
            }
            x += m_backgrounds[i]->Width();
        }
    }

    for (unsigned int i = 0; i < m_nebulae.size(); ++i) {
        GG::Pt ul = 
            ClientUpperLeft() + 
            GG::Pt(static_cast<int>((m_nebula_centers[i].x - s_nebula_size / 2.0) * m_zoom_factor),
                   static_cast<int>((m_nebula_centers[i].y - s_nebula_size / 2.0) * m_zoom_factor));
        m_nebulae[i]->OrthoBlit(ul, 
                                ul + GG::Pt(static_cast<int>(s_nebula_size * m_zoom_factor), 
                                            static_cast<int>(s_nebula_size * m_zoom_factor)), 
                                0,
                                false);
    }
}

void MapWnd::RenderStarlanes()
{
    double LINE_SCALE = std::max(1.0, 0.666 * m_zoom_factor);
    double INNER_LINE_PORTION = 0.3;
    double INNER_LINE_WIDTH = (LINE_SCALE / 2.0) * INNER_LINE_PORTION; // these are actually half-widths in either direction
    double OUTER_LINE_WIDTH = (LINE_SCALE / 2.0);
    double CENTER_ALPHA = 0.7;
    double INNER_LINE_EDGE_ALPHA = 0.4;
    double OUTER_LINE_EDGE_ALPHA = 0.0;

    glDisable(GL_TEXTURE_2D);

    GG::Pt ul = ClientUpperLeft();
    for (std::set<StarlaneData>::iterator it = m_starlanes.begin(); it != m_starlanes.end(); ++it) {
        double center1[2] = {ul.x + it->Src()->X() * m_zoom_factor, ul.y + it->Src()->Y() * m_zoom_factor};
        double center2[2] = {ul.x + it->Dst()->X() * m_zoom_factor, ul.y + it->Dst()->Y() * m_zoom_factor};
        double lane_length = GetUniverse().LinearDistance(it->Src()->ID(), it->Dst()->ID());
        double left_vec[2] = {-(center2[1] - center1[1]) / lane_length, (center2[0] - center1[0]) / lane_length};
        double far_left1[2] = {center1[0] + OUTER_LINE_WIDTH * left_vec[0], center1[1] + OUTER_LINE_WIDTH * left_vec[1]};
        double far_left2[2] = {center2[0] + OUTER_LINE_WIDTH * left_vec[0], center2[1] + OUTER_LINE_WIDTH * left_vec[1]};
        double left1[2] = {center1[0] + INNER_LINE_WIDTH * left_vec[0], center1[1] + INNER_LINE_WIDTH * left_vec[1]};
        double left2[2] = {center2[0] + INNER_LINE_WIDTH * left_vec[0], center2[1] + INNER_LINE_WIDTH * left_vec[1]};
        double right1[2] = {center1[0] - INNER_LINE_WIDTH * left_vec[0], center1[1] - INNER_LINE_WIDTH * left_vec[1]};
        double right2[2] = {center2[0] - INNER_LINE_WIDTH * left_vec[0], center2[1] - INNER_LINE_WIDTH * left_vec[1]};
        double far_right1[2] = {center1[0] - OUTER_LINE_WIDTH * left_vec[0], center1[1] - OUTER_LINE_WIDTH * left_vec[1]};
        double far_right2[2] = {center2[0] - OUTER_LINE_WIDTH * left_vec[0], center2[1] - OUTER_LINE_WIDTH * left_vec[1]};

        GG::Clr color = GG::CLR_WHITE;
        if (it->Src()->Owners().size() == 1 && it->Dst()->Owners().size() == 1 &&
            *it->Src()->Owners().begin() == *it->Dst()->Owners().begin()) {
            color = Empires().Lookup(*it->Src()->Owners().begin())->Color();
        }

        glBegin(GL_TRIANGLE_STRIP);
        color.a = static_cast<unsigned char>(255 * OUTER_LINE_EDGE_ALPHA);
        glColor4ubv(color.v);
        glVertex2dv(far_left2);
        glVertex2dv(far_left1);
        color.a = static_cast<unsigned char>(255 * INNER_LINE_EDGE_ALPHA);
        glColor4ubv(color.v);
        glVertex2dv(left2);
        glVertex2dv(left1);
        color.a = static_cast<unsigned char>(255 * CENTER_ALPHA);
        glColor4ubv(color.v);
        glVertex2dv(center2);
        glVertex2dv(center1);
        color.a = static_cast<unsigned char>(255 * INNER_LINE_EDGE_ALPHA);
        glColor4ubv(color.v);
        glVertex2dv(right2);
        glVertex2dv(right1);
        color.a = static_cast<unsigned char>(255 * OUTER_LINE_EDGE_ALPHA);
        glColor4ubv(color.v);
        glVertex2dv(far_right2);
        glVertex2dv(far_right1);
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
}

void MapWnd::RenderFleetMovementLines()
{
    const GLushort PATTERN = 0xF0F0;
    const int GLUSHORT_BIT_LENGTH = sizeof(GLushort) * 8;
    const double RATE = 0.25;
    const double PROJECTED_PATH_RATE = 0.35;
    const int SHIFT = static_cast<int>(GG::GUI::GetGUI()->Ticks() * RATE / GLUSHORT_BIT_LENGTH) % GLUSHORT_BIT_LENGTH;
    const int PROJECTED_PATH_SHIFT =
        static_cast<int>(GG::GUI::GetGUI()->Ticks() * PROJECTED_PATH_RATE / GLUSHORT_BIT_LENGTH) % GLUSHORT_BIT_LENGTH;
    const unsigned int STIPPLE = (PATTERN << SHIFT) | (PATTERN >> (GLUSHORT_BIT_LENGTH - SHIFT));
    const unsigned int PROJECTED_PATH_STIPPLE =
        (PATTERN << PROJECTED_PATH_SHIFT) | (PATTERN >> (GLUSHORT_BIT_LENGTH - PROJECTED_PATH_SHIFT));
    double LINE_SCALE = std::max(1.0, 1.333 * m_zoom_factor);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(static_cast<int>(LINE_SCALE), STIPPLE);
    glLineWidth(LINE_SCALE);

    GG::Pt ul = ClientUpperLeft();
    for (std::map<Fleet*, MovementLineData>::iterator it = m_fleet_lines.begin(); it != m_fleet_lines.end(); ++it) {
        // this is obviously less efficient than using GL_LINE_STRIP, but GL_LINE_STRIP sometimes produces nasty artifacts 
        // when the begining of a line segment starts offscreen
        glBegin(GL_LINES);
        glColor4ubv(it->second.color.v);
        const std::list<System*>& destinations = it->second.destinations;
        glVertex2d(ul.x + it->second.x * m_zoom_factor, ul.y + it->second.y * m_zoom_factor);
        for (std::list<System*>::const_iterator dest_it = destinations.begin(); dest_it != destinations.end(); ++dest_it) {
            if (it->first->SystemID() == (*dest_it)->ID())
                continue;
            glVertex2d(ul.x + (*dest_it)->X() * m_zoom_factor, ul.y + (*dest_it)->Y() * m_zoom_factor);
            std::list<System*>::const_iterator temp_it = dest_it;
            if (++temp_it != destinations.end())
                glVertex2d(ul.x + (*dest_it)->X() * m_zoom_factor, ul.y + (*dest_it)->Y() * m_zoom_factor);
        }
        glEnd();
    }

    glLineStipple(static_cast<int>(LINE_SCALE), PROJECTED_PATH_STIPPLE);
    if (!m_projected_fleet_lines.destinations.empty()) {
        glBegin(GL_LINES);
        glColor4ubv(m_projected_fleet_lines.color.v);
        const std::list<System*>& destinations = m_projected_fleet_lines.destinations;
        glVertex2d(ul.x + m_projected_fleet_lines.x * m_zoom_factor, ul.y + m_projected_fleet_lines.y * m_zoom_factor);
        for (std::list<System*>::const_iterator dest_it = destinations.begin(); dest_it != destinations.end(); ++dest_it) {
            glVertex2d(ul.x + (*dest_it)->X() * m_zoom_factor, ul.y + (*dest_it)->Y() * m_zoom_factor);
            std::list<System*>::const_iterator temp_it = dest_it;
            if (++temp_it != destinations.end())
                glVertex2d(ul.x + (*dest_it)->X() * m_zoom_factor, ul.y + (*dest_it)->Y() * m_zoom_factor);
        }
        glEnd();
    }

    glLineWidth(1.0);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_LINE_STIPPLE);
    glEnable(GL_TEXTURE_2D);
}

void MapWnd::MoveBackgrounds(const GG::Pt& move)
{   
    double move_x, move_y;

    for (int i = 0; i < NUM_BACKGROUNDS; ++i) {
        move_x = move.x * m_bg_scroll_rate[i];
        move_y = move.y * m_bg_scroll_rate[i];
        m_bg_position_X[i] += move_x;
        m_bg_position_Y[i] += move_y;    
    }
}

void MapWnd::CorrectMapPosition(GG::Pt &move_to_pt)
{
    int contents_width = static_cast<int>(m_zoom_factor * Universe::UniverseWidth());
    int app_width =  GG::GUI::GetGUI()->AppWidth();
    int app_height = GG::GUI::GetGUI()->AppHeight();
    int map_margin_width = static_cast<int>(app_width / 2.0);

    if (app_width - map_margin_width < contents_width || app_height - map_margin_width < contents_width) {
        if (map_margin_width < move_to_pt.x)
            move_to_pt.x = map_margin_width;
        if (move_to_pt.x + contents_width < app_width - map_margin_width)
            move_to_pt.x = app_width - map_margin_width - contents_width;
        if (map_margin_width < move_to_pt.y)
            move_to_pt.y = map_margin_width;
        if (move_to_pt.y + contents_width < app_height - map_margin_width)
            move_to_pt.y = app_height - map_margin_width - contents_width;
    } else {
        if (move_to_pt.x < 0)
            move_to_pt.x = 0;
        if (app_width < move_to_pt.x + contents_width)
            move_to_pt.x = app_width - contents_width;
        if (move_to_pt.y < 0)
            move_to_pt.y = 0;
        if (app_height < move_to_pt.y + contents_width)
            move_to_pt.y = app_height - contents_width;
    }
}

void MapWnd::SystemDoubleClicked(int system_id)
{
    if (!m_in_production_view_mode) {
        if (!m_production_wnd->Visible())
            ToggleProduction();
        CenterOnSystem(system_id);
        m_production_wnd->SelectSystem(system_id);
    }
}

void MapWnd::SystemLeftClicked(int system_id)
{
    SelectSystem(system_id);
    SystemLeftClickedSignal(system_id);
}

void MapWnd::SystemRightClicked(int system_id)
{
    if (!m_in_production_view_mode)
        SystemRightClickedSignal(system_id);
}

void MapWnd::MouseEnteringSystem(int system_id)
{
    if (!m_in_production_view_mode)
        SystemBrowsedSignal(system_id);
}

void MapWnd::MouseLeavingSystem(int system_id)
{
    if (!m_in_production_view_mode)
        SystemBrowsedSignal(UniverseObject::INVALID_OBJECT_ID);
}

void MapWnd::UniverseObjectDeleted(const UniverseObject *obj)
{
    m_fleet_lines.erase(const_cast<Fleet*>(universe_object_cast<const Fleet*>(obj)));
}

void MapWnd::RegisterPopup( MapWndPopup* popup )
{
    if (popup) {
        m_popups.push_back(popup);
    }
}

void MapWnd::RemovePopup( MapWndPopup* popup )
{
    if (popup) {
        std::list<MapWndPopup*>::iterator it = std::find( m_popups.begin(), m_popups.end(), popup );
        if (it != m_popups.end())
            m_popups.erase(it);
    }
}

void MapWnd::Cleanup()
{
    CloseAllPopups();
    RemoveAccelerators();
    m_research_wnd->Hide();
    m_production_wnd->Hide();
    m_in_production_view_mode = false;
    m_toolbar->Hide();
}

void MapWnd::Sanitize()
{
    Cleanup();
    m_side_panel->MoveTo(GG::Pt(GG::GUI::GetGUI()->AppWidth() - SIDE_PANEL_WIDTH, m_toolbar->LowerRight().y));
    m_chat_display->MoveTo(GG::Pt(LAYOUT_MARGIN, m_turn_update->LowerRight().y + LAYOUT_MARGIN));
    m_chat_edit->MoveTo(GG::Pt(LAYOUT_MARGIN, GG::GUI::GetGUI()->AppHeight() - CHAT_EDIT_HEIGHT - LAYOUT_MARGIN));
    m_sitrep_panel->MoveTo(GG::Pt((GG::GUI::GetGUI()->AppWidth() - SITREP_PANEL_WIDTH) / 2, (GG::GUI::GetGUI()->AppHeight() - SITREP_PANEL_HEIGHT) / 2));
    m_sitrep_panel->Resize(GG::Pt(SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT));
    MoveTo(GG::Pt(-GG::GUI::GetGUI()->AppWidth(), -GG::GUI::GetGUI()->AppHeight()));
    m_zoom_factor = 1.0;
    m_research_wnd->Sanitize();
    m_production_wnd->Sanitize();
}

bool MapWnd::ReturnToMap()
{
    if (m_sitrep_panel->Visible())
        m_sitrep_panel->Hide();
    if (m_research_wnd->Visible()) {
        m_research_wnd->Hide();
        HumanClientApp::GetApp()->MoveDown(m_research_wnd);
    }
    if (m_production_wnd->Visible()) {
        m_production_wnd->Hide();
        if (m_in_production_view_mode) {
            m_in_production_view_mode = false;
            ShowAllPopups();
            if (!m_side_panel->Visible())
                m_side_panel->SetSystem(m_side_panel->SystemID());
        }
        HumanClientApp::GetApp()->MoveDown(m_production_wnd);
    }
    return true;
}

bool MapWnd::OpenChatWindow()
{
    if (!m_chat_display->Visible() || !m_chat_edit->Visible()) {
        EnableAlphaNumAccels();
        m_chat_display->Show();
        DisableAlphaNumAccels();
        m_chat_edit->Show();
        GG::GUI::GetGUI()->SetFocusWnd(m_chat_edit);
        g_chat_display_show_time = GG::GUI::GetGUI()->Ticks();
        return true;
    }
    return false;
}

bool MapWnd::EndTurn()
{
    Cleanup();
    HumanClientApp::GetApp()->StartTurn();
    return true;
}

bool MapWnd::ToggleSitRep()
{
    m_projected_fleet_lines = MovementLineData();
    if (m_sitrep_panel->Visible()) {
        m_sitrep_panel->Hide();
    } else {
        // hide other "competing" windows
        m_research_wnd->Hide();
        HumanClientApp::GetApp()->MoveDown(m_research_wnd);
        m_production_wnd->Hide();
        if (m_in_production_view_mode) {
            m_in_production_view_mode = false;
            ShowAllPopups();
            if (!m_side_panel->Visible())
                m_side_panel->SetSystem(m_side_panel->SystemID());
        }
        HumanClientApp::GetApp()->MoveDown(m_production_wnd);

        // show the sitrep window
        m_sitrep_panel->Show();
    }
    return true;
}

bool MapWnd::ToggleResearch()
{
    m_projected_fleet_lines = MovementLineData();
    if (m_research_wnd->Visible()) {
        m_research_wnd->Hide();
    } else {
        // hide other "competing" windows
        m_sitrep_panel->Hide();
        m_production_wnd->Hide();
        if (m_in_production_view_mode) {
            m_in_production_view_mode = false;
            ShowAllPopups();
            if (!m_side_panel->Visible())
                m_side_panel->SetSystem(m_side_panel->SystemID());
        }

        // show the research window
        m_research_wnd->Show();
        GG::GUI::GetGUI()->MoveUp(m_research_wnd);
        m_research_wnd->Reset();
    }
    return true;
}

bool MapWnd::ToggleProduction()
{
    m_projected_fleet_lines = MovementLineData();
    if (m_production_wnd->Visible()) {
        m_production_wnd->Hide();
        m_in_production_view_mode = false;
        ShowAllPopups();
        if (!m_side_panel->Visible())
            m_side_panel->SetSystem(m_side_panel->SystemID());
    } else {
        // hide other "competing" windows
        m_sitrep_panel->Hide();
        m_research_wnd->Hide();

        // show the production window
        m_production_wnd->Show();
        m_in_production_view_mode = true;
        HideAllPopups();
        m_side_panel->Hide();
        GG::GUI::GetGUI()->MoveUp(m_production_wnd);

        m_production_wnd->Reset();
    }
    return true;
}

bool MapWnd::ShowMenu()
{
    if (!m_menu_showing) {
        m_projected_fleet_lines = MovementLineData();
        m_menu_showing = true;
        InGameMenu menu;
        menu.Run();
        m_menu_showing = false;
    }
    return true;
}

bool MapWnd::CloseSystemView()
{
    SelectSystem(UniverseObject::INVALID_OBJECT_ID);
    m_side_panel->Hide();   // redundant, but safer to keep in case the behavior of SelectSystem changes
    return true;
}

bool MapWnd::KeyboardZoomIn()
{
    Zoom(1);
    return true;
}
bool MapWnd::KeyboardZoomOut()
{
    Zoom(-1);
    return true;
}

void MapWnd::RefreshFoodResourceIndicator()
{
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->EmpireID() );
    
    m_food->SetValue(empire->GetFoodResPool().Stockpile()); // set first value to stockpiled food

    double production = empire->GetFoodResPool().Production();
    double spent = empire->TotalFoodDistributed();
    
    m_food->SetValue(production - spent, 1);    // set second (bracketed) value to predicted stockpile change
}

void MapWnd::RefreshMineralsResourceIndicator()
{
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->EmpireID() );
    
    m_mineral->SetValue(empire->GetMineralResPool().Stockpile());

    double production = empire->GetMineralResPool().Production();
    double spent = empire->GetProductionQueue().TotalPPsSpent();
    
    m_mineral->SetValue(production - spent, 1);
}

void MapWnd::RefreshTradeResourceIndicator()
{
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->EmpireID() );
    
    m_trade->SetValue(empire->GetTradeResPool().Stockpile());

    double production = empire->GetTradeResPool().Production();
    double spent = empire->TotalTradeSpending();

    m_trade->SetValue(production - spent, 1);
}

void MapWnd::RefreshResearchResourceIndicator()
{
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->EmpireID() );
    
    m_research->SetValue(empire->GetResearchResPool().Production());
    /*
    m_research->SetValue(empire->GetResearchResPool().Stockpile());
    double production = empire->GetResearchResPool().Production();
    double spent = empire->GetResearchQueue().TotalRPsSpent();
    m_research->SetValueSecond(production - spent);
    */
}

void MapWnd::RefreshIndustryResourceIndicator()
{
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->EmpireID() );
    
    m_industry->SetValue(empire->GetIndustryResPool().Production());
    /*
    m_industry->SetValue(empire->GetIndustryResPool().Stockpile());
    double production = empire->GetIndustryResPool().Production();
    double spent = empire->GetProductionQueue().TotalPPsSpent();
    m_industry->SetValueSecond(production - spent);
    */
}

void MapWnd::RefreshPopulationIndicator()
{
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->EmpireID() );
    
    m_population->SetValue(empire->GetPopulationPool().Population());
    m_population->SetValue(empire->GetPopulationPool().Growth(), 1);
}

bool MapWnd::ZoomToHomeSystem()
{
    int id = Empires().Lookup(HumanClientApp::GetApp()->EmpireID())->HomeworldID();

    if (id != UniverseObject::INVALID_OBJECT_ID) {
        UniverseObject *object = GetUniverse().Object(id);
        if (!object) return false;
        CenterOnSystem(object->SystemID());
        SelectSystem(object->SystemID());
    }

    return true;
}

bool MapWnd::ZoomToPrevOwnedSystem()
{
    // TODO: go through these in some sorted order (the sort method used in the SidePanel system name drop-list)
    Universe::ObjectIDVec vec = GetUniverse().FindObjectIDs(OwnedVisitor<System>(HumanClientApp::GetApp()->EmpireID()));
    Universe::ObjectIDVec::iterator it = std::find(vec.begin(), vec.end(), m_current_owned_system);
    if (it == vec.end()) {
        m_current_owned_system = vec.empty() ? UniverseObject::INVALID_OBJECT_ID : vec.back();
    } else {
        m_current_owned_system = it == vec.begin() ? vec.back() : *--it;
    }

    if (m_current_owned_system != UniverseObject::INVALID_OBJECT_ID) {
        CenterOnSystem(m_current_owned_system);
        SelectSystem(m_current_owned_system);
    }

    return true;
}

bool MapWnd::ZoomToNextOwnedSystem()
{
    // TODO: go through these in some sorted order (the sort method used in the SidePanel system name drop-list)
    Universe::ObjectIDVec vec = GetUniverse().FindObjectIDs(OwnedVisitor<System>(HumanClientApp::GetApp()->EmpireID()));
    Universe::ObjectIDVec::iterator it = std::find(vec.begin(), vec.end(), m_current_owned_system);
    if (it == vec.end()) {
        m_current_owned_system = vec.empty() ? UniverseObject::INVALID_OBJECT_ID : vec.front();
    } else {
        Universe::ObjectIDVec::iterator next_it = it;
        ++next_it;
        m_current_owned_system = next_it == vec.end() ? vec.front() : *next_it;
    }

    if (m_current_owned_system != UniverseObject::INVALID_OBJECT_ID) {
        CenterOnSystem(m_current_owned_system);
        SelectSystem(m_current_owned_system);
    }

    return true;
}

bool MapWnd::ZoomToPrevIdleFleet()
{
    Universe::ObjectIDVec vec = GetUniverse().FindObjectIDs(StationaryFleetVisitor(HumanClientApp::GetApp()->EmpireID()));
    Universe::ObjectIDVec::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet);
    if (it == vec.end()) {
        m_current_fleet = vec.empty() ? UniverseObject::INVALID_OBJECT_ID : vec.back();
    } else {
        m_current_fleet = it == vec.begin() ? vec.back() : *--it;
    }

    if (m_current_fleet != UniverseObject::INVALID_OBJECT_ID) {
        CenterOnFleet(m_current_fleet);
        SelectFleet(m_current_fleet);
    }

    return true;
}

bool MapWnd::ZoomToNextIdleFleet()
{
    Universe::ObjectIDVec vec = GetUniverse().FindObjectIDs(StationaryFleetVisitor(HumanClientApp::GetApp()->EmpireID()));
    Universe::ObjectIDVec::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet);
    if (it == vec.end()) {
        m_current_fleet = vec.empty() ? UniverseObject::INVALID_OBJECT_ID : vec.front();
    } else {
        Universe::ObjectIDVec::iterator next_it = it;
        ++next_it;
        m_current_fleet = next_it == vec.end() ? vec.front() : *next_it;
    }

    if (m_current_fleet != UniverseObject::INVALID_OBJECT_ID) {
        CenterOnFleet(m_current_fleet);
        SelectFleet(m_current_fleet);
    }

    return true;
}

bool MapWnd::ZoomToPrevFleet()
{
    Universe::ObjectIDVec vec = GetUniverse().FindObjectIDs(OwnedVisitor<Fleet>(HumanClientApp::GetApp()->EmpireID()));
    Universe::ObjectIDVec::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet);
    if (it == vec.end()) {
        m_current_fleet = vec.empty() ? UniverseObject::INVALID_OBJECT_ID : vec.back();
    } else {
        m_current_fleet = it == vec.begin() ? vec.back() : *--it;
    }

    if (m_current_fleet != UniverseObject::INVALID_OBJECT_ID) {
        CenterOnFleet(m_current_fleet);
        SelectFleet(m_current_fleet);
    }

    return true;
}

bool MapWnd::ZoomToNextFleet()
{
    Universe::ObjectIDVec vec = GetUniverse().FindObjectIDs(OwnedVisitor<Fleet>(HumanClientApp::GetApp()->EmpireID()));
    Universe::ObjectIDVec::iterator it = std::find(vec.begin(), vec.end(), m_current_fleet);
    if (it == vec.end()) {
        m_current_fleet = vec.empty() ? UniverseObject::INVALID_OBJECT_ID : vec.front();
    } else {
        Universe::ObjectIDVec::iterator next_it = it;
        ++next_it;
        m_current_fleet = next_it == vec.end() ? vec.front() : *next_it;
    }

    if (m_current_fleet != UniverseObject::INVALID_OBJECT_ID) {
        CenterOnFleet(m_current_fleet);
        SelectFleet(m_current_fleet);
    }

    return true;
}

void MapWnd::SetAccelerators()
{
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_ESCAPE, 0);

    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_RETURN, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_ENTER, 0);

    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_RETURN, GG::GGKMOD_CTRL);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_ENTER, GG::GGKMOD_CTRL);

    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F2, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F3, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F4, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_F10, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_s, 0);

    // Keys for zooming
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_e, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_r, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_PLUS, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_KP_MINUS, 0);

    // Keys for showing systems
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_d, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_x, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_c, 0);

    // Keys for showing fleets
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_f, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_g, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_v, 0);
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_b, 0);

#ifndef FREEORION_RELEASE
    GG::GUI::GetGUI()->SetAccelerator(GG::GGK_r, GG::GGKMOD_CTRL);
#endif
}

void MapWnd::RemoveAccelerators()
{
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_ESCAPE, 0);

    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_RETURN, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_KP_ENTER, 0);
    
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_RETURN, GG::GGKMOD_CTRL);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_KP_ENTER, GG::GGKMOD_CTRL);

    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_F2, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_F3, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_F4, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_F10, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_s, 0);

    // Zoom keys
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_e, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_r, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_KP_PLUS, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_KP_MINUS, 0);

    // Keys for showing systems
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_d, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_x, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_c, 0);

    // Keys for showing fleets
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_f, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_g, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_v, 0);
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_b, 0);

#ifndef FREEORION_RELEASE
    GG::GUI::GetGUI()->RemoveAccelerator(GG::GGK_r, GG::GGKMOD_CTRL);
#endif
}

/* Disables keyboard accelerators that use an alphanumeric key
   without modifiers. This is useful if a keyboard input is required,
   so that the keys aren't interpreted as an accelerator.
   @note Repeated calls of DisableAlphaNumAccels have to be followed by the
   same number of calls to EnableAlphaNumAccels to re-enable the
   accelerators.
*/
void MapWnd::DisableAlphaNumAccels()
{
    for (GG::GUI::const_accel_iterator i = GG::GUI::GetGUI()->accel_begin();
         i != GG::GUI::GetGUI()->accel_end(); ++i) {
        if (i->second != 0) // we only want to disable keys without modifiers
            continue; 
        GG::Key key = i->first;
        if ((key >= GG::GGK_a && key <= GG::GGK_z) || 
            (key >= GG::GGK_0 && key <= GG::GGK_9)) {
            m_disabled_accels_list.insert(key);
        }
    }
    for (std::set<GG::Key>::iterator i = m_disabled_accels_list.begin();
         i != m_disabled_accels_list.end(); ++i) {
        GG::GUI::GetGUI()->RemoveAccelerator(*i, 0);
    }
}

// Re-enable accelerators disabled by DisableAlphaNumAccels
void MapWnd::EnableAlphaNumAccels()
{
    for (std::set<GG::Key>::iterator i = m_disabled_accels_list.begin();
         i != m_disabled_accels_list.end(); ++i) {
        GG::GUI::GetGUI()->SetAccelerator(*i, 0);
    }
    m_disabled_accels_list.clear();
}

void MapWnd::CloseAllPopups()
{
    for (std::list<MapWndPopup*>::iterator it = m_popups.begin(); it != m_popups.end(); ) {
        // get popup and increment iterator first since closing the popup will change this list by removing the popup
        MapWndPopup* popup = *it++;
        popup->Close();
    }   
    // clear list
    m_popups.clear();
}

void MapWnd::HideAllPopups()
{
    for (std::list<MapWndPopup*>::iterator it = m_popups.begin(); it != m_popups.end(); ++it) {
        (*it)->Hide();
    }
}

void MapWnd::ShowAllPopups()
{
    for (std::list<MapWndPopup*>::iterator it = m_popups.begin(); it != m_popups.end(); ++it) {
        (*it)->Show();
    }
}
