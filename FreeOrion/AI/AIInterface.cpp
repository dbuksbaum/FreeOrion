#include "AIInterface.h"

#include "../util/MultiplayerCommon.h"
#include "../network/ClientNetworking.h"
#include "../client/AI/AIClientApp.h"

#include "../universe/Universe.h"
#include "../universe/UniverseObject.h"
#include "../universe/Planet.h"
#include "../universe/Fleet.h"
#include "../universe/Ship.h"
#include "../universe/Tech.h"
#include "../Empire/Empire.h"

#include "../util/OrderSet.h"

#include <stdexcept>
#include <string>
#include <map>

//////////////////////////////////
//          AI Base             //
//////////////////////////////////
AIBase::~AIBase()
{}

void AIBase::GenerateOrders() {
    AIInterface::DoneTurn();
}

void AIBase::HandleChatMessage(int sender_id, const std::string& msg)
{}

void AIBase::StartNewGame()
{}

void AIBase::ResumeLoadedGame(const std::string& save_state_string)
{}

const std::string& AIBase::GetSaveStateString() {
    static std::string default_state_string("AIBase default save state string");
    Logger().debugStream() << "AIBase::GetSaveStateString() returning: " << default_state_string;
    return default_state_string;
}


//////////////////////////////////
//        AI Interface          //
//////////////////////////////////
namespace {
    // stuff used in AIInterface, but not needed to be visible outside this file

    // start of turn initialization for meters
    void InitMeterEstimatesAndDiscrepancies() {
        Universe& universe = AIClientApp::GetApp()->GetUniverse();
        universe.InitMeterEstimatesAndDiscrepancies();
    }

    // start of turn initialization for Empire ResourcePools.  determines where supplies can be delivered, and 
    // between which systems resources can be exchanged (which is necessary to know before resource pools can be
    // updated
    void InitResourcePoolsAndSupply() {
        EmpireManager& manager = AIClientApp::GetApp()->Empires();

        // determine sytems where fleets can delivery supply, and groups of systems that can exchange resources
        for (EmpireManager::iterator it = manager.begin(); it != manager.end(); ++it) {
            Empire* empire = it->second;
            empire->UpdateSupplyUnobstructedSystems();
            empire->UpdateSystemSupplyRanges();
            empire->UpdateFleetSupply();
            empire->UpdateResourceSupply();
            empire->InitResourcePools();
        }
    }
}

namespace AIInterface {
    const std::string& PlayerName() {
        return AIClientApp::GetApp()->PlayerName();
    }

    const std::string& PlayerName(int player_id) {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        std::map<int, PlayerInfo>::const_iterator it = players.find(player_id);
        if (it != players.end())
            return it->second.name;
        else {
            Logger().debugStream() << "AIInterface::PlayerName(" << boost::lexical_cast<std::string>(player_id) << ") - passed an invalid player_id";
            throw std::invalid_argument("AIInterface::PlayerName : given invalid player_id");
        }
    }

    int PlayerID() {
        return AIClientApp::GetApp()->PlayerID();
    }

    int EmpireID() {
        return AIClientApp::GetApp()->EmpireID();
    }

    int PlayerEmpireID(int player_id) {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        for (std::map<int, PlayerInfo>::const_iterator it = players.begin(); it != players.end(); ++it) {
            if (it->first == player_id)
                return it->second.empire_id;
        }
        return -1;  // default invalid value
    }

    std::vector<int>  AllEmpireIDs() {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        std::vector<int> empire_ids;
        for (std::map<int, PlayerInfo>::const_iterator it = players.begin(); it != players.end(); ++it)
            empire_ids.push_back(it->second.empire_id);
        return empire_ids;
    }

    const Empire* GetEmpire() {
        return AIClientApp::GetApp()->Empires().Lookup(AIClientApp::GetApp()->EmpireID());
    }

    const Empire* GetEmpire(int empire_id) {
        return AIClientApp::GetApp()->Empires().Lookup(empire_id);
    }

    int EmpirePlayerID(int empire_id) {
        int player_id = AIClientApp::GetApp()->GetEmpirePlayerID(empire_id);
        if (-1 == player_id)
            Logger().debugStream() << "AIInterface::EmpirePlayerID(" << boost::lexical_cast<std::string>(empire_id) << ") - passed an invalid empire_id";
        return player_id;
    }

    std::vector<int> AllPlayerIDs() {
        const std::map<int, PlayerInfo>& players = AIClientApp::GetApp()->Players();
        std::vector<int> player_ids;
        for (std::map<int, PlayerInfo>::const_iterator it = players.begin(); it != players.end(); ++it)
            player_ids.push_back(it->first);
        return player_ids;
    }

    bool PlayerIsAI(int player_id) {
        return false;   // TODO: implement this
    }

    bool PlayerIsHost(int player_id) {
        return false;   // TODO: implement this
    }

    const Universe& GetUniverse() {
        return AIClientApp::GetApp()->GetUniverse();
    }

    const Tech* GetTech(const std::string& tech_name) {
        return TechManager::GetTechManager().GetTech(tech_name);
    }

    int CurrentTurn() {
        return AIClientApp::GetApp()->CurrentTurn();
    }

    void InitTurn() {
        InitMeterEstimatesAndDiscrepancies();
        UpdateMeterEstimates();
        InitResourcePoolsAndSupply();
        UpdateResourcePoolsAndQueues();
    }

    void UpdateMeterEstimates(bool pretend_unowned_planets_owned_by_this_ai_empire) {
        std::vector<Planet*> unowned_planets;
        int player_id = -1;
        Universe& universe = AIClientApp::GetApp()->GetUniverse();
        if (pretend_unowned_planets_owned_by_this_ai_empire) {
            // add this player ownership to all planets that the player can see but which aren't currently colonized.
            // this way, any effects the player knows about that would act on those planets if the player colonized them
            // include those planets in their scope.  This lets effects from techs the player knows alter the max
            // population of planet that is displayed to the player, even if those effects have a condition that causes
            // them to only act on planets the player owns (so as to not improve enemy planets if a player reseraches a
            // tech that should only benefit him/herself)
            player_id = AIInterface::PlayerID();

            // get all planets the player knows about that aren't yet colonized (aren't owned by anyone).  Add this
            // the current player's ownership to all, while remembering which planets this is done to
            std::vector<Planet*> all_planets = universe.FindObjects<Planet>();
            Universe::InhibitUniverseObjectSignals(true);
            for (std::vector<Planet*>::iterator it = all_planets.begin(); it != all_planets.end(); ++it) {
                 Planet* planet = *it;
                 if (planet->Owners().empty()) {
                     unowned_planets.push_back(planet);
                     planet->AddOwner(player_id);
                 }
            }
        }

        // update meter estimates with temporary ownership
        universe.UpdateMeterEstimates();

        if (pretend_unowned_planets_owned_by_this_ai_empire) {
            // remove temporary ownership added above
            for (std::vector<Planet*>::iterator it = unowned_planets.begin(); it != unowned_planets.end(); ++it)
                (*it)->RemoveOwner(player_id);
            Universe::InhibitUniverseObjectSignals(false);
        }
    }

    void UpdateResourcePoolsAndQueues() {
        EmpireManager& manager = AIClientApp::GetApp()->Empires();
        for (EmpireManager::iterator it = manager.begin(); it != manager.end(); ++it)
            it->second->UpdateResourcePools();
    }

    int IssueFleetMoveOrder(int fleet_id, int destination_id) {
        const Universe& universe = AIClientApp::GetApp()->GetUniverse();

        const Fleet* fleet = universe.Object<Fleet>(fleet_id);
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueFleetMoveOrder : passed an invalid fleet_id";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        if (!fleet->WhollyOwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetMoveOrder : passed fleet_id of fleet not owned only by player";
            return 0;
        }

        int start_id = fleet->SystemID();
        if (start_id == UniverseObject::INVALID_OBJECT_ID)
            start_id = fleet->NextSystemID();

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetMoveOrder(empire_id, fleet_id, start_id, destination_id)));

        return 1;
    }

    int IssueRenameOrder(int object_id, const std::string& new_name) {
        if (new_name.empty()) {
            Logger().errorStream() << "AIInterface::IssueRenameOrder : passed an empty new name";
            return 0;
        }

        const Universe& universe = AIClientApp::GetApp()->GetUniverse();
        int empire_id = AIClientApp::GetApp()->EmpireID();
        const UniverseObject* obj = universe.Object(object_id);

        if (!obj) {
            Logger().errorStream() << "AIInterface::IssueRenameOrder : passed an invalid object_id";
            return 0;
        }
        if (!obj->WhollyOwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueRenameOrder : passed object_id of object not owned only by player";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new RenameOrder(empire_id, object_id, new_name)));

        return 1;
    }

    int IssueNewFleetOrder(const std::string& fleet_name, const std::vector<int>& ship_ids) {
        if (ship_ids.empty()) {
            Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed empty vector of ship_ids";
            return 0;
        }

        if (fleet_name == "") {
            Logger().errorStream() << "AIInterface::IssueNewFleetOrder : tried to create a nameless fleet";
            return 0;
        }

        const Universe& universe = AIClientApp::GetApp()->GetUniverse();
        int empire_id = AIClientApp::GetApp()->EmpireID();
        const Ship* ship = 0;
        
        // make sure all ships exist and are owned just by this player       
        for (std::vector<int>::const_iterator it = ship_ids.begin(); it != ship_ids.end(); ++it) {
            ship = universe.Object<Ship>(*it);
            if (!ship) {
                Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed an invalid ship_id";
                return 0;
            }
            if (!ship->WhollyOwnedBy(empire_id)) {
                Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed ship_id of ship not owned only by player";
                return 0;
            }
        }

        // make sure all ships are at a system, and that all are at the same system
        int system_id = ship->SystemID();
        if (system_id == UniverseObject::INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed ship_ids of ships at different locations";
            return 0;
        }

        std::vector<int>::const_iterator it = ship_ids.begin();
        for (++it; it != ship_ids.end(); ++it) {
            const Ship* ship2 = universe.Object<Ship>(*it);
            if (ship2->SystemID() != system_id) {
                Logger().errorStream() << "AIInterface::IssueNewFleetOrder : passed ship_ids of ships at different locations";
                return 0;
            }
        }

        int new_fleet_id = ClientApp::GetApp()->GetNewObjectID();
        if (new_fleet_id == UniverseObject::INVALID_OBJECT_ID) 
            throw std::runtime_error("Couldn't get new object ID when transferring ship to new fleet");

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new NewFleetOrder(empire_id, fleet_name, new_fleet_id, system_id, ship_ids)));

        return 1;
    }

    int IssueNewFleetOrder(const std::string& fleet_name, int ship_id) {
        std::vector<int> ship_ids;
        ship_ids.push_back(ship_id);
        return IssueNewFleetOrder(fleet_name, ship_ids);
    }

    int IssueFleetTransferOrder(int ship_id, int new_fleet_id) {
        const Universe& universe = AIClientApp::GetApp()->GetUniverse();
        int empire_id = AIClientApp::GetApp()->EmpireID();

        const Ship* ship = universe.Object<Ship>(ship_id);
        if (!ship) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed an invalid ship_id";
            return 0;
        }
        int ship_sys_id = ship->SystemID();
        if (ship_sys_id == UniverseObject::INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : ship is not in a system";
            return 0;
        }
        if (!ship->WhollyOwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed ship_id of ship not owned only by player";
            return 0;
        }

        const Fleet* fleet = universe.Object<Fleet>(new_fleet_id);
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed an invalid new_fleet_id";
            return 0;
        }
        int fleet_sys_id = fleet->SystemID();
        if (fleet_sys_id == UniverseObject::INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : new fleet is not in a system";
            return 0;
        }
        if (!fleet->WhollyOwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : passed fleet_id of fleet not owned only by player";
            return 0;
        }

        if (fleet_sys_id != ship_sys_id) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : new fleet and ship are not in the same system";
            return 0;
        }

        int old_fleet_id = ship->FleetID();
        if (new_fleet_id == old_fleet_id) {
            Logger().errorStream() << "AIInterface::IssueFleetTransferOrder : ship is already in new fleet";
            return 0;
        }

        std::vector<int> ship_ids;
        ship_ids.push_back(ship_id);
        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetTransferOrder(empire_id, old_fleet_id, new_fleet_id, ship_ids)));

        return 1;
    }

    int IssueFleetColonizeOrder(int ship_id, int planet_id) {
        const Universe& universe = AIClientApp::GetApp()->GetUniverse();
        int empire_id = AIClientApp::GetApp()->EmpireID();

        // make sure ship_id is a ship...
        const Ship* ship = universe.Object<Ship>(ship_id);
        if (!ship) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : passed an invalid ship_id";
            return 0;
        }

        // get fleet of ship
        const Fleet* fleet = universe.Object<Fleet>(ship->FleetID());
        if (!fleet) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : ship with passed ship_id has invalid fleet_id";
            return 0;
        }

        // make sure player owns ship and its fleet
        if (!fleet->WhollyOwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : empire does not own fleet of passed ship";
            return 0;
        }
        if (!ship->WhollyOwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : empire does not own passed ship";
            return 0;
        }

        // verify that planet exists and is un-occupied.
        const Planet* planet = universe.Object<Planet>(planet_id);
        if (!planet) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : no planet with passed planet_id";
            return 0;
        }
        if (!planet->Unowned()) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : planet with passed planet_id is already owned or colonized";
            return 0;
        }

        // verify that planet is in same system as the fleet
        if (planet->SystemID() != fleet->SystemID()) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : fleet and planet are not in the same system";
            return 0;
        }
        if (ship->SystemID() == UniverseObject::INVALID_OBJECT_ID) {
            Logger().errorStream() << "AIInterface::IssueFleetColonizeOrder : ship is not in a system";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new FleetColonizeOrder(empire_id, ship_id, planet_id)));

        return 1;
    }

    int IssueDeleteFleetOrder() {
        return 0;
    }

    int IssueChangeFocusOrder(int planet_id, FocusType focus_type, bool primary) {
        const Universe& universe = AIClientApp::GetApp()->GetUniverse();
        int empire_id = AIClientApp::GetApp()->EmpireID();
        const Planet* planet = universe.Object<Planet>(planet_id);
        if (!planet) {
            Logger().errorStream() << "AIInterface::IssueChangeFocusOrder : no planet with passed planet_id";
            return 0;
        }
        if (!planet->WhollyOwnedBy(empire_id)) {
            Logger().errorStream() << "AIInterface::IssueChangeFocusOrder : empire does not own planet with passed planet_id";
            return 0;
        }
        if (focus_type <= INVALID_FOCUS_TYPE || focus_type >= NUM_FOCI) {
            Logger().errorStream() << "AIInterface::IssueChangeFocusOrder : invalid focus specified";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ChangeFocusOrder(empire_id, planet_id, focus_type, primary)));

        return 1;
    }

    int IssueEnqueueTechOrder(const std::string& tech_name, int position) {
        const Tech* tech = GetTech(tech_name);
        if (!tech) {
            Logger().errorStream() << "AIInterface::IssueEnqueueTechOrder : passed tech_name that is not the name of a tech.";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ResearchQueueOrder(empire_id, tech_name, position)));

        return 1;
    }
    int IssueDequeueTechOrder(const std::string& tech_name) {
        const Tech* tech = GetTech(tech_name);
        if (!tech) {
            Logger().errorStream() << "AIInterface::IssueDequeueTechOrder : passed tech_name that is not the name of a tech.";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ResearchQueueOrder(empire_id, tech_name)));

        return 1;
    }

    int IssueEnqueueBuildingProductionOrder(const std::string& item_name, int location_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        const Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        if (!empire->BuildableItem(BT_BUILDING, item_name, location_id)) {
            Logger().errorStream() << "AIInterface::IssueEnqueueBuildingProductionOrder : specified item_name and location_id that don't indicate an item that can be built at that location";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ProductionQueueOrder(empire_id, BT_BUILDING, item_name, 1, location_id)));

        return 1;
    }

    int IssueEnqueueShipProductionOrder(int design_id, int location_id) {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        const Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        if (!empire->BuildableItem(BT_SHIP, design_id, location_id)) {
            Logger().errorStream() << "AIInterface::IssueEnqueueShipProductionOrder : specified design_id and location_id that don't indicate a design that can be built at that location";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ProductionQueueOrder(empire_id, BT_SHIP, design_id, 1, location_id)));

        return 1;
    }

    int IssueRequeueProductionOrder(int old_queue_index, int new_queue_index) {
        if (old_queue_index == new_queue_index) {
            Logger().errorStream() << "AIInterface::IssueRequeueProductionOrder : passed same old and new indexes... nothing to do.";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        const Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        const ProductionQueue& queue = empire->GetProductionQueue();
        if (old_queue_index < 0 || static_cast<int>(queue.size()) <= old_queue_index) {
            Logger().errorStream() << "AIInterface::IssueRequeueProductionOrder : passed old_queue_index outside range of items on queue.";
            return 0;
        }

        // After removing an earlier entry in queue, all later entries are shifted down one queue index, so
        // inserting before the specified item index should now insert before the previous item index.  This
        // also allows moving to the end of the queue, rather than only before the last item on the queue.
        int actual_new_index = new_queue_index;
        if (old_queue_index < new_queue_index)
            actual_new_index = new_queue_index - 1;

        if (new_queue_index < 0 || static_cast<int>(queue.size()) <= actual_new_index) {
            Logger().errorStream() << "AIInterface::IssueRequeueProductionOrder : passed new_queue_index outside range of items on queue.";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ProductionQueueOrder(empire_id, old_queue_index, new_queue_index)));

        return 1;
    }

    int IssueDequeueProductionOrder(int queue_index) {
        int empire_id = AIClientApp::GetApp()->EmpireID();
        const Empire* empire = AIClientApp::GetApp()->Empires().Lookup(empire_id);

        const ProductionQueue& queue = empire->GetProductionQueue();
        if (queue_index < 0 || static_cast<int>(queue.size()) <= queue_index) {
            Logger().errorStream() << "AIInterface::IssueDequeueProductionOrder : passed queue_index outside range of items on queue.";
            return 0;
        }

        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ProductionQueueOrder(empire_id, queue_index)));

        return 1;
    }

    int IssueCreateShipDesignOrder(const std::string& name, const std::string& description,
                                   const std::string& hull,
                                   const std::vector<std::string>& parts,
                                   const std::string& graphic, const std::string& model)
    {
        if (name.empty() || description.empty() || hull.empty() || graphic.empty()) {
            Logger().errorStream() << "AIInterface::IssueCreateShipDesignOrderOrder : passed an empty name, description, hull or graphic.";
            return 0;
        }
        if (!ShipDesign::ValidDesign(hull, parts)) {
            Logger().errorStream() << "AIInterface::IssueCreateShipDesignOrderOrder : pass a hull and parts that do not make a valid ShipDesign";
            return 0;
        }

        int empire_id = AIClientApp::GetApp()->EmpireID();
        int current_turn = CurrentTurn();

        // create design from stuff chosen in UI
        ShipDesign* design = new ShipDesign(name, description, empire_id, current_turn,
                                            hull, parts, graphic, model);

        if (!design) {
            Logger().errorStream() << "AIInterface::IssueCreateShipDesignOrderOrder failed to create a new ShipDesign object";
            return 0;
        }

        int new_design_id = AIClientApp::GetApp()->GetNewDesignID();
        AIClientApp::GetApp()->Orders().IssueOrder(OrderPtr(new ShipDesignOrder(empire_id, new_design_id, *design)));

        return 1;
    }

    void SendPlayerChatMessage(int recipient_player_id, const std::string& message_text) {
        if (recipient_player_id == -1)
            AIClientApp::GetApp()->Networking().SendMessage(GlobalChatMessage(PlayerID(), message_text));
        else
            AIClientApp::GetApp()->Networking().SendMessage(SingleRecipientChatMessage(PlayerID(), recipient_player_id, message_text));
    }

    void DoneTurn() {
        Logger().debugStream() << "AIInterface::DoneTurn()";
        AIClientApp::GetApp()->StartTurn(); // encodes order sets and sends turn orders message.  "done" the turn for the client, but "starts" the turn for the server
    }

    void LogOutput(const std::string& log_text) {
        Logger().debugStream() << log_text;
    }

    void ErrorOutput(const std::string& error_text) {
        Logger().errorStream() << error_text;
    }
} // namespace AIInterface
