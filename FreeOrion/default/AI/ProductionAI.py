import freeOrionAIInterface as fo
import FreeOrionAI as foAI
from EnumsAI import AIExplorableSystemType, AIPriorityType, getAIPriorityResourceTypes, getAIPriorityProductionTypes, AIFocusType, AIFleetMissionType
import PlanetUtilsAI
import AIstate
from FleetUtilsAI import extractFleetIDsWithoutMissionTypes, getEmpireFleetIDsByRole

def generateProductionOrders():
    "generate production orders"

    print "Production Queue Management:"
    empire = fo.getEmpire()
    totalPP = empire.productionPoints
    print ""
    print "  Total Available Production Points: " + str(totalPP)

    totalPPSpent = fo.getEmpire().productionQueue.totalSpent
    print "  Total Production Points Spent:     " + str(totalPPSpent)

    wastedPP = totalPP - totalPPSpent
    print "  Wasted Production Points:          " + str(wastedPP)
    print ""

    colonisablePlanetIDs = AIstate.colonisablePlanetIDs
    for element in colonisablePlanetIDs:
        print "  Colonizable Planet ID, Score:             " + str(element)

    colonyFleetIDs = getEmpireFleetIDsByRole(AIFleetMissionType.FLEET_MISSION_COLONISATION)
    for element in colonyFleetIDs:
        print "  Colony Fleet ID:                          " + str(element)

    numColonyFleets = len(extractFleetIDsWithoutMissionTypes(colonyFleetIDs))
    print "  Number of Colony Fleets Without Missions: " + str(numColonyFleets)
    print ""

    print "Possible building types to build:"
    possibleBuildingTypes = empire.availableBuildingTypes
    for buildingTypeID in possibleBuildingTypes:
        buildingType = fo.getBuildingType(buildingTypeID)
        print "    " + str(buildingType.name) + " cost:" + str(buildingType.productionCost) + " time:" + str(buildingType.productionTime)

    print ""
    print "Possible ship designs to build:"
    possibleShipDesigns = empire.availableShipDesigns
    for shipDesignID in possibleShipDesigns:
        shipDesign = fo.getShipDesign(shipDesignID)
        print "    " + str(shipDesign.name(True)) + " cost:" + str(shipDesign.productionCost) + " time:" + str(shipDesign.productionTime)

    print ""
    print "Projects already in Production Queue:"
    productionQueue = empire.productionQueue
    for element in productionQueue:
        print "    " + element.name + " turns:" + str(element.turnsLeft) + " PP:" + str(element.allocation)

    print ""
    # get the highest production priorities
    print "Production Queue Priorities:"
    productionPriorities = {}
    for priorityType in getAIPriorityProductionTypes():
        productionPriorities[priorityType] = foAI.foAIstate.getPriority(priorityType)

    sortedPriorities = productionPriorities.items()
    sortedPriorities.sort(lambda x,y: cmp(x[1], y[1]), reverse=True)

    topPriority = -1
    for evaluationPair in sortedPriorities:
        if topPriority < 0:
            topPriority = evaluationPair[0]
        print "    ID|Score: " + str(evaluationPair)

    print "  Top Production Queue Priority: " + str(topPriority)

    locationIDs = getAvailableBuildLocations(shipDesignID)
    if len(locationIDs) > 0 and wastedPP > 0:
        for shipDesignID in possibleShipDesigns:
            shipDesign = fo.getShipDesign(shipDesignID)
            explorationShipName = "Scout"
            colonyShipName = "Colony Ship"
            outpostShipName = "Outpost Ship"
            troopShipName = "Troop Ship"
            if topPriority == 5 and shipDesign.name(True) == explorationShipName:
                # exploration ship
                print ""
                print "adding new ship to production queue: " + shipDesign.name(True)
                fo.issueEnqueueShipProductionOrder(shipDesignID, locationIDs[0])
            elif topPriority == 6 and shipDesign.canColonize and shipDesign.name(True) == outpostShipName:
                # outpost ship
                print ""
                print "adding new ship to production queue: " + shipDesign.name(True)
                fo.issueEnqueueShipProductionOrder(shipDesignID, locationIDs[0])
            elif topPriority == 7 and shipDesign.canColonize and shipDesign.name(True) == colonyShipName:
                # colony ship
                print ""
                print "adding new ship to production queue: " + shipDesign.name(True)
                fo.issueEnqueueShipProductionOrder(shipDesignID, locationIDs[0])
            elif topPriority == 8 and shipDesign.name(True) == troopShipName:
                # troop ship
                print ""
                print "adding new ship to production queue: " + shipDesign.name(True)
                fo.issueEnqueueShipProductionOrder(shipDesignID, locationIDs[0])
            elif topPriority == 9 and shipDesign.isArmed:
                # military ship
                print ""
                print "adding new ship to production queue: " + shipDesign.name(True)
                fo.issueEnqueueShipProductionOrder(shipDesignID, locationIDs[0])
            elif shipDesign.attack > 0:
                # military ship
                print ""
                print "adding new ship to production queue: " + shipDesign.name(True)
                fo.issueEnqueueShipProductionOrder(shipDesignID, locationIDs[0])
    print ""

def getAvailableBuildLocations(shipDesignID):
    "returns locations where shipDesign can be built"

    result = []

    systemIDs = foAI.foAIstate.getExplorableSystems(AIExplorableSystemType.EXPLORABLE_SYSTEM_EXPLORED)
    planetIDs = PlanetUtilsAI.getPlanetsInSystemsIDs(systemIDs)
    shipDesign = fo.getShipDesign(shipDesignID)
    empire = fo.getEmpire()
    empireID = empire.empireID
    for planetID in planetIDs:
        if shipDesign.productionLocationForEmpire(empireID, planetID):
            result.append(planetID)

    return result

def spentPP():
    "calculate PPs spent this turn so far"

    queue = fo.getEmpire().productionQueue
    return queue.totalSpent
