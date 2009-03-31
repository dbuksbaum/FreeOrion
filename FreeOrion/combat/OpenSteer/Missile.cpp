#include "Missile.h"

#include "PathingEngine.h"


#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif


Missile::Missile() :
    m_proximity_token(0),
    m_empire_id(-1),
    m_last_steer(),
    m_stats(),
    m_destination(),
    m_target(),
    m_health(0.0),
    m_pathing_engine()
{}

Missile::Missile(int empire_id, const PartType& part, CombatObjectPtr target,
                 const OpenSteer::Vec3& position, const OpenSteer::Vec3& direction,
                 PathingEngine& pathing_engine) :
    m_proximity_token(0),
    m_empire_id(empire_id),
    m_last_steer(),
    m_stats(boost::get<LRStats>(part.Stats())),
    m_destination(target->position()),
    m_target(target),
    m_health(m_stats.m_health),
    m_pathing_engine(&pathing_engine)
{ Init(position, direction); }

Missile::~Missile()
{ delete m_proximity_token; }

double Missile::HealthAndShield() const
{ return m_health; }

double Missile::Health() const
{ return m_health; }

double Missile::FractionalHealth() const
{ return 1.0; }

double Missile::AntiFighterStrength() const
{ return 0.0; }

double Missile::AntiShipStrength(CombatShipPtr target/* = CombatShipPtr()*/) const
{ return 0.0; }

void Missile::update(const float /*current_time*/, const float elapsed_time)
{
    OpenSteer::Vec3 steer = m_last_steer;
    if (m_pathing_engine->UpdateNumber() % PathingEngine::UPDATE_SETS ==
        serialNumber % PathingEngine::UPDATE_SETS) {
        const float AT_DESTINATION = speed();
        const float AT_DEST_SQUARED = AT_DESTINATION * AT_DESTINATION;
        float distance_squared = (m_destination - position()).lengthSquared();
        CombatObjectPtr target = m_target.lock();
        if (distance_squared < AT_DEST_SQUARED) {
            if (target)
                target->Damage(m_stats.m_damage, NON_PD_DAMAGE);
            // TODO: Remove self from engine.
            return;
        } else {
            if (target)
                m_destination = target->position();
        }
        steer = Steer();
    }
    applySteeringForce(steer, elapsed_time);
    m_last_steer = steer;
    m_proximity_token->UpdatePosition(position());
}

void Missile::regenerateLocalSpace(const OpenSteer::Vec3& newVelocity,
                                   const float elapsedTime)
{}

void Missile::Damage(double d, DamageSource source)
{
    if (source == NON_PD_DAMAGE)
        d *= CombatShip::NON_PD_VS_FIGHTER_FACTOR;
    m_health = std::max(0.0, m_health - d);
}

void Missile::Damage(const CombatFighterPtr& source)
{ assert(!"Missiles can't attack fighters."); }

void Missile::Init(const OpenSteer::Vec3& position_, const OpenSteer::Vec3& direction)
{
    m_proximity_token =
        m_pathing_engine->GetProximityDB().Insert(
            this, MISSILE_FLAG, EmpireFlag(m_empire_id));

    SimpleVehicle::reset();
    SimpleVehicle::setMaxForce(9.0 * 18.0);
    SimpleVehicle::setMaxSpeed(m_stats.m_speed);

    // TODO: setMass()

    SimpleVehicle::regenerateOrthonormalBasis(direction, OpenSteer::Vec3(0, 0, 1));

    SimpleVehicle::setPosition(position_);
    SimpleVehicle::setSpeed(0);

    m_proximity_token->UpdatePosition(position());
}

OpenSteer::Vec3 Missile::Steer()
{
    const float OBSTACLE_AVOIDANCE_TIME = 2.0;

    const OpenSteer::Vec3 avoidance =
        steerToAvoidObstacles(OBSTACLE_AVOIDANCE_TIME,
                              m_pathing_engine->Obstacles().begin(),
                              m_pathing_engine->Obstacles().end());

    if (avoidance != OpenSteer::Vec3::zero)
        return avoidance;

    return (m_destination - position()).normalize();
}
