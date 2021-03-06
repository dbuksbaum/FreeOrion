// -*- C++ -*-
#ifndef _Missile_h_
#define _Missile_h_

#include "PathingEngineFwd.h"

#include "CombatObject.h"
#include "../../universe/ShipDesign.h"


class Ship;

class Missile :
    public CombatObject
{
public:
    Missile(const Ship& launcher, const PartType& part, CombatObjectPtr target,
            const OpenSteer::Vec3& position, const OpenSteer::Vec3& direction,
            PathingEngine& pathing_engine);
    ~Missile();

    const LRStats& Stats() const;
    const std::string& PartName() const;
    virtual double StructureAndShield() const;
    virtual double Structure() const;
    virtual double FractionalStructure() const;
    virtual double AntiFighterStrength() const;
    virtual double AntiShipStrength(CombatShipPtr target = CombatShipPtr()) const;
    virtual bool IsFighter() const;
    virtual bool IsShip() const;
    virtual int Owner() const;

    virtual void update(const float elapsed_time, bool force);
    virtual void regenerateLocalSpace(const OpenSteer::Vec3& newVelocity,
                                      const float elapsedTime);

    virtual void Damage(double d, DamageSource source);
    virtual void Damage(const CombatFighterPtr& source);
    virtual void TurnStarted(unsigned int number);
    virtual void SignalDestroyed();

    void SetWeakPtr(const MissilePtr& ptr);
    MissilePtr shared_from_this();

private:
    Missile();

    void Init(const Ship& launcher,
              const OpenSteer::Vec3& position_,
              const OpenSteer::Vec3& direction);
    OpenSteer::Vec3 Steer();

    ProximityDBToken* m_proximity_token;
    int m_empire_id;
    std::string m_part_name;
    OpenSteer::Vec3 m_last_steer;
    OpenSteer::Vec3 m_destination; // Only the X and Y values should be nonzero.
    CombatObjectWeakPtr m_target;
    double m_structure;
    LRStats m_stats;
    PathingEngine* m_pathing_engine;

    boost::weak_ptr<Missile> m_weak_ptr;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version);
};

#endif
