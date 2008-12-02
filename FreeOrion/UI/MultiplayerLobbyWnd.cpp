#include "MultiplayerLobbyWnd.h"

#include "CUIControls.h"
#include "../client/human/HumanClientApp.h"
#include "../network/Message.h"
#include "../util/MultiplayerCommon.h"
#include "../util/Serialize.h"

#if defined(_MSC_VER)
  // HACK! this keeps VC 7.x from barfing when it sees "typedef __int64 int64_t;"
  // in boost/cstdint.h when compiling under windows
#  if defined(int64_t)
#    undef int64_t
#  endif
#elif defined(WIN32)
  // HACK! this keeps gcc 3.x from barfing when it sees "typedef long long uint64_t;"
  // in boost/cstdint.h when compiling under windows
#  define BOOST_MSVC -1
#endif

#include <boost/cast.hpp>
#include <boost/serialization/vector.hpp>

#include <GG/Button.h>
#include <GG/DrawUtil.h>
#include <GG/StaticGraphic.h>
#include <GG/TextControl.h>


namespace {
    const GG::Y PLAYER_ROW_HEIGHT(22);
    const GG::X EMPIRE_NAME_WIDTH(170);

    struct PlayerRow : GG::ListBox::Row
    {
        typedef boost::signal<void ()> DataChangedSignalType;

        PlayerRow() {}
        PlayerRow(const PlayerSetupData& player_data) : m_player_data(player_data) {}

        PlayerSetupData       m_player_data;
        DataChangedSignalType DataChangedSignal;
    };

    struct NewGamePlayerRow : PlayerRow
    {
        NewGamePlayerRow(const PlayerSetupData& player_data, bool disabled) : 
            PlayerRow(player_data)
        {
            Resize(GG::Pt(EMPIRE_NAME_WIDTH, PLAYER_ROW_HEIGHT + 6));
            push_back(player_data.m_player_name, ClientUI::Font(), ClientUI::Pts(), ClientUI::TextColor());
            CUIEdit* edit = new CUIEdit(GG::X0, GG::Y0, EMPIRE_NAME_WIDTH, m_player_data.m_empire_name, ClientUI::GetFont(), GG::CLR_ZERO, ClientUI::TextColor(), GG::CLR_ZERO);
            push_back(edit);
            EmpireColorSelector* color_selector = new EmpireColorSelector(PLAYER_ROW_HEIGHT);
            color_selector->SelectColor(m_player_data.m_empire_color);
            push_back(color_selector);

            if (disabled) {
                edit->Disable();
                color_selector->Disable();
            } else {
                Connect(edit->EditedSignal, &NewGamePlayerRow::NameChanged, this);
                Connect(color_selector->ColorChangedSignal, &NewGamePlayerRow::ColorChanged, this);
            }
        }

    private:
        void NameChanged(const std::string& str)
        {
            m_player_data.m_empire_name = str;
            DataChangedSignal();
        }
        void ColorChanged(const GG::Clr& clr)
        {
            m_player_data.m_empire_color = clr;
            DataChangedSignal();
        }
    };

    struct LoadGamePlayerRow : PlayerRow
    {
        LoadGamePlayerRow(const PlayerSetupData& player_data, const std::map<int, SaveGameEmpireData>& save_game_empire_data, bool host, bool disabled) : 
            PlayerRow(player_data),
            m_empire_list(0),
            m_save_game_empire_data(save_game_empire_data)
        {
            Resize(GG::Pt(EMPIRE_NAME_WIDTH, PLAYER_ROW_HEIGHT + 6));
            push_back(player_data.m_player_name, ClientUI::Font(), ClientUI::Pts(), ClientUI::TextColor());
            m_empire_list =
                new CUIDropDownList(GG::X0, GG::Y0, EMPIRE_NAME_WIDTH, PLAYER_ROW_HEIGHT, 5 * PLAYER_ROW_HEIGHT);
            m_empire_list->SetStyle(GG::LIST_NOSORT);
            std::map<int, SaveGameEmpireData>::const_iterator save_game_empire_it = m_save_game_empire_data.end();
            for (std::map<int, SaveGameEmpireData>::const_iterator it = m_save_game_empire_data.begin(); it != m_save_game_empire_data.end(); ++it) {
                m_empire_list->Insert(new CUISimpleDropDownListRow(it->second.m_name));
                // Note that this logic will select based on empire id first.  Only when such a match fails does it
                // attempt to match the current player name to the one in the save game.  Note also that only the host
                // attempts a name match; the other clients just take whatever they're given.
                if (it->first == m_player_data.m_save_game_empire_id || (host && it->second.m_player_name == m_player_data.m_player_name)) {
                    m_empire_list->Select(--m_empire_list->end());
                    m_player_data.m_empire_name = it->second.m_name;
                    m_player_data.m_empire_color = it->second.m_color;
                    m_player_data.m_save_game_empire_id = it->second.m_id;
                    save_game_empire_it = it;
                }
            }
            push_back(m_empire_list);
            m_color_selector = new EmpireColorSelector(PLAYER_ROW_HEIGHT);
            m_color_selector->SelectColor(m_player_data.m_empire_color);
            push_back(m_color_selector);
            push_back(save_game_empire_it != m_save_game_empire_data.end() ? save_game_empire_it->second.m_player_name : "",
                      ClientUI::Font(), ClientUI::Pts(), ClientUI::TextColor());

            m_color_selector->Disable();

            if (disabled)
                m_empire_list->Disable();
            else
                Connect(m_empire_list->SelChangedSignal, &LoadGamePlayerRow::EmpireChanged, this);
        }

    private:
        void EmpireChanged(GG::DropDownList::iterator selected_it)
        {
            assert(selected_it != m_empire_list->end());
            std::map<int, SaveGameEmpireData>::const_iterator it = m_save_game_empire_data.begin();
            std::advance(it, m_empire_list->IteratorToIndex(selected_it));
            m_player_data.m_empire_name = it->second.m_name;
            m_player_data.m_empire_color = it->second.m_color;
            m_player_data.m_save_game_empire_id = it->second.m_id;
            m_color_selector->SelectColor(m_player_data.m_empire_color);
            operator[](3)->SetText(it->second.m_player_name);
            DataChangedSignal();
        }

        EmpireColorSelector*                     m_color_selector;
        CUIDropDownList*                         m_empire_list;
        const std::map<int, SaveGameEmpireData>& m_save_game_empire_data;
    };

    const GG::X    LOBBY_WND_WIDTH(800);
    const GG::Y    LOBBY_WND_HEIGHT(600);
    const int           CONTROL_MARGIN = 5; // gap to leave between controls in the window
    const GG::X    GALAXY_SETUP_PANEL_WIDTH(250);
    const GG::Y    SAVED_GAMES_LIST_ROW_HEIGHT(22);
    const GG::Y    SAVED_GAMES_LIST_DROP_HEIGHT(10 * SAVED_GAMES_LIST_ROW_HEIGHT);
    const GG::X    CHAT_WIDTH(250);
    GG::Pt              g_preview_ul;
    const GG::Pt        PREVIEW_SZ(GG::X(248), GG::Y(186));
    const int           PREVIEW_MARGIN = 3;

}

MultiplayerLobbyWnd::MultiplayerLobbyWnd(bool host,
                                         const CUIButton::ClickedSlotType& start_game_callback,
                                         const CUIButton::ClickedSlotType& cancel_callback) : 
    CUIWnd(UserString("MPLOBBY_WINDOW_TITLE"), (GG::GUI::GetGUI()->AppWidth() - LOBBY_WND_WIDTH) / 2, 
           (GG::GUI::GetGUI()->AppHeight() - LOBBY_WND_HEIGHT) / 2, LOBBY_WND_WIDTH, LOBBY_WND_HEIGHT, 
           GG::ONTOP | GG::CLICKABLE),
    m_handling_lobby_update(false),
    m_host(host),
    m_chat_box(0),
    m_chat_input_edit(0),
    m_new_load_game_buttons(0),
    m_galaxy_setup_panel(0),
    m_saved_games_list(0),
    m_preview_image(0),
    m_players_lb(0),
    m_start_game_bn(0),
    m_cancel_bn(0),
    m_start_conditions_text(0)
{
    TempUISoundDisabler sound_disabler;

    GG::X x(CONTROL_MARGIN);
    m_chat_input_edit = new CUIEdit(x, ClientHeight() - (ClientUI::Pts() + 10) - 2 * CONTROL_MARGIN, CHAT_WIDTH - x, "");
    m_chat_box = new CUIMultiEdit(x, GG::Y(CONTROL_MARGIN), CHAT_WIDTH - x, m_chat_input_edit->UpperLeft().y - 2 * CONTROL_MARGIN, "", 
                                  GG::MULTI_LINEWRAP | GG::MULTI_READ_ONLY | GG::MULTI_TERMINAL_STYLE);
    m_chat_box->SetMaxLinesOfHistory(250);

    const GG::Y RADIO_BN_HT(ClientUI::Pts() + 4);

    m_galaxy_setup_panel = new GalaxySetupPanel(CHAT_WIDTH + 2 * CONTROL_MARGIN, RADIO_BN_HT, GALAXY_SETUP_PANEL_WIDTH);

    m_new_load_game_buttons =
        new GG::RadioButtonGroup(CHAT_WIDTH + CONTROL_MARGIN, GG::Y(CONTROL_MARGIN),
                                 GG::X(Value(m_galaxy_setup_panel->LowerRight().y + 100)), m_galaxy_setup_panel->LowerRight().y + RADIO_BN_HT - CONTROL_MARGIN,
                                 GG::VERTICAL);
    m_new_load_game_buttons->AddButton(
        new CUIStateButton(GG::X0, GG::Y0, GG::X(100), RADIO_BN_HT, UserString("NEW_GAME_BN"), GG::FORMAT_LEFT, GG::SBSTYLE_3D_RADIO));
    m_new_load_game_buttons->AddButton(
        new CUIStateButton(GG::X0, GG::Y0, GG::X(100), RADIO_BN_HT, UserString("LOAD_GAME_BN"), GG::FORMAT_LEFT, GG::SBSTYLE_3D_RADIO));

    m_saved_games_list = new CUIDropDownList(CHAT_WIDTH + 2 * CONTROL_MARGIN, m_new_load_game_buttons->LowerRight().y + CONTROL_MARGIN, 
                                             GALAXY_SETUP_PANEL_WIDTH, SAVED_GAMES_LIST_ROW_HEIGHT, SAVED_GAMES_LIST_DROP_HEIGHT);
    m_saved_games_list->SetStyle(GG::LIST_NOSORT);

    g_preview_ul = GG::Pt(ClientWidth() - PREVIEW_SZ.x - CONTROL_MARGIN - PREVIEW_MARGIN, GG::Y(CONTROL_MARGIN + PREVIEW_MARGIN));
    boost::shared_ptr<GG::Texture> temp_tex(new GG::Texture());
    m_preview_image = new GG::StaticGraphic(g_preview_ul.x, g_preview_ul.y, PREVIEW_SZ.x, PREVIEW_SZ.y, temp_tex, GG::GRAPHIC_FITGRAPHIC);

    x = CHAT_WIDTH + CONTROL_MARGIN;
    GG::Y y = std::max(m_saved_games_list->LowerRight().y, m_preview_image->LowerRight().y) + CONTROL_MARGIN;
    m_players_lb = new CUIListBox(x, y, ClientWidth() - CONTROL_MARGIN - x, m_chat_input_edit->UpperLeft().y - CONTROL_MARGIN - y);
    m_players_lb->SetStyle(GG::LIST_NOSORT | GG::LIST_NOSEL);

    if (m_host)
        m_start_game_bn = new CUIButton(GG::X0, GG::Y0, GG::X(125), UserString("START_GAME_BN"));
    m_cancel_bn = new CUIButton(GG::X0, GG::Y0, GG::X(125), UserString("CANCEL"));
    m_cancel_bn->MoveTo(GG::Pt(ClientWidth() - m_cancel_bn->Width() - CONTROL_MARGIN, ClientHeight() - m_cancel_bn->Height() - CONTROL_MARGIN));
    if (m_host)
        m_start_game_bn->MoveTo(GG::Pt(m_cancel_bn->UpperLeft().x - CONTROL_MARGIN - m_start_game_bn->Width(),
                                       ClientHeight() - m_cancel_bn->Height() - CONTROL_MARGIN));

    m_start_conditions_text = new GG::TextControl(x, ClientHeight() - m_cancel_bn->Height() - CONTROL_MARGIN,
                                                  m_cancel_bn->UpperLeft().x - x, GG::Y(ClientUI::Pts()*3/2),
                                                  UserString("MULTIPLAYER_GAME_START_CONDITIONS"),
                                                  ClientUI::GetFont(),
                                                  ClientUI::TextColor(), GG::FORMAT_LEFT);

    AttachChild(m_chat_box);
    AttachChild(m_chat_input_edit);
    AttachChild(m_new_load_game_buttons);
    AttachChild(m_galaxy_setup_panel);
    AttachChild(m_saved_games_list);
    AttachChild(m_preview_image);
    AttachChild(m_players_lb);
    AttachChild(m_start_game_bn);
    AttachChild(m_cancel_bn);
    AttachChild(m_start_conditions_text);

    // default settings (new game)
    m_new_load_game_buttons->SetCheck(0);
    PreviewImageChanged(m_galaxy_setup_panel->PreviewImage());
    m_saved_games_list->Disable();

    if (!m_host) {
        for (int i = 0; i < m_new_load_game_buttons->NumButtons(); ++i) {
            m_new_load_game_buttons->DisableButton(i);
        }
        m_galaxy_setup_panel->Disable();
        m_saved_games_list->Disable();
    }

    if (m_host) {
        Connect(m_new_load_game_buttons->ButtonChangedSignal, &MultiplayerLobbyWnd::NewLoadClicked, this);
        Connect(m_galaxy_setup_panel->SettingsChangedSignal, &MultiplayerLobbyWnd::GalaxySetupPanelChanged, this);
        Connect(m_saved_games_list->SelChangedSignal, &MultiplayerLobbyWnd::SaveGameChanged, this);
        GG::Connect(m_start_game_bn->ClickedSignal, start_game_callback);
    }
    Connect(m_galaxy_setup_panel->ImageChangedSignal, &MultiplayerLobbyWnd::PreviewImageChanged, this);
    GG::Connect(m_cancel_bn->ClickedSignal, cancel_callback);
}

bool MultiplayerLobbyWnd::LoadGameSelected() const
{ return m_new_load_game_buttons->CheckedButton() == 1; }

void MultiplayerLobbyWnd::Render()
{
    CUIWnd::Render();
    GG::Pt image_ul = g_preview_ul + ClientUpperLeft(), image_lr = image_ul + PREVIEW_SZ;
    GG::FlatRectangle(GG::Pt(image_ul.x - PREVIEW_MARGIN, image_ul.y - PREVIEW_MARGIN),
                      GG::Pt(image_lr.x + PREVIEW_MARGIN, image_lr.y + PREVIEW_MARGIN), 
                      GG::CLR_BLACK, ClientUI::WndInnerBorderColor(), 1);
}

void MultiplayerLobbyWnd::KeyPress(GG::Key key, boost::uint32_t key_code_point, GG::Flags<GG::ModKey> mod_keys)
{
    if ((key == GG::GGK_RETURN || key == GG::GGK_KP_ENTER) && GG::GUI::GetGUI()->FocusWnd() == m_chat_input_edit) {
        int receiver = -1; // all players by default
        std::string text = m_chat_input_edit->WindowText();
        HumanClientApp::GetApp()->Networking().SendMessage(LobbyChatMessage(HumanClientApp::GetApp()->PlayerID(), receiver, text));
        m_chat_input_edit->SetText("");
        *m_chat_box += m_lobby_data.m_players[HumanClientApp::GetApp()->PlayerID()].m_player_name + ": " + text + "\n";
    } else if (m_start_game_bn && !m_start_game_bn->Disabled() && (key == GG::GGK_RETURN || key == GG::GGK_KP_ENTER)) {
        m_start_game_bn->ClickedSignal();
    } else if (key == GG::GGK_ESCAPE) {
       m_cancel_bn->ClickedSignal();
    }
}

void MultiplayerLobbyWnd::ChatMessage(int player_id, const std::string& msg)
{
    std::map<int, PlayerSetupData>::iterator it = m_lobby_data.m_players.find(player_id);
    *m_chat_box += (it != m_lobby_data.m_players.end() ? (it->second.m_player_name + ": ") : "[unknown]: ") + msg + '\n';
}

void MultiplayerLobbyWnd::LobbyUpdate(const MultiplayerLobbyData& lobby_data)
{
    m_handling_lobby_update = true;

    m_new_load_game_buttons->SetCheck(!lobby_data.m_new_game);
    m_galaxy_setup_panel->SetFromSetupData(lobby_data);

    m_saved_games_list->Clear();
    for (unsigned int i = 0; i < lobby_data.m_save_games.size(); ++i) {
        m_saved_games_list->Insert(new CUISimpleDropDownListRow(lobby_data.m_save_games[i]));
        if (static_cast<int>(i) == lobby_data.m_save_file_index)
            m_saved_games_list->Select(lobby_data.m_save_file_index);
    }

    m_lobby_data = lobby_data;

    bool send_update_back = PopulatePlayerList();

    m_handling_lobby_update = false;

    if (m_host && send_update_back)
        SendUpdate();
}

void MultiplayerLobbyWnd::LobbyExit(int player_id)
{
    std::string player_name = m_lobby_data.m_players[player_id].m_player_name;
    m_lobby_data.m_players.erase(player_id);
    for (GG::ListBox::iterator it = m_players_lb->begin(); it != m_players_lb->end(); ++it) {
        if (player_name == (**it)[0]->WindowText()) {
            delete m_players_lb->Erase(it);
            break;
        }
    }
    if (m_host)
        m_start_game_bn->Disable(!CanStart());
}

void MultiplayerLobbyWnd::NewLoadClicked(int idx)
{
    switch (idx) {
    case 0:
        m_lobby_data.m_new_game = true;
        m_galaxy_setup_panel->Disable(false);
        m_saved_games_list->Disable();
        break;
    case 1:
        m_lobby_data.m_new_game = false;
        m_galaxy_setup_panel->Disable();
        m_saved_games_list->Disable(false);
        break;
    default:
        break;
    }
    PopulatePlayerList();
    SendUpdate();
}

void MultiplayerLobbyWnd::GalaxySetupPanelChanged()
{
    m_galaxy_setup_panel->GetSetupData(m_lobby_data);
    SendUpdate();
}

void MultiplayerLobbyWnd::SaveGameChanged(GG::DropDownList::iterator it)
{
    m_lobby_data.m_save_file_index = m_saved_games_list->IteratorToIndex(it);
    m_lobby_data.m_save_game_empire_data.clear();
    PopulatePlayerList();
    SendUpdate();
}

void MultiplayerLobbyWnd::PreviewImageChanged(boost::shared_ptr<GG::Texture> new_image)
{
    if (m_preview_image) {
        DeleteChild(m_preview_image);
        m_preview_image = 0;
    }
    m_preview_image = new GG::StaticGraphic(g_preview_ul.x, g_preview_ul.y, PREVIEW_SZ.x, PREVIEW_SZ.y, new_image, GG::GRAPHIC_FITGRAPHIC);
    AttachChild(m_preview_image);
}

void MultiplayerLobbyWnd::PlayerDataChanged()
{
    m_lobby_data.m_players.clear();
    for (GG::ListBox::iterator it = m_players_lb->begin(); it != m_players_lb->end(); ++it) {
        const PlayerRow* row = boost::polymorphic_downcast<const PlayerRow*>(*it);
        m_lobby_data.m_players[row->m_player_data.m_player_id] = row->m_player_data;
    }
    if (m_host)
        m_start_game_bn->Disable(!CanStart());
    SendUpdate();
}

bool MultiplayerLobbyWnd::PopulatePlayerList()
{
    bool retval = false;

    m_players_lb->Clear();

    for (unsigned int i = 0; i < m_lobby_data.m_players.size(); ++i) {
        int id = m_lobby_data.m_players[i].m_player_id;
        if (m_lobby_data.m_new_game) {
            NewGamePlayerRow* row =
                new NewGamePlayerRow(m_lobby_data.m_players[id],
                                     !m_host && id != HumanClientApp::GetApp()->PlayerID());
            m_players_lb->Insert(row);
            Connect(row->DataChangedSignal, &MultiplayerLobbyWnd::PlayerDataChanged, this);
        } else {
            bool disable = !m_host && id != HumanClientApp::GetApp()->PlayerID() ||
                m_lobby_data.m_save_game_empire_data.empty();
            LoadGamePlayerRow* row =
                new LoadGamePlayerRow(m_lobby_data.m_players[id],
                                      m_lobby_data.m_save_game_empire_data,
                                      m_host, disable);
            m_players_lb->Insert(row);
            Connect(row->DataChangedSignal, &MultiplayerLobbyWnd::PlayerDataChanged, this);
            if (row->m_player_data.m_save_game_empire_id != m_lobby_data.m_players[id].m_save_game_empire_id) {
                m_lobby_data.m_players[id] = row->m_player_data;
                retval = true;
            }
        }
    }

    if (m_lobby_data.m_new_game) {
        m_players_lb->SetNumCols(3);
        m_players_lb->SetColAlignment(2, GG::ALIGN_RIGHT);
    } else {
        m_players_lb->SetNumCols(4);
        m_players_lb->SetColAlignment(2, GG::ALIGN_RIGHT);
        m_players_lb->SetColAlignment(3, GG::ALIGN_RIGHT);
    }

    if (m_host)
        m_start_game_bn->Disable(!CanStart());

    return retval;
}

void MultiplayerLobbyWnd::SendUpdate()
{
    if (!m_handling_lobby_update) {
        int player_id = HumanClientApp::GetApp()->PlayerID();
        if (player_id != -1)
            HumanClientApp::GetApp()->Networking().SendMessage(LobbyUpdateMessage(player_id, m_lobby_data));
    }
}

bool MultiplayerLobbyWnd::PlayerDataAcceptable() const
{
    std::set<std::string> empire_names;
    std::set<unsigned int> empire_colors;
    for (GG::ListBox::iterator it = m_players_lb->begin(); it != m_players_lb->end(); ++it) {
        const PlayerRow& row = dynamic_cast<const PlayerRow&>(**it);
        if (row.m_player_data.m_empire_name.empty())
            return false;
        empire_names.insert(row.m_player_data.m_empire_name);
        unsigned int color_as_uint =
            row.m_player_data.m_empire_color.r << 24 |
            row.m_player_data.m_empire_color.g << 16 |
            row.m_player_data.m_empire_color.b << 8 |
            row.m_player_data.m_empire_color.a;
        empire_colors.insert(color_as_uint);
    }
    return empire_names.size() == m_players_lb->NumRows() &&
        empire_colors.size() == m_players_lb->NumRows();
}

bool MultiplayerLobbyWnd::CanStart() const
{ return PlayerDataAcceptable() && (LoadGameSelected() || 1 < m_players_lb->NumRows()); }
