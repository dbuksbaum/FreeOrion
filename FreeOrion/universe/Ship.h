// -*- C++ -*-
#ifndef _Ship_h_
#define _Ship_h_

#ifndef _UniverseObject_h_
#include "UniverseObject.h"
#endif

class Fleet;
class ShipDesign;

/** a class representing a single FreeOrion ship*/
class Ship : public UniverseObject
{
public:
    /** \name Structors */ //@{
    Ship(); ///< default ctor
    Ship(int empire_id, const std::string& design_name); ///< general ctor taking just the ship's empire and design name; from this the design can be looked up and used to create the ship
    Ship(const XMLElement& elem); ///< ctor that constructs a Ship object from an XMLElement. \throw std::invalid_argument May throw std::invalid_argument if \a elem does not encode a Ship object
    //@}

    /** \name Accessors */ //@{
    const  ShipDesign* Design() const; ///< returns the design of the ship, containing engine type, weapons, etc.
    int    FleetID() const;            ///< returns the ID of the fleet the ship is residing in
    Fleet* GetFleet() const;           ///< returns the ID of the fleet the ship is residing in

    virtual UniverseObject::Visibility GetVisibility(int empire_id) const;
    virtual const std::string& PublicName(int empire_id) const;
    virtual XMLElement XMLEncode(int empire_id = Universe::ALL_EMPIRES) const;

    bool IsArmed() const;

    virtual UniverseObject* Accept(const UniverseObjectVisitor& visitor) const;
    //@}

    /** \name Mutators */ //@{   
    void         SetFleetID(int fleet_id) {m_fleet_id = fleet_id; StateChangedSignal();} ///< sets the ID of the fleet the ship resides in
    virtual void MovementPhase();
    virtual void PopGrowthProductionResearchPhase();
    //@}

private:
    std::string m_design_name;
    int         m_fleet_id;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

// template implementations
template <class Archive>
void Ship::serialize(Archive& ar, const unsigned int version)
{
    ar  & BOOST_SERIALIZATION_BASE_OBJECT_NVP(UniverseObject)
        & BOOST_SERIALIZATION_NVP(m_design_name)
        & BOOST_SERIALIZATION_NVP(m_fleet_id);
}

#endif // _Ship_h_


