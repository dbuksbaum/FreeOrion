//MapWnd.cpp

#include "MapWnd.h"

#include "ClientUI.h"
#include "CUIControls.h"
#include "../universe/Fleet.h"
#include "FleetButton.h"
#include "FleetWindow.h"
#include "GGDrawUtil.h"
#include "../client/human/HumanClientApp.h"
#include "../network/Message.h"
#include "../util/OptionsDB.h"
#include "../universe/Planet.h"
#include "../universe/Predicates.h"
#include "../util/Random.h"
#include "SidePanel.h"
#include "SitRepPanel.h"
#include "../universe/System.h"
#include "SystemIcon.h"
#include "../universe/Universe.h"
#include "../universe/UniverseObject.h"
#include "TurnProgressWnd.h"

#include <vector>


namespace {
    const double ZOOM_STEP_SIZE = 1.25;
    const int NUM_NEBULA_TEXTURES = 5;
    const int MIN_NEBULAE = 3; // this min and max are for a 1000.0-width galaxy
    const int MAX_NEBULAE = 6;
    const int END_TURN_BTN_WIDTH = 60;
    const int SITREP_PANEL_WIDTH = 400;
    const int SITREP_PANEL_HEIGHT = 300;
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
    void SetSystems(const System* src_system, const System* dst_system) 
    {
        src = std::max(src_system, dst_system); 
        dst = std::min(src_system, dst_system);
    }

    const System* Src() const {return src;}
    const System* Dst() const {return dst;}

private:
    const System* src;
    const System* dst;
};


////////////////////////////////////////////////
// MapWnd::MovementLineData
////////////////////////////////////////////////
struct MapWnd::MovementLineData
{
    MovementLineData() {}
    MovementLineData(double x_, double y_, const std::list<System*>& dest) : x(x_), y(y_), destinations(dest) {}
    double x;
    double y;
    std::list<System*> destinations;
    // TODO : color, other properties(?), based on moving empire and destination, etc.
};


////////////////////////////////////////////////////////////
// MapWndPopup
////////////////////////////////////////////////////////////

MapWndPopup::MapWndPopup( const std::string& t, int x, int y, int h, int w, Uint32 flags ):
    CUI_Wnd( t, x, y, h, w, flags )
{
    // register with map wnd
    HumanClientApp::GetUI()->GetMapWnd()->RegisterPopup( this );
}

MapWndPopup::MapWndPopup(const GG::XMLElement& elem):
    CUI_Wnd( elem )
{
}

MapWndPopup::~MapWndPopup( )
{
    // remove from map wnd
    HumanClientApp::GetUI()->GetMapWnd()->RemovePopup( this );
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
double    MapWnd::s_min_scale_factor = 0.5;
double    MapWnd::s_max_scale_factor = 8.0;
const int MapWnd::SIDE_PANEL_WIDTH = 300;

namespace {
    const int MAP_MARGIN_WIDTH = MapWnd::SIDE_PANEL_WIDTH; // the number of pixels of system-less space around all four sides of the starfield
}

MapWnd::MapWnd() :
    GG::Wnd(-GG::App::GetApp()->AppWidth(), -GG::App::GetApp()->AppHeight(),
            static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor) + GG::App::GetApp()->AppWidth() + MAP_MARGIN_WIDTH, 
            static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor) + GG::App::GetApp()->AppHeight() + MAP_MARGIN_WIDTH, 
            GG::Wnd::CLICKABLE | GG::Wnd::DRAGABLE),
    m_backgrounds(NUM_BACKGROUNDS),
    m_bg_scroll_rate(NUM_BACKGROUNDS),
    m_bg_position_X(NUM_BACKGROUNDS),
    m_bg_position_Y(NUM_BACKGROUNDS),
    m_zoom_factor(1.0),
    m_drag_offset(-1, -1),
    m_dragged(false)
{
    SetText("MapWnd");

    m_side_panel = new SidePanel(GG::App::GetApp()->AppWidth() - SIDE_PANEL_WIDTH, 0, SIDE_PANEL_WIDTH, GG::App::GetApp()->AppHeight());
    AttachChild(m_side_panel);
    Connect(SelectedSystemSignal(), &SidePanel::SetSystem, m_side_panel);

    m_sitrep_panel = new SitRepPanel( (GG::App::GetApp()->AppWidth()-SITREP_PANEL_WIDTH)/2, (GG::App::GetApp()->AppHeight()-SITREP_PANEL_HEIGHT)/2, SITREP_PANEL_WIDTH, SITREP_PANEL_HEIGHT );
    AttachChild(m_sitrep_panel);
	
	m_options_showing = false;

    //set up background images
    m_backgrounds[0].reset(new GG::Texture());
    m_backgrounds[0]->Load(ClientUI::ART_DIR + "starfield1.png");
    m_bg_position_X[0] = 10.0;
    m_bg_position_Y[0] = 10.0;
    m_bg_scroll_rate[0] = 0.125;

    m_backgrounds[1].reset(new GG::Texture());
    m_backgrounds[1]->Load(ClientUI::ART_DIR + "starfield2.png");
    m_bg_position_X[1] = 10.0;
    m_bg_position_Y[1] = 10.0;
    m_bg_scroll_rate[1] = 0.25;

    m_backgrounds[2].reset(new GG::Texture());
    m_backgrounds[2]->Load(ClientUI::ART_DIR + "starfield3.png");
    m_bg_position_X[2] = 10.0;
    m_bg_position_Y[2] = 10.0;
    m_bg_scroll_rate[2] = 0.5;

    // create buttons
    m_turn_update = new CUIButton(5, 5, END_TURN_BTN_WIDTH, "" );

    //attach buttons
    AttachChild(m_turn_update);

    //connect signals and slots
    GG::Connect(m_turn_update->ClickedSignal(), &MapWnd::OnTurnUpdate, this);

    // register keyboard accelerators for function keys, and listen for them
    GG::Connect(GG::App::GetApp()->AcceleratorSignal(GG::GGK_F2, 0), &MapWnd::ToggleSitRep, this);
    GG::Connect(GG::App::GetApp()->AcceleratorSignal(GG::GGK_F10, 0), &MapWnd::ShowOptions, this);
}

MapWnd::~MapWnd()
{
    GG::App::GetApp()->RemoveAccelerator(GG::GGK_F2, 0);
    GG::App::GetApp()->RemoveAccelerator(GG::GGK_F10, 0);
}

GG::Pt MapWnd::ClientUpperLeft() const
{
    return UpperLeft() + GG::Pt(GG::App::GetApp()->AppWidth(), GG::App::GetApp()->AppHeight());
}

GG::XMLElement MapWnd::SaveGameData() const
{
    GG::XMLElement retval("MapWnd");
    retval.AppendChild(GG::XMLElement("upper_left", UpperLeft().XMLEncode()));
    retval.AppendChild(GG::XMLElement("m_zoom_factor", boost::lexical_cast<std::string>(m_zoom_factor)));
    retval.AppendChild(GG::XMLElement("m_nebulae"));
    for (unsigned int i = 0; i < m_nebulae.size(); ++i) {
        retval.LastChild().AppendChild(GG::XMLElement("nebula" + boost::lexical_cast<std::string>(i)));
        retval.LastChild().LastChild().AppendChild(GG::XMLElement("filename", m_nebulae[i]->Filename()));
        retval.LastChild().LastChild().AppendChild(GG::XMLElement("position", m_nebula_centers[i].XMLEncode()));
    }
    return retval;
}

bool MapWnd::Render()
{
    RenderBackgrounds();
    RenderStarlanes();
    RenderFleetMovementLines();
    return true;
}

void MapWnd::Keypress (GG::Key key, Uint32 key_mods)
{
    switch (key) {
    case GG::GGK_RETURN: { // start turn
        HumanClientApp::GetApp()->StartTurn();
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
    m_turn_update->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);
    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::App::GetApp()->AppWidth(), GG::App::GetApp()->AppHeight()));
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
    if (!m_dragged)
        m_selected_system_signal(UniverseObject::INVALID_OBJECT_ID);
    m_dragged = false;
}

void MapWnd::RClick(const GG::Pt& pt, Uint32 keys)
{
    // if fleet window quickclose is enabled, the right-click will close fleet windows; if there are no open fleet 
    // windows or the quickclose option is disabled, just pop up a context-sensitive menu
    if (!GetOptionsDB().Get<bool>("UI.fleet-window-quickclose") || !FleetWnd::CloseAllFleetWnds()) {
        // TODO : provide a context-sensitive menu for the main map, if needed
    }
}

void MapWnd::MouseWheel(const GG::Pt& pt, int move, Uint32 keys)
{
    GG::Pt ul = ClientUpperLeft();
    GG::Pt center = GG::Pt( GG::App::GetApp()->AppWidth() / 2,  GG::App::GetApp()->AppHeight() / 2);
    GG::Pt ul_offset = ul - center;
    if (0 < move) {
        if (m_zoom_factor * ZOOM_STEP_SIZE < s_max_scale_factor) {
            ul_offset.x = static_cast<int>(ul_offset.x * ZOOM_STEP_SIZE);
            ul_offset.y = static_cast<int>(ul_offset.y * ZOOM_STEP_SIZE);
            m_zoom_factor *= ZOOM_STEP_SIZE;
        } else {
            ul_offset.x = static_cast<int>(ul_offset.x * s_max_scale_factor / m_zoom_factor);
            ul_offset.y = static_cast<int>(ul_offset.y * s_max_scale_factor / m_zoom_factor);
            m_zoom_factor = s_max_scale_factor;
        }
    } else if ( 0 > move ) {
        if (s_min_scale_factor < m_zoom_factor / ZOOM_STEP_SIZE) {
            ul_offset.x = static_cast<int>(ul_offset.x / ZOOM_STEP_SIZE);
            ul_offset.y = static_cast<int>(ul_offset.y / ZOOM_STEP_SIZE);
            m_zoom_factor /= ZOOM_STEP_SIZE;
        } else {
            ul_offset.x = static_cast<int>(ul_offset.x * s_min_scale_factor / m_zoom_factor);
            ul_offset.y = static_cast<int>(ul_offset.y * s_min_scale_factor / m_zoom_factor);
            m_zoom_factor = s_min_scale_factor;
        }
    } else {
        return; // Windows platform always sends an additional event with a move of 0. This should be ignored
    }

    for (unsigned int i = 0; i < m_system_icons.size(); ++i) {
        const System& system = m_system_icons[i]->GetSystem();
        GG::Pt icon_ul(static_cast<int>((system.X() - ClientUI::SYSTEM_ICON_SIZE / 2) * m_zoom_factor), 
                       static_cast<int>((system.Y() - ClientUI::SYSTEM_ICON_SIZE / 2) * m_zoom_factor));
        m_system_icons[i]->SizeMove(icon_ul.x, icon_ul.y, 
                             static_cast<int>(icon_ul.x + ClientUI::SYSTEM_ICON_SIZE * m_zoom_factor + 0.5), 
                             static_cast<int>(icon_ul.y + ClientUI::SYSTEM_ICON_SIZE * m_zoom_factor + 0.5));
    }

    for (unsigned int i = 0; i < m_moving_fleet_buttons.size(); ++i) {
        Fleet* fleet = *m_moving_fleet_buttons[i]->Fleets().begin();
        double x = fleet->X();
        double y = fleet->Y();
        GG::Pt button_ul(static_cast<int>((x - ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE / 2) * m_zoom_factor), 
                         static_cast<int>((y - ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE / 2) * m_zoom_factor));
        m_moving_fleet_buttons[i]->SizeMove(button_ul.x, button_ul.y, 
                                     static_cast<int>(button_ul.x + ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE * m_zoom_factor + 0.5), 
                                     static_cast<int>(button_ul.y + ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE * m_zoom_factor + 0.5));
    }

    GG::Pt map_move = ul_offset + center - ul;
    OffsetMove(map_move);
    MoveBackgrounds(map_move);
    m_side_panel->OffsetMove(-map_move);
    m_turn_update->OffsetMove(-map_move);
    m_sitrep_panel->OffsetMove(-map_move);

    // this correction ensures that zooming in doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);
    GG::Pt final_move = move_to_pt - ul;
    m_side_panel->OffsetMove(-final_move);
    m_turn_update->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);
    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::App::GetApp()->AppWidth(), GG::App::GetApp()->AppHeight()));
}

void MapWnd::InitTurn(int turn_number)
{ 
    GG::App::GetApp()->SetAccelerator(GG::GGK_F2, 0);
    GG::App::GetApp()->SetAccelerator(GG::GGK_F10, 0);

    Universe& universe = ClientApp::GetUniverse();

    // assumes the app is wider than it is tall, and so if it fits in the height it will fit in the width
    if (GG::App::GetApp()->AppHeight() - 2.0 * MAP_MARGIN_WIDTH < Universe::UniverseWidth() * s_min_scale_factor)
        s_min_scale_factor = std::max(0.05, (GG::App::GetApp()->AppHeight() - 2.0 * MAP_MARGIN_WIDTH) / Universe::UniverseWidth());

    Resize(static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor) + GG::App::GetApp()->AppWidth() + MAP_MARGIN_WIDTH,
	   static_cast<int>(Universe::UniverseWidth() * s_max_scale_factor) + GG::App::GetApp()->AppHeight() + MAP_MARGIN_WIDTH);

    // set up nebulae on the first turn
    if (m_nebulae.empty()) {
	    // chosen so that the density of nebulae will be about MIN_NEBULAE to MAX_NEBULAE for a 1000.0-width galaxy
	    const double DENSITY_SCALE_FACTOR = (Universe::UniverseWidth() * Universe::UniverseWidth()) / (1000.0 * 1000.0);
	    int num_nebulae = RandSmallInt(static_cast<int>(MIN_NEBULAE * DENSITY_SCALE_FACTOR), 
				                       static_cast<int>(MAX_NEBULAE * DENSITY_SCALE_FACTOR));
	    m_nebulae.resize(num_nebulae);
	    m_nebula_centers.resize(num_nebulae);
	    SmallIntDistType universe_placement = SmallIntDist(0, static_cast<int>(Universe::UniverseWidth()));
	    SmallIntDistType nebula_type = SmallIntDist(1, NUM_NEBULA_TEXTURES);
	    for (int i = 0; i < num_nebulae; ++i) {
	        std::string nebula_filename = "nebula" + boost::lexical_cast<std::string>(nebula_type()) + ".png";
	        m_nebulae[i].reset(new GG::Texture());
	        m_nebulae[i]->Load(ClientUI::ART_DIR + nebula_filename);
	        m_nebula_centers[i] = GG::Pt(universe_placement(), universe_placement());
	    }
    }

    // this gets cleared here instead of with the movement line stuff because that would clear some movement lines that come from the SystemIcons below
    m_fleet_lines.clear();

    // systems and starlanes
    for (unsigned int i = 0; i < m_system_icons.size(); ++i) {
        DeleteChild(m_system_icons[i]);
    }
    m_system_icons.clear();
    m_starlanes.clear();

    std::vector<System*> systems = universe.FindObjects<System>();
    for (unsigned int i = 0; i < systems.size(); ++i) {
        // system
        SystemIcon* icon = new SystemIcon(systems[i]->ID(), m_zoom_factor);
        m_system_icons.push_back(icon);
        icon->InstallEventFilter(this);
        AttachChild(icon);
        GG::Connect(icon->LeftClickedSignal(), &MapWnd::SelectSystem, this);

        // system's starlanes
        int n = 0;
        for (System::lane_iterator it = systems[i]->begin_lanes(); it != systems[i]->end_lanes(); ++it) {
            if (!it->second) {
                System* dest_system = dynamic_cast<System*>(universe.Object(it->first));
                m_starlanes.insert(StarlaneData(systems[i], dest_system));
            }
        }
    }        

    // fleets not in systems
    for (unsigned int i = 0; i < m_moving_fleet_buttons.size(); ++i) {
        DeleteChild(m_moving_fleet_buttons[i]);
    }
    m_moving_fleet_buttons.clear();

    Universe::ObjectVec fleets = universe.FindObjects(IsMovingFleetFunctor());
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
    m_side_panel->Hide();

    // set turn button to current turn
    m_turn_update->SetText( ClientUI::String("MAP_BTN_TURN_UPDATE") + boost::lexical_cast<std::string>(turn_number ) );    
    MoveChildUp( m_turn_update );

    MoveChildUp(m_sitrep_panel);
    // are there any sitreps to show?
    Empire *empire = HumanClientApp::GetApp()->Empires().Lookup( HumanClientApp::GetApp()->PlayerID() );
    m_sitrep_panel->Update();
    if ( empire->NumSitRepEntries( ) )
        m_sitrep_panel->Show();
    else
        m_sitrep_panel->Hide();
}

void MapWnd::RestoreFromSaveData(const GG::XMLElement& elem)
{
    m_zoom_factor = boost::lexical_cast<double>(elem.Child("m_zoom_factor").Text());

    for (unsigned int i = 0; i < m_system_icons.size(); ++i) {
        const System& system = m_system_icons[i]->GetSystem();
        GG::Pt icon_ul(static_cast<int>((system.X() - ClientUI::SYSTEM_ICON_SIZE / 2) * m_zoom_factor), 
                       static_cast<int>((system.Y() - ClientUI::SYSTEM_ICON_SIZE / 2) * m_zoom_factor));
        m_system_icons[i]->SizeMove(icon_ul.x, icon_ul.y, 
                             static_cast<int>(icon_ul.x + ClientUI::SYSTEM_ICON_SIZE * m_zoom_factor + 0.5), 
                             static_cast<int>(icon_ul.y + ClientUI::SYSTEM_ICON_SIZE * m_zoom_factor + 0.5));
    }

    for (unsigned int i = 0; i < m_moving_fleet_buttons.size(); ++i) {
        Fleet* fleet = *m_moving_fleet_buttons[i]->Fleets().begin();
        double x = fleet->X();
        double y = fleet->Y();
        GG::Pt button_ul(static_cast<int>((x - ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE / 2) * m_zoom_factor), 
                         static_cast<int>((y - ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE / 2) * m_zoom_factor));
        m_moving_fleet_buttons[i]->SizeMove(button_ul.x, button_ul.y, 
                                     static_cast<int>(button_ul.x + ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE * m_zoom_factor + 0.5), 
                                     static_cast<int>(button_ul.y + ClientUI::SYSTEM_ICON_SIZE * ClientUI::FLEET_BUTTON_SIZE * m_zoom_factor + 0.5));
    }

    GG::Pt ul = UpperLeft();
    GG::Pt map_move = GG::Pt(elem.Child("upper_left").Child("GG::Pt")) - ul;
    OffsetMove(map_move);
    MoveBackgrounds(map_move);
    m_side_panel->OffsetMove(-map_move);
    m_turn_update->OffsetMove(-map_move);
    m_sitrep_panel->OffsetMove(-map_move);

    // this correction ensures that zooming in doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);
    GG::Pt final_move = move_to_pt - ul;
    m_side_panel->OffsetMove(-final_move);
    m_turn_update->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);
    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::App::GetApp()->AppWidth(), GG::App::GetApp()->AppHeight()));

    m_nebulae.clear();
    m_nebula_centers.clear();
    const GG::XMLElement& nebulae = elem.Child("m_nebulae");
    for (int i = 0; i < nebulae.NumChildren(); ++i) {
        const GG::XMLElement& curr_nebula = nebulae.Child(i);
        m_nebulae.push_back(GG::App::GetApp()->GetTexture(curr_nebula.Child("filename").Text()));
        m_nebula_centers.push_back(GG::Pt(curr_nebula.Child("position").Child("GG::Pt")));
    }
}

void MapWnd::ShowSystemNames()
{
    for (unsigned int i = 0; i < m_system_icons.size(); ++i)
        m_system_icons[i]->ShowName();
}

void MapWnd::HideSystemNames()
{
    for (unsigned int i = 0; i < m_system_icons.size(); ++i)
        m_system_icons[i]->HideName();
}

void MapWnd::CenterOnMapCoord(double x, double y)
{
    GG::Pt ul = ClientUpperLeft();
    double current_x = (GG::App::GetApp()->AppWidth() / 2 - ul.x) / m_zoom_factor;
    double current_y = (GG::App::GetApp()->AppHeight() / 2 - ul.y) / m_zoom_factor;
    GG::Pt map_move = GG::Pt(static_cast<int>((current_x - x) * m_zoom_factor), 
                             static_cast<int>((current_y - y) * m_zoom_factor));
    OffsetMove(map_move);
    MoveBackgrounds(map_move);
    m_side_panel->OffsetMove(-map_move);
    m_turn_update->OffsetMove(-map_move);
    m_sitrep_panel->OffsetMove(-map_move);

    // this correction ensures that the centering doesn't leave too large a margin to the side
    GG::Pt move_to_pt = ul = ClientUpperLeft();
    CorrectMapPosition(move_to_pt);
    GG::Pt final_move = move_to_pt - ul;
    m_side_panel->OffsetMove(-final_move);
    m_turn_update->OffsetMove(-final_move);
    m_sitrep_panel->OffsetMove(-final_move);
    MoveBackgrounds(final_move);
    MoveTo(move_to_pt - GG::Pt(GG::App::GetApp()->AppWidth(), GG::App::GetApp()->AppHeight()));
}

void MapWnd::CenterOnSystem(int systemID)
{
    if (System* system = dynamic_cast<System*>(GetUniverse().Object(systemID)))
        CenterOnSystem(system);
}

void MapWnd::CenterOnFleet(int fleetID)
{
    if (Fleet* fleet = dynamic_cast<Fleet*>(GetUniverse().Object(fleetID)))
        CenterOnFleet(fleet);
}

void MapWnd::CenterOnSystem(System* system)
{
    CenterOnMapCoord(system->X(), system->Y());
}

void MapWnd::CenterOnFleet(Fleet* fleet)
{
    CenterOnMapCoord(fleet->X(), fleet->Y());
}

void MapWnd::SelectSystem(int systemID)
{
    m_selected_system_signal(systemID);
}

void MapWnd::SelectFleet(int fleetID)
{
    if (Fleet* fleet = dynamic_cast<Fleet*>(GetUniverse().Object(fleetID)))
        SelectFleet(fleet);
}

void MapWnd::SelectSystem(System* system)
{
    SelectSystem(system->ID());
}

void MapWnd::SelectFleet(Fleet* fleet)
{
    if (System* system = fleet->GetSystem()) {
        for (unsigned int i = 0; i < m_system_icons.size(); ++i) {
            if (&m_system_icons[i]->GetSystem() == system) {
                m_system_icons[i]->ClickFleetButton(fleet);
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
        GG::Pt ul = ClientUpperLeft();
        if (fleet->FinalDestinationID() != UniverseObject::INVALID_OBJECT_ID &&
            fleet->FinalDestinationID() != fleet->SystemID() &&
            destinations.find(fleet->FinalDestinationID()) == destinations.end()) {
            destinations.insert(fleet->FinalDestinationID());
            GG::Pt fleet_ul = fleet_button->UpperLeft();
            GG::Pt sz = fleet_button->Size();
            m_fleet_lines[fleet] = MovementLineData((fleet_ul.x + sz.x / 2 - ul.x) / m_zoom_factor, 
                                                    (fleet_ul.y + sz.y / 2 - ul.y) / m_zoom_factor, 
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

void MapWnd::OnTurnUpdate()
{
    //hide sidepanel
    m_side_panel->SetSystem(UniverseObject::INVALID_OBJECT_ID);
    // delete app popups
    DeleteAllPopups( );

    HumanClientApp::GetApp()->StartTurn();
}

bool MapWnd::EventFilter(GG::Wnd* w, const GG::Wnd::Event& event)
{
    if (event.Type() == GG::Wnd::Event::RClick) {
        // if fleet window quickclose is enabled, the right-click will close fleet windows; if there are no open fleet 
        // windows or the quickclose option is disabled, just let Wnd w handle it.
        if (!GetOptionsDB().Get<bool>("UI.fleet-window-quickclose") || !FleetWnd::CloseAllFleetWnds())
            return false;
        else
            return true;
    }
    return false;
}

void MapWnd::RenderBackgrounds()
{
    double x, y;
    glColor3d(1.0, 1.0, 1.0);
    for (int i = 0; i < NUM_BACKGROUNDS; ++i) {
        int bg_width = m_backgrounds[i]->Width();
        int bg_height = m_backgrounds[i]->Height();
        int app_width = GG::App::GetApp()->AppWidth();
        int app_height = GG::App::GetApp()->AppHeight();
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
            GG::Pt(static_cast<int>((m_nebula_centers[i].x - m_nebulae[i]->Width() / 2.0) * m_zoom_factor),
                   static_cast<int>((m_nebula_centers[i].y - m_nebulae[i]->Height() / 2.0) * m_zoom_factor));
        m_nebulae[i]->OrthoBlit(ul, 
                                ul + GG::Pt(static_cast<int>(m_nebulae[i]->Width() * m_zoom_factor), 
                                            static_cast<int>(m_nebulae[i]->Width() * m_zoom_factor)), 
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
    const unsigned int PATTERN = 0xF0F0F0F0;
    const double RATE = 0.5;
    const int SHIFT = static_cast<int>(GG::App::GetApp()->Ticks() * RATE / 32.0) % 32;
    const unsigned int STIPPLE = (PATTERN << SHIFT) | (PATTERN >> (32 - SHIFT));
    double LINE_SCALE = std::max(1.0, 1.333 * m_zoom_factor);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(static_cast<int>(LINE_SCALE), STIPPLE);
    glLineWidth(LINE_SCALE);
    glColor4d(1.0, 1.0, 1.0, 1.0);

    GG::Pt ul = ClientUpperLeft();
    for (std::map<Fleet*, MovementLineData>::iterator it = m_fleet_lines.begin(); it != m_fleet_lines.end(); ++it) {
        glBegin(GL_LINE_STRIP);
        const std::list<System*>& destinations = it->second.destinations;
        glVertex2d(ul.x + it->second.x * m_zoom_factor, ul.y + it->second.y * m_zoom_factor);
        for (std::list<System*>::const_iterator dest_it = destinations.begin(); dest_it != destinations.end(); ++dest_it) {
            if (it->first->SystemID() == (*dest_it)->ID())
                continue;
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
    int app_width =  GG::App::GetApp()->AppWidth();
    int app_height = GG::App::GetApp()->AppHeight();
    if (app_width < contents_width) {
        if (MAP_MARGIN_WIDTH < move_to_pt.x)
            move_to_pt.x = MAP_MARGIN_WIDTH;
        if (move_to_pt.x < app_width - contents_width - 2 * MAP_MARGIN_WIDTH)
            move_to_pt.x = app_width - contents_width - 2 * MAP_MARGIN_WIDTH;
    } else {
        if (move_to_pt.x < 0)
            move_to_pt.x = 0;
        if (app_width - contents_width < move_to_pt.x)
            move_to_pt.x = app_width - contents_width;
    }

    if (app_height < contents_width) {
        if (MAP_MARGIN_WIDTH < move_to_pt.y)
            move_to_pt.y = MAP_MARGIN_WIDTH;
        if (move_to_pt.y < app_height - contents_width - 2 * MAP_MARGIN_WIDTH)
            move_to_pt.y = app_height - contents_width - 2 * MAP_MARGIN_WIDTH;
    } else {
        if (move_to_pt.y < 0)
            move_to_pt.y = 0;
        if (app_height - contents_width < move_to_pt.y)
            move_to_pt.y = app_height - contents_width;
    }
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

void MapWnd::DeleteAllPopups( )
{
    for (std::list<MapWndPopup*>::iterator it = m_popups.begin(); it != m_popups.end(); ) {
        // get popup and increment iterator first since closing the popup will change this list by removing the poup
        MapWndPopup* popup = *it++;
        popup->Close( );
    }   
    // clear list
    m_popups.clear( );
}

bool MapWnd::ToggleSitRep()
{
    if (m_sitrep_panel->Visible())
        m_sitrep_panel->Hide();
    else
        m_sitrep_panel->Show();
    return true;
}

bool MapWnd::ShowOptions()
{
    if (!m_options_showing) {
        m_options_showing = true;
        InGameOptions options;
        options.Run();
        m_options_showing = false;
    }
    return true;
}

