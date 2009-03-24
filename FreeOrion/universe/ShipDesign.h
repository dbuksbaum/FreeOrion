// -*- C++ -*-
#ifndef _ShipDesign_h_
#define _ShipDesign_h_

#include <string>
#include <vector>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>

#include "Enums.h"

namespace Condition {
    struct ConditionBase;
}
namespace Effect {
    class EffectsGroup;
}

/** Part stats for the PC_SHORT_RANGE and PC_POINT_DEFENSE part types. */
struct DirectFireStats
{
    DirectFireStats();
    DirectFireStats(double damage,
                    double ROF,
                    double range);

    double m_damage;
    double m_ROF;
    double m_range;

    /** The factor by which PD damage should be multiplied when used in
        defense of the ship firing it. */
    static const double PD_SELF_DEFENSE_FACTOR;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int)
        {
            ar  & BOOST_SERIALIZATION_NVP(m_damage)
                & BOOST_SERIALIZATION_NVP(m_ROF)
                & BOOST_SERIALIZATION_NVP(m_range);
        }
};

/** Part stats for the PC_MISSILES part type. */
struct LRStats
{
    LRStats();
    LRStats(double damage,
            double ROF,
            double range,
            double speed,
            double stealth,
            double health,
            int capacity);

    double m_damage;
    double m_ROF;
    double m_range;
    double m_speed;
    double m_stealth;
    double m_health;
    int m_capacity;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int)
        {
            ar  & BOOST_SERIALIZATION_NVP(m_damage)
                & BOOST_SERIALIZATION_NVP(m_ROF)
                & BOOST_SERIALIZATION_NVP(m_range)
                & BOOST_SERIALIZATION_NVP(m_speed)
                & BOOST_SERIALIZATION_NVP(m_stealth)
                & BOOST_SERIALIZATION_NVP(m_health)
                & BOOST_SERIALIZATION_NVP(m_capacity);
        }
};

/** Part stats for the PC_FIGHTERS part type. */
struct FighterStats
{
    FighterStats();
    FighterStats(CombatFighterType type,
                 double anti_fighter_damage,
                 double anti_ship_damage,
                 double launch_rate,
                 double speed,
                 double stealth,
                 double health,
                 double detection,
                 int capacity);

    CombatFighterType m_type;
    double m_anti_fighter_damage;
    double m_anti_ship_damage;
    double m_launch_rate;
    double m_speed;
    double m_stealth;
    double m_health;
    double m_detection;
    int m_capacity;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int)
        {
            ar  & BOOST_SERIALIZATION_NVP(m_type)
                & BOOST_SERIALIZATION_NVP(m_anti_fighter_damage)
                & BOOST_SERIALIZATION_NVP(m_anti_ship_damage)
                & BOOST_SERIALIZATION_NVP(m_launch_rate)
                & BOOST_SERIALIZATION_NVP(m_speed)
                & BOOST_SERIALIZATION_NVP(m_stealth)
                & BOOST_SERIALIZATION_NVP(m_health)
                & BOOST_SERIALIZATION_NVP(m_detection)
                & BOOST_SERIALIZATION_NVP(m_capacity);
        }
};

/** A variant type containing all ShipPartClass-specific stats for a PartType.
    Note that most parts need only a single value to represent their
    capabilities.  This is represented by the double variant. */
typedef boost::variant<double, DirectFireStats, LRStats, FighterStats> PartTypeStats;

/** A type of ship part */
class PartType {
public:
    /** \name Structors */ //@{
    PartType();
    PartType(const std::string& name, const std::string& description,
             ShipPartClass part_class, const PartTypeStats& stats, double cost,
             int build_time, std::vector<ShipSlotType> mountable_slot_types,
             const Condition::ConditionBase* location,
             const std::string& graphic);
    ~PartType();
    //@}

    /** \name Accessors */ //@{
    const std::string&   Name() const;           ///< returns name of part
    std::string          Description() const;    ///< returns description, including a description of the stats and effects of this part

    ShipPartClass        Class() const;          ///< returns that class of part that this is.

    const PartTypeStats& Stats() const;          ///< returns how good the part is at its function.  might be weapon or shield strength, or cargo hold capacity

    bool                 CanMountInSlotType(ShipSlotType slot_type) const;   ///< returns true if this part can be placed in a slot of the indicated type

    double               Cost() const;           ///< returns cost of part
    int                  BuildTime() const;      ///< returns additional turns to build design that this part adds

    const std::string&   Graphic() const;        ///< returns graphic that represents part in UI

    const Condition::ConditionBase*
                         Location() const;       ///< returns the condition that determines the locations where ShipDesign containing part can be produced
    const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >&
                         Effects() const;        ///< returns the EffectsGroups that encapsulate the effects this part has
    //@}

private:
    std::string         m_name;
    std::string         m_description;

    ShipPartClass       m_class;

    PartTypeStats       m_stats;

    double              m_cost;         // in PP
    int                 m_build_time;   // in turns

    std::vector<ShipSlotType>
                        m_mountable_slot_types;

    const Condition::ConditionBase*
                        m_location;
    std::vector<boost::shared_ptr<const Effect::EffectsGroup> >
                        m_effects;

    std::string         m_graphic;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Holds FreeOrion ship part types */
class PartTypeManager {
public:
    typedef std::map<std::string, PartType*>::const_iterator iterator;

    /** \name Accessors */ //@{
    /** returns the part type with the name \a name; you should use the free function GetPartType() instead */
    const PartType* GetPartType(const std::string& name) const;

    /** iterator to the first part type */
    iterator begin() const;

    /** iterator to the last + 1th part type */
    iterator end() const;

    /** returns the instance of this singleton class; you should use the free function GetPartTypeManager() instead */
    static const PartTypeManager& GetPartTypeManager();
    //@}

private:
    PartTypeManager();
    ~PartTypeManager();

    std::map<std::string, PartType*>    m_parts;
    static PartTypeManager*             s_instance;
};


/** returns the singleton part type manager */
const PartTypeManager& GetPartTypeManager();

/** Returns the ship PartType specification object with name \a name.  If no such PartType exists,
    0 is returned instead. */
const PartType* GetPartType(const std::string& name);


/** Specification for the hull, or base, on which ship designs are created by adding parts.  The hull 
    determines some final design characteristics directly, and also determine how many parts can be
    added to the design. */
class HullType {
public:
    struct Slot {
        Slot();
        Slot(ShipSlotType slot_type, double x_, double y_);
        ShipSlotType type;
        double x, y;
    };

    /** \name Structors */ //@{
    HullType();
    HullType(const std::string& name, const std::string& description, double speed,
             double starlane_speed, double fuel, double health, double cost, int build_time,
             const std::vector<Slot>& slots, const Condition::ConditionBase* location,
             const std::string& graphic);
    ~HullType();
    //@}

    /** \name Accessors */ //@{
    const std::string&  Name() const;           ///< returns name of hull
    std::string         Description() const;    ///< returns description, including a description of the stats and effects of this hull

    double              Speed() const;          ///< returns combat speed of hull
    double              StarlaneSpeed() const;  ///< returns starlane speed of hull
    double              Fuel() const;           ///< returns fuel capacity of hull

    double              Health() const;         ///< returns health of hull

    double              Cost() const;           ///< returns cost of hull
    int                 BuildTime() const;      ///< returns base build time for this hull, before parts are added

    unsigned int        NumSlots() const;                       ///< returns total number of of slots in hull
    unsigned int        NumSlots(ShipSlotType slot_type) const; ///< returns number of of slots of indicated type in hull
    const std::vector<Slot>&
                        Slots() const;          ///< returns vector of slots in hull

    const Condition::ConditionBase*
                        Location() const;       ///< returns the condition that determines the locations where ShipDesign containing hull can be produced
    const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >&
                        Effects() const;        ///< returns the EffectsGroups that encapsulate the effects this part hull has

    const std::string&  Graphic() const;        ///< returns the image that represents the hull on the design screen
    //@}

private:
    std::string                 m_name;
    std::string                 m_description;
    double                      m_speed;
    double                      m_starlane_speed;
    double                      m_fuel;
    double                      m_health;

    double                      m_cost;         // in PP
    int                         m_build_time;   // in turns

    std::vector<Slot>           m_slots;

    const Condition::ConditionBase*
                                m_location;
    std::vector<boost::shared_ptr<const Effect::EffectsGroup> >
                                m_effects;

    std::string                 m_graphic;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Holds FreeOrion hull types */
class HullTypeManager {
public:
    typedef std::map<std::string, HullType*>::const_iterator iterator;

    /** \name Accessors */ //@{
    /** returns the hull type with the name \a name; you should use the free function GetHullType() instead */
    const HullType* GetHullType(const std::string& name) const;

    /** iterator to the first hull type */
    iterator begin() const;

    /** iterator to the last + 1th hull type */
    iterator end() const;

    /** returns the instance of this singleton class; you should use the free function GetHullTypeManager() instead */
    static const HullTypeManager& GetHullTypeManager();
    //@}

private:
    HullTypeManager();
    ~HullTypeManager();

    std::map<std::string, HullType*> m_hulls;
    static HullTypeManager* s_instance;
};

/** returns the singleton hull type manager */
const HullTypeManager& GetHullTypeManager();

/** Returns the ship HullType specification object with name \a name.  If no such HullType exists,
    0 is returned instead. */
const HullType* GetHullType(const std::string& name);


class ShipDesign {
public:
    /** \name Structors */ //@{
    ShipDesign(); ///< default ctor
    ShipDesign(const std::string& name, const std::string& description, int designed_by_empire_id,
               int designed_on_turn, const std::string& hull, const std::vector<std::string>& parts,
               const std::string& graphic, const std::string& model);
    //@}

    /** \name Accessors */ //@{
    int                             ID() const;                 ///< returns id number of design
    const std::string&              Name() const;               ///< returns name of design
    const std::string&              Description() const;        ///< returns description of design
    int                             DesignedByEmpire() const;   ///< returns id of empire that created design
    int                             DesignedOnTurn() const;     ///< returns turn on which design was created

    double                          StarlaneSpeed() const;      ///< returns design speed along starlanes
    double                          Speed() const;              ///< returns design speed on the battle map

    bool                            CanColonize() const;
    bool                            IsArmed() const;

    /////// TEMPORARY ///////
    double      Defense() const;
    double      Attack() const;
    double      Cost() const;
    int         BuildTime() const;
    /////// TEMPORARY ///////

    const std::string&              Hull() const;                           ///< returns name of hull on which design is based
    const HullType*                 GetHull() const;                        ///< returns HullType on which design is based

    const std::vector<std::string>& Parts() const;                          ///< returns vector of names of all parts in design
    std::vector<std::string>        Parts(ShipSlotType slot_type) const;    ///< returns vector of names of parts in slots of indicated type

    const std::string&              Graphic() const;                        ///< returns filename of graphic for design
    const std::string&              Model() const;                          ///< returns filename of 3D model that represents ships of design
    //@}

    bool                            ProductionLocation(int empire_id, int location_id) const;   ///< returns true iff the empire with ID empire_id can produce this design at the location with location_id

    /** \name Mutators */ //@{
    void                            SetID(int id);                          ///< sets the ID number of the design to \a id .  Should only be used by Universe class when inserting new design into Universe.
    void                            Rename(const std::string& name);        ///< renames this design to \a name
    //@}

    ///< returns true if the \a hull and parts vectors passed make a valid ShipDesign
    static bool                     ValidDesign(const std::string& hull, const std::vector<std::string>& parts);

    ///< returns true if the \a design passed is a valid ShipDesign in terms of its hull and parts.  does not check any other member variables
    static bool                     ValidDesign(const ShipDesign& design);

private:
    int                         m_id;

    std::string                 m_name;
    std::string                 m_description;

    int                         m_designed_by_empire_id;
    int                         m_designed_on_turn;

    std::string                 m_hull;
    std::vector<std::string>    m_parts;

    std::string                 m_graphic;
    std::string                 m_3D_model;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Returns the ShipDesign specification object with id \a ship_design_id.  If no such ShipDesign
    is present in the Universe (because it doesn't exist, or isn't know to this client), 0 is
    returned instead. */
const ShipDesign* GetShipDesign(int ship_design_id);


// template implementations
template <class Archive>
void PartType::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_NVP(m_name)
        & BOOST_SERIALIZATION_NVP(m_description)
        & BOOST_SERIALIZATION_NVP(m_class)
        & BOOST_SERIALIZATION_NVP(m_stats)
        & BOOST_SERIALIZATION_NVP(m_cost)
        & BOOST_SERIALIZATION_NVP(m_build_time)
        & BOOST_SERIALIZATION_NVP(m_mountable_slot_types)
        & BOOST_SERIALIZATION_NVP(m_location)
        & BOOST_SERIALIZATION_NVP(m_effects)
        & BOOST_SERIALIZATION_NVP(m_graphic);
}

template <class Archive>
void HullType::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_NVP(m_name)
        & BOOST_SERIALIZATION_NVP(m_description)
        & BOOST_SERIALIZATION_NVP(m_speed)
        & BOOST_SERIALIZATION_NVP(m_starlane_speed)
        & BOOST_SERIALIZATION_NVP(m_fuel)
        & BOOST_SERIALIZATION_NVP(m_cost)
        & BOOST_SERIALIZATION_NVP(m_build_time)
        & BOOST_SERIALIZATION_NVP(m_slots)
        & BOOST_SERIALIZATION_NVP(m_location)
        & BOOST_SERIALIZATION_NVP(m_effects)
        & BOOST_SERIALIZATION_NVP(m_graphic);
}

template <class Archive>
void ShipDesign::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_NVP(m_id)
        & BOOST_SERIALIZATION_NVP(m_name)
        & BOOST_SERIALIZATION_NVP(m_description)
        & BOOST_SERIALIZATION_NVP(m_designed_by_empire_id)
        & BOOST_SERIALIZATION_NVP(m_designed_on_turn)
        & BOOST_SERIALIZATION_NVP(m_hull)
        & BOOST_SERIALIZATION_NVP(m_parts)
        & BOOST_SERIALIZATION_NVP(m_graphic)
        & BOOST_SERIALIZATION_NVP(m_3D_model);
}

#endif // _ShipDesign_h_
