// -*- C++ -*-
#ifndef _CombatFighter_h_
#define _CombatFighter_h_

#include "PathingEngineFwd.h"

#include "SimpleVehicle.h"
#include "../CombatOrder.h"

#include <boost/enable_shared_from_this.hpp>

#include <list>


class PathingEngine;

enum CombatFighterType {
    /** A fighter that is better at attacking other fighters than at
        attacking ships. */
    INTERCEPTOR,

    /** A fighter that is better at attacking ships than at attacking
        other fighters. */
    BOMBER
};

class CombatFighterFormation
{
public:
    typedef std::list<CombatFighterPtr>::const_iterator const_iterator;

    explicit CombatFighterFormation(PathingEngine& pathing_engine);
    ~CombatFighterFormation();

    const CombatFighter& Leader() const;
    OpenSteer::Vec3 Centroid() const;
    bool empty() const;
    std::size_t size() const;
    const_iterator begin() const;
    const_iterator end() const;

    void SetLeader(const CombatFighterPtr& fighter);
    void push_back(const CombatFighterPtr& fighter);
    void erase(const CombatFighterPtr& fighter);
    void erase(CombatFighter* fighter);

private:
    CombatFighterPtr m_leader;
    std::list<CombatFighterPtr> m_members;
    PathingEngine& m_pathing_engine;
};

class CombatFighter :
    public OpenSteer::SimpleVehicle,
    public boost::enable_shared_from_this<CombatFighter>
{
public:
    static const int FORMATION_SIZE = 5;

    CombatFighter(CombatObjectPtr base, CombatFighterType type, int empire_id,
                  int fighter_id, PathingEngine& pathing_engine,
                  const CombatFighterFormationPtr& formation,
                  int formation_position);
    CombatFighter(CombatObjectPtr base, CombatFighterType type, int empire_id,
                  int fighter_id, PathingEngine& pathing_engine,
                  const CombatFighterFormationPtr& formation);
    ~CombatFighter();

    virtual float maxForce() const;
    virtual float maxSpeed() const;
    int ID() const;
    const FighterMission& CurrentMission() const;

    virtual void update(const float /*current_time*/, const float elapsed_time);
    virtual void regenerateLocalSpace(const OpenSteer::Vec3& newVelocity, const float elapsedTime);

    CombatFighterFormationPtr Formation();

    void AppendMission(const FighterMission& mission);
    void ClearMissions();

private:
    void Init();
    OpenSteer::Vec3 GlobalFormationPosition();
    void RemoveMission();
    void UpdateMissionQueue();
    OpenSteer::Vec3 Steer();
    CombatObjectPtr WeakestAttacker(const CombatObjectPtr& attackee);
    CombatShipPtr WeakestHostileShip();

    ProximityDBToken* m_proximity_token;
    bool m_leader;
    CombatFighterType m_type;
    int m_empire_id;
    int m_id;
    OpenSteer::Vec3 m_last_steer;

    std::list<FighterMission> m_mission_queue;
    float m_mission_weight;
    OpenSteer::Vec3 m_mission_destination; // Only the X and Y values should be nonzero.
    CombatObjectPtr m_mission_subtarget;
    CombatObjectWeakPtr m_base;

    int m_formation_position;
    CombatFighterFormationPtr m_formation;
    OpenSteer::Vec3 m_out_of_formation;

    PathingEngine& m_pathing_engine;

    // TODO: Temporary only!
    bool m_instrument;
    FighterMission::Type m_last_mission;
};

#endif
