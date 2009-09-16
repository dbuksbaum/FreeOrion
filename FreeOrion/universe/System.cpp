#include "System.h"

#include "Fleet.h"
#include "Planet.h"

#include "../Empire/Empire.h"
#include "../Empire/EmpireManager.h"

#include "../util/MultiplayerCommon.h"
#include "Predicates.h"

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include <stdexcept>



System::System() :
    UniverseObject(),
    m_star(INVALID_STAR_TYPE),
    m_orbits(0)
{}

System::System(StarType star, int orbits, const std::string& name, double x, double y,
               const std::set<int>& owners/* = std::set<int>()*/) :
    UniverseObject(name, x, y, owners),
    m_star(star),
    m_orbits(orbits)
{
    if (m_star < INVALID_STAR_TYPE || NUM_STAR_TYPES < m_star)
        throw std::invalid_argument("System::System : Attempted to create a system \"" + Name() + "\" with an invalid star type.");
    if (m_orbits < 0)
        throw std::invalid_argument("System::System : Attempted to create a system \"" + Name() + "\" with fewer than 0 orbits.");

    UniverseObject::Init();
}

System::System(StarType star, int orbits, const StarlaneMap& lanes_and_holes, 
               const std::string& name, double x, double y, const std::set<int>& owners/* = std::set<int>()*/) : 
    UniverseObject(name, x, y, owners),
    m_star(star),
    m_orbits(orbits),
    m_starlanes_wormholes(lanes_and_holes)
{
    if (m_star < INVALID_STAR_TYPE || NUM_STAR_TYPES < m_star)
        throw std::invalid_argument("System::System : Attempted to create a system \"" + Name() + "\" with an invalid star type.");
    if (m_orbits < 0)
        throw std::invalid_argument("System::System : Attempted to create a system \"" + Name() + "\" with fewer than 0 orbits.");
    SetSystem(ID());

    UniverseObject::Init();
}

System::~System()
{}

StarType System::GetStarType() const
{
    return m_star;
}

int System::Orbits() const
{
    return m_orbits;
}

int System::Starlanes() const
{
    int retval = 0;
    for (const_lane_iterator it = begin_lanes(); it != end_lanes(); ++it) {
        if (!it->second)
            ++retval;
    }
    return retval;
}

int System::Wormholes() const
{
    int retval = 0;
    for (const_lane_iterator it = begin_lanes(); it != end_lanes(); ++it) {
        if (it->second)
            ++retval;
    }
    return retval;
}

bool System::HasStarlaneTo(int id) const
{
   const_lane_iterator it = m_starlanes_wormholes.find(id);
   return (it == m_starlanes_wormholes.end() ? false : it->second == false);
}

bool System::HasWormholeTo(int id) const
{
   const_lane_iterator it = m_starlanes_wormholes.find(id);
   return (it == m_starlanes_wormholes.end() ? false : it->second == true);
}

int System::SystemID() const
{
    // Systems don't have a valid UniverseObject::m_system_id since it is never set by inserting them into (another) system.
    // When new systems are created, it's also not set because UniverseObject are created without a valid id number, and only
    // get one when being inserted into the Universe.  Universe::Insert overloads could check what is being inserted, but
    // having SystemID be virtual and overriding it it in this class (System) is easier.
    return this->ID();
}

std::vector<UniverseObject*> System::FindObjects() const
{
    Universe& universe = GetUniverse();
    std::vector<UniverseObject*> retval;
    for (ObjectMultimap::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        UniverseObject* obj = universe.Object(it->second);
        if (!obj) {
            Logger().errorStream() << "System::FindObjects couldn't get a UniverseObject for an object ID in the system";
            continue;
        }
        retval.push_back(obj);
    }
    return retval;
}

std::vector<int> System::FindObjectIDs() const
{
    Universe& universe = GetUniverse();
    std::vector<int> retval;
    // add objects contained in this system, and objects contained in those objects
    for (ObjectMultimap::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        UniverseObject* object = universe.Object(it->second);
        retval.push_back(it->second);
        std::vector<int> contained_objects = object->FindObjectIDs();
        std::copy(contained_objects.begin(), contained_objects.end(), std::back_inserter(retval));
    }
    return retval;
}

System::ObjectIDVec System::FindObjectIDs(const UniverseObjectVisitor& visitor) const
{
    const Universe& universe = GetUniverse();
    ObjectIDVec retval;
    for (ObjectMultimap::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (universe.Object(it->second)->Accept(visitor))
            retval.push_back(it->second);
    }
    return retval;
}

System::ObjectIDVec System::FindObjectIDsInOrbit(int orbit, const UniverseObjectVisitor& visitor) const
{
    const Universe& universe = GetUniverse();
    ObjectIDVec retval;
    std::pair<ObjectMultimap::const_iterator, ObjectMultimap::const_iterator> range = m_objects.equal_range(orbit);
    for (ObjectMultimap::const_iterator it = range.first; it != range.second; ++it) {
        if (universe.Object(it->second)->Accept(visitor))
            retval.push_back(it->second);
    }
    return retval;
}

System::ConstObjectVec System::FindObjects(const UniverseObjectVisitor& visitor) const
{
    const Universe& universe = GetUniverse();
    ConstObjectVec retval;
    for (ObjectMultimap::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (const UniverseObject* obj = universe.Object(it->second)->Accept(visitor))
            retval.push_back(obj);
    }
    return retval;
}

System::const_orbit_iterator System::begin() const
{
    return m_objects.begin();
}

System::const_orbit_iterator System::end() const
{
    return m_objects.end();
}

bool System::Contains(int object_id) const
{
    const UniverseObject* object = GetUniverse().Object(object_id);
    if (object)
        return (ID() == object->SystemID());
    else
        return false;
}

std::pair<System::const_orbit_iterator, System::const_orbit_iterator> System::orbit_range(int o) const
{
    return m_objects.equal_range(o);
}

std::pair<System::const_orbit_iterator, System::const_orbit_iterator> System::non_orbit_range() const
{
    return m_objects.equal_range(-1);
}

System::const_lane_iterator System::begin_lanes() const
{
    return m_starlanes_wormholes.begin();
}

System::const_lane_iterator System::end_lanes() const
{
    return m_starlanes_wormholes.end();
}

System::ConstObjectVec System::FindObjectsInOrbit(int orbit, const UniverseObjectVisitor& visitor) const
{
    const Universe& universe = GetUniverse();
    ConstObjectVec retval;
    std::pair<ObjectMultimap::const_iterator, ObjectMultimap::const_iterator> range = m_objects.equal_range(orbit);
    for (ObjectMultimap::const_iterator it = range.first; it != range.second; ++it) {
        if (const UniverseObject* obj = universe.Object(it->second)->Accept(visitor))
            retval.push_back(obj);
    }
    return retval;
}

UniverseObject* System::Accept(const UniverseObjectVisitor& visitor) const
{
    return visitor.Visit(const_cast<System* const>(this));
}

int System::Insert(UniverseObject* obj)
{
    return Insert(obj, -1);
}

int System::Insert(UniverseObject* obj, int orbit)
{
    if (!obj) {
        Logger().errorStream() << "System::Insert() : Attempted to place a null object in a System";
        return -1;
    }

    //Logger().debugStream() << "System::Insert system " << this->Name() <<  " (object " << obj->Name() << ", orbit " << orbit << ")";
    //Logger().debugStream() << "..initial objects in system: ";
    //for (ObjectMultimap::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
    //    Logger().debugStream() << ".... id: " << it->second << " name: " << GetUniverse().Object(it->second)->Name();
    //Logger().debugStream() << "..initial object systemid: " << obj->SystemID();

    if (orbit < -1) {
        Logger().errorStream() << "System::Insert() : Attempted to place an object in an orbit less than -1";
        return -1;
    }

    if (orbit >= m_orbits) {
        Logger().errorStream() << "System::Insert() : Attempted to place an object in a non-existing orbit with number higher than the largest numbered orbit";
        return -1;
    }

    // inform obj of its new location and system id.  this should propegate to all objects contained within obj
    obj->MoveTo(this);          // ensure object is physically at same location in universe as this system
    obj->SetSystem(this->ID()); // should do nothing if object is already in this system, but just to ensure things are consistent...


    // update obj and its contents in this System's bookkeeping
    std::list<UniverseObject*> inserted_objects;
    inserted_objects.push_back(obj);
    // recursively get all objects contained within obj
    for(std::list<UniverseObject*>::iterator it = inserted_objects.begin(); it != inserted_objects.end(); ++it) {
        std::vector<UniverseObject*> contained_objects = (*it)->FindObjects();
        std::copy(contained_objects.begin(), contained_objects.end(), std::back_inserter(inserted_objects));
    }


    // for all inserted objects, up date this System's bookkeeping about orbit and whether it is contained herein
    for (std::list<UniverseObject*>::iterator it = inserted_objects.begin(); it != inserted_objects.end(); ++it) {
        int cur_id = (*it)->ID();

        // check if obj is already in system's internal bookkeeping.  if it is, check if it is in the wrong
        // orbit.  if it is both, remove it from its current orbit
        for (orbit_iterator orbit_it = m_objects.begin(); orbit_it != m_objects.end(); ++orbit_it) {
            if (orbit != orbit_it->first && orbit_it->second == cur_id) {
                m_objects.erase(orbit_it);
                break;                  // assuming no duplicate entries
            }
        }

        // add obj to system's internal bookkeeping
        std::pair<int, int> insertion(orbit, cur_id);
        m_objects.insert(insertion);
    }


    // special cases for if object is a planet or fleet
    if (universe_object_cast<Planet*>(obj))
        UpdateOwnership();
    else if (Fleet* fleet = universe_object_cast<Fleet*>(obj))
        FleetInsertedSignal(*fleet);

    StateChangedSignal();

    return orbit;
}

int System::Insert(int obj_id, int orbit)
{
    if (orbit < -1)
        throw std::invalid_argument("System::Insert() : Attempted to place an object in an orbit less than -1");
    UniverseObject* object = GetUniverse().Object(obj_id);
    if (!object)
        throw std::invalid_argument("System::Insert() : Attempted to place an object in a System, when the object is not already in the Universe");

    return Insert(object, orbit);
}

void System::Remove(UniverseObject* obj)
{
    //Logger().debugStream() << "System::Remove( " << obj->Name() << " )";
    //Logger().debugStream() << "..objects in system: ";
    //for (ObjectMultimap::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
    //    Logger().debugStream() << ".... " << GetUniverse().Object(it->second)->Name();

    // ensure object and (recursive) contents are all removed from system
    std::list<UniverseObject*> removed_objects;
    removed_objects.push_back(obj);
    obj = 0;    // to ensure I don't accidentally use obj instead of cur_obj in subsequent code
    // recursively get all objects contained within obj
    for(std::list<UniverseObject*>::iterator it = removed_objects.begin(); it != removed_objects.end(); ++it) {
        std::vector<UniverseObject*> contained_objects = (*it)->FindObjects();
        std::copy(contained_objects.begin(), contained_objects.end(), std::back_inserter(removed_objects));
    }


    // keep track of what is removed...
    bool removed_something = false;
    std::vector<Fleet*> removed_fleets;
    bool removed_planet = false;


    // do actual removal
    for (std::list<UniverseObject*>::iterator it = removed_objects.begin(); it != removed_objects.end(); ++it) {
        UniverseObject* cur_obj = *it;

        if (this->ID() != cur_obj->SystemID())
            Logger().debugStream() << "System::Remove tried to remove an object whose system id was not this system.  Its current system id is: " << cur_obj->SystemID();

        for (ObjectMultimap::iterator map_it = m_objects.begin(); map_it != m_objects.end(); ++map_it) {
            if (map_it->second == cur_obj->ID()) {
                m_objects.erase(map_it);
                cur_obj->SetSystem(INVALID_OBJECT_ID);

                removed_something = true;

                if (universe_object_cast<Planet*>(cur_obj))
                    removed_planet = true;
                if (Fleet* fleet = universe_object_cast<Fleet*>(cur_obj))
                    removed_fleets.push_back(fleet);

                break;                  // assuming no duplicate entries
            }
        }
    }

    if (removed_something) {
        // UI bookeeping
        if (removed_planet)
            UpdateOwnership();
        for (std::vector<Fleet*>::const_iterator it = removed_fleets.begin(); it != removed_fleets.end(); ++it)
            FleetRemovedSignal(**it);

        StateChangedSignal();
    } else {
        // didn't find object.  this doesn't necessarily indicate a problem, as removing objects from
        // systems may be done redundantly when inserting or moving objects that contain other objects
        Logger().debugStream() << "System::Remove didn't find object in system";
    }
}

void System::Remove(int id)
{
    Remove(GetUniverse().Object(id));
}

void System::SetStarType(StarType type)
{
    m_star = type;
    if (m_star <= INVALID_STAR_TYPE || NUM_STAR_TYPES <= m_star)
        Logger().errorStream() << "System::SetStarType set star type to " << boost::lexical_cast<std::string>(type);
    StateChangedSignal();
}

void System::AddStarlane(int id)
{
   m_starlanes_wormholes[id] = false;
   StateChangedSignal();
}

void System::AddWormhole(int id)
{
   m_starlanes_wormholes[id] = true;
   StateChangedSignal();
}

bool System::RemoveStarlane(int id)
{
   bool retval = false;
   if (retval = HasStarlaneTo(id)) {
      m_starlanes_wormholes.erase(id);
      StateChangedSignal();
   }
   return retval;
}

bool System::RemoveWormhole(int id)
{
   bool retval = false;
   if (retval = HasWormholeTo(id)) {
      m_starlanes_wormholes.erase(id);
      StateChangedSignal();
   }
   return retval;
}

System::ObjectVec System::FindObjects(const UniverseObjectVisitor& visitor)
{
    Universe& universe = GetUniverse();
    ObjectVec retval;
    for (ObjectMultimap::iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (UniverseObject* obj = universe.Object(it->second)->Accept(visitor))
            retval.push_back(obj);
    }
    return retval;
}

System::ObjectVec System::FindObjectsInOrbit(int orbit, const UniverseObjectVisitor& visitor)
{
    Universe& universe = GetUniverse();
    ObjectVec retval;
    std::pair<ObjectMultimap::iterator, ObjectMultimap::iterator> range = m_objects.equal_range(orbit);
    for (ObjectMultimap::iterator it = range.first; it != range.second; ++it) {
        if (UniverseObject* obj = universe.Object(it->second)->Accept(visitor))
            retval.push_back(obj);
    }
    return retval;
}

void System::AddOwner(int id)
{
}

void System::RemoveOwner(int id)
{
}

void System::MovementPhase()
{
}

void System::PopGrowthProductionResearchPhase()
{
}

System::orbit_iterator System::begin()
{
    return m_objects.begin();
}

System::orbit_iterator System::end()
{
    return m_objects.end();
}

std::pair<System::orbit_iterator, System::orbit_iterator> System::orbit_range(int o)
{
    return m_objects.equal_range(o);
}

std::pair<System::orbit_iterator, System::orbit_iterator> System::non_orbit_range()
{
    return m_objects.equal_range(-1);
}

bool System::OrbitOccupied(int orbit) const 
{
    return (m_objects.find(orbit) != m_objects.end());
}

std::set<int> System::FreeOrbits() const
{
    std::set<int> occupied;
    for (const_orbit_iterator it = begin(); it != end(); ++it)
        occupied.insert(it->first);
    std::set<int> retval;
    for (int i = 0; i < m_orbits; ++i)
        if (occupied.find(i) == occupied.end())
            retval.insert(i);
    return retval;
}

System::lane_iterator System::begin_lanes()
{
    return m_starlanes_wormholes.begin();
}

System::lane_iterator System::end_lanes()
{
    return m_starlanes_wormholes.end();
}

System::StarlaneMap System::VisibleStarlanes(int empire_id) const
{
    if (empire_id == ALL_EMPIRES)
        return m_starlanes_wormholes;

    Universe& universe = GetUniverse();

    // starlanes are visible if both systems have basic visibility or greater,
    // and one or both systems has partial visibility or greater

    StarlaneMap retval;
    for (StarlaneMap::const_iterator it = m_starlanes_wormholes.begin(); it != m_starlanes_wormholes.end(); ++it) {
        Visibility vis1 = universe.GetObjectVisibilityByEmpire(it->first, empire_id);
        Visibility vis2 = universe.GetObjectVisibilityByEmpire(it->second, empire_id);

        if ((vis1 >= VIS_PARTIAL_VISIBILITY && vis2 >= VIS_BASIC_VISIBILITY) || (vis2 >= VIS_PARTIAL_VISIBILITY && vis1 >= VIS_BASIC_VISIBILITY))
            retval.insert(*it);
    }
    return retval;
}

System::ObjectMultimap System::VisibleContainedObjects(int empire_id) const
{
    ObjectMultimap retval;
    Universe& universe = GetUniverse();
    for (ObjectMultimap::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        int object_id = it->second;
        if (universe.GetObjectVisibilityByEmpire(object_id, empire_id) >= VIS_BASIC_VISIBILITY)
            retval.insert(*it);
    }
    return retval;
}

void System::UpdateOwnership()
{
    std::set<int> owners = Owners();
    for (std::set<int>::iterator it = owners.begin(); it != owners.end(); ++it) {
        UniverseObject::RemoveOwner(*it);
    }
    for (ObjectMultimap::iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (Planet *planet = GetUniverse().Object<Planet>(it->second)) {
            for (std::set<int>::const_iterator it2 = planet->Owners().begin(); it2 != planet->Owners().end(); ++it2) {
                UniverseObject::AddOwner(*it2);
            }
        }
    }
}


// free functions

double SystemRadius()
{ return 1000.0 + 50.0; }

double StarRadius()
{ return 80.0; }

double OrbitalRadius(unsigned int orbit)
{
    assert(orbit < 10);
    return (SystemRadius() - 50.0) / 10 * (orbit + 1) - 20.0;
}

double StarlaneEntranceOrbitalRadius()
{ return 1000.0 + 25.0; }

double StarlaneEntranceRadius()
{ return 20.0; }

double StarlaneEntranceOrbitalPosition(int from_system, int to_system)
{
    System* system_1 = GetUniverse().Object<System>(from_system);
    System* system_2 = GetUniverse().Object<System>(to_system);
    assert(system_1 && system_2);
    return std::atan2(system_2->Y() - system_1->Y(), system_2->X() - system_1->X());
}
