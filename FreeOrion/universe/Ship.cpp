#include "Ship.h"

#include "../util/AppInterface.h"
#include "Fleet.h"
#include "../util/MultiplayerCommon.h"
#include "Predicates.h"
#include "ShipDesign.h"

#include <log4cpp/Appender.hh>
#include <log4cpp/Category.hh>
#include <log4cpp/PatternLayout.hh>
#include <log4cpp/FileAppender.hh>
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;
#include <stdexcept>

namespace {
    void GrowFuelMeter(Meter* fuel) {
        double regen = 0.5;        
        double new_cur = std::min(fuel->Max(), fuel->Max() + regen);
        fuel->SetCurrent(new_cur);
    }
}
Ship::Ship() :
    m_design_id(INVALID_OBJECT_ID),
    m_fleet_id(INVALID_OBJECT_ID)
{
    InsertMeter(METER_FUEL, Meter());
}

Ship::Ship(int empire_id, int design_id) :
    m_design_id(design_id),
    m_fleet_id(INVALID_OBJECT_ID)
{
    if (!GetShipDesign(design_id))
        throw std::invalid_argument("Attempted to construct a Ship with an invalid design id");

    AddOwner(empire_id);

    InsertMeter(METER_FUEL, Meter());
}

const ShipDesign* Ship::Design() const
{
    return GetShipDesign(m_design_id);
}

int Ship::ShipDesignID() const
{
    return m_design_id;
}

int Ship::FleetID() const
{
    return m_fleet_id;
}

Fleet* Ship::GetFleet() const
{
    return m_fleet_id == INVALID_OBJECT_ID ? 0 : GetUniverse().Object<Fleet>(m_fleet_id);
}

UniverseObject::Visibility Ship::GetVisibility(int empire_id) const
{
    UniverseObject::Visibility vis = NO_VISIBILITY;

    if (Universe::ALL_OBJECTS_VISIBLE || empire_id == ALL_EMPIRES || OwnedBy(empire_id))
        vis = FULL_VISIBILITY;

    // Ship is visible if its fleet is visible
    UniverseObject::Visibility retval = FleetID() == INVALID_OBJECT_ID ? NO_VISIBILITY : (GetFleet() ? GetFleet()->GetVisibility(empire_id) : vis);
    return retval;
}

bool Ship::IsArmed() const
{
    return Design()->Attack() > 0;
}

double Ship::Speed() const
{
    return Design()->Speed();
}

const std::string& Ship::PublicName(int empire_id) const
{
    // Disclose real ship name only to fleet owners. Rationale: a player who doesn't know the design for a particular
    // ship can easily guess it if the ship's name is "Scout"
    if (Universe::ALL_OBJECTS_VISIBLE || empire_id == ALL_EMPIRES || OwnedBy(empire_id))
        return Name();
    else
        return UserString("FW_FOREIGN_SHIP");
}

UniverseObject* Ship::Accept(const UniverseObjectVisitor& visitor) const
{
    return visitor.Visit(const_cast<Ship* const>(this));
}

double Ship::ProjectedCurrentMeter(MeterType type) const
{
    const Meter* fuel = GetMeter(METER_FUEL);
    Meter copy;

    switch (type) {
    case METER_FUEL:
        assert(fuel);
        copy = Meter(*fuel);
        GrowFuelMeter(&copy);
        return copy.Current();
        break;
    default:
        return UniverseObject::ProjectedCurrentMeter(type);
        break;
    }
}

void Ship::MovementPhase()
{
}

void Ship::PopGrowthProductionResearchPhase()
{
    GrowFuelMeter(GetMeter(METER_FUEL));
}

bool Ship::AdjustFuel(double amount)
{
    double new_fuel = GetMeter(METER_FUEL)->Current() + amount;
    if (new_fuel > GetMeter(METER_FUEL)->Max() || new_fuel < Meter::METER_MIN) return false;
    GetMeter(METER_FUEL)->SetCurrent(new_fuel);
    return true;
}

