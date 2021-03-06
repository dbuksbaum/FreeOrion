#include "AppInterface.h"

#ifdef FREEORION_BUILD_SERVER
# include "../server/ServerApp.h"
#else // a client build
# ifdef FREEORION_BUILD_HUMAN
#  include "../client/human/HumanClientApp.h"
# else
#  undef int64_t
#  include "../client/AI/AIClientApp.h"
# endif
#endif

#include "../util/MultiplayerCommon.h"


const int INVALID_GAME_TURN = -(2 << 15) + 1;
const int BEFORE_FIRST_TURN = -(2 << 14);
const int IMPOSSIBLY_LARGE_TURN = 2 << 15;

EmpireManager& Empires()
{
#ifdef FREEORION_BUILD_SERVER
    return ServerApp::GetApp()->Empires();
#else
    return ClientApp::GetApp()->Empires();
#endif
}

Universe& GetUniverse()
{
#ifdef FREEORION_BUILD_SERVER
    return ServerApp::GetApp()->GetUniverse();
#else
    return ClientApp::GetApp()->GetUniverse();
#endif
}

ObjectMap& GetMainObjectMap()
{
#ifdef FREEORION_BUILD_SERVER
    return GetUniverse().Objects();
#else
    return GetUniverse().EmpireKnownObjects(ClientApp::GetApp()->EmpireID());
#endif
}

log4cpp::Category& Logger()
{ return log4cpp::Category::getRoot(); }

int GetNewObjectID()
{
#ifdef FREEORION_BUILD_SERVER
    return GetUniverse().GenerateObjectID();
#else
    return ClientApp::GetApp()->GetNewObjectID();
#endif
}

int GetNewDesignID()
{
#ifdef FREEORION_BUILD_SERVER
    return GetUniverse().GenerateDesignID();
#elif defined(FREEORION_BUILD_UTIL)
    return UniverseObject::INVALID_OBJECT_ID;
#else
    return ClientApp::GetApp()->GetNewDesignID();
#endif
}

int CurrentTurn()
{
#ifdef FREEORION_BUILD_SERVER
    return ServerApp::GetApp()->CurrentTurn();
#else
    return const_cast<const ClientApp*>(ClientApp::GetApp())->CurrentTurn();
#endif
}
