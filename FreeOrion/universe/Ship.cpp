#include "Ship.h"
#include "XMLDoc.h"

#ifdef FREEORION_BUILD_SERVER
#include "../server/ServerApp.h"
#endif

#include <log4cpp/Appender.hh>
#include <log4cpp/Category.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/FileAppender.hh>
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
#include <stdexcept>

////////////////////////////////////////////////
// ShipDesign
////////////////////////////////////////////////
ShipDesign::ShipDesign() : 
   empire(-1),
   name(""),
   attack(0),
   defense(0),
   cost(10000000),
   id(ShipDesign::SCOUT)
{
   //TODO
}

ShipDesign::ShipDesign(const GG::XMLElement& elem)
{
   if (elem.Tag() != "ShipDesign")
      throw std::invalid_argument("Attempted to construct a ShipDesign from an XMLElement that had a tag other than \"ShipDesign\"");

   id = lexical_cast<int> ( elem.Child("id").Attribute("value") );
   empire = lexical_cast<int> ( elem.Child("empire").Attribute("value") );
   name = elem.Child("name").Text();
   attack = lexical_cast<int> ( elem.Child("attack").Attribute("value") );
   defense = lexical_cast<int> ( elem.Child("defense").Attribute("value") );
   cost = lexical_cast<int> ( elem.Child("cost").Attribute("value") );

}

GG::XMLElement ShipDesign::XMLEncode() const
{
   using GG::XMLElement;
   using boost::lexical_cast;

   XMLElement element("ShipDesign");

   XMLElement sd_ID("id");
   sd_ID.SetAttribute( "value", lexical_cast<std::string>(id) );
   element.AppendChild(sd_ID);

   XMLElement sd_empire("empire");
   sd_empire.SetAttribute( "value", lexical_cast<std::string>(empire) );
   element.AppendChild(sd_empire);
   
   XMLElement sd_name("name");
   sd_name.SetText(name);
   element.AppendChild(sd_name);

   XMLElement sd_attack("attack");
   sd_attack.SetAttribute( "value", lexical_cast<std::string>(attack) );
   element.AppendChild(sd_attack);

   XMLElement sd_defense("defense");
   sd_defense.SetAttribute( "value", lexical_cast<std::string>(defense) );
   element.AppendChild(sd_defense);

   XMLElement sd_cost("cost");
   sd_cost.SetAttribute( "value", lexical_cast<std::string>(cost) );
   element.AppendChild(sd_cost);


   return element;

}


int ShipDesign::WarpSpeed() const
{
    return 1; // for 0.1, and the early revs.  This will change later
}

////////////////////////////////////////////////
// Ship
////////////////////////////////////////////////
Ship::Ship() : 
   m_design(ShipDesign())
{
   //TODO
}

Ship::Ship(int empire_id, int design_id)
{
   // This constructor should only be used by the server, will not work if called from client.
   
#ifdef FREEORION_BUILD_SERVER

   // Lookup empire where design is located
   ServerApp* server_app = ServerApp::GetApp();
   Empire* empire = (server_app->Empires()).Lookup(empire_id);

   if (empire->CopyShipDesign(design_id, m_design) != true)
   {
      throw std::invalid_argument("Attempted to construct a Ship with an invalid design ID");
   }

#endif
   
}

Ship::Ship(const GG::XMLElement& elem) : 
  UniverseObject(elem.Child("UniverseObject")),
  m_design(ShipDesign(elem.Child("m_design")))
{
   if (elem.Tag() != "Ship")
      throw std::invalid_argument("Attempted to construct a Ship from an XMLElement that had a tag other than \"Ship\"");

   m_fleet_id = lexical_cast<int> ( elem.Child("m_fleet_id").Attribute("value") );
}

UniverseObject::Visibility Ship::Visible(int empire_id) const
{
   // Ship is visible if the fleet it is in is visible
#ifdef FREEORION_BUILD_SERVER
   ServerApp* server_app = ServerApp::GetApp();
   Empire* empire = (server_app->Empires()).Lookup(empire_id);
   
   if ((empire->HasFleet(FleetID())) || (empire->HasVisibleFleet(FleetID())))
   {
      return FULL_VISIBILITY;
   }
#endif
   return NO_VISIBILITY;
}


GG::XMLElement Ship::XMLEncode() const
{
   using GG::XMLElement;
   using boost::lexical_cast;

   XMLElement element("Ship");

   element.AppendChild( UniverseObject::XMLEncode() );

   element.AppendChild( m_design.XMLEncode() );

   XMLElement fleet_id("m_fleet_id");
   fleet_id.SetAttribute( "value", lexical_cast<std::string>(m_fleet_id) );
   element.AppendChild(fleet_id);

   return element;
}

GG::XMLElement Ship::XMLEncode(int empire_id) const
{
   // ships are always fully encoded so partial version is
   // the same as the full
   using GG::XMLElement;
   using boost::lexical_cast;

   XMLElement element("Ship");

   element.AppendChild( UniverseObject::XMLEncode() );

   element.AppendChild( m_design.XMLEncode() );

   return element;
}
  	
void Ship::MovementPhase(std::vector<SitRepEntry>& sit_reps)
{
   //TODO
}

void Ship::PopGrowthProductionResearchPhase(std::vector<SitRepEntry>& sit_reps)
{
   //TODO
}



