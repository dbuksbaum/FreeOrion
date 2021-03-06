#include "CombatSystem.h"

#include "../universe/Universe.h"
#include "../util/AppInterface.h"
#include "../util/MultiplayerCommon.h"
#include "../universe/Predicates.h"

#include "../universe/Planet.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/ShipDesign.h"
#include "../universe/System.h"
#include "../Empire/Empire.h"

#include "../util/Random.h"
#include "../util/AppInterface.h"

#include "../server/ServerApp.h"
#include "../network/Message.h"

////////////////////////////////////////////////
// CombatInfo
////////////////////////////////////////////////
CombatInfo::CombatInfo() :
    system_id(UniverseObject::INVALID_OBJECT_ID)
{}

CombatInfo::CombatInfo(int system_id_) :
    system_id(system_id_)
{
    const Universe& universe = GetUniverse();
    const ObjectMap& universe_objects = universe.Objects();

    const System* system = universe_objects.Object<System>(system_id);
    if (!system) {
        Logger().errorStream() << "CombatInfo constructed with invalid system id: " << system_id;
        return;
    }

    // add copy of system to full / complete objects in combat
    System* copy_system = system->Clone();
    objects.Insert(system_id, copy_system);


    // find ships and their owners in system
    std::vector<int> ship_ids = system->FindObjectIDs<Ship>();
    for (std::vector<int>::const_iterator it = ship_ids.begin(); it != ship_ids.end(); ++it) {
        int ship_id = *it;
        const Ship* ship = GetObject<Ship>(ship_id);
        if (!ship) {
            Logger().errorStream() << "CombatInfo::CombatInfo couldn't get ship with id " << ship_id << " in system " << system->Name() << " (" << system_id << ")";
            continue;
        }

        // add owner to empires that have assets in this battle
        empire_ids.insert(ship->Owner());

        // add copy of ship to full / complete copy of objects in system
        Ship* copy = ship->Clone();
        objects.Insert(ship_id, copy);
    }

    // find planets and their owners in system
    std::vector<int> planet_ids = system->FindObjectIDs<Planet>();
    for (std::vector<int>::const_iterator it = planet_ids.begin(); it != planet_ids.end(); ++it) {
        int planet_id = *it;
        const Planet* planet = GetObject<Planet>(planet_id);
        if (!planet) {
            Logger().errorStream() << "CombatInfo::CombatInfo couldn't get planet with id " << planet_id << " in system " << system->Name() << " (" << system_id << ")";
            continue;
        }

        // add owner to empires that have assets in this battle
        empire_ids.insert(planet->Owner());

        // add copy of ship to full / complete copy of objects in system
        Planet* copy = planet->Clone();
        objects.Insert(planet_id, copy);
    }

    // TODO: should buildings be considered separately?

    // now that all participants in the battle have been found, loop through
    // objects again to assemble each participant empire's latest
    // known information about all objects in this battle

    // system
    for (std::set<int>::const_iterator empire_it = empire_ids.begin(); empire_it != empire_ids.end(); ++empire_it) {
        int empire_id = *empire_it;
        if (empire_id == ALL_EMPIRES)
            continue;
        System* visibility_limited_copy = system->Clone(empire_id);
        empire_known_objects[empire_id].Insert(system_id, visibility_limited_copy);
    }
    // ships
    for (std::vector<int>::const_iterator it = ship_ids.begin(); it != ship_ids.end(); ++it) {
        int ship_id = *it;
        const Ship* ship = GetObject<Ship>(ship_id);
        if (!ship) {
            Logger().errorStream() << "CombatInfo::CombatInfo couldn't get ship with id " << ship_id << " in system " << system->Name() << " (" << system_id << ")";
            continue;
        }

        for (std::set<int>::const_iterator empire_it = empire_ids.begin(); empire_it != empire_ids.end(); ++empire_it) {
            int empire_id = *empire_it;
            if (empire_id == ALL_EMPIRES)
                continue;
            if (universe.GetObjectVisibilityByEmpire(ship_id, empire_id) >= VIS_BASIC_VISIBILITY) {
                Ship* visibility_limited_copy = ship->Clone(empire_id);
                empire_known_objects[empire_id].Insert(ship_id, visibility_limited_copy);
            }
        }
    }
    // planets
    for (std::vector<int>::const_iterator it = planet_ids.begin(); it != planet_ids.end(); ++it) {
        int planet_id = *it;
        const Planet* planet = GetObject<Planet>(planet_id);
        if (!planet) {
            Logger().errorStream() << "CombatInfo::CombatInfo couldn't get planet with id " << planet_id << " in system " << system->Name() << " (" << system_id << ")";
            continue;
        }

        for (std::set<int>::const_iterator empire_it = empire_ids.begin(); empire_it != empire_ids.end(); ++empire_it) {
            int empire_id = *empire_it;
            if (empire_id == ALL_EMPIRES)
                continue;
            if (universe.GetObjectVisibilityByEmpire(planet_id, empire_id) >= VIS_BASIC_VISIBILITY) {
                Planet* visibility_limited_copy = planet->Clone(empire_id);
                empire_known_objects[empire_id].Insert(planet_id, visibility_limited_copy);
            }
        }
    }

    // after battle is simulated, any changes to latest known or actual objects
    // will be copied back to the main Universe's ObjectMap and the Universe's
    // empire latest known objects ObjectMap
}

void CombatInfo::Clear() {
    system_id = UniverseObject::INVALID_OBJECT_ID;
    empire_ids.clear();
    objects.Clear();
    for (std::map<int, ObjectMap>::iterator it = empire_known_objects.begin(); it != empire_known_objects.end(); ++it)
        it->second.Clear();
    destroyed_object_ids.clear();
    destroyed_object_knowers.clear();
}

const System* CombatInfo::GetSystem() const {
    return this->objects.Object<System>(this->system_id);
}

System* CombatInfo::GetSystem() {
    return this->objects.Object<System>(this->system_id);
}

void CombatInfo::GetEmpireIdsToSerialize(std::set<int>& filtered_empire_ids, int encoding_empire) const {
    if (encoding_empire == ALL_EMPIRES) {
        filtered_empire_ids = this->empire_ids;
        return;
    }
    // TODO: include only empires that the encoding empire knows are present in the system / battle
    filtered_empire_ids = this->empire_ids; // for now, include all empires involved in battle
}

void CombatInfo::GetObjectsToSerialize(ObjectMap& filtered_objects, int encoding_empire) const {
    if (&filtered_objects == &this->objects)
        return;

    filtered_objects.Clear();

    if (encoding_empire == ALL_EMPIRES) {
        filtered_objects = this->objects;
        return;
    }
    // TODO: include only objects that the encoding empire has visibility of
    //       using the combat visibility system.
    filtered_objects = this->objects;       // for now, include all objects in battle / system
}

void CombatInfo::GetEmpireKnownObjectsToSerialize(std::map<int, ObjectMap>& filtered_empire_known_objects, int encoding_empire) const {
    if (&filtered_empire_known_objects == &this->empire_known_objects)
        return;

    for (std::map<int, ObjectMap>::iterator it = filtered_empire_known_objects.begin(); it != filtered_empire_known_objects.end(); ++it)
        it->second.Clear();
    filtered_empire_known_objects.clear();

    if (encoding_empire == ALL_EMPIRES) {
        filtered_empire_known_objects = this->empire_known_objects;
        return;
    }

    // include only latest known objects for the encoding empire
    std::map<int, ObjectMap>::const_iterator it = this->empire_known_objects.find(encoding_empire);
    if (it != this->empire_known_objects.end()) {
        const ObjectMap& map = it->second;
        filtered_empire_known_objects[encoding_empire].Copy(map, ALL_EMPIRES);
    }
}

void CombatInfo::GetDestroyedObjectsToSerialize(std::set<int>& filtered_destroyed_objects, int encoding_empire) const {
    if (encoding_empire == ALL_EMPIRES) {
        filtered_destroyed_objects = this->destroyed_object_ids;
        return;
    }
    // TODO: decide if some filtering is needed for destroyed objects... it may not be.
    filtered_destroyed_objects = this->destroyed_object_ids;
}

void CombatInfo::GetDestroyedObjectKnowersToSerialize(std::map<int, std::set<int> >& filtered_destroyed_object_knowers, int encoding_empire) const {
    if (encoding_empire == ALL_EMPIRES) {
        filtered_destroyed_object_knowers = this->destroyed_object_knowers;
        return;
    }
    // TODO: decide if some filtering is needed for which empires know about which
    // other empires know which objects have been destroyed during the battle.
    filtered_destroyed_object_knowers = this->destroyed_object_knowers;
}

////////////////////////////////////////////////
// AutoResolveCombat
////////////////////////////////////////////////
namespace {
    void AttackShipShip(Ship* attacker, Ship* target) {
        if (!attacker || ! target) return;

        const ShipDesign* attacker_design = attacker->Design();
        if (!attacker_design)
            return;
        double damage = attacker_design->Attack();

        if (damage <= 0.0)
            return;

        Logger().debugStream() << "AttackShipShip: attacker: " << attacker->Dump() << "\ntarget: " << target->Dump();


        Meter* target_shield = target->UniverseObject::GetMeter(METER_SHIELD);
        Meter* target_structure = target->UniverseObject::GetMeter(METER_STRUCTURE);
        if (!target_shield || ! target_structure) {
            Logger().errorStream() << "couldn't get target structure or shield meter";
            return;
        }

        // damage shields, limited by shield current value and damage amount.
        // remaining damage, if any, above shield current value goes to structure
        double shield_damage = std::min(target_shield->Current(), damage);
        double structure_damage = 0.0;
        if (shield_damage >= target_shield->Current())
            structure_damage = std::min(target_structure->Current(), damage - shield_damage);

        if (shield_damage > 0) {
            target_shield->AddToCurrent(-shield_damage);
            Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << shield_damage << " shield damage to Ship " << target->Name() << " (" << target->ID() << ")";
        }
        if (structure_damage > 0) {
            target_structure->AddToCurrent(-structure_damage);
            Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << structure_damage << " structure damage to Ship " << target->Name() << " (" << target->ID() << ")";
        }

        Logger().debugStream() << "after AttackShipShip: attacker: " << attacker->Dump() << "\ntarget: " << target->Dump();
    }

    void AttackShipPlanet(Ship* attacker, Planet* target) {
        if (!attacker || ! target) return;

        const ShipDesign* attacker_design = attacker->Design();
        if (!attacker_design)
            return;
        double damage = attacker_design->Attack();

        if (damage <= 0.0)
            return;

        Logger().debugStream() << "AttackShipPlanet: ship: " << attacker->Dump() << "\nplanet: " << target->Dump();

        Meter* target_shield = target->GetMeter(METER_SHIELD);
        Meter* target_construction = target->UniverseObject::GetMeter(METER_CONSTRUCTION);
        if (!target_shield) {
            Logger().errorStream() << "couldn't get target shield meter";
            return;
        }
        if (!target_construction) {
            Logger().errorStream() << "couldn't get target construction meter";
            return;
        }

        // damage shields, limited by shield current value and damage amount.
        // remaining damage, if any, above shield current value goes to construction
        double shield_damage = std::min(target_shield->Current(), damage);
        double construction_damage = 0.0;
        if (shield_damage >= target_shield->Current())
            construction_damage = std::min(target_construction->Current(), damage - shield_damage);

        if (shield_damage >= 0) {
            target_shield->AddToCurrent(-shield_damage);
            Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << shield_damage << " shield damage to Planet " << target->Name() << " (" << target->ID() << ")";
        }
        if (construction_damage >= 0) {
            target_construction->AddToCurrent(-construction_damage);
            Logger().debugStream() << "COMBAT: Ship " << attacker->Name() << " (" << attacker->ID() << ") does " << construction_damage << " construction damage to Planet " << target->Name() << " (" << target->ID() << ")";
        }

        target->SetLastTurnAttackedByShip(CurrentTurn());

        Logger().debugStream() << "after AttackShipPlanet: ship: " << attacker->Dump() << "\nplanet: " << target->Dump();
    }

    void AttackPlanetShip(Planet* attacker, Ship* target) {
        if (!attacker || ! target) return;

        double damage = attacker->UniverseObject::GetMeter(METER_DEFENSE)->Current();   // planet "Defense" meter is actually its attack power

        if (damage <= 0.0)
            return;

        Logger().debugStream() << "AttackPlanetShip: planet: " << attacker->Dump() << "\nship: " << target->Dump();

        Meter* target_shield = target->UniverseObject::GetMeter(METER_SHIELD);
        Meter* target_structure = target->UniverseObject::GetMeter(METER_STRUCTURE);
        if (!target_shield || ! target_structure) {
            Logger().errorStream() << "couldn't get target structure or shield meter";
            return;
        }

        // damage shields, limited by shield current value and damage amount.
        // remaining damage, if any, above shield current value goes to structure
        double shield_damage = std::min(target_shield->Current(), damage);
        double structure_damage = 0.0;
        if (shield_damage >= target_shield->Current())
            structure_damage = std::min(target_structure->Current(), damage - shield_damage);

        if (shield_damage >= 0) {
            target_shield->AddToCurrent(-shield_damage);
            Logger().debugStream() << "COMBAT: Planet " << attacker->Name() << " (" << attacker->ID() << ") does " << shield_damage << " shield damage to Ship " << target->Name() << " (" << target->ID() << ")";
        }
        if (structure_damage >= 0) {
            target_structure->AddToCurrent(-structure_damage);
            Logger().debugStream() << "COMBAT: Planet " << attacker->Name() << " (" << attacker->ID() << ") does " << structure_damage << " structure damage to Ship " << target->Name() << " (" << target->ID() << ")";
        }

        Logger().debugStream() << "after AttackPlanetShip: planet: " << attacker->Dump() << "\nship: " << target->Dump();
    }

    void AttackPlanetPlanet(Planet* attacker, Planet* target) {
        Logger().debugStream() << "AttackPlanetPlanet does nothing!";
        // intentionally left empty
    }
}

void AutoResolveCombat(CombatInfo& combat_info) {
    if (combat_info.objects.Empty())
        return;

    const System* system = combat_info.objects.Object<System>(combat_info.system_id);
    if (!system)
        Logger().errorStream() << "AutoResolveCombat couldn't get system with id " << combat_info.system_id;


    Logger().debugStream() << "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";
    Logger().debugStream() << "AutoResolveCombat objects before resolution: " << combat_info.objects.Dump();

    // reasonably unpredictable but reproducible random seeding
    const UniverseObject* first_object = combat_info.objects.begin()->second;
    int seed = first_object->ID() + CurrentTurn();
    Seed(seed);


    // compile list of valid objects to attack or be attacked in this combat
    std::vector<int> all_combat_object_IDs;
    for (ObjectMap::iterator it = combat_info.objects.begin(); it != combat_info.objects.end(); ++it) {
        const UniverseObject* obj = it->second;
        if (universe_object_cast<const System*>(obj))
            continue;
        if (universe_object_cast<const Fleet*>(obj))
            continue;
        all_combat_object_IDs.push_back(it->first);
    }


    // map from empire to set of IDs of objects that empire's objects
    // could target.  presently valid targets are objects not owned by
    // the empire, that are not systems or fleets
    std::map<int, std::vector<int> > empire_valid_targets;
    for (std::vector<int>::const_iterator object_it = all_combat_object_IDs.begin(); object_it != all_combat_object_IDs.end(); ++object_it) {
        int object_id = *object_it;
        const UniverseObject* obj = combat_info.objects.Object(object_id);

        // which empire owns this object
        int owner = obj->Owner();

        // for all empires, can they attack this object?
        for (std::set<int>::const_iterator empire_it = combat_info.empire_ids.begin();
             empire_it != combat_info.empire_ids.end(); ++empire_it)
        {
            int empire_id = *empire_it;
            if (empire_id != owner) {
                empire_valid_targets[empire_id].push_back(object_id);
            }
        }
    }

    // Each combat "round" a randomly-selected object in the battle attacks
    // something, if it is able to do so.  The number of rounds scales with the
    // number of objects, so the total actions per object is the same for
    // battles, roughly independent of number of objects in the battle
    const int NUM_COMBAT_ROUNDS = 10*all_combat_object_IDs.size();

    for (int round = 1; round <= NUM_COMBAT_ROUNDS; ++round) {
        Logger().debugStream() << "Combat at " << system->Name() << " (" << combat_info.system_id << ") Round " << round;

        // select attacking object in battle
        SmallIntDistType object_num_dist = SmallIntDist(0, all_combat_object_IDs.size() - 1);  // to pick an object id from the vector
        int attacker_index = object_num_dist();
        int attacker_id = all_combat_object_IDs.at(attacker_index);

        // check if object is already destroyed.  if so, skip this round
        if (combat_info.destroyed_object_ids.find(attacker_id) != combat_info.destroyed_object_ids.end()) {
            //Logger().debugStream() << "skipping destroyed object as attack object";
            continue;
        }

        UniverseObject* attacker = combat_info.objects.Object(attacker_id);
        if (!attacker) {
            Logger().errorStream() << "AutoResolveCombat couldn't get object with id " << attacker_id;
            continue;
        }

        // fleets and the system can't attack
        if (universe_object_cast<const System*>(attacker)) {
            //Logger().debugStream() << "skipping system as attack object";
            continue;
        }
        if (universe_object_cast<const Fleet*>(attacker)) {
            //Logger().debugStream() << "skipping fleet as attack object";
            continue;
        }
        // TODO: skip buildings?


        // select object from valid targets for this object's owner.
        // get attacker owner id
        int attacker_owner_id = attacker->Owner();

        // get valid targets set for attacker owner
        std::map<int, std::vector<int> >::iterator target_vec_it = empire_valid_targets.find(attacker_owner_id);
        if (target_vec_it == empire_valid_targets.end()) {
            Logger().debugStream() << "couldn't find target set for owner with id: " << attacker_owner_id;
            continue;
        }
        const std::vector<int>& valid_target_ids = target_vec_it->second;


        Logger().debugStream() << "Attacker: " << attacker->Dump();

        SmallIntDistType target_id_num_dist = SmallIntDist(0, valid_target_ids.size() - 1);  // to pick an object id from the vector

        // select target object
        int target_index = target_id_num_dist();
        int target_id = valid_target_ids.at(target_index);

        // check if object is already destroyed.  if so, skip this round
        if (combat_info.destroyed_object_ids.find(target_id) != combat_info.destroyed_object_ids.end()) {
            Logger().debugStream() << "skipping already destroyed object with id " << target_id;
            continue;
        }


        UniverseObject* target = combat_info.objects.Object(target_id);
        if (!target) {
            Logger().errorStream() << "AutoResolveCombat couldn't get object with id " << target_id;
            continue;
        }

        Logger().debugStream() << "Target: " << target->Dump();

        // do actual attack
        if (Ship* attack_ship = universe_object_cast<Ship*>(attacker)) {
            if (Ship* target_ship = universe_object_cast<Ship*>(target)) {
                AttackShipShip(attack_ship, target_ship);
            } else if (Planet* target_planet = universe_object_cast<Planet*>(target)) {
                AttackShipPlanet(attack_ship, target_planet);
            }
        } else if (Planet* attack_planet = universe_object_cast<Planet*>(attacker)) {
            if (Ship* target_ship = universe_object_cast<Ship*>(target)) {
                AttackPlanetShip(attack_planet, target_ship);
            } else if (Planet* target_planet = universe_object_cast<Planet*>(target)) {
                AttackPlanetPlanet(attack_planet, target_planet);
            }
        }


        // check for destruction of ships
        if (universe_object_cast<Ship*>(target)) {
            if (target->CurrentMeterValue(METER_STRUCTURE) <= 0.0) {
                // object id destroyed
                combat_info.destroyed_object_ids.insert(target_id);
                // all empires in battle know object was destroyed
                for (std::set<int>::const_iterator it = combat_info.empire_ids.begin(); it != combat_info.empire_ids.end(); ++it) {
                    int empire_id = *it;
                    if (empire_id != ALL_EMPIRES)
                        combat_info.destroyed_object_knowers[empire_id].insert(target_id);
                }
            }
        }
    }

    // ensure every participant knows what happened.  TODO: this should probably
    // be more discriminating about what info is or isn't copied.
    for (std::map<int, ObjectMap>::iterator it = combat_info.empire_known_objects.begin();
         it != combat_info.empire_known_objects.end(); ++it)
    {
        it->second.Copy(combat_info.objects);
    }

    Logger().debugStream() << "AutoResolveCombat objects after resolution: " << combat_info.objects.Dump();
}
