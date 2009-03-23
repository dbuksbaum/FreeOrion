def checkValidity(value):
    if (value == None or value < 0):
        print "value: " + str(value) + " is not valid"
        return False
    return True

def __getInterval(low, high):
    "returns integer numbers from interval <low, high>"
    
    result = []
    # <low, high)
    for i in range(low, high):
        result.append(i)
    # <high, high>
    result.append(high)

    return result

class AIPriorityType(object):
    PRIORITY_INVALID = -1
    PRIORITY_RESOURCE_FOOD = 0
    PRIORITY_RESOURCE_MINERALS = 1
    PRIORITY_RESOURCE_PRODUCTION = 2
    PRIORITY_RESOURCE_RESEARCH = 3
    PRIORITY_RESOURCE_TRADE = 4
    PRIORITY_PRODUCTION_EXPLORATION = 5
    PRIORITY_PRODUCTION_COLONISATION = 6
    PRIORITY_PRODUCTION_MILITARY = 7
    PRIORITY_PRODUCTION_BUILDINGS = 8
    PRIORITY_RESEARCH_LEARNING = 9
    PRIORITY_RESEARCH_GROWTH = 10
    PRIORITY_RESEARCH_PRODUCTION = 11
    PRIORITY_RESEARCH_CONSTRUCTION = 12
    PRIORITY_RESEARCH_ECONOMICS = 13
    PRIORITY_RESEARCH_SHIPS = 14
    
def getAIPriorityResourceTypes():
    return __getInterval(0, 4)
def getAIPriorityProductionTypes():
    return __getInterval(5, 8)
def getAIPriorityResearchTypes():
    return __getInterval(9, 14)
def getAIPriorityTypes():
    return __getInterval(0, 14)

    
class AIExplorableSystemType(object):
    EXPLORABLE_SYSTEM_INVALID = -1
    EXPLORABLE_SYSTEM_UNEXPLORED = 0
    EXPLORABLE_SYSTEM_TARGET = 1
    EXPLORABLE_SYSTEM_EXPLORED = 2    
    
def getAIExplorableSystemTypes():
    return __getInterval(0, 2)


class AIFleetMissionType(object):
    FLEET_MISSION_INVALID = -1    
    FLEET_MISSION_EXPLORATION = 0
    FLEET_MISSION_COLONISATION = 1
    FLEET_MISSION_SPLIT_FLEET = 2
    FLEET_MISSION_MERGE_FLEET = 3
    FLEET_MISSION_HIT_AND_RUN = 4
    FLEET_MISSION_ATTACK = 5
    FLEET_MISSION_DEFEND = 6
    FLEET_MISSION_LAST_STAND = 7
        
def getAIFleetMissionTypes():
    return __getInterval(0, 7)


class AIFleetOrderType(object):
    ORDER_INVALID = -1
    ORDER_MOVE = 0
    ORDER_WAIT_FOR_REFUEL = 1
    ORDER_SPLIT_FLEET = 2
    ORDER_MERGE_FLEET = 3
    ORDER_COLONISE = 4
    ORDER_ATACK = 5
    ORDER_DEFEND = 6

def getAIFleetOrderTypes():
    return __getInterval(0, 6)


class AIShipRoleType(object):
    SHIP_ROLE_INVALID = -1
    SHIP_ROLE_MILITARY_ATTACK = 0
    SHIP_ROLE_MILITARY_LONGRANGE = 1
    SHIP_ROLE_MILITARY_MISSILES = 2
    SHIP_ROLE_MILITARY_POINTDEFENSE = 3
    SHIP_ROLE_CIVILIAN_EXPLORATION = 4
    SHIP_ROLE_CIVILIAN_COLONISATION = 5
    
def getAIShipRolesTypes():
    return __getInterval(0, 5)


class AITargetType(object):
    TARGET_INVALID = -1
    TARGET_BUILDING = 0
    TARGET_TECHNOLOGY = 1
    TARGET_PLANET = 2
    TARGET_SYSTEM = 3
    TARGET_SHIP = 4
    TARGET_FLEET = 5
    TARGET_EMPIRE = 6
    TARGET_ALL_OTHER_EMPIRES = 7
    
def getAITargetTypes():
    return __getInterval(0, 7)