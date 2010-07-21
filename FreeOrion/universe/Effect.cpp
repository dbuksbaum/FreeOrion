#include "Effect.h"

#include "../util/AppInterface.h"
#include "ValueRef.h"
#include "Condition.h"
#include "Universe.h"
#include "UniverseObject.h"
#include "../Empire/EmpireManager.h"
#include "../Empire/Empire.h"
#include "Building.h"
#include "Planet.h"
#include "System.h"
#include "Fleet.h"
#include "Ship.h"
#include "Tech.h"

#include <cctype>

using namespace Effect;
using namespace boost::io;
using boost::lexical_cast;

extern int g_indent;

namespace {
    boost::tuple<bool, ValueRef::OpType, double>
    SimpleMeterModification(MeterType meter, const ValueRef::ValueRefBase<double>* ref)
    {
        boost::tuple<bool, ValueRef::OpType, double> retval(false, ValueRef::PLUS, 0.0);
        if (const ValueRef::Operation<double>* op = dynamic_cast<const ValueRef::Operation<double>*>(ref)) {
            if (!op->LHS() || !op->RHS())
                return retval;
            if (const ValueRef::Variable<double>* var = dynamic_cast<const ValueRef::Variable<double>*>(op->LHS())) {
                if (const ValueRef::Constant<double>* constant = dynamic_cast<const ValueRef::Constant<double>*>(op->RHS())) {
                    std::string meter_str = UserString(lexical_cast<std::string>(meter));
                    if (!meter_str.empty())
                        meter_str[0] = std::toupper(meter_str[0]);
                    retval.get<0>() = var->PropertyName().size() == 1 &&
                        ("Current" + meter_str) == var->PropertyName()[0];
                    retval.get<1>() = op->GetOpType();
                    retval.get<2>() = constant->Value();
                    return retval;
                }
            } else if (const ValueRef::Variable<double>* var = dynamic_cast<const ValueRef::Variable<double>*>(op->RHS())) {
                if (const ValueRef::Constant<double>* constant = dynamic_cast<const ValueRef::Constant<double>*>(op->LHS())) {
                    std::string meter_str = UserString(lexical_cast<std::string>(meter));
                    if (!meter_str.empty())
                        meter_str[0] = std::toupper(meter_str[0]);
                    retval.get<0>() = var->PropertyName().size() == 1 &&
                        ("Current" + meter_str) == var->PropertyName()[0];
                    retval.get<1>() = op->GetOpType();
                    retval.get<2>() = constant->Value();
                    return retval;
                }
            }
        }
        return retval;
    }

    /** Creates a new fleet at \a system and inserts \a ship into it.  Used
     * when a ship has been moved by the MoveTo effect separately from the
     * fleet that previously held it.  Also used by CreateShip effect to give
     * the new ship a fleet.  All ships need to be within fleets. */
    Fleet* CreateNewFleet(System* system, Ship* ship) {
        Universe& universe = GetUniverse();
        if (!system || !ship)
            return 0;

        int owner_empire_id = ALL_EMPIRES;
        const std::set<int>& owners = ship->Owners();
        if (!owners.empty())
            owner_empire_id = *(owners.begin());

        int new_fleet_id = GetNewObjectID();

        std::vector<int> ship_ids;
        ship_ids.push_back(ship->ID());
        std::string fleet_name = Fleet::GenerateFleetName(ship_ids, new_fleet_id);

        Fleet* fleet = new Fleet(fleet_name, system->X(), system->Y(), owner_empire_id);
        fleet->GetMeter(METER_STEALTH)->SetCurrent(Meter::LARGE_VALUE);

        universe.InsertID(fleet, new_fleet_id);
        system->Insert(fleet);

        fleet->AddShip(ship->ID());

        return fleet;
    }

    /** creates a new fleet at a specified \a x and \a y location within the
     * Universe, and and inserts \a ship into it.  Used when a ship has been
     * moved by the MoveTo effect separately from the fleet that previously
     * held it.  All ships need to be within fleets. */
    Fleet* CreateNewFleet(double x, double y, Ship* ship) {
        Universe& universe = GetUniverse();
        if (!ship)
            return 0;

        int owner_empire_id = ALL_EMPIRES;
        const std::set<int>& owners = ship->Owners();
        if (!owners.empty())
            owner_empire_id = *(owners.begin());

        int new_fleet_id = GetNewObjectID();

        std::vector<int> ship_ids;
        ship_ids.push_back(ship->ID());
        std::string fleet_name = Fleet::GenerateFleetName(ship_ids, new_fleet_id);

        Fleet* fleet = new Fleet(fleet_name, x, y, owner_empire_id);
        fleet->GetMeter(METER_STEALTH)->SetCurrent(Meter::LARGE_VALUE);

        universe.InsertID(fleet, new_fleet_id);

        fleet->AddShip(ship->ID());

        return fleet;
    }

    /** Explores the system with the specified \a system_id for the owner of the specified \a target_object.  Used when
        moving objects into a system with the MoveTo effect, as otherwise the system wouldn't get explored, and objects
        being moved into unexplored systems might disappear for players or confuse the AI. */
    void ExploreSystem(int system_id, const UniverseObject* target_object) {
        if (!target_object) return;
        const std::set<int>& owners = target_object->Owners();
        if (!owners.empty())
            if (Empire* owner_empire = Empires().Lookup(*owners.begin()))
                owner_empire->AddExploredSystem(system_id);
    }

    /** Resets the previous and next systems of \a fleet and recalcultes /
     * resets the fleet's move route.  Used after a fleet has been moved with
     * the MoveTo effect, as its previous route was assigned based on its
     * previous location, and may not be valid for its new location. */
    void UpdateFleetRoute(Fleet* fleet, int new_next_system, int new_previous_system) {
        if (!fleet) {
            Logger().errorStream() << "UpdateFleetRoute passed a null fleet pointer";
            return;
        }

        const Universe& universe = GetUniverse();
        const ObjectMap& objects = universe.Objects();

        const System* next_system = objects.Object<System>(new_next_system);
        if (!next_system) {
            Logger().errorStream() << "UpdateFleetRoute couldn't get new next system with id: " << new_next_system;
            return;
        }

        if (new_previous_system != UniverseObject::INVALID_OBJECT_ID && !objects.Object<System>(new_previous_system)) {
            Logger().errorStream() << "UpdateFleetRoute couldn't get new previous system with id: " << new_previous_system;
        }

        fleet->SetNextAndPreviousSystems(new_next_system, new_previous_system);


        // recalculate route from the shortest path between first system on path and final destination

        int owner = ALL_EMPIRES;
        const std::set<int>& owners = fleet->Owners();
        if (!owners.empty())
            owner = *owners.begin();

        int start_system = fleet->SystemID();
        if (start_system == UniverseObject::INVALID_OBJECT_ID)
            start_system = new_next_system;

        int dest_system = fleet->FinalDestinationID();

        std::pair<std::list<int>, double> route_pair = universe.ShortestPath(start_system, dest_system, owner);

        // if shortest path is empty, the route may be impossible or trivial, so just set route to move fleet
        // to the next system that it was just set to move to anyway.
        if (route_pair.first.empty())
            route_pair.first.push_back(new_next_system);


        // set fleet with newly recalculated route
        fleet->SetRoute(route_pair.first);
    }

    /** returns true of the owners of the two passed objects are the same, and both are owned, false otherwise */
    bool SameOwners(const UniverseObject* obj1, const UniverseObject* obj2) {
        if (!obj1 || !obj2) return false;

        const std::set<int>& owners1 = obj1->Owners();
        const std::set<int>& owners2 = obj2->Owners();

        int owner1 = *owners1.begin();
        int owner2 = *owners2.begin();

        return owner1 == owner2;
    }

    bool PartMatchesEffect(const PartType& part,
                           ShipPartClass part_class,
                           CombatFighterType fighter_type,
                           const std::string& part_name,
                           ShipSlotType slot_type)
    {
        if (slot_type != INVALID_SHIP_SLOT_TYPE && !part.CanMountInSlotType(slot_type))
            return false;

        if (part_name != "")
            return part_name == part.Name();

        switch (part.Class()) {
        case PC_SHORT_RANGE:
        case PC_MISSILES:
        case PC_POINT_DEFENSE:
            return part.Class() == part_class;
        case PC_FIGHTERS:
            return boost::get<FighterStats>(part.Stats()).m_type == fighter_type;
        default:
            break;
        }

        return false;
    }

    Condition::ObjectSet EffectTargetSetToConditionObjectSet(const TargetSet& target_set) {
        Condition::ObjectSet retval;
        std::copy(target_set.begin(), target_set.end(), std::inserter(retval, retval.begin()));
        return retval;
    }

    TargetSet ConditionObjectSetToEffectTargetSet(const Condition::ObjectSet& object_set) {
        TargetSet retval;
        for (Condition::ObjectSet::const_iterator it = object_set.begin(); it != object_set.end(); ++it)
            retval.insert(const_cast<UniverseObject*>(*it));
        return retval;
    }
}

///////////////////////////////////////////////////////////
// EffectsGroup                                          //
///////////////////////////////////////////////////////////
EffectsGroup::EffectsGroup(const Condition::ConditionBase* scope, const Condition::ConditionBase* activation,
                           const std::vector<EffectBase*>& effects, const std::string& stacking_group/* = ""*/) :
    m_scope(scope),
    m_activation(activation),
    m_stacking_group(stacking_group),
    m_explicit_description(""), // TODO: Get this from stringtable when available
    m_effects(effects)
{}

EffectsGroup::~EffectsGroup()
{
    delete m_scope;
    delete m_activation;
    for (unsigned int i = 0; i < m_effects.size(); ++i) {
        delete m_effects[i];
    }
}

void EffectsGroup::GetTargetSet(int source_id, TargetSet& targets, const TargetSet& potential_targets) const
{
    targets.clear();

    UniverseObject* source = GetObject(source_id);
    if (!source) {
        Logger().errorStream() << "EffectsGroup::GetTargetSet passed invalid source object with id " << source_id;
        return;
    }

    // evaluate the activation condition only on the source object
    Condition::ObjectSet non_targets;
    non_targets.insert(source);
    Condition::ObjectSet matched_targets;
    m_activation->Eval(source, matched_targets, non_targets);

    // if the activation condition did not evaluate to true for the source object, do nothing
    if (matched_targets.empty())
        return;

    // remove source object from target set after activation condition check
    matched_targets.clear();

    // convert potential targets TargetSet to a Condition::ObjectSet so that condition can be applied to it
    non_targets = EffectTargetSetToConditionObjectSet(potential_targets);

    // evaluate the scope condition
    m_scope->Eval(source, matched_targets, non_targets);

    // convert result back to TargetSet
    targets = ConditionObjectSetToEffectTargetSet(matched_targets);
}

void EffectsGroup::GetTargetSet(int source_id, TargetSet& targets) const
{
    ObjectMap& objects = GetUniverse().Objects();
    TargetSet potential_targets;
    for (ObjectMap::iterator it = objects.begin(); it != objects.end(); ++it)
        potential_targets.insert(it->second);
    GetTargetSet(source_id, targets, potential_targets);
}

void EffectsGroup::Execute(int source_id, const TargetSet& targets) const
{
    UniverseObject* source = GetObject(source_id);
    if (!source) {
        Logger().errorStream() << "EffectsGroup::Execute unable to get source object with id " << source_id;
        return;
    }

    // execute effects on targets
    for (TargetSet::const_iterator it = targets.begin(); it != targets.end(); ++it) {
        //Logger().debugStream() << "effectsgroup source: " << source->Name() << " target " << (*it)->Name();
        for (unsigned int i = 0; i < m_effects.size(); ++i)
            m_effects[i]->Execute(source, *it);
    }
}

void EffectsGroup::Execute(int source_id, const TargetSet& targets, int effect_index) const
{
    UniverseObject* source = GetObject(source_id);
    if (!source) {
        // TODO: Don't necessarily need to abort at this stage... some effects can function without a source object.
        Logger().errorStream() << "EffectsGroup::Execute unable to get source object with id " << source_id;
        return;
    }

    assert(0 <= effect_index && effect_index < static_cast<int>(m_effects.size()));

    // execute effect on targets
    for (TargetSet::const_iterator it = targets.begin(); it != targets.end(); ++it)
        m_effects[effect_index]->Execute(source, *it);
}

const std::string& EffectsGroup::StackingGroup() const
{
    return m_stacking_group;
}

const std::vector<EffectBase*>& EffectsGroup::EffectsList() const
{
    return m_effects;
}

EffectsGroup::Description EffectsGroup::GetDescription() const
{
    Description retval;
    if (dynamic_cast<const Condition::Self*>(m_scope))
        retval.scope_description = UserString("DESC_EFFECTS_GROUP_SELF_SCOPE");
    else
        retval.scope_description = str(FlexibleFormat(UserString("DESC_EFFECTS_GROUP_SCOPE")) % m_scope->Description());
    if (dynamic_cast<const Condition::Self*>(m_activation) || dynamic_cast<const Condition::All*>(m_activation))
        retval.activation_description = UserString("DESC_EFFECTS_GROUP_ALWAYS_ACTIVE");
    else
        retval.activation_description = str(FlexibleFormat(UserString("DESC_EFFECTS_GROUP_ACTIVATION")) % m_activation->Description());
    for (unsigned int i = 0; i < m_effects.size(); ++i) {
        retval.effect_descriptions.push_back(m_effects[i]->Description());
    }
    return retval;
}

std::string EffectsGroup::DescriptionString() const
{
    if (!m_explicit_description.empty()) {
        return UserString(m_explicit_description);
    } else {
        std::stringstream retval;
        Description description = GetDescription();
        retval << str(FlexibleFormat(UserString("DESC_EFFECTS_GROUP_SCOPE_DESC")) % description.scope_description);
        if (!dynamic_cast<const Condition::Self*>(m_activation) && !dynamic_cast<const Condition::All*>(m_activation))
            retval << str(FlexibleFormat(UserString("DESC_EFFECTS_GROUP_ACTIVATION_DESC")) % description.activation_description);
        for (unsigned int i = 0; i < description.effect_descriptions.size(); ++i) {
            retval << str(FlexibleFormat(UserString("DESC_EFFECTS_GROUP_EFFECT_DESC")) % description.effect_descriptions[i]);
        }
        return retval.str();
    }
}

std::string EffectsGroup::Dump() const
{
    std::string retval = DumpIndent() + "EffectsGroup\n";
    ++g_indent;
    retval += DumpIndent() + "scope =\n";
    ++g_indent;
    retval += m_scope->Dump();
    --g_indent;
    if (m_activation) {
        retval += DumpIndent() + "activation =\n";
        ++g_indent;
        retval += m_activation->Dump();
        --g_indent;
    }
    if (!m_stacking_group.empty())
        retval += DumpIndent() + "stackinggroup = \"" + m_stacking_group + "\"\n";
    if (m_effects.size() == 1) {
        retval += DumpIndent() + "effects =\n";
        ++g_indent;
        retval += m_effects[0]->Dump();
        --g_indent;
    } else {
        retval += DumpIndent() + "effects = [\n";
        ++g_indent;
        for (unsigned int i = 0; i < m_effects.size(); ++i) {
            retval += m_effects[i]->Dump();
        }
        --g_indent;
        retval += DumpIndent() + "]\n";
    }
    --g_indent;
    return retval;
}


///////////////////////////////////////////////////////////
// EffectsDescription function                           //
///////////////////////////////////////////////////////////
std::string EffectsDescription(const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >& effects_groups)
{
    std::stringstream retval;
    if (effects_groups.size() == 1) {
        retval << str(FlexibleFormat(UserString("DESC_EFFECTS_GROUP_EFFECTS_GROUP_DESC")) % effects_groups[0]->DescriptionString());
    } else {
        for (unsigned int i = 0; i < effects_groups.size(); ++i) {
            retval << str(FlexibleFormat(UserString("DESC_EFFECTS_GROUP_NUMBERED_EFFECTS_GROUP_DESC")) % (i + 1) % effects_groups[i]->DescriptionString());
        }
    }
    return retval.str();
}


///////////////////////////////////////////////////////////
// EffectBase                                            //
///////////////////////////////////////////////////////////
EffectBase::~EffectBase()
{}


///////////////////////////////////////////////////////////
// SetMeter                                              //
///////////////////////////////////////////////////////////
SetMeter::SetMeter(MeterType meter, const ValueRef::ValueRefBase<double>* value) :
    m_meter(meter),
    m_value(value)
{}

SetMeter::~SetMeter()
{
    delete m_value;
}

void SetMeter::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (Meter* m = target->GetMeter(m_meter)) {
        double val = m_value->Eval(source, target, m->Current());
        m->SetCurrent(val);
    }
}

std::string SetMeter::Description() const
{
    bool simple;
    ValueRef::OpType op;
    double const_operand;
    boost::tie(simple, op, const_operand) = SimpleMeterModification(m_meter, m_value);
    if (simple) {
        char op_char = '+';
        switch (op) {
        case ValueRef::PLUS:    op_char = '+'; break;
        case ValueRef::MINUS:   op_char = '-'; break;
        case ValueRef::TIMES:   op_char = '*'; break;
        case ValueRef::DIVIDES: op_char = '/'; break;
        default: op_char = '?';
        }
        return str(FlexibleFormat(UserString("DESC_SIMPLE_SET_METER_CURRENT"))
                   % UserString(lexical_cast<std::string>(m_meter))
                   % op_char
                   % lexical_cast<std::string>(const_operand));
    } else {
        return str(FlexibleFormat(UserString("DESC_COMPLEX_SET_METER_CURRENT"))
                   % UserString(lexical_cast<std::string>(m_meter))
                   % m_value->Description());
    }
}

std::string SetMeter::Dump() const
{
    std::string retval = DumpIndent() + "Set";
    switch (m_meter) {
    case METER_TARGET_POPULATION:   retval += "TargetPopulation"; break;
    case METER_TARGET_HEALTH:       retval += "TargetHealth"; break;
    case METER_TARGET_FARMING:      retval += "TargetFarming"; break;
    case METER_TARGET_INDUSTRY:     retval += "TargetIndustry"; break;
    case METER_TARGET_RESEARCH:     retval += "TargetResearch"; break;
    case METER_TARGET_TRADE:        retval += "TargetTrade"; break;
    case METER_TARGET_MINING:       retval += "TargetMining"; break;
    case METER_TARGET_CONSTRUCTION: retval += "TargetConstruction"; break;

    case METER_MAX_FUEL:            retval += "MaxFuel"; break;
    case METER_MAX_SHIELD:          retval += "MaxShield"; break;
    case METER_MAX_STRUCTURE:       retval += "MaxStructure"; break;
    case METER_MAX_DEFENSE:         retval += "MaxDefense"; break;

    case METER_POPULATION:          retval += "Population"; break;
    case METER_HEALTH:              retval += "Health"; break;
    case METER_FARMING:             retval += "Farming"; break;
    case METER_INDUSTRY:            retval += "Industry"; break;
    case METER_RESEARCH:            retval += "Research"; break;
    case METER_TRADE:               retval += "Trade"; break;
    case METER_MINING:              retval += "Mining"; break;
    case METER_CONSTRUCTION:        retval += "Construction"; break;

    case METER_FUEL:                retval += "Fuel"; break;
    case METER_SHIELD:              retval += "Shield"; break;
    case METER_STRUCTURE:           retval += "Structure"; break;
    case METER_DEFENSE:             retval += "Defense"; break;

    case METER_FOOD_CONSUMPTION:    retval += "FoodConsumption"; break;
    case METER_SUPPLY:              retval += "Supply"; break;
    case METER_STEALTH:             retval += "Stealth"; break;
    case METER_DETECTION:           retval += "Detection"; break;
    case METER_BATTLE_SPEED:        retval += "BattleSpeed"; break;
    case METER_STARLANE_SPEED:      retval += "StarlaneSpeed"; break;

    default: retval += "?"; break;

    }
    retval += " value = " + m_value->Dump() + "\n";
    return retval;
}


///////////////////////////////////////////////////////////
// SetShipPartMeter                                      //
///////////////////////////////////////////////////////////
SetShipPartMeter::SetShipPartMeter(MeterType meter,
                                   ShipPartClass part_class,
                                   const ValueRef::ValueRefBase<double>* value,
                                   ShipSlotType slot_type/* = INVALID_SHIP_SLOT_TYPE*/) :
    m_part_class(part_class),
    m_fighter_type(INVALID_COMBAT_FIGHTER_TYPE),
    m_part_name(),
    m_slot_type(slot_type),
    m_meter(meter),
    m_value(value)
{ assert(m_part_class != PC_FIGHTERS); }

SetShipPartMeter::SetShipPartMeter(MeterType meter,
                                   CombatFighterType fighter_type,
                                   const ValueRef::ValueRefBase<double>* value,
                                   ShipSlotType slot_type/* = INVALID_SHIP_SLOT_TYPE*/) :
    m_part_class(INVALID_SHIP_PART_CLASS),
    m_fighter_type(fighter_type),
    m_part_name(),
    m_slot_type(slot_type),
    m_meter(meter),
    m_value(value)
{}

SetShipPartMeter::SetShipPartMeter(MeterType meter,
                                   const std::string& part_name,
                                   const ValueRef::ValueRefBase<double>* value,
                                   ShipSlotType slot_type/* = INVALID_SHIP_SLOT_TYPE*/) :
    m_part_class(INVALID_SHIP_PART_CLASS),
    m_fighter_type(INVALID_COMBAT_FIGHTER_TYPE),
    m_part_name(part_name),
    m_slot_type(slot_type),
    m_meter(meter),
    m_value(value)
{}

SetShipPartMeter::~SetShipPartMeter()
{ delete m_value; }

void SetShipPartMeter::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (!target) {
        Logger().debugStream() << "SetShipPartMeter::Execute passed null target pointer";
        return;
    }

    Ship* ship = universe_object_cast<Ship*>(target);
    if (!ship) {
        Logger().errorStream() << "SetShipPartMeter::Execute acting on non-ship target:";
        target->Dump();
        return;
    }

    // loop through all parts in the ship design, applying effect to each if appropriate
    const std::vector<std::string>& design_parts = ship->Design()->Parts();
    for (std::size_t i = 0; i < design_parts.size(); ++i) {
        const std::string& target_part_name = design_parts[i];
        if (target_part_name.empty())
            continue;   // slots in a design may be empty... this isn't an error

        Meter* meter = ship->GetMeter(m_meter, target_part_name);
        if (!meter)
            continue;   // some parts may not have the requested meter.  this isn't an error

        const PartType* target_part = GetPartType(target_part_name);
        if (!target_part) {
            Logger().errorStream() << "SetShipPartMeter::Execute couldn't get part type: " << target_part_name;
            continue;
        }

        // verify that found part matches the target part type information for
        // this effect: same name, same class and slot type, or same fighter type
        if (PartMatchesEffect(*target_part, m_part_class, m_fighter_type, m_part_name, m_slot_type)) {
            double val = m_value->Eval(source, target, meter->Current());
            meter->SetCurrent(val);
        }
    }
}

std::string SetShipPartMeter::Description() const
{
    // TODO
    return "";
}

std::string SetShipPartMeter::Dump() const
{
    std::string retval = DumpIndent() + "SetShipPartMeter";
    // TODO
    return retval;
}


///////////////////////////////////////////////////////////
// SetEmpireStockpile                                    //
///////////////////////////////////////////////////////////
SetEmpireStockpile::SetEmpireStockpile(ResourceType stockpile, const ValueRef::ValueRefBase<double>* value) :
    m_stockpile(stockpile),
    m_value(value)
{}

SetEmpireStockpile::~SetEmpireStockpile()
{
    delete m_value;
}

void SetEmpireStockpile::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (source->Owners().size() != 1)
        return;

    if (Empire* empire = Empires().Lookup(*source->Owners().begin())) {
        double value = m_value->Eval(source, target, empire->ResourceStockpile(m_stockpile));
        empire->SetResourceStockpile(m_stockpile, value);
    }
}

std::string SetEmpireStockpile::Description() const
{
    std::string value_str = ValueRef::ConstantExpr(m_value) ? lexical_cast<std::string>(m_value->Eval(0, 0, boost::any())) : m_value->Description();
    return str(FlexibleFormat(UserString("DESC_SET_EMPIRE_STOCKPILE")) % UserString(lexical_cast<std::string>(m_stockpile)) % value_str);
}

std::string SetEmpireStockpile::Dump() const
{
    std::string retval = DumpIndent();
    switch (m_stockpile) {
    case RE_FOOD:       retval += "SetOwnerFoodStockpile"; break;
    case RE_MINERALS:   retval += "SetOwnerMineralStockpile"; break;
    case RE_TRADE:      retval += "SetOwnerTradeStockpile"; break;
    default:            retval += "?"; break;
    }
    retval += " value = " + m_value->Dump() + "\n";
    return retval;
}


///////////////////////////////////////////////////////////
// SetEmpireCapitol                                      //
///////////////////////////////////////////////////////////
SetEmpireCapitol::SetEmpireCapitol()
{}

void SetEmpireCapitol::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (const Planet* planet = universe_object_cast<const Planet*>(target)) {   // verify that target object is a planet
        const std::set<int>& owners = planet->Owners();                         // get owner(s)
        if (owners.size() == 1)                                                 // verify that there is only a single owner
            if (Empire* empire = Empires().Lookup(*owners.begin()))             // get that owner empire object
                empire->SetCapitolID(planet->ID());                             // make target planet the capitol of its owner empire
    }
}

std::string SetEmpireCapitol::Description() const
{
    return UserString("DESC_SET_EMPIRE_CAPITOL");
}

std::string SetEmpireCapitol::Dump() const
{
    return DumpIndent() + "SetEmpireCapitol\n";
}


///////////////////////////////////////////////////////////
// SetPlanetType                                         //
///////////////////////////////////////////////////////////
SetPlanetType::SetPlanetType(const ValueRef::ValueRefBase<PlanetType>* type) :
    m_type(type)
{}

SetPlanetType::~SetPlanetType()
{
    delete m_type;
}

void SetPlanetType::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (Planet* p = universe_object_cast<Planet*>(target)) {
        PlanetType type = m_type->Eval(source, target, p->Type());
        p->SetType(type);
        if (type == PT_ASTEROIDS)
            p->SetSize(SZ_ASTEROIDS);
        else if (type == PT_GASGIANT)
            p->SetSize(SZ_GASGIANT);
        else if (p->Size() == SZ_ASTEROIDS)
            p->SetSize(SZ_TINY);
        else if (p->Size() == SZ_GASGIANT)
            p->SetSize(SZ_HUGE);
    }
}

std::string SetPlanetType::Description() const
{
    std::string value_str = ValueRef::ConstantExpr(m_type) ? UserString(lexical_cast<std::string>(m_type->Eval(0, 0, boost::any()))) : m_type->Description();
    return str(FlexibleFormat(UserString("DESC_SET_PLANET_TYPE")) % value_str);
}

std::string SetPlanetType::Dump() const
{
    return DumpIndent() + "SetPlanetType type = " + m_type->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// SetPlanetSize                                         //
///////////////////////////////////////////////////////////
SetPlanetSize::SetPlanetSize(const ValueRef::ValueRefBase<PlanetSize>* size) :
    m_size(size)
{}

SetPlanetSize::~SetPlanetSize()
{
    delete m_size;
}

void SetPlanetSize::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (Planet* p = universe_object_cast<Planet*>(target)) {
        PlanetSize size = m_size->Eval(source, target, p->Size());
        p->SetSize(size);
        if (size == SZ_ASTEROIDS)
            p->SetType(PT_ASTEROIDS);
        else if (size == SZ_GASGIANT)
            p->SetType(PT_GASGIANT);
        else if (p->Type() == PT_ASTEROIDS || p->Type() == PT_GASGIANT)
            p->SetType(PT_BARREN);
    }
}

std::string SetPlanetSize::Description() const
{
    std::string value_str = ValueRef::ConstantExpr(m_size) ? UserString(lexical_cast<std::string>(m_size->Eval(0, 0, boost::any()))) : m_size->Description();
    return str(FlexibleFormat(UserString("DESC_SET_PLANET_SIZE")) % value_str);
}

std::string SetPlanetSize::Dump() const
{
    return DumpIndent() + "SetPlanetSize size = " + m_size->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// SetSpecies                                            //
///////////////////////////////////////////////////////////
SetSpecies::SetSpecies(const ValueRef::ValueRefBase<std::string>* species) :
    m_species_name(species)
{}

SetSpecies::~SetSpecies()
{
    delete m_species_name;
}

void SetSpecies::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (Planet* p = universe_object_cast<Planet*>(target)) {
        std::string species_name = m_species_name->Eval(source, target, p->SpeciesName());
        p->SetSpecies(species_name);
    } else if (Ship* s = universe_object_cast<Ship*>(target)) {
        std::string species_name = m_species_name->Eval(source, target, s->SpeciesName());
        s->SetSpecies(species_name);
    }
}

std::string SetSpecies::Description() const
{
    std::string value_str = ValueRef::ConstantExpr(m_species_name) ? UserString(m_species_name->Eval(0, 0, boost::any())) : m_species_name->Description();
    return str(FlexibleFormat(UserString("DESC_SET_SPECIES")) % value_str);
}

std::string SetSpecies::Dump() const
{
    return DumpIndent() + "SetSpecies name = " + m_species_name->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// AddOwner                                              //
///////////////////////////////////////////////////////////
AddOwner::AddOwner(const ValueRef::ValueRefBase<int>* empire_id) :
    m_empire_id(empire_id)
{}

AddOwner::~AddOwner()
{
    delete m_empire_id;
}

void AddOwner::Execute(const UniverseObject* source, UniverseObject* target) const
{
    int empire_id = m_empire_id->Eval(source, target, boost::any());
    assert(Empires().Lookup(empire_id));
    target->AddOwner(empire_id);
}

std::string AddOwner::Description() const
{
    std::string value_str = ValueRef::ConstantExpr(m_empire_id) ? Empires().Lookup(m_empire_id->Eval(0, 0, boost::any()))->Name() : m_empire_id->Description();
    return str(FlexibleFormat(UserString("DESC_ADD_OWNER")) % value_str);
}

std::string AddOwner::Dump() const
{
    return DumpIndent() + "AddOwner empire = " + m_empire_id->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// RemoveOwner                                           //
///////////////////////////////////////////////////////////
RemoveOwner::RemoveOwner(const ValueRef::ValueRefBase<int>* empire_id) :
    m_empire_id(empire_id)
{}

RemoveOwner::~RemoveOwner()
{
    delete m_empire_id;
}

void RemoveOwner::Execute(const UniverseObject* source, UniverseObject* target) const
{
    int empire_id = m_empire_id->Eval(source, target, boost::any());
    const Empire* empire = Empires().Lookup(empire_id);
    if (!empire) {
        Logger().errorStream() << "RemoveOwner::Execute couldn't get empire with id " << empire_id;
        return;
    }
    target->RemoveOwner(empire_id);
}

std::string RemoveOwner::Description() const
{
    std::string value_str = ValueRef::ConstantExpr(m_empire_id) ? Empires().Lookup(m_empire_id->Eval(0, 0, boost::any()))->Name() : m_empire_id->Description();
    return str(FlexibleFormat(UserString("DESC_REMOVE_OWNER")) % value_str);
}

std::string RemoveOwner::Dump() const
{
    return DumpIndent() + "RemoveOwner empire = " + m_empire_id->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// CreatePlanet                                          //
///////////////////////////////////////////////////////////
CreatePlanet::CreatePlanet(const ValueRef::ValueRefBase<PlanetType>* type, const ValueRef::ValueRefBase<PlanetSize>* size) :
    m_type(type),
    m_size(size)
{}

CreatePlanet::~CreatePlanet()
{
    delete m_type;
    delete m_size;
}

void CreatePlanet::Execute(const UniverseObject* source, UniverseObject* target) const
{
    System* location = GetObject<System>(target->SystemID());
    if (!location) {
        Logger().errorStream() << "CreatePlanet::Execute couldn't get a System object at which to create the planet";
        return;
    }

    PlanetSize size = m_size->Eval(source, target, boost::any());
    PlanetType type = m_type->Eval(source, target, boost::any());
    if (size == INVALID_PLANET_SIZE || type == INVALID_PLANET_TYPE) {
        Logger().errorStream() << "CreatePlanet::Execute got invalid size or type of planet to create...";
        return;
    }

    //  determine if and which orbits are available
    std::set<int> free_orbits = location->FreeOrbits();
    if (free_orbits.empty()) {
        Logger().errorStream() << "CreatePlanet::Execute couldn't find any free orbits in system where planet was to be created";
        return;
    }

    Planet* planet = new Planet(type, size);
    assert(planet);
    int new_planet_id = GetNewObjectID();
    GetUniverse().InsertID(planet, new_planet_id);

    int orbit = *(free_orbits.begin());
    location->Insert(planet, orbit);
}

std::string CreatePlanet::Description() const
{
    std::string type_str = ValueRef::ConstantExpr(m_type) ? UserString(lexical_cast<std::string>(m_type->Eval(0, 0, boost::any()))) : m_type->Description();
    std::string size_str = ValueRef::ConstantExpr(m_size) ? UserString(lexical_cast<std::string>(m_size->Eval(0, 0, boost::any()))) : m_size->Description();

    return str(FlexibleFormat(UserString("DESC_CREATE_PLANET"))
               % type_str
               % size_str);
}

std::string CreatePlanet::Dump() const
{
    return DumpIndent() + "CreatePlanet size = " + m_size->Dump() + " type = " + m_type->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// CreateBuilding                                        //
///////////////////////////////////////////////////////////
CreateBuilding::CreateBuilding(const std::string& building_type) :
    m_type(building_type)
{}

void CreateBuilding::Execute(const UniverseObject* source, UniverseObject* target) const
{
    Planet* location = universe_object_cast<Planet*>(target);
    if (!location) {
        Logger().errorStream() << "CreateBuilding::Execute couldn't get a Planet object at which to create the building";
        return;
    }

    const BuildingType* building_type = GetBuildingType(m_type);
    if (!building_type) {
        Logger().errorStream() << "CreateBuilding::Execute couldn't get building type";
        return;
    }

    Building* building = new Building(ALL_EMPIRES, m_type);
    if (!building) {
        Logger().errorStream() << "CreateBuilding::Execute couldn't create building!";
        return;
    }

    int new_building_id = GetNewObjectID();
    GetUniverse().InsertID(building, new_building_id);

    location->AddBuilding(new_building_id);

    const std::set<int>& owners = location->Owners();
    for (std::set<int>::const_iterator it = owners.begin(); it != owners.end(); ++it)
        building->AddOwner(*it);
}

std::string CreateBuilding::Description() const
{
    return str(FlexibleFormat(UserString("DESC_CREATE_BUILDING"))
               % UserString(m_type));
}

std::string CreateBuilding::Dump() const
{
    return DumpIndent() + "CreateBuilding type = " + m_type + "\n";
}


///////////////////////////////////////////////////////////
// CreateShip                                            //
///////////////////////////////////////////////////////////
CreateShip::CreateShip(const std::string& predefined_ship_design_name, const ValueRef::ValueRefBase<int>* empire_id) :
    m_design_name(predefined_ship_design_name),
    m_empire_id(empire_id)
{}

void CreateShip::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (!target) {
        Logger().errorStream() << "CreateShip::Execute passed null target";
        return;
    }

    System* system = GetObject<System>(target->SystemID());
    if (!system) {
        Logger().errorStream() << "CreateShip::Execute passed a target not in a system";
        return;
    }

    const ShipDesign* ship_design = GetPredefinedShipDesign(m_design_name);
    if (!ship_design) {
        Logger().errorStream() << "CreateShip::Execute couldn't get predefined ship design with name " << m_design_name;
        return;
    }
    int design_id = ship_design->ID();
    if (design_id == ShipDesign::INVALID_DESIGN_ID) {
        Logger().errorStream() << "CreateShip::Execute got design with id -1 from premade ship design name " << m_design_name;
        return;
    }

    int empire_id = m_empire_id->Eval(source, target, boost::any());
    Empire* empire = Empires().Lookup(empire_id);
    if (!empire) {
        Logger().errorStream() << "RemoveOwner::Execute couldn't get empire with id " << empire_id;
        return;
    }

    std::string species_name;
    if (const Planet* capitol_planet = GetObject<Planet>(empire->CapitolID()))
        species_name = capitol_planet->SpeciesName();

    //// possible future modification: try to put new ship into existing fleet if
    //// ownership with target object's fleet works out (if target is a ship)
    //// attempt to find a
    //Fleet* fleet = universe_object_cast<Fleet*>(target);
    //if (!fleet)
    //    if (const Ship* ship = universe_object_cast<const Ship*>(target))
    //        fleet = ship->FleetID();
    //// etc.

    Ship* ship = new Ship(empire_id, design_id, species_name);
    if (!ship) {
        Logger().errorStream() << "CreateShip::Execute couldn't create ship!";
        return;
    }
    ship->Rename(empire->NewShipName());
    ship->UniverseObject::GetMeter(METER_FUEL)->SetCurrent(Meter::LARGE_VALUE);
    ship->UniverseObject::GetMeter(METER_SHIELD)->SetCurrent(Meter::LARGE_VALUE);
    ship->UniverseObject::GetMeter(METER_DETECTION)->SetCurrent(Meter::LARGE_VALUE);
    ship->UniverseObject::GetMeter(METER_STEALTH)->SetCurrent(Meter::LARGE_VALUE);
    ship->UniverseObject::GetMeter(METER_HEALTH)->SetCurrent(Meter::LARGE_VALUE);

    int new_ship_id = GetNewObjectID();
    GetUniverse().InsertID(ship, new_ship_id);


    CreateNewFleet(system, ship);
}

std::string CreateShip::Description() const
{
    std::string owner_str = ValueRef::ConstantExpr(m_empire_id) ? Empires().Lookup(m_empire_id->Eval(0, 0, boost::any()))->Name() : m_empire_id->Description();
    return str(FlexibleFormat(UserString("DESC_CREATE_SHIP"))
               % UserString(m_design_name)
               % owner_str);
}

std::string CreateShip::Dump() const
{
    return DumpIndent() + "CreateShip predefined_ship_design_name = " + m_design_name + " empire = " + m_empire_id->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// Destroy                                               //
///////////////////////////////////////////////////////////
Destroy::Destroy()
{}

void Destroy::Execute(const UniverseObject* source, UniverseObject* target) const
{
    GetUniverse().EffectDestroy(target->ID());
}

std::string Destroy::Description() const
{
    return UserString("DESC_DESTROY");
}

std::string Destroy::Dump() const
{
    return DumpIndent() + "Destroy\n";
}


///////////////////////////////////////////////////////////
// AddSpecial                                            //
///////////////////////////////////////////////////////////
AddSpecial::AddSpecial(const std::string& name) :
    m_name(name)
{}

void AddSpecial::Execute(const UniverseObject* source, UniverseObject* target) const
{
    target->AddSpecial(m_name);
}

std::string AddSpecial::Description() const
{
    return str(FlexibleFormat(UserString("DESC_ADD_SPECIAL")) % UserString(m_name));
}

std::string AddSpecial::Dump() const
{
    return DumpIndent() + "AddSpecial name = \"" + m_name + "\"\n";
}


///////////////////////////////////////////////////////////
// RemoveSpecial                                         //
///////////////////////////////////////////////////////////
RemoveSpecial::RemoveSpecial(const std::string& name) :
    m_name(name)
{}

void RemoveSpecial::Execute(const UniverseObject* source, UniverseObject* target) const
{
    target->RemoveSpecial(m_name);
}

std::string RemoveSpecial::Description() const
{
    return str(FlexibleFormat(UserString("DESC_REMOVE_SPECIAL")) % UserString(m_name));
}

std::string RemoveSpecial::Dump() const
{
    return DumpIndent() + "RemoveSpecial name = \"" + m_name + "\"\n";
}


///////////////////////////////////////////////////////////
// SetStarType                                           //
///////////////////////////////////////////////////////////
SetStarType::SetStarType(const ValueRef::ValueRefBase<StarType>* type) :
    m_type(type)
{}

SetStarType::~SetStarType()
{
    delete m_type;
}

void SetStarType::Execute(const UniverseObject* source, UniverseObject* target) const
{
    if (System* s = universe_object_cast<System*>(target)) {
        s->SetStarType(m_type->Eval(source, target, s->GetStarType()));
    }
}

std::string SetStarType::Description() const
{
    std::string value_str = ValueRef::ConstantExpr(m_type) ? UserString(lexical_cast<std::string>(m_type->Eval(0, 0, boost::any()))) : m_type->Description();
    return str(FlexibleFormat(UserString("DESC_SET_STAR_TYPE")) % value_str);
}

std::string SetStarType::Dump() const
{
    return DumpIndent() + "SetStarType type = " + m_type->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// MoveTo                                                //
///////////////////////////////////////////////////////////
MoveTo::MoveTo(const Condition::ConditionBase* location_condition) :
    m_location_condition(location_condition)
{}

MoveTo::~MoveTo()
{
    delete m_location_condition;
}

void MoveTo::Execute(const UniverseObject* source, UniverseObject* target) const
{
    Universe& universe = GetUniverse();
    ObjectMap& objects = universe.Objects();

    // get all objects in an ObjectSet
    Condition::ObjectSet potential_locations;
    for (ObjectMap::iterator it = objects.begin(); it != objects.end(); ++it)
        potential_locations.insert(it->second);

    Condition::ObjectSet valid_locations;

    // apply location condition to determine valid location to move target to
    m_location_condition->Eval(source, valid_locations, potential_locations);

    // early exit if there are no valid locations - can't move anything if there's nowhere to move to
    if (valid_locations.empty())
        return;

    // "randomly" pick a destination
    UniverseObject* destination = const_cast<UniverseObject*>(*valid_locations.begin());


    // do the moving...

    if (Fleet* fleet = universe_object_cast<Fleet*>(target)) {
        // fleets can be inserted into the system that contains the destination object (or the 
        // destination object istelf if it is a system
        if (System* dest_system = GetObject<System>(destination->SystemID())) {
            if (fleet->SystemID() != dest_system->ID()) {
                dest_system->Insert(target);
                ExploreSystem(dest_system->ID(), target);
                UpdateFleetRoute(fleet, UniverseObject::INVALID_OBJECT_ID, UniverseObject::INVALID_OBJECT_ID);  // inserted into dest_system, so next and previous systems are invalid objects
            }
        } else {
            fleet->UniverseObject::MoveTo(destination);

            // fleet has been moved to a location that is not a system.  Presumably this will be located on a starlane between two
            // other systems, which may or may not have been explored.  Regardless, the fleet needs to be given a new next and
            // previous system so it can move into a system, or can be ordered to a new location, and so that it won't try to move
            // off of starlanes towards some other system from its current location (if it was heading to another system) and so it
            // won't be stuck in the middle of a starlane, unable to move (if it wasn't previously moving)

            // if destination object is a fleet or is part of a fleet, can use that fleet's previous and next systems to get
            // valid next and previous systems for the target fleet.
            const Fleet* dest_fleet = 0;

            dest_fleet = universe_object_cast<const Fleet*>(destination);
            if (!dest_fleet)
                if (const Ship* dest_ship = universe_object_cast<const Ship*>(destination))
                    dest_fleet = GetObject<Fleet>(dest_ship->FleetID());

            if (dest_fleet) {
                UpdateFleetRoute(fleet, dest_fleet->NextSystemID(), dest_fleet->PreviousSystemID());
            } else {
                // need to do something more fancy, although as of this writing, there are no other types of UniverseObject subclass
                // that can be located between systems other than fleets and ships, so this shouldn't matter for now...
                Logger().errorStream() << "Effect::MoveTo::Execute couldn't find a way to set the previous and next systems for the target fleet!";
            }
        }

    } else if (Ship* ship = universe_object_cast<Ship*>(target)) {
        // TODO: make sure colonization doesn't interfere with this effect, and vice versa

        Fleet* old_fleet = GetObject<Fleet>(ship->FleetID());
        Fleet* dest_fleet = universe_object_cast<Fleet*>(destination);  // may be 0 if destination is not a fleet
        bool same_owners = SameOwners(ship, destination);
        int dest_sys_id = destination->SystemID();
        int ship_sys_id = ship->SystemID();

        if (dest_fleet && same_owners) {
            // ship is moving to a different fleet owned by the same empire, so can be inserted into it
            dest_fleet->AddShip(ship->ID());    // does nothing if fleet already contains the ship

        } else if (dest_sys_id == ship_sys_id && dest_sys_id != UniverseObject::INVALID_OBJECT_ID) {
            // ship is moving to the system it is already in, but isn't being or can't be moved into a specific fleet, so the ship
            // can be left in its current fleet and at its current location

        } else if (destination->X() == ship->X() && destination->Y() == ship->Y()) {
            // ship is moving to the same location it's already at, but isn't being or can't be moved to a specific fleet, so the ship
            // can be left in its current fleet and at its current location

        } else {
            // need to create a new fleet for ship
            Fleet* new_fleet = 0;
            if (System* dest_system = GetObject<System>(destination->SystemID())) {
                new_fleet = CreateNewFleet(dest_system, ship);                          // creates new fleet, inserts fleet into system and ship into fleet
                ExploreSystem(dest_system->ID(), target);

            } else {
                new_fleet = CreateNewFleet(destination->X(), destination->Y(), ship);   // creates new fleet and inserts ship into fleet
            }
        }

        if (old_fleet && old_fleet->NumShips() < 1)
            universe.EffectDestroy(old_fleet->ID());

    } else if (Planet* planet = universe_object_cast<Planet*>(target)) {
        // planets need to be located in systems, so get system that contains destination object
        if (System* dest_system = GetObject<System>(destination->SystemID())) {
            // check if planet is already in this system.  if so, don't need to do anything
            if (planet->SystemID() == UniverseObject::INVALID_OBJECT_ID || planet->SystemID() != dest_system->ID()) {
                //  determine if and which orbits are available
                std::set<int> free_orbits = dest_system->FreeOrbits();
                if (!free_orbits.empty()) {
                    int orbit = *(free_orbits.begin());
                    dest_system->Insert(target, orbit);
                    ExploreSystem(dest_system->ID(), target);
                }
            }
        }
        // don't move planets to a location outside a system

    } else if (Building* building = universe_object_cast<Building*>(target)) {
        // buildings need to be located on planets, so if destination is a planet, insert building into it,
        // or attempt to get the planet on which the destination object is located and insert target building into that
        if (Planet* dest_planet = universe_object_cast<Planet*>(destination)) {
            dest_planet->AddBuilding(building->ID());
            if (const System* dest_system = GetObject<System>(dest_planet->SystemID()))
                ExploreSystem(dest_system->ID(), target);


        } else if (Building* dest_building = universe_object_cast<Building*>(destination)) {
            if (Planet* dest_planet = GetObject<Planet>(dest_building->PlanetID())) {
                dest_planet->AddBuilding(building->ID());
                if (const System* dest_system = GetObject<System>(dest_planet->SystemID()))
                    ExploreSystem(dest_system->ID(), target);
            }
        }
        // else if destination is something else that can be on a planet...
    }
}

std::string MoveTo::Description() const
{
    std::string value_str = m_location_condition->Description();
    return str(FlexibleFormat(UserString("DESC_MOVE_TO")) % value_str);
}

std::string MoveTo::Dump() const
{
    return DumpIndent() + "MoveTo destination = " + m_location_condition->Dump() + "\n";
}


///////////////////////////////////////////////////////////
// Victory                                               //
///////////////////////////////////////////////////////////
Victory::Victory(const std::string& reason_string) :
    m_reason_string(reason_string)
{}

void Victory::Execute(const UniverseObject* source, UniverseObject* target) const
{
    GetUniverse().EffectVictory(target->ID(), m_reason_string);
}

std::string Victory::Description() const
{
    return UserString("DESC_VICTORY");
}

std::string Victory::Dump() const
{
    return DumpIndent() + "Victory reason = \"" + m_reason_string + "\"\n";
}


///////////////////////////////////////////////////////////
// SetTechAvailability                                   //
///////////////////////////////////////////////////////////
SetTechAvailability::SetTechAvailability(const std::string& tech_name, const ValueRef::ValueRefBase<int>* empire_id, bool available, bool include_tech) :
    m_tech_name(tech_name),
    m_empire_id(empire_id),
    m_available(available),
    m_include_tech(include_tech)
{}

SetTechAvailability::~SetTechAvailability()
{
    delete m_empire_id;
}

void SetTechAvailability::Execute(const UniverseObject* source, UniverseObject* target) const
{
    Empire* empire = Empires().Lookup(m_empire_id->Eval(source, target, boost::any()));
    if (!empire) return;

    const Tech* tech = GetTech(m_tech_name);
    if (!tech) {
        Logger().errorStream() << "SetTechAvailability::Execute couldn't get tech with name " << m_tech_name;
        return;
    }

    const std::vector<ItemSpec>& items = tech->UnlockedItems();
    for (unsigned int i = 0; i < items.size(); ++i) {
        if (m_available)
            empire->UnlockItem(items[i]);
        else
            empire->LockItem(items[i]);
    }

    if (m_include_tech) {
        if (m_available)
            empire->AddTech(m_tech_name);
        else
            empire->RemoveTech(m_tech_name);
    }
}

std::string SetTechAvailability::Description() const
{
    std::string affected = str(FlexibleFormat(UserString(m_include_tech ? "DESC_TECH_AND_ITEMS_AFFECTED" : "DESC_ITEMS_ONLY_AFFECTED")) % m_tech_name);
    std::string empire_str = ValueRef::ConstantExpr(m_empire_id) ? Empires().Lookup(m_empire_id->Eval(0, 0, boost::any()))->Name() : m_empire_id->Description();
    return str(FlexibleFormat(UserString(m_available ? "DESC_SET_TECH_AVAIL" : "DESC_SET_TECH_UNAVAIL"))
               % affected
               % empire_str);
}

std::string SetTechAvailability::Dump() const
{
    std::string retval = DumpIndent();
    if (m_available && m_include_tech)
        retval += "GiveTechToOwner";
    if (!m_available && m_include_tech)
        retval += "RevokeTechFromOwner";
    if (m_available && !m_include_tech)
        retval += "UnlockTechItemsForOwner";
    if (!m_available && !m_include_tech)
        retval += "LockTechItemsForOwner";
    retval += " name = \"" + m_tech_name + "\"\n";
    return retval;
}


///////////////////////////////////////////////////////////
// SetEffectTarget                                       //
///////////////////////////////////////////////////////////
SetEffectTarget::SetEffectTarget(const ValueRef::ValueRefBase<int>* effect_target_id) :
    m_effect_target_id(effect_target_id)
{}

SetEffectTarget::~SetEffectTarget()
{
    delete m_effect_target_id;
}

void SetEffectTarget::Execute(const UniverseObject* source, UniverseObject* target) const
{
    // TODO: implement after Effect targets are implemented
}

std::string SetEffectTarget::Description() const
{
    // TODO: implement after Effect targets are implemented
    return "ERROR: SetEffectTarget is currently unimplemented.";
}

std::string SetEffectTarget::Dump() const
{
    return "";
}
