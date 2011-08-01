#include "Ship.h"

#include "../util/AppInterface.h"
#include "../util/MultiplayerCommon.h"
#include "Fleet.h"
#include "Predicates.h"
#include "ShipDesign.h"
#include "Species.h"
#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"

namespace {
    /** returns true iff one of the empires with the indiated ids can provide
      * fleet supply directly or has resource connections to the system with
      * the id \a system_id 
      * in short: decides whether a fleet gets resupplied at the indicated
      *           system*/
    bool FleetOrResourceSupplyableAtSystemByAnyOfEmpiresWithIDs(int system_id, const std::set<int>& owner_ids) {
        for (std::set<int>::const_iterator it = owner_ids.begin(); it != owner_ids.end(); ++it)
            if (const Empire* empire = Empires().Lookup(*it))
                if (empire->FleetOrResourceSupplyableAtSystem(system_id))
                    return true;
        return false;
    }
}

class Species;
const Species* GetSpecies(const std::string& name);

Ship::Ship() :
    m_design_id(ShipDesign::INVALID_DESIGN_ID),
    m_fleet_id(INVALID_OBJECT_ID),
    m_ordered_scrapped(false),
    m_ordered_colonize_planet_id(INVALID_OBJECT_ID),
    m_produced_by_empire_id(ALL_EMPIRES)
{}

Ship::Ship(int empire_id, int design_id, const std::string& species_name, int produced_by_empire_id/* = ALL_EMPIRES*/) :
    m_design_id(design_id),
    m_fleet_id(INVALID_OBJECT_ID),
    m_ordered_scrapped(false),
    m_ordered_colonize_planet_id(INVALID_OBJECT_ID),
    m_species_name(species_name),
    m_produced_by_empire_id(produced_by_empire_id)
{
    if (!GetShipDesign(design_id))
        throw std::invalid_argument("Attempted to construct a Ship with an invalid design id");

    if (!m_species_name.empty() && !GetSpecies(m_species_name))
        Logger().debugStream() << "Ship created with invalid species name: " << m_species_name;

    AddOwner(empire_id);

    UniverseObject::Init();

    AddMeter(METER_FUEL);
    AddMeter(METER_MAX_FUEL);
    AddMeter(METER_SHIELD);
    AddMeter(METER_MAX_SHIELD);
    AddMeter(METER_DETECTION);
    AddMeter(METER_STRUCTURE);
    AddMeter(METER_MAX_STRUCTURE);
    AddMeter(METER_BATTLE_SPEED);
    AddMeter(METER_STARLANE_SPEED);

    const std::vector<std::string>& part_names = Design()->Parts();
    for (std::size_t i = 0; i < part_names.size(); ++i) {
        if (part_names[i] != "") {
            const PartType* part = GetPartType(part_names[i]);
            if (!part) {
                Logger().errorStream() << "Ship::Ship couldn't get part with name " << part_names[i];
                continue;
            }

            switch (part->Class()) {
            case PC_SHORT_RANGE:
            case PC_POINT_DEFENSE: {
                m_part_meters[std::make_pair(METER_DAMAGE,              part->Name())];
                m_part_meters[std::make_pair(METER_ROF,                 part->Name())];
                m_part_meters[std::make_pair(METER_RANGE,               part->Name())];
                break;
            }
            case PC_MISSILES: {
                std::pair<std::size_t, std::size_t>& part_missiles =
                    m_missiles[part_names[i]];
                ++part_missiles.first;
                part_missiles.second += boost::get<LRStats>(part->Stats()).m_capacity;
                m_part_meters[std::make_pair(METER_DAMAGE,              part->Name())];
                m_part_meters[std::make_pair(METER_ROF,                 part->Name())];
                m_part_meters[std::make_pair(METER_RANGE,               part->Name())];
                m_part_meters[std::make_pair(METER_SPEED,               part->Name())];
                m_part_meters[std::make_pair(METER_STEALTH,             part->Name())];
                m_part_meters[std::make_pair(METER_STRUCTURE,           part->Name())];
                m_part_meters[std::make_pair(METER_CAPACITY,            part->Name())];
                break;
            }
            case PC_FIGHTERS: {
                std::pair<std::size_t, std::size_t>& part_fighters =
                    m_fighters[part_names[i]];
                ++part_fighters.first;
                part_fighters.second += boost::get<FighterStats>(part->Stats()).m_capacity;
                m_part_meters[std::make_pair(METER_ANTI_SHIP_DAMAGE,    part->Name())];
                m_part_meters[std::make_pair(METER_ANTI_FIGHTER_DAMAGE, part->Name())];
                m_part_meters[std::make_pair(METER_LAUNCH_RATE,         part->Name())];
                m_part_meters[std::make_pair(METER_FIGHTER_WEAPON_RANGE,part->Name())];
                m_part_meters[std::make_pair(METER_SPEED,               part->Name())];
                m_part_meters[std::make_pair(METER_STEALTH,             part->Name())];
                m_part_meters[std::make_pair(METER_STRUCTURE,           part->Name())];
                m_part_meters[std::make_pair(METER_DETECTION,           part->Name())];
                m_part_meters[std::make_pair(METER_CAPACITY,            part->Name())];
                break;
            }
            default:
                break;
            }
        }
    }
}

Ship* Ship::Clone(int empire_id) const
{
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(this->ID(), empire_id);

    if (!(vis >= VIS_BASIC_VISIBILITY && vis <= VIS_FULL_VISIBILITY))
        return 0;

    Ship* retval = new Ship();
    retval->Copy(this, empire_id);
    return retval;
}

void Ship::Copy(const UniverseObject* copied_object, int empire_id)
{
    if (copied_object == this)
        return;
    const Ship* copied_ship = universe_object_cast<Ship*>(copied_object);
    if (!copied_ship) {
        Logger().errorStream() << "Ship::Copy passed an object that wasn't a Ship";
        return;
    }

    int copied_object_id = copied_object->ID();
    Visibility vis = GetUniverse().GetObjectVisibilityByEmpire(copied_object_id, empire_id);

    UniverseObject::Copy(copied_object, vis);

    if (vis >= VIS_BASIC_VISIBILITY) {
        this->m_fleet_id =                  copied_ship->m_fleet_id;

        if (vis >= VIS_PARTIAL_VISIBILITY) {
            this->m_design_id =             copied_ship->m_design_id;
            this->m_fighters =              copied_ship->m_fighters;
            this->m_missiles =              copied_ship->m_missiles;
            for (PartMeters::const_iterator it = copied_ship->m_part_meters.begin();
                 it != copied_ship->m_part_meters.end();
                 ++it) {
                this->m_part_meters[it->first];
            }
            this->m_species_name =          copied_ship->m_species_name;

            if (vis >= VIS_FULL_VISIBILITY) {
                this->m_ordered_scrapped =          copied_ship->m_ordered_scrapped;
                this->m_ordered_colonize_planet_id= copied_ship->m_ordered_colonize_planet_id;
                this->m_part_meters =               copied_ship->m_part_meters;
                this->m_produced_by_empire_id =     copied_ship->m_produced_by_empire_id;
            }
        }
    }
}

const std::string& Ship::TypeName() const
{
    return UserString("SHIP");
}

std::string Ship::Dump() const
{
    std::stringstream os;
    os << UniverseObject::Dump();
    os << " design id: " << m_design_id
       << " fleet id: " << m_fleet_id
       << " species name: " << m_species_name
       << " produced by empire id: " << m_produced_by_empire_id
       << " fighters: ";
    //typedef std::map<std::string, std::pair<std::size_t, std::size_t> > ConsumablesMap;
    for (ConsumablesMap::const_iterator it = m_fighters.begin(); it != m_fighters.end();) {
        const std::string& part_name = it->first;
        int num_consumables_available = it->second.second;
        ++it;
        os << part_name << ": " << num_consumables_available << (it == m_fighters.end() ? "" : ", ");
    }
    os << " missiles: ";
    for (ConsumablesMap::const_iterator it = m_missiles.begin(); it != m_missiles.end();) {
        const std::string& part_name = it->first;
        int num_consumables_available = it->second.second;
        ++it;
        os << part_name << ": " << num_consumables_available << (it == m_missiles.end() ? "" : ", ");
    }
    //typedef std::map<std::pair<MeterType, std::string>, Meter> PartMeters;
    os << " part meters: ";
    for (PartMeters::const_iterator it = m_part_meters.begin(); it != m_part_meters.end();) {
        const std::string part_name = it->first.second;
        MeterType meter_type = it->first.first;
        const Meter& meter = it->second;
        ++it;
        os << UserString(part_name) << " "
           << UserString(GG::GetEnumMap<MeterType>().FromEnum(meter_type))
           << ": " << meter.Current() << "  ";
    }
    return os.str();
}

const ShipDesign* Ship::Design() const {
    return GetShipDesign(m_design_id);
}

int Ship::DesignID() const {
    return m_design_id;
}

int Ship::FleetID() const {
    return m_fleet_id;
}

int Ship::ProducedByEmpireID() const
{
    return m_produced_by_empire_id;
}

bool Ship::IsMonster() const {
    const ShipDesign* design = Design();
    if (design)
        return design->IsMonster();
    else
        return false;
}

bool Ship::IsArmed() const {
    const ShipDesign* design = Design();
    if (design)
        return design->IsArmed();
    else
        return false;
}

bool Ship::CanColonize() const {
    if (m_species_name.empty())
        return false;
    const Species* species = GetSpecies(m_species_name);
    if (!species)
        return false;
    if (!species->CanColonize())
        return false;

    const ShipDesign* design = Design();
    if (!design)
        return false;
    if (!design->CanColonize())
        return false;
}

const std::string& Ship::SpeciesName() const {
    return m_species_name;
}

double Ship::Speed() const {
    const ShipDesign* design = Design();
    if (design)
        return design->StarlaneSpeed();
    else
        return false;
}

const Ship::ConsumablesMap& Ship::Fighters() const {
    return m_fighters;
}

const Ship::ConsumablesMap& Ship::Missiles() const {
    return m_missiles;
}

const std::string& Ship::PublicName(int empire_id) const {
    // Disclose real ship name only to fleet owners. Rationale: a player who
    // doesn't know the design for a particular ship can easily guess it if the
    // ship's name is "Scout"
    if (Universe::ALL_OBJECTS_VISIBLE || empire_id == ALL_EMPIRES || OwnedBy(empire_id))
        return Name();
    else if (Unowned() && IsMonster())
        if (const ShipDesign* design = Design())
            return design->Name();
        else
            return UserString("SM_MONSTER");
    else if (Unowned())
        return UserString("FW_ROGUE_SHIP");
    else
        return UserString("FW_FOREIGN_SHIP");
}

UniverseObject* Ship::Accept(const UniverseObjectVisitor& visitor) const {
    return visitor.Visit(const_cast<Ship* const>(this));
}

double Ship::NextTurnCurrentMeterValue(MeterType type) const {

    if (type == INVALID_METER_TYPE || type == METER_FUEL) {
        // todo: consider fleet movement or being stationary, which may parly replenish fuel
        // todo: consider fleet passing through or being in a supplied system, which replenishes fuel
    } else if (type == INVALID_METER_TYPE || type == METER_SUPPLY) {
        // todo: consider fleet passing through or being in a supplied system, which replenishes supplies
    }

    return UniverseObject::NextTurnCurrentMeterValue(type);
}

bool Ship::OrderedScrapped() const
{ return m_ordered_scrapped; }

int Ship::OrderedColonizePlanet() const
{ return m_ordered_colonize_planet_id; }

const Meter* Ship::GetMeter(MeterType type, const std::string& part_name) const
{ return const_cast<Ship*>(this)->GetMeter(type, part_name); }

void Ship::SetFleetID(int fleet_id)
{
    m_fleet_id = fleet_id;
    StateChangedSignal();
}

void Ship::Resupply()
{
    Meter* fuel_meter = UniverseObject::GetMeter(METER_FUEL);
    const Meter* max_fuel_meter = UniverseObject::GetMeter(METER_MAX_FUEL);
    if (!fuel_meter || !max_fuel_meter) {
        Logger().errorStream() << "Ship::Resupply couldn't get fuel meters!";
        return;
    }

    fuel_meter->SetCurrent(max_fuel_meter->Current());

    for (ConsumablesMap::iterator it = m_fighters.begin();
         it != m_fighters.end();
         ++it) {
        it->second.second =
            it->second.first *
            boost::get<FighterStats>(GetPartType(it->first)->Stats()).m_capacity;
    }

    for (ConsumablesMap::iterator it = m_missiles.begin();
         it != m_missiles.end();
         ++it) {
        it->second.second =
            it->second.first *
            boost::get<LRStats>(GetPartType(it->first)->Stats()).m_capacity;
    }
}

void Ship::AddFighters(const std::string& part_name, std::size_t n)
{
    assert(m_fighters[part_name].second + n <=
           m_fighters[part_name].first *
           boost::get<FighterStats>(GetPartType(part_name)->Stats()).m_capacity);
    m_fighters[part_name].second += n;
}

void Ship::RemoveFighters(const std::string& part_name, std::size_t n)
{
    assert(m_fighters[part_name].second < n);
    m_fighters[part_name].second -= n;
}

void Ship::RemoveMissiles(const std::string& part_name, std::size_t n)
{
    assert(m_missiles[part_name].second < n);
    m_missiles[part_name].second -= n;
}

void Ship::SetSpecies(const std::string& species_name)
{
    if (!GetSpecies(species_name))
        Logger().errorStream() << "Ship::SetSpecies couldn't get species with name " << species_name;
    m_species_name = species_name;
}

void Ship::MoveTo(double x, double y)
{
    UniverseObject::MoveTo(x, y);

    // if ship is being moved away from its fleet, remove from the fleet.  otherwise, keep ship in fleet.
    if (Fleet* fleet = GetObject<Fleet>(this->FleetID())) {
        //Logger().debugStream() << "Ship::MoveTo removing " << this->ID() << " from fleet " << fleet->Name();
        fleet->RemoveShip(this->ID());
    }
}

void Ship::SetOrderedScrapped(bool b)
{
    if (b == m_ordered_scrapped) return;
    m_ordered_scrapped = b;
    StateChangedSignal();
    if (Fleet* fleet = GetObject<Fleet>(this->FleetID())) {
        fleet->RecalculateFleetSpeed();
        fleet->StateChangedSignal();
    }
}

void Ship::SetColonizePlanet(int planet_id)
{
    if (planet_id == m_ordered_colonize_planet_id) return;
    m_ordered_colonize_planet_id = planet_id;
    StateChangedSignal();
    if (Fleet* fleet = GetObject<Fleet>(this->FleetID())) {
        fleet->RecalculateFleetSpeed();
        fleet->StateChangedSignal();
    }
}

void Ship::ClearColonizePlanet()
{
    SetColonizePlanet(INVALID_OBJECT_ID);
}

Meter* Ship::GetMeter(MeterType type, const std::string& part_name)
{
    Meter* retval = 0;
    PartMeters::iterator it = m_part_meters.find(std::make_pair(type, part_name));
    if (it != m_part_meters.end())
        retval = &it->second;
    return retval;
}

void Ship::ResetTargetMaxUnpairedMeters(MeterType meter_type/* = INVALID_METER_TYPE*/)
{
    UniverseObject::ResetTargetMaxUnpairedMeters(meter_type);

    if (meter_type == INVALID_METER_TYPE || meter_type == METER_MAX_FUEL)
        UniverseObject::GetMeter(METER_MAX_FUEL)->ResetCurrent();
    if (meter_type == INVALID_METER_TYPE || meter_type == METER_MAX_SHIELD)
        UniverseObject::GetMeter(METER_MAX_SHIELD)->ResetCurrent();
    if (meter_type == INVALID_METER_TYPE || meter_type == METER_MAX_STRUCTURE)
        UniverseObject::GetMeter(METER_MAX_STRUCTURE)->ResetCurrent();

    if (meter_type == INVALID_METER_TYPE || meter_type == METER_DETECTION)
        UniverseObject::GetMeter(METER_DETECTION)->ResetCurrent();
    if (meter_type == INVALID_METER_TYPE || meter_type == METER_BATTLE_SPEED)
        UniverseObject::GetMeter(METER_BATTLE_SPEED)->ResetCurrent();
    if (meter_type == INVALID_METER_TYPE || meter_type == METER_STARLANE_SPEED)
        UniverseObject::GetMeter(METER_STARLANE_SPEED)->ResetCurrent();

    for (PartMeters::iterator it = m_part_meters.begin(); it != m_part_meters.end(); ++it) {
        if (meter_type == INVALID_METER_TYPE || it->first.first == meter_type)
            it->second.ResetCurrent();
    }
}

void Ship::ClampMeters()
{
    UniverseObject::ClampMeters();

    UniverseObject::GetMeter(METER_MAX_FUEL)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_FUEL)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_FUEL)->Current());
    UniverseObject::GetMeter(METER_MAX_SHIELD)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_SHIELD)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_SHIELD)->Current());
    UniverseObject::GetMeter(METER_MAX_STRUCTURE)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_STRUCTURE)->ClampCurrentToRange(Meter::DEFAULT_VALUE, UniverseObject::GetMeter(METER_MAX_STRUCTURE)->Current());

    UniverseObject::GetMeter(METER_DETECTION)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_BATTLE_SPEED)->ClampCurrentToRange();
    UniverseObject::GetMeter(METER_STARLANE_SPEED)->ClampCurrentToRange();

    for (PartMeters::iterator it = m_part_meters.begin(); it != m_part_meters.end(); ++it) {
        it->second.ClampCurrentToRange();
    }
}
