#include "CombatSetupWnd.h"

#include "InfoPanels.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/ShipDesign.h"
#include "../util/OptionsDB.h"

#include <GG/DrawUtil.h>
#include <GG/GUI.h>

#include <OgreEntity.h>
#include <OgreMaterialManager.h>
#include <OgreSceneManager.h>

#include <boost/cast.hpp>


namespace {

    // HACK! These functions and classes were cut-and-pasted from FleetWnd.
    // If they continue to be used without modification, move them into a
    // common location.

    const int           ICON_SIZE = 38;
    const int           PAD = 4;

    GG::Y LabelHeight() {
        return GG::Y(ClientUI::Pts() * 3/2);
    }

    GG::Y StatIconHeight() {
        return GG::Y(ClientUI::Pts()*4/3);
    }

    GG::X StatIconWidth() {
        return GG::X(Value(StatIconHeight()) * 5/2);
    }

    GG::X StatIconSpacingWidth() {
        return StatIconWidth();
    }

    GG::Y ListRowHeight() {
        return GG::Y(std::max(ICON_SIZE, Value(LabelHeight() + StatIconHeight()) + PAD));
    }

    class ShipDataPanel : public GG::Control {
    public:
        ShipDataPanel(GG::X w, GG::Y h, const Ship* ship) :
            Control(GG::X0, GG::Y0, w, h, GG::Flags<GG::WndFlag>()),
            m_ship(ship),
            m_ship_icon(0),
            m_ship_name_text(0),
            m_design_name_text(0),
            m_stat_icons(),
            m_selected(false)
            {
                if (!m_ship)
                    throw std::invalid_argument("ShipDataPanel::ShipDataPanel() : Attempted to construct a ShipDataPanel from a null ship pointer.");

                EnableChildClipping();

                m_ship_name_text = new GG::TextControl(GG::X(Value(h)), GG::Y0, GG::X1, LabelHeight(),
                                                       m_ship->Name(), ClientUI::GetFont(),
                                                       ClientUI::TextColor(), GG::FORMAT_LEFT | GG::FORMAT_VCENTER);
                AttachChild(m_ship_name_text);

                if (const ShipDesign* design = m_ship->Design()) {
                    m_design_name_text = new GG::TextControl(GG::X(Value(h)), GG::Y0, GG::X1, LabelHeight(),
                                                             design->Name(), ClientUI::GetFont(),
                                                             ClientUI::TextColor(), GG::FORMAT_RIGHT | GG::FORMAT_VCENTER);
                    AttachChild(m_design_name_text);
                }

                std::vector<MeterType> meters;
                meters.push_back(METER_HEALTH);     meters.push_back(METER_FUEL);   meters.push_back(METER_DETECTION);
                meters.push_back(METER_STEALTH);    meters.push_back(METER_SHIELD);


                // tooltip info
                int tooltip_delay = GetOptionsDB().Get<int>("UI.tooltip-delay");

                const Universe::EffectAccountingMap& effect_accounting_map = GetUniverse().GetEffectAccountingMap();
                const std::map<MeterType, std::vector<Universe::EffectAccountingInfo> >* meter_map = 0;
                Universe::EffectAccountingMap::const_iterator map_it = effect_accounting_map.find(m_ship->ID());
                if (map_it != effect_accounting_map.end())
                    meter_map = &(map_it->second);


                for (std::vector<MeterType>::const_iterator it = meters.begin(); it != meters.end(); ++it) {
                    StatisticIcon* icon = new StatisticIcon(GG::X0, GG::Y0, StatIconWidth(), StatIconHeight(),
                                                            ClientUI::MeterIcon(*it), 0, 0, false);
                    m_stat_icons.push_back(std::make_pair(*it, icon));
                    AttachChild(icon);

                    // create tooltip explaining effects on meter if such info is available
                    icon->SetBrowseModeTime(tooltip_delay);
                    boost::shared_ptr<GG::BrowseInfoWnd> browse_wnd(new MeterBrowseWnd(*it, m_ship->ID()));
                    icon->SetBrowseInfoWnd(browse_wnd);
                }

                m_ship_connection = GG::Connect(m_ship->StateChangedSignal, &ShipDataPanel::Refresh, this);
                Refresh();
            }

        ~ShipDataPanel() {
            delete m_ship_icon;
            m_ship_connection.disconnect();
        }

        virtual void    Render() {
            // main background position and colour
            const GG::Clr& background_colour = ClientUI::WndColor();
            GG::Pt ul = UpperLeft(), lr = LowerRight();

            // title background colour and position
            const GG::Clr& unselected_colour = ClientUI::WndOuterBorderColor();
            const GG::Clr& selected_colour = ClientUI::WndInnerBorderColor();
            GG::Clr border_colour = m_selected ? selected_colour : unselected_colour;
            if (Disabled())
                border_colour = DisabledColor(border_colour);
            GG::Pt text_ul = ul + GG::Pt(GG::X(ICON_SIZE),   GG::Y0);
            GG::Pt text_lr = ul + GG::Pt(Width(),            LabelHeight());

            // render
            GG::FlatRectangle(ul,       lr,         background_colour,  border_colour, 1);  // background and border
            GG::FlatRectangle(text_ul,  text_lr,    border_colour,      GG::CLR_ZERO, 0);   // title background box
        }

        void            Select(bool b) {
            if (m_selected != b) {
                m_selected = b;

                const GG::Clr& unselected_text_color = ClientUI::TextColor();
                const GG::Clr& selected_text_color = GG::CLR_BLACK;

                GG::Clr text_color_to_use = m_selected ? selected_text_color : unselected_text_color;

                if (Disabled())
                    text_color_to_use = DisabledColor(text_color_to_use);

                m_ship_name_text->SetTextColor(text_color_to_use);
                if (m_design_name_text)
                    m_design_name_text->SetTextColor(text_color_to_use);
            }
        }

        virtual void    SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
            const GG::Pt old_size = Size();
            GG::Control::SizeMove(ul, lr);
            //std::cout << "ShipDataPanel::SizeMove new size: (" << Value(Width()) << ", " << Value(Height()) << ")" << std::endl;
            if (old_size != Size())
                DoLayout();
        }

    private:
        void            SetShipIcon() {
            delete m_ship_icon;
            m_ship_icon = 0;
            const int ICON_OFFSET = Value((Size().y - ICON_SIZE) / 2);
            boost::shared_ptr<GG::Texture> icon;
            const ShipDesign* design = m_ship->Design();
            if (design)
                icon = ClientUI::ShipIcon(design->ID());
            else
                icon = ClientUI::ShipIcon(-1);  // default icon
            m_ship_icon = new GG::StaticGraphic(GG::X(ICON_OFFSET), GG::Y(ICON_OFFSET), GG::X(ICON_SIZE), GG::Y(ICON_SIZE),
                                                icon, GG::GRAPHIC_FITGRAPHIC | GG::GRAPHIC_PROPSCALE);
            AttachChild(m_ship_icon);
        }

        void            Refresh() {
            SetShipIcon();

            m_ship_name_text->SetText(m_ship->Name());

            if (const ShipDesign* design = m_ship->Design()) {
                if (m_design_name_text)
                    m_design_name_text->SetText(design->Name());
            }

            // set stat icon values
            GG::Pt icon_ul(GG::X(ICON_SIZE) + GG::X(PAD), LabelHeight());
            for (std::vector<std::pair<MeterType, StatisticIcon*> >::const_iterator it = m_stat_icons.begin(); it != m_stat_icons.end(); ++it) {
                it->second->SetValue(m_ship->MeterPoints(it->first));
            }

            DoLayout();
        }

        void            DoLayout() {
            if (m_ship_icon) {
                // position icon in centre of available space
                int ICON_OFFSET = std::max(0, (Value(Height()) - ICON_SIZE) / 2);
                GG::Pt icon_ul = GG::Pt(GG::X(ICON_OFFSET), GG::Y(ICON_OFFSET));
                GG::Pt icon_lr = icon_ul + GG::Pt(GG::X(ICON_SIZE), GG::Y(ICON_SIZE));

                m_ship_icon->SizeMove(icon_ul, icon_lr);
            }


            GG::Pt name_ul = GG::Pt(GG::X(ICON_SIZE + PAD), GG::Y0);
            GG::Pt name_lr = GG::Pt(Width() - GG::X(PAD),   LabelHeight());
            m_ship_name_text->SizeMove(name_ul, name_lr);

            if (m_design_name_text)
                m_design_name_text->SizeMove(name_ul, name_lr);

            // set stat icon positions
            GG::Pt icon_ul(GG::X(ICON_SIZE) + GG::X(PAD), LabelHeight());
            for (std::vector<std::pair<MeterType, StatisticIcon*> >::const_iterator it = m_stat_icons.begin(); it != m_stat_icons.end(); ++it) {
                GG::Pt icon_lr = icon_ul + GG::Pt(StatIconWidth(), StatIconHeight());
                it->second->SizeMove(icon_ul, icon_lr);
                icon_ul += GG::Pt(StatIconSpacingWidth(), GG::Y0);
            }
        }

        const Ship* const                                   m_ship;
        GG::StaticGraphic*                                  m_ship_icon;
        GG::TextControl*                                    m_ship_name_text;
        GG::TextControl*                                    m_design_name_text;
        std::vector<std::pair<MeterType, StatisticIcon*> >  m_stat_icons;   // statistic icons and associated meter types
        bool                                                m_selected;
        boost::signals::connection                          m_ship_connection;
    };

    class ShipRow : public GG::ListBox::Row
    {
    public:
        ShipRow(Ship* ship, GG::X w, GG::Y h) :
            GG::ListBox::Row(w, h, ""),
            m_ship(ship)
            {
                SetName("ShipRow");
                EnableChildClipping();
                push_back(new ShipDataPanel(w, h, m_ship));
            }

        void            SizeMove(const GG::Pt& ul, const GG::Pt& lr) {
            const GG::Pt old_size = Size();
            GG::ListBox::Row::SizeMove(ul, lr);
            //std::cout << "ShipRow::SizeMove size: (" << Value(Width()) << ", " << Value(Height()) << ")" << std::endl;
            if (!empty() && old_size != Size())
                at(0)->Resize(Size());
        }

        int             ShipID() const {return m_ship->ID();}

        Ship* const     m_ship;
    };

    // End of cut-and-pasted stuff

    const GG::Y SETUP_WND_HEIGHT(250);

    // HACK! This must be kept in synch with CombatWnd.cpp.
    const Ogre::uint32 REGULAR_OBJECTS_MASK = 1 << 0;

}

CombatSetupWnd::CombatSetupWnd(std::vector<Fleet*> fleets,
                               Ogre::SceneManager* scene_manager,
                               GG::Flags<GG::WndFlag> flags/* = GG::INTERACTIVE | GG::DRAGABLE*/) :
    CUIWnd("Ships", GG::X(PAD), GG::GUI::GetGUI()->AppHeight() - SETUP_WND_HEIGHT - GG::Y(PAD),
           GG::X(300), SETUP_WND_HEIGHT, flags),
    m_listbox(new CUIListBox(GG::X0, GG::Y0, GG::X1, GG::Y1)),
    m_selected_placeable_ship(0),
    m_placeable_ship_node(0),
    m_scene_manager(scene_manager)
{
    AttachChild(m_listbox);
    GridLayout();

    Universe& universe = GetUniverse();
    const GG::Pt row_size = ListRowSize();

    for (std::size_t i = 0; i < fleets.size(); ++i) {
        for (Fleet::const_iterator it = fleets[i]->begin(); it != fleets[i]->end(); ++it) {
            ShipRow* row = new ShipRow(universe.Object<Ship>(*it), GG::X1, row_size.y);
            m_listbox->Insert(row);
            row->Resize(row_size);
        }
    }

    GG::Connect(m_listbox->SelChangedSignal, &CombatSetupWnd::PlaceableShipSelected_, this);
}

GG::Pt CombatSetupWnd::ListRowSize() const
{ return GG::Pt(m_listbox->Width() - ClientUI::ScrollWidth() - 5, ListRowHeight()); }

Ship* CombatSetupWnd::PlaceableShip() const
{ return m_selected_placeable_ship; }

Ogre::SceneNode* CombatSetupWnd::PlaceableShipNode() const
{ return m_selected_placeable_ship ? m_placeable_ship_node : 0; }

void CombatSetupWnd::EndShipPlacement()
{
    m_selected_placeable_ship = 0;
    if (m_placeable_ship_node)
        m_placeable_ship_node->setVisible(false);
    m_listbox->DeselectAll();
}

void CombatSetupWnd::PlaceableShipSelected_(const GG::ListBox::SelectionSet& sels)
{
    assert(sels.size() <= 1u);
    if (sels.empty()) {
        PlaceableShipSelected(0);
    } else {
        GG::ListBox::Row* row = **sels.begin();
        ShipRow* ship_row = boost::polymorphic_downcast<ShipRow*>(row);
        PlaceableShipSelected(const_cast<Ship*>(ship_row->m_ship));
    }
}

void CombatSetupWnd::PlaceableShipSelected(Ship* ship)
{
    if (ship != m_selected_placeable_ship) {
        m_selected_placeable_ship = ship;
        if (m_placeable_ship_node) {
            m_placeable_ship_node->detachAllObjects();
        } else if (m_selected_placeable_ship) {
            m_placeable_ship_node =
                m_scene_manager->getRootSceneNode()->createChildSceneNode("placed_ship_node");
        }

        if (m_selected_placeable_ship) {
            Ogre::MaterialPtr material =
                Ogre::MaterialManager::getSingleton().getByName("ship_being_placed");
            material->getTechnique(0)->getPass(0)->getTextureUnitState(0)->
                setTextureName(ship->Design()->Model() + "_Color.png");
            material->getTechnique(0)->getPass(0)->getTextureUnitState(1)->
                setTextureName(ship->Design()->Model() + "_Glow.png");
            material->getTechnique(0)->getPass(0)->getTextureUnitState(2)->
                setTextureName(ship->Design()->Model() + "_Normal.png");
            material->getTechnique(0)->getPass(0)->getTextureUnitState(3)->
                setTextureName(ship->Design()->Model() + "_Specular.png");
            std::string mesh_name = ship->Design()->Model() + ".mesh";
            Ogre::Entity*& entity = m_placeable_ship_entities[mesh_name];
            if (!entity) {
                entity = m_scene_manager->createEntity("placement_" + mesh_name, mesh_name);
                entity->setCastShadows(true);
                entity->setVisibilityFlags(REGULAR_OBJECTS_MASK);
                entity->setMaterialName("ship_being_placed");
            }
            m_placeable_ship_node->attachObject(entity);
        }
    }
}
