// -*- C++ -*-
#ifndef _Universe_h_
#define _Universe_h_

#include "Enums.h"
#include "Predicates.h"
#include "../util/AppInterface.h"

#include <boost/signal.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/type_traits/remove_const.hpp>

#include <vector>
#include <list>
#include <map>
#include <string>
#include <set>

#ifdef FREEORION_WIN32
// because the linker gets confused about Win32 API functions...
#  undef GetObject
#  undef GetObjectA
#endif

struct PlayerSetupData;
class Empire;
struct UniverseObjectVisitor;
class XMLElement;
class ShipDesign;
class UniverseObject;
class System;
namespace Condition {
    struct ConditionBase;
    typedef std::set<const UniverseObject*> ObjectSet;
}
namespace Effect {
    class EffectsGroup;
    typedef std::set<UniverseObject*> TargetSet;
}


/** Contains a set of objects that make up a (known or complete) Universe. */
class ObjectMap {
public:
    typedef std::map<int, UniverseObject*>::const_iterator          iterator;       ///< iterator that allows modification of pointed-to UniverseObjects
    typedef std::map<int, const UniverseObject*>::const_iterator    const_iterator; ///< iterator that does not allow modification of UniverseObjects

    /** \name Structors */ //@{
    ObjectMap();                            ///< default ctor
    ~ObjectMap();                           ///< dtor

    /** Copies contents of this ObjectMap to a new ObjectMap, which is
      * returned.  Copies are limited to only duplicate information that the
      * empire with id \a empire_id would know about the copied objects. */
    ObjectMap*                          Clone(int empire_id = ALL_EMPIRES) const;
    //@}

    /** \name Accessors */ //@{
    /** Returns number of objects in this ObjectMap */
    int                                 NumObjects() const;

    /** Returns true if this ObjectMap contains no objects */
    bool                                Empty() const;

    /** Returns a pointer to the universe object with ID number \a id, or 0 if
      * none exists */
    const UniverseObject*               Object(int id) const;

    /** Returns a pointer to the universe object with ID number \a id, or 0 if
      * none exists */
    UniverseObject*                     Object(int id);

    /** Returns a pointer to the object of type T with ID number \a id.
      * Returns 0 if none exists or the object with ID \a id is not of
      * type T. */
    template <class T>
    const T*                            Object(int id) const;

    /** Returns a pointer to the object of type T with ID number \a id.
      * Returns 0 if none exists or the object with ID \a id is not of
      * type T */
    template <class T>
    T*                                  Object(int id);

    /** Returns a vector containing the objects with ids in \a object_ids */
    std::vector<const UniverseObject*>  FindObjects(const std::vector<int>& object_ids) const;

    /** Returns a vector containing the objects with ids in \a object_ids */
    std::vector<UniverseObject*>        FindObjects(const std::vector<int>& object_ids);

    /** Returns all the objects that match \a visitor */
    std::vector<const UniverseObject*>  FindObjects(const UniverseObjectVisitor& visitor) const;

    /** Returns all the objects that match \a visitor */
    std::vector<UniverseObject*>        FindObjects(const UniverseObjectVisitor& visitor);

    /** Returns all the objects of type T */
    template <class T>
    std::vector<const T*>               FindObjects() const;

    /** Returns all the objects of type T */
    template <class T>
    std::vector<T*>                     FindObjects();

    /** Returns the IDs of all the objects that match \a visitor */
    std::vector<int>                    FindObjectIDs(const UniverseObjectVisitor& visitor) const;

    /** Returns the IDs of all the objects of type T */
    template <class T>
    std::vector<int>                    FindObjectIDs() const;

    /** Returns the IDs of all objects in this ObjectMap */
    std::vector<int>                    FindObjectIDs() const;

    /** iterators */
    iterator                            begin();
    iterator                            end();
    const_iterator                      const_begin() const;
    const_iterator                      const_end() const;

    std::string                         Dump() const;
    //@}

    /** \name Mutators */ //@{

    /** Copies the contents of the ObjectMap \a copied_map into this ObjectMap.
      * Each object in \a copied_map has information transferred to this map.
      * If there already is a version of an object in \a copied_map in this map
      * then information is copied onto this map's version of the object using
      * the UniverseObject::Copy function.  If there is no corresponding object
      * in this map, a new object is created using the UinverseObject::Clone
      * function.  The copied objects are complete copies if \a empire_id is
      * ALL_EMPIRES, but if another \a empire_id is specified, the copied
      * information is limited by passing \a empire_id to are limited to the
      * Copy or Clone functions of the copied UniverseObjects.  Any objects
      * in this ObjectMap that have no corresponding object in \a copied_map
      * are left unchanged. */
    void                                Copy(const ObjectMap& copied_map, int empire_id = ALL_EMPIRES);

    /** Copies the passed \a object into this ObjectMap, overwriting any
      * existing information about that object or creating a new object in this
      * map as appropriate with UniverseObject::Copy or UniverseObject::Clone.
      * The object is fully copied if \a empire_id is ALL_EMPIRES, but if
      * another empire id is specified, then the copied informatio is limited
      * by passed that \a empire_id to Copy or Clone of the object.  The
      * passed object is unchanged. */
    void                                Copy(const UniverseObject* obj, int empire_id = ALL_EMPIRES);

    /** Copies the objects of the ObjectMap \a copied_map that are visible to
      * the empire with id \a empire_id into this ObjectMap.  Copied objects
      * are complete copies of all information in \a copied_map about objects
      * that are visible, and no information about not-visible objects is
      * copied.  Any existing objects in this ObjectMap that are not visible to
      * the empire with id \a empire_id are left unchanged.  If \a empire_id is
      * ALL_EMPIRES, then all objects in \a copied_map are copied completely
      * and this function acts just like ObjectMap::Copy .*/
    void                                CompleteCopyVisible(const ObjectMap& copied_map, int empire_id = ALL_EMPIRES);

    /** Adds object \a obj to the map under id \a id if id is a valid object id
      * and obj is an object with that id set.  If there already was an object
      * in the map with the id \a id then that object is first removed, and
      * is returned. This ObjectMap takes ownership of the passed
      * UniverseObject. The caller takes ownership of any returned
      * UniverseObject. */
    UniverseObject*                     Insert(int id, UniverseObject* obj);

    /** Removes object with id \a id from map, and returns that object, if
      * there was an object under that ID in the map.  If no such object
      * existed in the map, 0 is returned and nothing is removed. The caller
      * takes ownership of any returned UniverseObject. */
    UniverseObject*                     Remove(int id);

    /** Removes object with id \a id from map, and deletes that object, if
      * there was an object under that ID in the map.  If no such object
      * existed in the map, nothing is done. */
    void                                Delete(int id);

    /** Empties map and deletes all objects within. */
    void                                Clear();

    /** Swaps the contents of *this with \a rhs. */
    void                                swap(ObjectMap& rhs);
    //@}

private:
    void                                CopyObjectsToConstObjects();

    std::map<int, UniverseObject*>          m_objects;
    std::map<int, const UniverseObject*>    m_const_objects;

    friend class Universe;
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};


/** The Universe class contains the majority of FreeOrion gamestate: All the
  * UniverseObjects in a game, and (of less importance) all ShipDesigns in a
  * game.  (Other gamestate is contained in the Empire class.)
  * The Universe class also provides functions with which to access objects in
  * it, information about the objects, and information about the objects'
  * relationships to each other.  As well, there are functions that generate
  * and populate new Universe gamestates when new games are started. */
class Universe
{
private:
    typedef std::map<int, ObjectMap>                EmpireObjectMap;                ///< Known information each empire had about objects in the Universe; keyed by empire id

public:
    typedef std::map<Visibility, int>               VisibilityTurnMap;              ///< Most recent turn number on which a something, such as a Universe object, was observed at various Visibility ratings or better

private:
    typedef std::map<int, VisibilityTurnMap>        ObjectVisibilityTurnMap;        ///< Most recent turn number on which the objects were observed at various Visibility ratings; keyed by object id
    typedef std::map<int, ObjectVisibilityTurnMap>  EmpireObjectVisibilityTurnMap;  ///< Each empire's most recent turns on which object information was known; keyed by empire id

    typedef std::map<int, std::set<int> >           ObjectKnowledgeMap;             ///< IDs of Empires which know information about an object (or deleted object); keyed by object id

public:
    static const bool ALL_OBJECTS_VISIBLE;                                          ///< Set to true to make everything visible for everyone. Useful for debugging.

    typedef std::map<int, Visibility>               ObjectVisibilityMap;            ///< map from object id to Visibility level for a particular empire
    typedef std::map<int, ObjectVisibilityMap>      EmpireObjectVisibilityMap;      ///< map from empire id to ObjectVisibilityMap for that empire

    typedef std::map<int, ShipDesign*>              ShipDesignMap;                  ///< ShipDesigns in universe; keyed by design id
    typedef ShipDesignMap::const_iterator           ship_design_iterator;           ///< const iterator over ship designs created by players that are known by this client


    /** Combination of an EffectsGroup and the id of a source object. */
    struct SourcedEffectsGroup {
        SourcedEffectsGroup();
        SourcedEffectsGroup(int source_object_id_, const boost::shared_ptr<const Effect::EffectsGroup>& effects_group_);
        bool operator<(const SourcedEffectsGroup& right) const;
        int                                             source_object_id;
        boost::shared_ptr<const Effect::EffectsGroup>   effects_group;
    };
    /** Description of cause of an effect: the general cause type, and the
      * specific cause.  eg. Building and a particular BuildingType. */
    struct EffectCause {
        EffectCause();
        EffectCause(EffectsCauseType cause_type_, const std::string& specific_cause_);
        EffectsCauseType                                cause_type;         ///< general type of effect cause, eg. tech, building, special...
        std::string                                     specific_cause;     ///< name of specific cause, eg. "Wonder Farm", "Antenna Mk. VI"
    };
    /** Combination of targets and cause for an effects group. */
    struct EffectTargetAndCause {
        EffectTargetAndCause();
        EffectTargetAndCause(const Effect::TargetSet& target_set_, const EffectCause& effect_cause_);
        Effect::TargetSet                               target_set;
        EffectCause                                     effect_cause;
    };
    /** Map from (effects group and source object) to target set of for
      * that effects group with that source object.  A multimap is used
      * so that a single source object can have multiple instances of the
      * same effectsgroup.  This is useful when a Ship has multiple copies
      * of the same effects group due to having multiple copies of the same
      * ship part in its design. */
    typedef std::multimap<SourcedEffectsGroup, EffectTargetAndCause> EffectsTargetsCausesMap;

    /** Accounting information about what the causes are and changes produced
      * by effects groups acting on meters of objects. */
    struct EffectAccountingInfo : public EffectCause {
        EffectAccountingInfo();                                                 ///< default ctor
        int                                             source_id;              ///< source object of effect
        double                                          meter_change;           ///< net change on meter due to this effect, as best known by client's empire
        double                                          running_meter_total;    ///< meter total as of this effect.
    };
    /** Effect accounting information for all meters of all objects that are
      * acted on by effects. */
    typedef std::map<int, std::map<MeterType, std::vector<EffectAccountingInfo> > > EffectAccountingMap;


    /** \name Signal Types */ //@{
    typedef boost::signal<void (const UniverseObject *)> UniverseObjectDeleteSignalType; ///< emitted just before the UniverseObject is deleted
    //@}


    /** \name Structors */ //@{
    Universe();                                     ///< default ctor
    virtual ~Universe();                            ///< dtor
    //@}

    /** \name Accessors */ //@{
    /** Returns objects in this Universe. */
    const ObjectMap&        Objects() const;
    ObjectMap&              Objects();

    /** Returns latest known state of objects for the Empire with
      * id \a empire_id or the true / complete state of all objects in this
      * Universe (the same as calling Objects()) if \a empire_id is
      * ALL_EMPIRES*/
    const ObjectMap&        EmpireKnownObjects(int empire_id = ALL_EMPIRES) const;
    ObjectMap&              EmpireKnownObjects(int empire_id = ALL_EMPIRES);

    /** Returns IDs of objects that the Empire with id \a empire_id has vision
      * of on the current turn, or objects that at least one empire has vision
      * of on the current turn if \a empire_id = ALL_EMPIRES */
    std::set<int>           EmpireVisibleObjectIDs(int empire_id = ALL_EMPIRES) const;

    /** Returns IDs of objects that the Empire with id \a empire_id knows have
      * been destroyed.  Each empire's latest known objects data contains the
      * last known information about each object, whether it has been destroyed
      * or not.  If \a empire_id = ALL_EMPIRES an empty set of IDs is
      * returned. */
    const std::set<int>&    EmpireKnownDestroyedObjectIDs(int empire_id) const;

    const ShipDesign*       GetShipDesign(int ship_design_id) const;                        ///< returns the ship design with id \a ship_design id, or 0 if non exists
    ship_design_iterator    beginShipDesigns() const   {return m_ship_designs.begin();}     ///< returns the begin iterator for ship designs
    ship_design_iterator    endShipDesigns() const     {return m_ship_designs.end();}       ///< returns the end iterator for ship designs

    const ShipDesign*       GetGenericShipDesign(const std::string& name) const;

    /** Returns IDs of ship designs that the Empire with id \a empire_id has
      * seen during the game.  If \a empire_id = ALL_EMPIRES an empty set of
      * ids is returned */
    const std::set<int>&    EmpireKnownShipDesignIDs(int empire_id) const;

    Visibility              GetObjectVisibilityByEmpire(int object_id, int empire_id) const;///< returns the Visibility level of empire with id \a empire_id of UniverseObject with id \a object_id as determined by calling UpdateEmpireObjectVisibilities
    const VisibilityTurnMap&GetObjectVisibilityTurnMapByEmpire(int object_id, int empire_id) const; ///< returns the map from Visibility level to turn number on which the empire with id \a empire_id last had the various Visibility levels of the UniverseObject with id \a object_id .  The returned map may be empty or not have entries for all visibility levels, if the empire has not seen the object at that visibility level yet.

    double                  LinearDistance(int system1_id, int system2_id) const;           ///< returns the straight-line distance between the systems with the given IDs. \throw std::out_of_range This function will throw if either system ID is out of range.

    /** Returns the sequence of systems, including \a system1_id and
      * \a system2_id, that defines the shortest path from \a system1 to
      * \a system2, and the distance travelled to get there.  If no such path
      * exists, the list will be empty.  Note that the path returned may be via
      * one or more starlane, or may be "offroad".  The path is calculated
      * using the visibility for empire \a empire_id, or without regard to
      * visibility if \a empire_id == ALL_EMPIRES.
      * \throw std::out_of_range This function will throw if either system ID
      * is out of range. */
    std::pair<std::list<int>, double>
                            ShortestPath(int system1_id, int system2_id, int empire_id = ALL_EMPIRES) const;

    /** Returns the sequence of systems, including \a system1 and \a system2,
      * that defines the path with the fewest jumps from \a system1 to
      * \a system2, and the number of jumps to get there.  If no such path
      * exists, the list will be empty.  The path is calculated using the
      * visibility for empire \a empire_id, or without regard to visibility if
      * \a empire_id == ALL_EMPIRES.  \throw std::out_of_range This function
      * will throw if either system ID is out of range. */
    std::pair<std::list<int>, int>
                            LeastJumpsPath(int system1_id, int system2_id, int empire_id = ALL_EMPIRES) const;

    /** Returns whether there is a path known to empire \a empire_id between
      * system \a system1 and system \a system2.  The path is calculated using
      * the visibility for empire \a empire_id, or without regard to visibility
      * if \a empire_id == ALL_EMPIRES.  \throw std::out_of_range This function
      * will throw if either system ID is out of range. */
    bool                    SystemsConnected(int system1_id, int system2_id, int empire_id = ALL_EMPIRES) const;

    /** Returns true iff \a system is reachable from another system (i.e. it
      * has at least one known starlane to it).   This does not guarantee that
      * the system is reachable from any specific other system, as two separate
      * groups of locally but not globally interconnected systems may exist.
      * The starlanes considered depend on their visibility for empire
      * \a empire_id, or without regard to visibility if
      * \a empire_id == ALL_EMPIRES.
      * \throw std::out_of_range This function will throw if the system ID is
      * out of range. */
    bool                    SystemHasVisibleStarlanes(int system_id, int empire_id = ALL_EMPIRES) const;

    /** Returns the systems that are one starlane hop away from system
      * \a system.  The returned systems are indexed by distance from
      * \a system.  The neighborhood is calculated using the visibility
      * for empire \a empire_id, or without regard to visibility if
      * \a empire_id == ALL_EMPIRES.
      * \throw std::out_of_range This function will throw if the  system
      * ID is out of range. */
    std::map<double, int>   ImmediateNeighbors(int system_id, int empire_id = ALL_EMPIRES) const;

    /** Returns map, indexed by object id, to map, indexed by MeterType,
      * to vector of EffectAccountInfo for the meter, in order effects
      * were applied to the meter. */
    const EffectAccountingMap&
                            GetEffectAccountingMap() const {return m_effect_accounting_map;}

    /** Returns set of objects that have been marked by the Victory effect
      * to grant their owners victory. */
    const std::multimap<int, std::string>&
                            GetMarkedForVictory() const {return m_marked_for_victory;}

    mutable UniverseObjectDeleteSignalType UniverseObjectDeleteSignal; ///< the state changed signal object for this UniverseObject
    //@}

    /** \name Mutators */ //@{
    /** Inserts object \a obj into the universe; returns the ID number
      * assigned to the object, or -1 on failure.
      * \note Universe gains ownership of \a obj once it is inserted; the
      * caller should \a never delete \a obj after passing it to Insert(). */
    int             Insert(UniverseObject* obj);

    /** Inserts object \a obj of given ID into the universe; returns true
      * on proper insert, or false on failure.
      * \note Universe gains ownership of \a obj once it is inserted; the
      * caller should \a never delete \a obj after
      * passing it to InsertID().
      * Useful mostly for times when ID needs to be consistent on client
      * and server */
    bool            InsertID(UniverseObject* obj, int id);

    /** Inserts \a ship_design into the universe; returns the ship design ID
      * assigned to it, or -1 on failure.
      * \note Universe gains ownership of \a ship_design once inserted. */
    int             InsertShipDesign(ShipDesign* ship_design);

    /** Inserts \a ship_design into the universe with given \a id;  returns
      * true on success, or false on failure.
      * \note Universe gains ownership of \a ship_design once inserted. */
    bool            InsertShipDesignID(ShipDesign* ship_design, int id);

    /** Generates systems and planets, assigns homeworlds and populates them
      * with people, industry and bases, and places starting fleets.  Uses
      * predefined galaxy shapes. */
    void            CreateUniverse(int size, Shape shape,
                                   GalaxySetupOption age, GalaxySetupOption starlane_freq,
                                   GalaxySetupOption planet_density, GalaxySetupOption specials_freq,
                                   GalaxySetupOption life_freq,
                                   const std::map<int, PlayerSetupData>& player_setup_data);

    /** Clears main ObjectMap, empires' latest known objects map, and
      * ShipDesign map. */
    void            Clear();

    /** Determines all effectsgroups' target sets, then eesets meters and
      * executes only all effects on all objects whose ids are listed in
      * \a object_ids.  Then clamps meter values so target and max meters are
      * within a reasonable range and any current meters with associated max
      * meters are limited by their max. */
    void            ApplyAllEffectsAndUpdateMeters(const std::vector<int>& object_ids);

    /** Calls above ApplyAllEffectsAndUpdateMeters() function on all objects.*/
    void            ApplyAllEffectsAndUpdateMeters();

    /** Determines all effectsgroups' target sets, then eesets meters and
      * executes only SetMeter effects on all objects whose ids are listed in
      * \a object_ids.  Then clamps meter values so target and max meters are
      * within a reasonable range and any current meters with associated max
      * meters are limited by their max. */
    void            ApplyMeterEffectsAndUpdateMeters(const std::vector<int>& object_ids);

    /** Calls above ApplyMeterEffectsAndUpdateMeters() function on all objects.*/
    void            ApplyMeterEffectsAndUpdateMeters();


    /** For all objects and meters, determines discrepancies between actual meter
      * maxes and what the known universe should produce, and and stores in
      * m_effect_discrepancy_map. */
    void            InitMeterEstimatesAndDiscrepancies();

    /** Based on (known subset of, if in a client) universe and any orders
      * given so far this turn, updates estimated meter maxes for next turn
      * for the objects with ids indicated in \a objects_vec.  If
      * \a meter_type is INVALID_METER_TYPE, all meter types are updated, but
      * if \a meter_type is a valid meter type, just that type of meter is
      * updated. */
    void            UpdateMeterEstimates(const std::vector<int>& objects_vec,
                                         MeterType meter_type = INVALID_METER_TYPE);

    /** Updates indicated object's indicated meter, and if applicable, the
      * indicated meters of objects contained within the indicated object
      * If \a object_id is UniverseObject::INVALID_OBJECT_ID, then all
      * objects' meters are updated.  If \a meter_type is INVALID_METER_TYPE,
      * then all meter types are updated. */
    void            UpdateMeterEstimates(int object_id,
                                         MeterType meter_type = INVALID_METER_TYPE,
                                         bool update_contained_objects = false);

    /** Updates all meters for all (known) objects */
    void            UpdateMeterEstimates();

    /** Sets all objects' meters' initial values to their current values. */
    void            BackPropegateObjectMeters();

    /** Sets indicated objects' meters' initial values to their current values. */
    void            BackPropegateObjectMeters(const std::vector<int>& object_ids);

    /** Determines which empires can see which objects at what visibility
      * level, based on  */
    void            UpdateEmpireObjectVisibilities();

    /** Stores latest known information about each object for each empire and
      * updates the record of the last turn on which each empire has visibility
      * of object that can be seen on the current turn with the level of
      * visibility that the empire has this turn. */
    void            UpdateEmpireLatestKnownObjectsAndVisibilityTurns();

    /** Reconstructs the per-empire system graph views needed to calculate
      * routes based on visibility. */
    void            RebuildEmpireViewSystemGraphs(int for_empire_id = ALL_EMPIRES);

    /** Adds the object ID \a object_id to the set of object ids for the empire
      * with id \a empire_id that the empire knows have been destroyed. */
    void            SetEmpireKnowledgeOfDestroyedObject(int object_id, int empire_id);

    /** Adds the ship design ID \a ship_design_id to the set of ship design ids
      * known by the empire with id \a empire_id */
    void            SetEmpireKnowledgeOfShipDesign(int ship_design_id, int empire_id);

    /** Removes the object with ID number \a object_id from the universe's map
      * of existing objects, and adds the object's id to the set of destroyed
      * object ids.  If \a update_destroyed_object_knowers is true, empires
      * that currently have visibility of the object have its id added to
      * their set of objects' ids that are known to have been destroyed.  Older
      * or limited versions of objects remain in empires latest known objects
      * ObjectMap, regardless of whether the empire knows the object is
      * destroyed. */
    void            Destroy(int object_id, bool update_destroyed_object_knowers = true);

    /** Destroys object with ID \a object_id, and destroys any associted
      * objects, such as contained buildings of planets, contained anything of
      * systems, or fleets if their last ship has id \a object_id and the fleet
      * is thus empty. */
    void            RecursiveDestroy(int object_id);

    /** Used by the Destroy effect to mark an object for destruction later
      * during turn processing. (objects can't be destroyed immediately as
      * other effects might depend on their existence) */
    void            EffectDestroy(int object_id);

    /** Permanently deletes object with ID number \a object_id.  no
      * information about this object is retained in the Universe.  Can be
      * performed on objects whether or not the have been destroyed.  Returns
      * true if such an object was found, false otherwise. */
    bool            Delete(int object_id);

    /** Cleans up internal storage of now-invalidated empire ID. */
    void            HandleEmpireElimination(int empire_id);

    /** Used by the Victory effect to mark an object to give it owner victory. */
    void            EffectVictory(int object_id, const std::string& reason_string);

    /** Sets whether to inhibit UniverseObjectSignals.  Inhibits if \a inhibit
      * is true, and (re)enables UniverseObjectSignals if \a inhibit is false. */
    static void     InhibitUniverseObjectSignals(bool inhibit = true);
    //@}

    /** Returns the size of the galaxy map.  Does not measure absolute
      * distances; the ratio between map coordinates and actual distance varies
      * depending on universe size. */
    static double   UniverseWidth();

    /** Generates an object ID for a future object. Usually used by the server
      * to service new ID requests. */
    int             GenerateObjectID();

    /** Generates design ID for a new (ship) design. Usually used by the
      * server to service new ID requests. */
    int             GenerateDesignID();

    typedef std::vector<std::vector<std::set<System*> > > AdjacencyGrid;

    /** Returns true if UniverseOjbectSignals are inhibited, false otherwise. */
    static const bool&  UniverseObjectSignalsInhibited();

    /** HACK! This must be set to the encoding empire's id when serializing a
      * Universe, so that only the relevant parts of the Universe are
      * serialized.  The use of this global variable is done just so I don't
      * have to rewrite any custom boost::serialization classes that implement
      * empire-dependent visibility. */
    static int                      s_encoding_empire;

private:
    typedef std::vector< std::vector<double> > DistanceMatrix;

    /** Discrepancy between meter's value at start of turn, and the value that
      * this client calculate that the meter should have with the knowledge
      * available -> the unknown factor affecting the meter.  This is used
      * when generating effect accounting, in the case where the expected
      * and actual meter values don't match. */
    typedef std::map<int, std::map<MeterType, double> > EffectDiscrepancyMap;

    struct GraphImpl;

    /** Clears \a effects_targets_map, and then populates with all
      * EffectsGroups and their targets in the known universe. */
    void    GetEffectsAndTargets(EffectsTargetsCausesMap& effects_targets_map);

    /** Removes entries in \a effects_targets_map about effects groups acting
      * on objects in \a target_objects, and then repopulates for EffectsGroups
      * that act on at least one of the objects in \a target_objects. */
    void    GetEffectsAndTargets(EffectsTargetsCausesMap& targets_causes_map, const std::vector<int>& target_objects);

    /** Used by GetEffectsAndTargets to process a vector of effects groups.
      * Stores target set of specified \a effects_groups and \a source_object_id
      * in \a targets_causes_map */
    void    StoreTargetsAndCausesOfEffectsGroups(const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >& effects_groups,
                                                 int source_object_id, EffectsCauseType effect_cause_type,
                                                 const std::string& specific_cause_name,
                                                 const std::vector<int>& target_objects, EffectsTargetsCausesMap& targets_causes_map);

    /** Executes all effects.  For use on server when processing turns. */
    void    ExecuteEffects(const EffectsTargetsCausesMap& targets_causes_map);

    /** Executes only meter-altering effects; ignores other effects..  Can be
      * used on server or on clients to determine meter values after effects are
      * applied */
    void    ExecuteMeterEffects(const EffectsTargetsCausesMap& targets_causes_map);

    /** Does actual updating of meter estimates after the public function have
      * processed objects_vec or whatever they were passed and cleared the
      * relevant effect accounting for those objects and meters. */
    void    UpdateMeterEstimatesImpl(const std::vector<int>& objects_vec, MeterType meter_type = INVALID_METER_TYPE);


    /** Generates planets for all systems that have empty object maps (ie those
      * that aren't homeworld systems).*/
    void    PopulateSystems(GalaxySetupOption density, GalaxySetupOption specials_freq);

    /** Adds non-empire-affiliated native populations to planets. */
    void    GenerateNatives(GalaxySetupOption freq);

    /** Adds space monsters to systems. */
    void    GenerateSpaceMonsters(GalaxySetupOption freq);

    /** Creates starlanes and adds them systems already generated. */
    void    GenerateStarlanes(GalaxySetupOption freq, const AdjacencyGrid& adjacency_grid);

    /** Resizes the system graph to the appropriate size and populates
      * m_system_distances.  Uses the Universe latest known set of objects for
      * the empire with id \a for_empire_id or uses the main / true / visible
      * objects if \a for_empire_id is ALL_EMPIRES*/
    void    InitializeSystemGraph(int for_empire_id = ALL_EMPIRES);

    /** Picks systems to host homeworlds, generates planets for them, stores
      * the ID's of the homeworld planets into the homeworld vector. */
    void    GenerateHomeworlds(int players, std::vector<int>& homeworld_planet_ids);

    /** Names the planets in each system, based on the system's name. */
    void    NamePlanets();

    /** Creates Empires objects for each entry in \a player_setup_data with
      * empire id equal to the specified player ids (so that the calling code
      * can know which empire belongs to which player).  Homeworlds are
      * associated with the empires, and starting buildings and fleets are
      * created, and empire starting ship designs are created and added. */
    void    GenerateEmpires(std::vector<int>& homeworld_planet_ids, const std::map<int, PlayerSetupData>& player_setup_data);

    ObjectMap                       m_objects;                          ///< map from object id to UniverseObjects in the universe.  for the server: all of them, up to date and true information about object is stored;  for clients, only limited information based on what the client knows about is sent.
    EmpireObjectMap                 m_empire_latest_known_objects;      ///< map from empire id to (map from object id to latest known information about each object by that empire)

    EmpireObjectVisibilityMap       m_empire_object_visibility;         ///< map from empire id to (map from object id to visibility of that object for that empire)
    EmpireObjectVisibilityTurnMap   m_empire_object_visibility_turns;   ///< map from empire id to (map from object id to (map from Visibility rating to turn number on which the empire last saw the object at the indicated Visibility rating or higher)

    ObjectKnowledgeMap              m_empire_known_destroyed_object_ids;///< map from empire id to (set of object ids that the empire knows have been destroyed)

    ShipDesignMap                   m_ship_designs;                     ///< ship designs in the universe
    std::map<int, std::set<int> >   m_empire_known_ship_design_ids;     ///< ship designs known to each empire

    DistanceMatrix                  m_system_distances;                 ///< the straight-line distances between all the systems; this is an lower-triangular matrix, so only access the elements in (highID, lowID) order
    GraphImpl*                      m_graph_impl;                       ///< a graph in which the systems are vertices and the starlanes are edges

    EffectAccountingMap             m_effect_accounting_map;            ///< map from target object id, to map from target meter, to orderered list of structs with details of an effect and what it does to the meter
    EffectDiscrepancyMap            m_effect_discrepancy_map;           ///< map from target object id, to map from target meter, to discrepancy between meter's actual initial value, and the initial value that this meter should have as far as the client can tell: the unknown factor affecting the meter

    int                             m_last_allocated_object_id;
    int                             m_last_allocated_design_id;

    std::set<int>                   m_marked_destroyed;                 ///< used while applying effects to cache objects that have been destroyed.  this allows to-be-destroyed objects to remain undestroyed until all effects have been processed, which ensures that to-be-destroyed objects still exist when other effects need to access them as a source object
    std::multimap<int, std::string> m_marked_for_victory;               ///< used while applying effects to cache objects whose owner should be victorious.  Victory testing is done separately from effects execution, so this needs to be stored temporarily...

    static double                   s_universe_width;
    static bool                     s_inhibit_universe_object_signals;

    /** Fills \a designs_to_serialize with ShipDesigns known to the empire with
      * the ID \a encoding empire.  If encoding_empire is ALL_EMPIRES, then all
      * designs are included. */
    void    GetShipDesignsToSerialize(ShipDesignMap& designs_to_serialize, int encoding_empire) const;

    /** Fills \a objects with copies of UniverseObjects that should be sent
      * to the empire with id \a encoding_empires */
    void    GetObjectsToSerialize(ObjectMap& objects, int encoding_empire) const;

    /** Fills \a empire_latest_known_objects map with the latest known data
      * about UniverseObjects for the empire with id \a encoding_empire.  If
      * the encoding empire is ALL_EMPIRES then all stored empire object
      * knowledge is included. */
    void    GetEmpireKnownObjectsToSerialize(EmpireObjectMap& empire_latest_known_objects, int encoding_empire) const;

    /***/
    void    GetEmpireObjectVisibilityMap(EmpireObjectVisibilityMap& empire_object_visibility, int encoding_empire) const;

    /***/
    void    GetEmpireObjectVisibilityTurnMap(EmpireObjectVisibilityTurnMap& empire_object_visibility_turns, int encoding_empire) const;

    /***/
    void    GetEmpireKnownDestroyedObjects(ObjectKnowledgeMap& m_empire_known_destroyed_object_ids, int encoding_empire) const;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Free function UniverseObject getters. */
UniverseObject*         GetObject(int object_id);
UniverseObject*         GetEmpireKnownObject(int object_id, int empire_id);

template <class T>
T*                      GetObject(int object_id)
{
    return GetUniverse().Objects().template Object<T>(object_id);
}

template <class T>
T*                      GetEmpireKnownObject(int object_id, int empire_id)
{
    return GetUniverse().EmpireKnownObjects(empire_id).template Object<T>(object_id);
}


/** A combination of names of ShipDesign that can be put together to make a
  * fleet of ships, and a name for such a fleet, loaded from starting_fleets.txt
  * ShipDesign names refer to designs listed in premade_ship_designs.txt.
  * Useful for saving or specifying prearranged combinations of prearranged
  * ShipDesigns to automatically put together, such as during universe creation.*/
class FleetPlan
{
public:
    FleetPlan(const std::string& fleet_name, const std::vector<std::string>& ship_design_names,
              bool lookup_name_userstring = false);
    FleetPlan();
    virtual ~FleetPlan();
    const std::string&              Name() const;
    const std::vector<std::string>& ShipDesigns() const;
protected:
    std::string                     m_name;
    std::vector<std::string>        m_ship_designs;
    bool                            m_name_in_stringtable;
};

/** The combination of a FleetPlan and spawning instructions for start of game
  * monsters. */
class MonsterFleetPlan : public FleetPlan
{
public:
    MonsterFleetPlan(const std::string& fleet_name, const std::vector<std::string>& ship_design_names,
                     double spawn_rate = 1.0, int spawn_limit = 9999, const Condition::ConditionBase* location = 0,
                     bool lookup_name_userstring = false);
    MonsterFleetPlan();
    virtual ~MonsterFleetPlan();
    double                          SpawnRate() const;
    int                             SpawnLimit() const;
    const Condition::ConditionBase* Location() const;
protected:
    double                          m_spawn_rate;
    int                             m_spawn_limit;
    const Condition::ConditionBase* m_location;
};


// template implementations
#if (10 * __GNUC__ + __GNUC_MINOR__ > 33) && (!defined _UniverseObject_h_)
#  include "UniverseObject.h"
#endif

template <class T>
const T* ObjectMap::Object(int id) const
{
    const_iterator it = m_const_objects.find(id);
    return (it != m_const_objects.end() ?
            static_cast<T*>(it->second->Accept(UniverseObjectSubclassVisitor<typename boost::remove_const<T>::type>())) :
            0);
}

template <class T>
T* ObjectMap::Object(int id)
{
    iterator it = m_objects.find(id);
    return (it != m_objects.end() ?
            static_cast<T*>(it->second->Accept(UniverseObjectSubclassVisitor<typename boost::remove_const<T>::type>())) :
            0);
}

template <class T>
std::vector<const T*> ObjectMap::FindObjects() const
{
    std::vector<const T*> retval;
    for (ObjectMap::const_iterator it = m_const_objects.begin(); it != m_const_objects.end(); ++it) {
        if (const T* obj = static_cast<T*>(it->second->Accept(UniverseObjectSubclassVisitor<typename boost::remove_const<T>::type>())))
            retval.push_back(obj);
    }
    return retval;
}

template <class T>
std::vector<T*> ObjectMap::FindObjects()
{
    std::vector<T*> retval;
    for (ObjectMap::iterator it = m_objects.begin(); it != m_objects.end(); ++it) {
        if (T* obj = static_cast<T*>(it->second->Accept(UniverseObjectSubclassVisitor<typename boost::remove_const<T>::type>())))
            retval.push_back(obj);
    }
    return retval;
}

template <class T>
std::vector<int> ObjectMap::FindObjectIDs() const
{
    std::vector<int> retval;
    for (ObjectMap::const_iterator it = m_const_objects.begin(); it != m_const_objects.end(); ++it) {
        if (static_cast<T*>(it->second->Accept(UniverseObjectSubclassVisitor<typename boost::remove_const<T>::type>())))
            retval.push_back(it->first);
    }
    return retval;
}

#endif // _Universe_h_
