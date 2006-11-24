#include "ResourcePool.h"

#include "../universe/Planet.h"
#include "../util/AppInterface.h"
#include "../util/MultiplayerCommon.h"

#include <GG/SignalsAndSlots.h>

//////////////////////////////////////////////////
// ResourcePool
//////////////////////////////////////////////////
ResourcePool::ResourcePool(ResourceType type) :
    m_stockpile(0.0),
    m_max_stockpile(200.0), // change to 0.0 later when effects can alter the max stockpile
    m_production(0.0),
    m_type(type)
{}

ResourcePool::~ResourcePool()
{
    m_resource_centers.clear();
}

double ResourcePool::Stockpile() const
{
    return m_stockpile;
}

void ResourcePool::SetStockpile(double d)
{
    // allow m_stockpile to be < 0.0 or > m_max_stockpile for now.  will clamp during turn processing at last step.
    m_stockpile = d;
}

double ResourcePool::MaxStockpile() const
{   
    // need to ensure this is calculated from any effects...
    return m_max_stockpile;
}

void ResourcePool::SetMaxStockpile(double d)
{
    m_max_stockpile = std::min(0.0, d);
}

double ResourcePool::Production() const
{
    return m_production;
}

double ResourcePool::Available() const
{
    return m_production + m_stockpile;
}

void ResourcePool::SetResourceCenters(const std::vector<ResourceCenter*>& resource_center_vec)
{
    m_resource_centers = resource_center_vec;
}

void ResourcePool::Update()
{
    m_production = 0.0;

    // sum production from all ResourceCenters for resource point type appropriate for this pool
    switch (m_type)
    {
    case RE_FOOD:
        for(std::vector<ResourceCenter*>::const_iterator it = ResourceCenters().begin(); it != ResourceCenters().end(); ++it)
            m_production += (*it)->FarmingPoints();
        break;
    case RE_INDUSTRY:
        for(std::vector<ResourceCenter*>::const_iterator it = ResourceCenters().begin(); it != ResourceCenters().end(); ++it)
            m_production += (*it)->IndustryPoints();
        break;
    case RE_MINERALS:
        for(std::vector<ResourceCenter*>::const_iterator it = ResourceCenters().begin(); it != ResourceCenters().end(); ++it)
            m_production += (*it)->MiningPoints();
        break;
    case RE_RESEARCH:
        for(std::vector<ResourceCenter*>::const_iterator it = ResourceCenters().begin(); it != ResourceCenters().end(); ++it)
            m_production += (*it)->ResearchPoints();
        break;
    case RE_TRADE:
        for(std::vector<ResourceCenter*>::const_iterator it = ResourceCenters().begin(); it != ResourceCenters().end(); ++it)
            m_production += (*it)->TradePoints();
        break;
    default:
        throw std::runtime_error("ResourceCenterChanged was called without a valid m_type.");
    }
    ChangedSignal();
}

//////////////////////////////////////////////////
// PopulationPool
//////////////////////////////////////////////////
namespace {
    bool PopCenterLess(PopCenter* elem1, PopCenter* elem2)
	{
	    return elem1->PopPoints() < elem2->PopPoints();
	}
}

PopulationPool::PopulationPool() :
    m_population(0.0),
    m_growth(0.0)
{}

PopulationPool::~PopulationPool()
{
    m_pop_centers.clear();
}

double PopulationPool::Population() const
{
    return m_population;
}

double PopulationPool::Growth() const
{
    return m_growth;
}

void PopulationPool::SetPopCenters(const std::vector<PopCenter*>& pop_center_vec)
{
    m_pop_centers = pop_center_vec;
    std::sort(m_pop_centers.begin(), m_pop_centers.end(), &PopCenterLess);  // this ordering ensures higher population PopCenters get first priority for food distribution
}

void PopulationPool::Update()
{
    m_population = 0.0;
    m_growth = 0.0;
    // sum population from all PopCenters in this pool
    for (std::vector<PopCenter*>::const_iterator it = PopCenters().begin(); it != PopCenters().end(); ++it)
    {
        m_population += (*it)->PopPoints();
        m_growth += (*it)->FuturePopGrowth();
    }
    ChangedSignal();
}
