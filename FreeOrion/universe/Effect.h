// -*- C++ -*-
#ifndef _Effect_h_
#define _Effect_h_

#include <set>
#include <vector>

#include "Enums.h"

#include <boost/shared_ptr.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>

class UniverseObject;

namespace Condition {
    struct ConditionBase;
    typedef std::set<const UniverseObject*> ObjectSet;
}

namespace Effect {
    class EffectsGroup;
    class EffectBase;
    class SetMeter;
    class SetShipPartMeter;
    class SetEmpireMeter;
    class SetEmpireStockpile;
    class SetEmpireCapitol;
    class SetPlanetType;
    class SetPlanetSize;
    class SetSpecies;
    class AddOwner;
    class RemoveOwner;
    class CreatePlanet;
    class CreateBuilding;
    class CreateShip;
    class Destroy;
    class AddSpecial;
    class RemoveSpecial;
    class SetStarType;
    class SetTechAvailability;
    class MoveTo;
    class Victory;
    class GenerateSitRepMessage;

    typedef std::set<UniverseObject*> TargetSet;
}

namespace ValueRef {
    template <class T>
    struct ValueRefBase;
}

/** Contains one or more Effects, a Condition which indicates the objects in
  * the scope of the Effect(s), and a Condition which indicates whether or not
  * the Effect(s) will be executed on the objects in scope during the current
  * turn.  Since Conditions operate on sets of objects (usually all objects in
  * the universe), the activation condition bears some explanation.  It exists
  * to allow an EffectsGroup to be activated or suppressed based on the source
  * object only (the object to which the EffectsGroup is attached).  It does
  * this by considering the "universe" containing only the source object. If
  * the source object meets the activation condition, the EffectsGroup will be
  * active in the current turn. */
class Effect::EffectsGroup
{
public:
    struct Description
    {
        std::string scope_description;
        std::string activation_description;
        std::vector<std::string> effect_descriptions;
    };

    EffectsGroup(const Condition::ConditionBase* scope, const Condition::ConditionBase* activation,
                 const std::vector<EffectBase*>& effects, const std::string& stacking_group = "");
    virtual ~EffectsGroup();

    void                            GetTargetSet(int source_id, TargetSet& targets) const;
    void                            GetTargetSet(int source_id, TargetSet& targets, const TargetSet& potential_targets) const;
    void                            Execute(int source_id, const TargetSet& targets) const;                    // execute all effects in group
    void                            Execute(int source_id, const TargetSet& targets, int effect_index) const;  // execute effect with \a effect_index (but not other effects)
    const std::string&              StackingGroup() const;
    const std::vector<EffectBase*>& EffectsList() const;
    Description                     GetDescription() const;
    std::string                     DescriptionString() const;
    std::string                     Dump() const;

protected:
    const Condition::ConditionBase* m_scope;
    const Condition::ConditionBase* m_activation;
    std::string                     m_stacking_group;
    std::string                     m_explicit_description;
    std::vector<EffectBase*>        m_effects;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Returns a single string which describes a vector of EffectsGroups. */
std::string EffectsDescription(const std::vector<boost::shared_ptr<const Effect::EffectsGroup> >& effects_groups);

/** The base class for all Effects.  When an Effect is executed, the source
  * object (the object to which the Effect or its containing EffectGroup is
  * attached) and the target object are both required.  Note that this means
  * that ValueRefs contained within Effects can refer to values in either the
  * source or target objects. */
class Effect::EffectBase
{
public:
    virtual ~EffectBase();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const = 0;
    virtual std::string Description() const = 0;
    virtual std::string Dump() const = 0;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the meter of the given kind to \a value.  The max value of the meter
  * is set if \a max == true; otherwise the current value of the meter is set.
  * If the target of the Effect does not have the requested meter, nothing is
  * done. */
class Effect::SetMeter : public Effect::EffectBase
{
public:
    SetMeter(MeterType meter, const ValueRef::ValueRefBase<double>* value);
    virtual ~SetMeter();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;
    MeterType GetMeterType() const {return m_meter;};

private:
    MeterType                             m_meter;
    const ValueRef::ValueRefBase<double>* m_value;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the indicated meter on all ship parts in the indicated subset.  This
  * has no effect on non-Ship targets.  If slot_type is specified, only parts
  * that can mount in the indicated slot type (internal or external) are
  * affected (this is not the same at the slot type in which the part is
  * actually located, as a part might be mountable in both types, and
  * located in a different type than specified, and would be matched). */
class Effect::SetShipPartMeter : public Effect::EffectBase
{
public:
    /** Affects the \a meter_type meters that belong to all parts of class \a
        part_class.  \a part_class must specify a class of parts for which
        there is a ship meter (i.e. specifying PC_FUEL would be illegal). */
    SetShipPartMeter(MeterType meter_type,
                     ShipPartClass part_class,
                     const ValueRef::ValueRefBase<double>* value,
                     ShipSlotType slot_type = INVALID_SHIP_SLOT_TYPE);

    /** Affects the \a meter_type meters that belong to all PC_FIGHTERS parts
        of type \a fighter_type. */
    SetShipPartMeter(MeterType meter_type,
                     CombatFighterType fighter_type,
                     const ValueRef::ValueRefBase<double>* value,
                     ShipSlotType slot_type = INVALID_SHIP_SLOT_TYPE);

    /** Affects the \a meter_type meters that belong to all parts named \a
        part_name. */
    SetShipPartMeter(MeterType meter_type,
                     const std::string& part_name,
                     const ValueRef::ValueRefBase<double>* value,
                     ShipSlotType slot_type = INVALID_SHIP_SLOT_TYPE);

    virtual ~SetShipPartMeter();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    ShipPartClass                         m_part_class;
    CombatFighterType                     m_fighter_type;
    std::string                           m_part_name;
    ShipSlotType                          m_slot_type;
    MeterType                             m_meter;
    const ValueRef::ValueRefBase<double>* m_value;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the indicated meter on the empire with the indicated id to the
  * indicated value.  If \a meter is not a valid meter for empires,
  * does nothing. */
class Effect::SetEmpireMeter : public Effect::EffectBase
{
public:
    SetEmpireMeter(const std::string& meter, const ValueRef::ValueRefBase<double>* value);
    SetEmpireMeter(const ValueRef::ValueRefBase<int>* empire_id, const std::string& meter, const ValueRef::ValueRefBase<double>* value);
    virtual ~SetEmpireMeter();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<int>*      m_empire_id;
    const std::string                       m_meter;
    const ValueRef::ValueRefBase<double>*   m_value;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the empire stockpile of the target's owning empire to \a value.  If
  * the target does not have exactly one owner, nothing is done. */
class Effect::SetEmpireStockpile : public Effect::EffectBase
{
public:
    SetEmpireStockpile(ResourceType stockpile, const ValueRef::ValueRefBase<double>* value);
    SetEmpireStockpile(const ValueRef::ValueRefBase<int>* empire_id, ResourceType stockpile, const ValueRef::ValueRefBase<double>* value);
    virtual ~SetEmpireStockpile();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<int>*      m_empire_id;
    ResourceType                            m_stockpile;
    const ValueRef::ValueRefBase<double>*   m_value;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Makes the target planet the capitol of its owner's empire.  If the target
  * object is not a planet, does not have an owner, or has more than one owner
  * the effect does nothing. */
class Effect::SetEmpireCapitol : public Effect::EffectBase
{
public:
    SetEmpireCapitol();
    SetEmpireCapitol(const ValueRef::ValueRefBase<int>* empire_id);
    virtual ~SetEmpireCapitol();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<int>*      m_empire_id;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the planet type of the target to \a type.  This has no effect on non-Planet targets.  Note that changing the
    type of a PT_ASTEROID or PT_GASGIANT planet will also change its size to SZ_TINY or SZ_HUGE, respectively.
    Similarly, changing type to PT_ASTEROID or PT_GASGIANT will also cause the size to change to SZ_ASTEROID or
    SZ_GASGIANT, respectively. */
class Effect::SetPlanetType : public Effect::EffectBase
{
public:
    SetPlanetType(const ValueRef::ValueRefBase<PlanetType>* type);
    virtual ~SetPlanetType();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<PlanetType>* m_type;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the planet size of the target to \a size.  This has no effect on non-
  * Planet targets.  Note that changing the size of a PT_ASTEROID or PT_GASGIANT
  * planet will also change its type to PT_BARREN.  Similarly, changing size to
  * SZ_ASTEROID or SZ_GASGIANT will also cause the type to change to PT_ASTEROID
  * or PT_GASGIANT, respectively. */
class Effect::SetPlanetSize : public Effect::EffectBase
{
public:
    SetPlanetSize(const ValueRef::ValueRefBase<PlanetSize>* size);
    virtual ~SetPlanetSize();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<PlanetSize>* m_size;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the species on the target to \a species_name.  This works on planets
  * and ships, but has no effect on other objects. */
class Effect::SetSpecies : public Effect::EffectBase
{
public:
    SetSpecies(const ValueRef::ValueRefBase<std::string>* species);
    virtual ~SetSpecies();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<std::string>* m_species_name;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Adds empire \a empire_id as an owner of the target.  This has no effect if
  * \a empire_id was already an owner of the target object. */
class Effect::AddOwner : public Effect::EffectBase
{
public:
    AddOwner(const ValueRef::ValueRefBase<int>* empire_id);
    virtual ~AddOwner();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<int>* m_empire_id;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Removes empire \a empire_id as an owner of the target.  This has no effect
  * if \a empire_id was not already an owner of the target object. */
class Effect::RemoveOwner : public Effect::EffectBase
{
public:
    RemoveOwner(const ValueRef::ValueRefBase<int>* empire_id);
    virtual ~RemoveOwner();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<int>* m_empire_id;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Creates a new Planet with specified \a type and \a size at the system with
  * specified \a location_id */
class Effect::CreatePlanet : public Effect::EffectBase
{
public:
    CreatePlanet(const ValueRef::ValueRefBase<PlanetType>* type,
                 const ValueRef::ValueRefBase<PlanetSize>* size);
    virtual ~CreatePlanet();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;
private:
    const ValueRef::ValueRefBase<PlanetType>*   m_type;
    const ValueRef::ValueRefBase<PlanetSize>*   m_size;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Creates a new Building with specified \a type on the \a target Planet. */
class Effect::CreateBuilding : public Effect::EffectBase
{
public:
    CreateBuilding(const ValueRef::ValueRefBase<std::string>* building_type_name);
    virtual ~CreateBuilding();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;
private:
    const ValueRef::ValueRefBase<std::string>*  m_building_type_name;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Creates a new Ship with specified \a predefined_ship_design_name design
  * from those in the list of PredefinedShipDesignManager, and owned by the
  * empire with the specified \a empire_id */
class Effect::CreateShip : public Effect::EffectBase
{
public:
    CreateShip(const std::string& predefined_ship_design_name,
               const ValueRef::ValueRefBase<int>* empire_id,
               const ValueRef::ValueRefBase<std::string>* species_name);
    CreateShip(const ValueRef::ValueRefBase<int>* ship_design_id,
               const ValueRef::ValueRefBase<int>* empire_id,
               const ValueRef::ValueRefBase<std::string>* species_name);
    virtual ~CreateShip();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;
private:
    const std::string                           m_design_name;
    const ValueRef::ValueRefBase<int>*          m_design_id;
    const ValueRef::ValueRefBase<int>*          m_empire_id;
    const ValueRef::ValueRefBase<std::string>*  m_species_name;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Destroys the target object.  When executed on objects that contain other
  * objects (such as Fleets and Planets), all contained objects are destroyed
  * as well.  Destroy effects delay the desctruction of their targets until
  * after other all effects have executed, to ensure the source or target of
  * other effects are present when they execute. */
class Effect::Destroy : public Effect::EffectBase
{
public:
    Destroy();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Adds the Special with the name \a name to the target object. */
class Effect::AddSpecial : public Effect::EffectBase
{
public:
    AddSpecial(const std::string& name);

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    std::string m_name;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Removes the Special with the name \a name to the target object.  This has
  * no effect if no such Special was already attached to the target object. */
class Effect::RemoveSpecial : public Effect::EffectBase
{
public:
    RemoveSpecial(const std::string& name);

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    std::string m_name;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the star type of the target to \a type.  This has no effect on
  * non-System targets. */
class Effect::SetStarType : public Effect::EffectBase
{
public:
    SetStarType(const ValueRef::ValueRefBase<StarType>* type);
    virtual ~SetStarType();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const ValueRef::ValueRefBase<StarType>* m_type;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Moves an UniverseObject to a location of another UniverseObject with id
  * \a object_id.  If there are are no objects id that id, nothing is done. */
class Effect::MoveTo : public Effect::EffectBase
{
public:
    MoveTo(const Condition::ConditionBase* location_condition);
    virtual ~MoveTo();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    const Condition::ConditionBase* m_location_condition;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Causes the owner empire of the target object to win the game.  If the
  * target object has multiple owners, nothing is done. */
class Effect::Victory : public Effect::EffectBase
{
public:
    Victory(const std::string& reason_string);

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    std::string m_reason_string;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Sets the availability of tech \a tech_name to empire \a empire_id.  If
  * \a include_tech is true, the tech is fully available, just as if it were
  * researched normally; otherwise, only the items that the tech includes are
  * made available.  Note that this means this Effect is intended also to
  * be used to unlock buildings, ships, etc.  The tech and/or its items are
  * made available if \a available is true, or unavailable otherwise. */
class Effect::SetTechAvailability : public Effect::EffectBase
{
public:
    SetTechAvailability(const std::string& tech_name,
                        const ValueRef::ValueRefBase<int>* empire_id,
                        bool available,
                        bool include_tech);
    virtual ~SetTechAvailability();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    std::string                        m_tech_name;
    const ValueRef::ValueRefBase<int>* m_empire_id;
    bool                               m_available;
    bool                               m_include_tech;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

/** Generates a sitrep message for the empire with id \a recipient_empire_id.
  * The message text is the user string specified in \a message_string with
  * string substitutions into the message text as specified in \a message_parameters
  * which are substituted as string parameters %1%, %2%, %3%, etc. in the order
  * they are specified.  Extra parameters beyond those needed by \a message_string
  * are ignored, and missing parameters are left as blank text. */
class Effect::GenerateSitRepMessage : public Effect::EffectBase
{
public:
    GenerateSitRepMessage(const std::string& message_string,
                          const std::vector<std::pair<std::string, const ValueRef::ValueRefBase<std::string>*> >& message_parameters,
                          const ValueRef::ValueRefBase<int>* recipient_empire_id);
    virtual ~GenerateSitRepMessage();

    virtual void        Execute(const UniverseObject* source, UniverseObject* target) const;
    virtual std::string Description() const;
    virtual std::string Dump() const;

private:
    std::string                                         m_message_string;
    std::vector<std::pair<std::string,
        const ValueRef::ValueRefBase<std::string>*> >   m_message_parameters;
    const ValueRef::ValueRefBase<int>*                  m_recipient_empire_id;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};


// template implementations
template <class Archive>
void Effect::EffectsGroup::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_NVP(m_scope)
        & BOOST_SERIALIZATION_NVP(m_activation)
        & BOOST_SERIALIZATION_NVP(m_stacking_group)
        & BOOST_SERIALIZATION_NVP(m_explicit_description)
        & BOOST_SERIALIZATION_NVP(m_effects);
}

template <class Archive>
void Effect::EffectBase::serialize(Archive& ar, const unsigned int version)
{}

template <class Archive>
void Effect::SetMeter::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_meter)
        & BOOST_SERIALIZATION_NVP(m_value);
}

template <class Archive>
void Effect::SetShipPartMeter::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_part_class)
        & BOOST_SERIALIZATION_NVP(m_fighter_type)
        & BOOST_SERIALIZATION_NVP(m_part_name)
        & BOOST_SERIALIZATION_NVP(m_slot_type)
        & BOOST_SERIALIZATION_NVP(m_meter)
        & BOOST_SERIALIZATION_NVP(m_value);
}

template <class Archive>
void Effect::SetEmpireMeter::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_empire_id)
        & BOOST_SERIALIZATION_NVP(m_meter)
        & BOOST_SERIALIZATION_NVP(m_value);
}

template <class Archive>
void Effect::SetEmpireStockpile::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_empire_id)
        & BOOST_SERIALIZATION_NVP(m_stockpile)
        & BOOST_SERIALIZATION_NVP(m_value);
}

template <class Archive>
void Effect::SetEmpireCapitol::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_empire_id);
}

template <class Archive>
void Effect::SetPlanetType::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_type);
}

template <class Archive>
void Effect::SetPlanetSize::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_size);
}

template <class Archive>
void Effect::SetSpecies::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_species_name);
}

template <class Archive>
void Effect::AddOwner::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_empire_id);
}

template <class Archive>
void Effect::RemoveOwner::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_empire_id);
}

template <class Archive>
void Effect::CreatePlanet::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_type)
        & BOOST_SERIALIZATION_NVP(m_size);
}

template <class Archive>
void Effect::CreateBuilding::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_building_type_name);
}

template <class Archive>
void Effect::CreateShip::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_design_name)
        & BOOST_SERIALIZATION_NVP(m_empire_id);
}

template <class Archive>
void Effect::Destroy::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase);
}

template <class Archive>
void Effect::AddSpecial::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_name);
}

template <class Archive>
void Effect::RemoveSpecial::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_name);
}

template <class Archive>
void Effect::SetStarType::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_type);
}

template <class Archive>
void Effect::MoveTo::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_location_condition);
}

template <class Archive>
void Effect::Victory::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_reason_string);
}

template <class Archive>
void Effect::SetTechAvailability::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_tech_name)
        & BOOST_SERIALIZATION_NVP(m_empire_id)
        & BOOST_SERIALIZATION_NVP(m_available)
        & BOOST_SERIALIZATION_NVP(m_include_tech);
}

template <class Archive>
void Effect::GenerateSitRepMessage::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(EffectBase)
        & BOOST_SERIALIZATION_NVP(m_message_string)
        & BOOST_SERIALIZATION_NVP(m_message_parameters)
        & BOOST_SERIALIZATION_NVP(m_recipient_empire_id);
}

#endif // _Effect_h_
