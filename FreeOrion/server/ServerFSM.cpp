#include "ServerFSM.h"

#include "SaveLoad.h"
#include "ServerApp.h"
#include "../Empire/Empire.h"
#include "../universe/System.h"
#include "../universe/Species.h"
#include "../network/ServerNetworking.h"
#include "../network/Message.h"
#include "../util/Directories.h"
#include "../util/OrderSet.h"
#include "../util/OptionsDB.h"

#include <boost/filesystem/path.hpp>


namespace {
    const bool TRACE_EXECUTION = true;

    void SendMessageToAllPlayers(const Message& message) {
        ServerApp* server = ServerApp::GetApp();
        if (!server) {
            Logger().errorStream() << "SendMessageToAllPlayers couldn't get server.";
            return;
        }
        ServerNetworking& networking = server->Networking();

        for (ServerNetworking::const_established_iterator player_it = networking.established_begin();
             player_it != networking.established_end();
             ++player_it)
        {
            PlayerConnectionPtr player = *player_it;
            player->SendMessage(message);
        }
    }

    void SendMessageToHost(const Message& message) {
        ServerApp* server = ServerApp::GetApp();
        if (!server) {
            Logger().errorStream() << "SendMessageToHost couldn't get server.";
            return;
        }
        ServerNetworking& networking = server->Networking();

        ServerNetworking::established_iterator host_it = networking.GetPlayer(networking.HostPlayerID());
        if (host_it == networking.established_end()) {
            Logger().errorStream() << "SendMessageToHost couldn't get host player.";
            return;
        }

        PlayerConnectionPtr host = *host_it;
        host->SendMessage(message);
    }

    std::string GetHostNameFromSinglePlayerSetupData(const SinglePlayerSetupData& single_player_setup_data) {
        if (single_player_setup_data.m_new_game) {
            // for new games, get host player's name from PlayerSetupData for the
            // (should be only) human player
            for (std::vector<PlayerSetupData>::const_iterator setup_data_it = single_player_setup_data.m_players.begin();
                 setup_data_it != single_player_setup_data.m_players.end(); ++setup_data_it)
            {
                // In a single player game, the host player is always the human player, so
                // this is just a matter of finding which player setup data is for
                // a human player, and assigning that setup data to the host player id
                const PlayerSetupData& psd = *setup_data_it;
                if (psd.m_client_type == Networking::CLIENT_TYPE_HUMAN_PLAYER)
                    return psd.m_player_name;
            }

        } else {
            // for loading saved games, get host / human player's name from save file
            if (!single_player_setup_data.m_players.empty())
                Logger().errorStream() << "GetHostNameFromSinglePlayerSetupData got single player setup data to load a game, but also player setup data for a new game.  Ignoring player setup data";


            std::vector<PlayerSaveGameData> player_save_game_data;
            LoadPlayerSaveGameData(single_player_setup_data.m_filename,
                                   player_save_game_data);

            // find which player was the human (and thus the host) in the saved game
            for (std::vector<PlayerSaveGameData>::const_iterator save_data_it = player_save_game_data.begin();
                 save_data_it != player_save_game_data.end(); ++save_data_it)
            {
                const PlayerSaveGameData& psgd = *save_data_it;
                if (psgd.m_client_type == Networking::CLIENT_TYPE_HUMAN_PLAYER)
                    return psgd.m_name;
            }
        }
        return "";
    }
}


////////////////////////////////////////////////////////////
// ResolveCombat
////////////////////////////////////////////////////////////
ResolveCombat::ResolveCombat(System* system, std::set<int>& empire_ids) :
    m_system(system),
    m_empire_ids()
{ std::swap(m_empire_ids, empire_ids); }

////////////////////////////////////////////////////////////
// MessageEventBase
////////////////////////////////////////////////////////////
Disconnection::Disconnection(PlayerConnectionPtr& player_connection) :
    m_player_connection(player_connection)
{}

////////////////////////////////////////////////////////////
// MessageEventBase
////////////////////////////////////////////////////////////
MessageEventBase::MessageEventBase(Message& message, PlayerConnectionPtr& player_connection) :
    m_message(),
    m_player_connection(player_connection)
{ swap(m_message, message); }


////////////////////////////////////////////////////////////
// ServerFSM
////////////////////////////////////////////////////////////
ServerFSM::ServerFSM(ServerApp &server) :
    m_server(server)
{}

void ServerFSM::unconsumed_event(const sc::event_base &event)
{
    std::string most_derived_message_type_str = "[ERROR: Unknown Event]";
    const sc::event_base* event_ptr = &event;
    if (dynamic_cast<const Disconnection*>(event_ptr))
        most_derived_message_type_str = "Disconnection";
#define MESSAGE_EVENT_CASE(r, data, name)                               \
    else if (dynamic_cast<const name*>(event_ptr))                      \
        most_derived_message_type_str = BOOST_PP_STRINGIZE(name);
    BOOST_PP_SEQ_FOR_EACH(MESSAGE_EVENT_CASE, _, MESSAGE_EVENTS);
#undef MESSAGE_EVENT_CASE
    Logger().errorStream() << "ServerFSM : A " << most_derived_message_type_str << " event was passed to "
        "the ServerFSM.  This event is illegal in the FSM's current state.  It is being ignored.";
}

ServerApp& ServerFSM::Server()
{ return m_server; }

void ServerFSM::HandleNonLobbyDisconnection(const Disconnection& d)
{
    PlayerConnectionPtr& player_connection = d.m_player_connection;

    int id = player_connection->PlayerID();
    // this will not usually happen, since the host client process usually owns the server process, and will usually take it down if it fails
    if (Server().m_networking.PlayerIsHost(id)) {
        // if the host dies, there's really nothing else we can do -- the game's over
        std::string message = player_connection->PlayerName();
        for (ServerNetworking::const_established_iterator it = m_server.m_networking.established_begin(); it != m_server.m_networking.established_end(); ++it) {
            if ((*it)->PlayerID() != id) {
                (*it)->SendMessage(EndGameMessage((*it)->PlayerID(), Message::HOST_DISCONNECTED, player_connection->PlayerName()));
            }
        }
        Logger().debugStream() << "ServerFSM::HandleNonLobbyDisconnection : Host player disconnected; server terminating.";
        Sleep(2000); // HACK! Pause for a bit to let the player disconnected and end game messages propogate.
        m_server.Exit(1);
    } else if (m_server.m_eliminated_players.find(id) == m_server.m_eliminated_players.end()) { // player abnormally disconnected during a regular game
        Logger().debugStream() << "ServerFSM::HandleNonLobbyDisconnection : Lost connection to player #" << boost::lexical_cast<std::string>(id) 
                               << ", named \"" << player_connection->PlayerName() << "\"; server terminating.";
        std::string message = player_connection->PlayerName();
        for (ServerNetworking::const_established_iterator it = m_server.m_networking.established_begin(); it != m_server.m_networking.established_end(); ++it) {
            if ((*it)->PlayerID() != id) {
                // in the future we may find a way to recover from this, but for now we will immediately send a game ending message as well
                (*it)->SendMessage(EndGameMessage((*it)->PlayerID(), Message::NONHOST_DISCONNECTED, player_connection->PlayerName()));
            }
        }
    }

    // TODO: Add a way to have AIs play without humans... for AI debugging purposes

    // independently of everything else, if there are no humans left, it's time to terminate
    if (m_server.m_networking.empty() || m_server.m_ai_client_processes.size() == m_server.m_networking.NumEstablishedPlayers()) {
        Logger().debugStream() << "ServerFSM::HandleNonLobbyDisconnection : All human players disconnected; server terminating.";
        Sleep(2000); // HACK! Pause for a bit to let the player disconnected and end game messages propogate.
        m_server.Exit(1);
    }
}


////////////////////////////////////////////////////////////
// Idle
////////////////////////////////////////////////////////////
Idle::Idle(my_context c) :
    my_base(c)
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) Idle"; }

Idle::~Idle()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~Idle"; }

sc::result Idle::react(const HostMPGame& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) Idle.HostMPGame";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    std::string host_player_name = message.Text();
    // validate host name (was found and wasn't empty)
    if (host_player_name.empty()) {
        Logger().errorStream() << "Idle::react(const HostMPGame& msg) got an empty host player name";
        return discard_event();
    }

    Logger().debugStream() << "Idle::react(HostMPGame) about to establish host";

    int host_player_id = server.m_networking.NewPlayerID();
    player_connection->EstablishPlayer(host_player_id, host_player_name, Networking::CLIENT_TYPE_HUMAN_PLAYER);
    server.m_networking.SetHostPlayerID(host_player_id);

    Logger().debugStream() << "Idle::react(HostMPGame) about to send acknowledgement to host";
    player_connection->SendMessage(HostMPAckMessage(host_player_id));

    server.m_single_player_game = false;

    Logger().debugStream() << "Idle::react(HostMPGame) about to transit to MPLobby";

    return transit<MPLobby>();
}

sc::result Idle::react(const HostSPGame& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) Idle.HostSPGame";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    boost::shared_ptr<SinglePlayerSetupData> single_player_setup_data(new SinglePlayerSetupData);
    ExtractMessageData(message, *single_player_setup_data);


    // get host player's name from setup data or saved file
    std::string host_player_name;
    try {
        host_player_name = GetHostNameFromSinglePlayerSetupData(*single_player_setup_data);
    } catch (const std::exception& e) {
        PlayerConnectionPtr& player_connection = msg.m_player_connection;
        player_connection->SendMessage(ErrorMessage("UNABLE_TO_READ_SAVE_FILE"));
        return discard_event();
    }
    // validate host name (was found and wasn't empty)
    if (host_player_name.empty()) {
        Logger().errorStream() << "Idle::react(const HostSPGame& msg) got an empty host player name or couldn't find a human player";
        return discard_event();
    }


    int host_player_id = server.m_networking.NewPlayerID();
    player_connection->EstablishPlayer(host_player_id, host_player_name, Networking::CLIENT_TYPE_HUMAN_PLAYER);
    server.m_networking.SetHostPlayerID(host_player_id);
    player_connection->SendMessage(HostSPAckMessage(host_player_id));

    server.m_single_player_game = true;

    context<ServerFSM>().m_single_player_setup_data = single_player_setup_data;

    return transit<WaitingForSPGameJoiners>();
}


////////////////////////////////////////////////////////////
// MPLobby
////////////////////////////////////////////////////////////
MPLobby::MPLobby(my_context c) :
    my_base(c),
    m_lobby_data(new MultiplayerLobbyData(true)),
    m_server_save_game_data(new ServerSaveGameData())
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) MPLobby";
    ServerApp& server = Server();
    const PlayerConnectionPtr& player_connection = *(server.m_networking.GetPlayer(server.m_networking.HostPlayerID()));

    // assign host player info from connection to lobby data players list
    PlayerSetupData& player_setup_data = m_lobby_data->m_players[server.m_networking.HostPlayerID()];

    player_setup_data.m_player_name =           player_connection->PlayerName();
    player_setup_data.m_empire_name =           player_connection->PlayerName();    // default empire name to same as player name, for lack of a better choice
    player_setup_data.m_empire_color =          EmpireColors().at(0);               // since the host is the first joined player, it can be assumed that no other player is using this colour (unlike subsequent join game message responses)
    // leaving starting species name as default
    // leaving save game empire id as default
    player_setup_data.m_client_type =           player_connection->GetClientType();

    server.m_networking.SendMessage(ServerLobbyUpdateMessage(server.m_networking.HostPlayerID(), *m_lobby_data));
}

MPLobby::~MPLobby()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~MPLobby"; }

sc::result MPLobby::react(const Disconnection& d)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) MPLobby.Disconnection";
    ServerApp& server = Server();
    PlayerConnectionPtr& player_connection = d.m_player_connection;

    // if there are no humans left, it's time to terminate
    if (server.m_networking.empty() || server.m_ai_client_processes.size() == server.m_networking.NumEstablishedPlayers()) {
        Logger().debugStream() << "MPLobby.Disconnection : All human players disconnected; server terminating.";
        server.Exit(1);
    }

    // if the disconnected player wasn't in the lobby, don't need to do anything
    int id = player_connection->PlayerID();
    if (m_lobby_data->m_players.find(id) == m_lobby_data->m_players.end()) {
        Logger().debugStream() << "MPLobby.Disconnection : Disconnecting player (" << id << ") was not in lobby";
        return discard_event();
    }

    // did the host disconnect?  this may happen if the host crashes instead of exiting cleanly
    if (server.m_networking.PlayerIsHost(player_connection->PlayerID())) {
        // disconnect was the host.  need to pick a new host and inform players.
        int new_host_id = Networking::INVALID_PLAYER_ID;

        // scan through players for a human to host
        for (ServerNetworking::established_iterator players_it = server.m_networking.established_begin();
                players_it != server.m_networking.established_end(); ++players_it)
        {
            PlayerConnectionPtr player_connection = *players_it;
            if (player_connection->GetClientType() == Networking::CLIENT_TYPE_HUMAN_PLAYER ||
                player_connection->GetClientType() == Networking::CLIENT_TYPE_HUMAN_OBSERVER)
            {
                new_host_id = player_connection->PlayerID();
            }
        }

        if (new_host_id == Networking::INVALID_PLAYER_ID) {
            // couldn't find a host... abort
            Logger().debugStream() << "MPLobby.Disconnection : Host disconnected and couldn't find a replacement.  server terminating";
            server.Exit(1);
            return discard_event();
        }

        // set new host ID and inform players
        server.m_networking.SetHostPlayerID(new_host_id);
        for (ServerNetworking::const_iterator it = server.m_networking.begin(); it != server.m_networking.end(); ++it) {
            if (*it != player_connection)
                (*it)->SendMessage(HostIDMessage(new_host_id));
        }
    }

    // remove disconnected player from lobby
    m_lobby_data->m_players.erase(id);

    // send updated lobby data to players after disconnection-related changes
    for (ServerNetworking::const_established_iterator it = server.m_networking.established_begin();
         it != server.m_networking.established_end(); ++it)
    {
        (*it)->SendMessage(ServerLobbyUpdateMessage((*it)->PlayerID(), *m_lobby_data));
    }

    return discard_event();
}

sc::result MPLobby::react(const JoinGame& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) MPLobby.JoinGame";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    std::string player_name;
    Networking::ClientType client_type;
    ExtractMessageData(message, player_name, client_type);
    // TODO: check if player name is unique.  If not, modify it slightly to be unique.

    // assign unique player ID to newly connected player
    int player_id = server.m_networking.NewPlayerID();

    // establish player with requested client type and acknowldge via connection
    player_connection->EstablishPlayer(player_id, player_name, client_type);
    player_connection->SendMessage(JoinAckMessage(player_id));

    // inform player of host
    player_connection->SendMessageA(HostIDMessage(server.m_networking.HostPlayerID()));

    // assign player info from connection to lobby data players list
    PlayerSetupData& player_setup_data = m_lobby_data->m_players[player_id];
    player_setup_data.m_player_name = player_name;
    player_setup_data.m_client_type = client_type;

    // find unused empire colour
    //Logger().debugStream() << "finding colours for empire of player " << player_name;
    const std::vector<GG::Clr>& empire_colours = EmpireColors();
    GG::Clr empire_colour = empire_colours.at(0); // default
    for (std::vector<GG::Clr>::const_iterator it = empire_colours.begin(); it != empire_colours.end(); ++it) {
        const GG::Clr& possible_colour = *it;
        //Logger().debugStream() << "trying colour " << possible_colour.r << ", " << possible_colour.g << ", " << possible_colour.b;

        // check if any other player / empire is using this colour
        bool colour_is_new = true;
        for (std::map<int, PlayerSetupData>::const_iterator player_it = m_lobby_data->m_players.begin(); player_it != m_lobby_data->m_players.end(); ++player_it) {
            const GG::Clr& player_colour = player_it->second.m_empire_color;
            if (player_colour == possible_colour) {
                colour_is_new = false;
                break;
            }
        }

        // use colour and exit loop if no other empire is using the colour
        if (colour_is_new) {
            empire_colour = possible_colour;
            break;
        }

        //Logger().debugStream() << " ... colour already used.";
    }
    player_setup_data.m_empire_color = empire_colour;

    // TODO: generate a default empire name, or get one from the list of empire names
    player_setup_data.m_empire_name = player_name;

    for (ServerNetworking::const_established_iterator it = server.m_networking.established_begin();
         it != server.m_networking.established_end(); ++it)
    {
        (*it)->SendMessage(ServerLobbyUpdateMessage((*it)->PlayerID(), *m_lobby_data));
    }

    return discard_event();
}

sc::result MPLobby::react(const LobbyUpdate& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) MPLobby.LobbyUpdate";
    ServerApp& server = Server();
    const Message& message = msg.m_message;

    MultiplayerLobbyData incoming_lobby_data;
    ExtractMessageData(message, incoming_lobby_data);

    // extract and store incoming lobby data.  clients can only change some of
    // this information (galaxy setup data, whether it is a new game and what
    // save file index to load) directly, so other data is skipped (list of
    // save files, save game empire data from the save file, player data)
    // during this copying and is updated below from the save file(s)

    // GalaxySetupData
    m_lobby_data->m_size =          incoming_lobby_data.m_size;
    m_lobby_data->m_shape =         incoming_lobby_data.m_shape;
    m_lobby_data->m_age =           incoming_lobby_data.m_age;
    m_lobby_data->m_starlane_freq = incoming_lobby_data.m_starlane_freq;
    m_lobby_data->m_planet_density =incoming_lobby_data.m_planet_density;
    m_lobby_data->m_specials_freq = incoming_lobby_data.m_specials_freq;

    // directly configurable lobby data
    m_lobby_data->m_new_game =      incoming_lobby_data.m_new_game;
    m_lobby_data->m_players =       incoming_lobby_data.m_players;

    // update player connection types according to modified lobby selections
    for (ServerNetworking::const_established_iterator player_connection_it = server.m_networking.established_begin();
         player_connection_it != server.m_networking.established_end(); ++player_connection_it)
    {
        PlayerConnectionPtr player_connection = *player_connection_it;
        int player_id = player_connection->PlayerID();

        std::map<int, PlayerSetupData>::iterator player_setup_it = m_lobby_data->m_players.find(player_id);
        if (player_setup_it == m_lobby_data->m_players.end()) {
            Logger().errorStream() << "No player setup data for player " << player_id << " in MPLobby::react(const LobbyUpdate& msg)";
            continue;
        }
        Networking::ClientType client_type = player_setup_it->second.m_client_type;
        player_connection->SetClientType(client_type);
    }

    // to determine if a new save file was selected, check if the selected file
    // index is different, and the new file index is in the valid range
    bool new_save_file_selected = false;
    int new_file_index = incoming_lobby_data.m_save_file_index;
    int old_file_index = m_lobby_data->m_save_file_index;
    const int NUM_FILE_INDICES = static_cast<int>(m_lobby_data->m_save_games.size());
    if (new_file_index != old_file_index  &&  new_file_index >= 0  &&  new_file_index < NUM_FILE_INDICES) {
        new_save_file_selected = true;

        // update selected file index
        m_lobby_data->m_save_file_index = new_file_index;

        // reset assigned empires in save game for all players.  new loaded game may not have the same set of empire IDs to choose from
        for (std::map<int, PlayerSetupData>::iterator player_setup_it = m_lobby_data->m_players.begin();
             player_setup_it != m_lobby_data->m_players.end(); ++player_setup_it)
        {
            player_setup_it->second.m_save_game_empire_id = ALL_EMPIRES;
        }

        // refresh save game empire data
        boost::filesystem::path save_dir(GetSaveDir());
        const std::string& save_filename = m_lobby_data->m_save_games[new_file_index];
        try {
            LoadEmpireSaveGameData((save_dir / save_filename).string(),
                                   m_lobby_data->m_save_game_empire_data);
        } catch (const std::exception&) {
            // inform player who attempted to change the save file that there was a problem
            PlayerConnectionPtr& player_connection = msg.m_player_connection;
            player_connection->SendMessage(ErrorMessage("UNABLE_TO_READ_SAVE_FILE"));
            // revert to old save file
            m_lobby_data->m_save_file_index = old_file_index;
        }
    }


    // propegate lobby changes to players, so everyone has the latest updated
    // version of the lobby data
    for (ServerNetworking::const_established_iterator player_connection_it = server.m_networking.established_begin();
         player_connection_it != server.m_networking.established_end(); ++player_connection_it)
    {
        PlayerConnectionPtr player_connection = *player_connection_it;
        int player_id = player_connection->PlayerID();
        if (new_save_file_selected || player_id != message.SendingPlayer()) // new save file update needs to be sent to everyone, but otherwise messages can just go to players who didn't send the message that this function is responding to
            player_connection->SendMessage(ServerLobbyUpdateMessage(player_id, *m_lobby_data));
    }

    return discard_event();
}

sc::result MPLobby::react(const LobbyChat& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) MPLobby.LobbyChat";
    ServerApp& server = Server();
    const Message& message = msg.m_message;

    if (message.ReceivingPlayer() == Networking::INVALID_PLAYER_ID) { // the receiver is everyone (except the sender)
        for (ServerNetworking::const_established_iterator it = server.m_networking.established_begin(); it != server.m_networking.established_end(); ++it) {
            if ((*it)->PlayerID() != message.SendingPlayer())
                (*it)->SendMessage(ServerLobbyChatMessage(message.SendingPlayer(), (*it)->PlayerID(), message.Text()));
        }
    } else {
        server.m_networking.SendMessage(ServerLobbyChatMessage(message.SendingPlayer(), message.ReceivingPlayer(), message.Text()));
    }

    return discard_event();
}

sc::result MPLobby::react(const StartMPGame& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) MPLobby.StartMPGame";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    if (server.m_networking.PlayerIsHost(player_connection->PlayerID())) {
        if (m_lobby_data->m_new_game) {
            // if all expected player already connected, can skip waiting for
            // MP joiners and go directly to playing game
            int expected_players = m_lobby_data->m_players.size();
            if (expected_players == static_cast<int>(server.m_networking.NumEstablishedPlayers())) {
                server.NewMPGameInit(*m_lobby_data);
                return transit<PlayingGame>();
            }
            // otherwise, transit to waiting for MP joiners

        } else {
            // Load game...
            boost::filesystem::path save_dir(GetSaveDir());
            std::string save_filename = m_lobby_data->m_save_games[m_lobby_data->m_save_file_index];

            try {
                LoadGame((save_dir / save_filename).string(),
                         *m_server_save_game_data,
                         m_player_save_game_data,
                         GetUniverse(),
                         Empires(),
                         GetSpeciesManager());
            } catch (const std::exception&) {
                SendMessageToAllPlayers(ErrorMessage("UNABLE_TO_READ_SAVE_FILE"));
                return discard_event();
            }

            // if no AI clients need to be started, can go directly to playing game
            int expected_players = m_player_save_game_data.size();
            int needed_AI_clients = expected_players - server.m_networking.NumEstablishedPlayers();
            if (needed_AI_clients < 1) {
                server.LoadMPGameInit(*m_lobby_data,
                                      m_player_save_game_data,
                                      m_server_save_game_data);
                return transit<PlayingGame>();
            }
            // othewrise, transit to waiting for mp joiners
        }
    } else {
        Logger().errorStream() << "(ServerFSM) MPLobby.StartMPGame : Player #" << message.SendingPlayer()
                               << " attempted to initiate a game load, but is not the host.  Terminating connection.";
        server.m_networking.Disconnect(player_connection);
        return discard_event();
    }

    // copy locally stored data to common server fsm context so it can be retreived in WaitingForMPGameJoiners
    context<ServerFSM>().m_lobby_data = m_lobby_data;
    context<ServerFSM>().m_player_save_game_data = m_player_save_game_data;
    context<ServerFSM>().m_server_save_game_data = m_server_save_game_data;

    return transit<WaitingForMPGameJoiners>();
}


////////////////////////////////////////////////////////////
// WaitingForSPGameJoiners
////////////////////////////////////////////////////////////
WaitingForSPGameJoiners::WaitingForSPGameJoiners(my_context c) :
    my_base(c),
    m_single_player_setup_data(context<ServerFSM>().m_single_player_setup_data),
    m_server_save_game_data(new ServerSaveGameData()),
    m_num_expected_players(0)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForSPGameJoiners";

    context<ServerFSM>().m_single_player_setup_data.reset();
    ServerApp& server = Server();
    std::vector<PlayerSetupData>& players = m_single_player_setup_data->m_players;

    // Ensure all players have unique non-empty names   // TODO: the uniqueness part...
    unsigned int player_num = 1;
    for (std::vector<PlayerSetupData>::iterator psd_it = players.begin(); psd_it != players.end(); ++psd_it) {
        if (psd_it->m_player_name.empty()) {
            if (psd_it->m_client_type == Networking::CLIENT_TYPE_AI_PLAYER)
                psd_it->m_player_name = "AI_" + boost::lexical_cast<std::string>(player_num++);
            else if (psd_it->m_client_type == Networking::CLIENT_TYPE_HUMAN_PLAYER)
                psd_it->m_player_name = "Human_Player_" + boost::lexical_cast<std::string>(player_num++);
            else if (psd_it->m_client_type == Networking::CLIENT_TYPE_HUMAN_OBSERVER)
                psd_it->m_player_name = "Observer_" + boost::lexical_cast<std::string>(player_num++);
            else
                psd_it->m_player_name = "Player_" + boost::lexical_cast<std::string>(player_num++);
        }
    }

    if (m_single_player_setup_data->m_new_game) {
        // DO NOTHING

        // for new games, single player setup data contains full m_players
        // vector, so can just use the contents of that to create AI
        // clients

    } else {
        // for loaded games, all that is specified is the filename, and the
        // server needs to populate single player setup data's m_players
        // with data from the save file.
        if (!players.empty()) {
            Logger().errorStream() << "WaitingForSPGameJoiners::WaitingForSPGameJoiners got single player setup data to load a game, but also player setup data for a new game.  Ignoring player setup data";
            players.clear();
        }

        std::vector<PlayerSaveGameData> player_save_game_data;
        try {
            LoadPlayerSaveGameData(m_single_player_setup_data->m_filename,
                                   player_save_game_data);
        } catch (const std::exception& e) {
            SendMessageToHost(ErrorMessage("UNABLE_TO_READ_SAVE_FILE"));
            post_event(LoadSaveFileFailed());
            return;
        }

        // add player setup data for each player in saved gamed
        for (std::vector<PlayerSaveGameData>::const_iterator save_data_it = player_save_game_data.begin();
             save_data_it != player_save_game_data.end(); ++save_data_it)
        {
            const PlayerSaveGameData& psgd = *save_data_it;
            if (psgd.m_client_type == Networking::CLIENT_TYPE_HUMAN_PLAYER ||
                psgd.m_client_type == Networking::CLIENT_TYPE_AI_PLAYER)
            {
                PlayerSetupData psd;
                psd.m_player_name =         psgd.m_name;
                //psd.m_empire_name // left default
                //psd.m_empire_color // left default
                //psd.m_starting_species_name // left default
                psd.m_save_game_empire_id = psgd.m_empire_id;
                psd.m_client_type =         psgd.m_client_type;
                players.push_back(psd);
            }
        }
    }

    m_num_expected_players = players.size();
    m_expected_ai_player_names.clear();
    for (std::vector<PlayerSetupData>::const_iterator it = players.begin(); it != players.end(); ++it)
        if (it->m_client_type == Networking::CLIENT_TYPE_AI_PLAYER)
            m_expected_ai_player_names.insert(it->m_player_name);

    server.CreateAIClients(players);    // also disconnects any currently-connected AI clients

    // force immediate check if all expected AIs are present, so that the FSM
    // won't get stuck in this state waiting for JoinGame messages that will
    // never come since no other AIs are left to join
    post_event(CheckStartConditions());
}

WaitingForSPGameJoiners::~WaitingForSPGameJoiners()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~WaitingForSPGameJoiners"; }

sc::result WaitingForSPGameJoiners::react(const JoinGame& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForSPGameJoiners.JoinGame";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    std::string player_name("Default_Player_Name_in_WaitingForSPGameJoiners::react(const JoinGame& msg)");
    Networking::ClientType client_type(Networking::INVALID_CLIENT_TYPE);
    ExtractMessageData(message, player_name, client_type);

    int player_id = server.m_networking.NewPlayerID();

    // is this an AI?
    if (client_type == Networking::CLIENT_TYPE_AI_PLAYER) {
        // verify that player name was expected
        if (m_expected_ai_player_names.find(player_name) == m_expected_ai_player_names.end()) {
            // unexpected ai player
            Logger().errorStream() << "WaitingForSPGameJoiners::react(const JoinGame& msg) received join game message for player \"" << player_name << "\" which was not an expected AI player name.    Terminating connection.";
            server.m_networking.Disconnect(player_connection);
        } else {
            // expected player
            // let the networking system know what socket this player is on
            player_connection->EstablishPlayer(player_id, player_name, client_type);
            player_connection->SendMessage(JoinAckMessage(player_id));

            // remove name from expected names list, so as to only allow one connection per AI
            m_expected_ai_player_names.erase(player_name);
        }

    } else if (client_type == Networking::CLIENT_TYPE_HUMAN_PLAYER) {
        // verify that there is room left for this player
        int already_connected_players = m_expected_ai_player_names.size() + server.m_networking.NumEstablishedPlayers();
        if (already_connected_players >= m_num_expected_players) {
            // too many human players
            Logger().errorStream() << "WaitingForSPGameJoiners::react(const JoinGame& msg): A human player attempted to join the game but there was not enough room.  Terminating connection.";
            // TODO: send message to attempted joiner saying game is full
            server.m_networking.Disconnect(player_connection);
        } else {
            // expected human player
            player_connection->EstablishPlayer(player_id, player_name, client_type);
            player_connection->SendMessage(JoinAckMessage(player_id));
        }
    } else {
        Logger().errorStream() << "WaitingForSPGameJoiners::react(const JoinGame& msg): Received JoinGame message with invalid client type: " << client_type;
        return discard_event();
    }

    // if all expected players have connected, proceed to start new or load game
    if (static_cast<int>(server.m_networking.NumEstablishedPlayers()) == m_num_expected_players) {
        Logger().debugStream() << "WaitingForSPGameJoiners::react(const JoinGame& msg): all " << m_num_expected_players << " joined.  starting game...";
        if (m_single_player_setup_data->m_new_game) {
            server.NewSPGameInit(*m_single_player_setup_data);
        } else {
            try {
                LoadGame(m_single_player_setup_data->m_filename,
                         *m_server_save_game_data,
                         m_player_save_game_data,
                         GetUniverse(),
                         Empires(),
                         GetSpeciesManager());
            } catch (const std::exception&) {
                SendMessageToHost(ErrorMessage("UNABLE_TO_READ_SAVE_FILE"));
                return transit<Idle>();
            }

            server.LoadSPGameInit(m_player_save_game_data,
                                  m_server_save_game_data);
        }
        return transit<PlayingGame>();
    }

    return discard_event();
}

sc::result WaitingForSPGameJoiners::react(const CheckStartConditions& u)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForSPGameJoiners.CheckStartConditions";
    ServerApp& server = Server();

    if (static_cast<int>(server.m_networking.NumEstablishedPlayers()) == m_num_expected_players) {
        if (m_single_player_setup_data->m_new_game) {
            server.NewSPGameInit(*m_single_player_setup_data);
        } else {
            try {
                LoadGame(m_single_player_setup_data->m_filename,
                         *m_server_save_game_data,
                         m_player_save_game_data,
                         GetUniverse(),
                         Empires(),
                         GetSpeciesManager());
            } catch (const std::exception&) {
                SendMessageToHost(ErrorMessage("UNABLE_TO_READ_SAVE_FILE"));
                return transit<Idle>();
            }

            server.LoadSPGameInit(m_player_save_game_data,
                                  m_server_save_game_data);
        }
        return transit<PlayingGame>();
    }

    return discard_event();
}

sc::result WaitingForSPGameJoiners::react(const LoadSaveFileFailed& u)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForSPGameJoiners.LoadSaveFileFailed";
    return transit<Idle>();
}


////////////////////////////////////////////////////////////
// WaitingForMPGameJoiners
////////////////////////////////////////////////////////////
WaitingForMPGameJoiners::WaitingForMPGameJoiners(my_context c) :
    my_base(c),
    m_lobby_data(context<ServerFSM>().m_lobby_data),
    m_player_save_game_data(context<ServerFSM>().m_player_save_game_data),
    m_server_save_game_data(context<ServerFSM>().m_server_save_game_data),
    m_num_expected_players(0)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForMPGameJoiners";
    context<ServerFSM>().m_lobby_data.reset();
    context<ServerFSM>().m_player_save_game_data.clear();
    context<ServerFSM>().m_server_save_game_data.reset();
    ServerApp& server = Server();

    m_num_expected_players = m_lobby_data->m_players.size();

    std::vector<PlayerSetupData> player_setup_data;
    m_expected_ai_player_names.clear();

    for (std::map<int, PlayerSetupData>::const_iterator it = m_lobby_data->m_players.begin();
         it != m_lobby_data->m_players.end(); ++it)
    {
        player_setup_data.push_back(it->second);
        if (it->second.m_client_type == Networking::CLIENT_TYPE_AI_PLAYER)
            m_expected_ai_player_names.insert(it->second.m_player_name);
    }

    server.CreateAIClients(player_setup_data);
}

WaitingForMPGameJoiners::~WaitingForMPGameJoiners()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~WaitingForMPGameJoiners"; }

sc::result WaitingForMPGameJoiners::react(const JoinGame& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForMPGameJoiners.JoinGame";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    std::string player_name("Default_Player_Name_in_WaitingForMPGameJoiners::react(const JoinGame& msg)");
    Networking::ClientType client_type(Networking::INVALID_CLIENT_TYPE);
    ExtractMessageData(message, player_name, client_type);

    int player_id = server.m_networking.NewPlayerID();

    // is this an AI?
    if (client_type == Networking::CLIENT_TYPE_AI_PLAYER) {
        // verify that player name was expected
        if (m_expected_ai_player_names.find(player_name) == m_expected_ai_player_names.end()) {
            // unexpected ai player
            Logger().errorStream() << "WaitingForMPGameJoiners::react(const JoinGame& msg) received join game message for player \"" << player_name << "\" which was not an expected AI player name.    Terminating connection.";
            server.m_networking.Disconnect(player_connection);
        } else {
            // expected player
            // let the networking system know what socket this player is on
            player_connection->EstablishPlayer(player_id, player_name, client_type);
            player_connection->SendMessage(JoinAckMessage(player_id));

            // remove name from expected names list, so as to only allow one connection per AI
            m_expected_ai_player_names.erase(player_name);
        }

    } else if (client_type == Networking::CLIENT_TYPE_HUMAN_PLAYER) {
        // verify that there is room left for this player
        int already_connected_players = m_expected_ai_player_names.size() + server.m_networking.NumEstablishedPlayers();
        if (already_connected_players >= m_num_expected_players) {
            // too many human players
            Logger().errorStream() << "WaitingForSPGameJoiners.JoinGame : A human player attempted to join the game but there was not enough room.  Terminating connection.";
            server.m_networking.Disconnect(player_connection);
        } else {
            // expected human player
            player_connection->EstablishPlayer(player_id, player_name, client_type);
            player_connection->SendMessage(JoinAckMessage(player_id));
        }
    } else {
        Logger().errorStream() << "WaitingForMPGameJoiners::react(const JoinGame& msg): Received JoinGame message with invalid client type: " << client_type;
        return discard_event();
    }

    // if all expected players have connected, proceed to start new or load game
    if (static_cast<int>(server.m_networking.NumEstablishedPlayers()) == m_num_expected_players) {
        if (m_player_save_game_data.empty())
            server.NewMPGameInit(*m_lobby_data);
        else
            server.LoadMPGameInit(*m_lobby_data,
                                  m_player_save_game_data,
                                  m_server_save_game_data);
        return transit<PlayingGame>();
    }

    return discard_event();
}


////////////////////////////////////////////////////////////
// PlayingGame
////////////////////////////////////////////////////////////
PlayingGame::PlayingGame(my_context c) :
    my_base(c)
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) PlayingGame"; }

PlayingGame::~PlayingGame()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~PlayingGame"; }

sc::result PlayingGame::react(const PlayerChat& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) PlayingGame.PlayerChat";
    ServerApp& server = Server();
    for (ServerNetworking::const_established_iterator it = server.m_networking.established_begin(); it != server.m_networking.established_end(); ++it) {
        if (msg.m_message.ReceivingPlayer() == Networking::INVALID_PLAYER_ID || msg.m_message.ReceivingPlayer() == (*it)->PlayerID())
            (*it)->SendMessage(SingleRecipientChatMessage(msg.m_message.SendingPlayer(), (*it)->PlayerID(), msg.m_message.Text()));
    }
    return discard_event();
}

////////////////////////////////////////////////////////////
// WaitingForTurnEnd
////////////////////////////////////////////////////////////
WaitingForTurnEnd::WaitingForTurnEnd(my_context c) :
    my_base(c)
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForTurnEnd"; }

WaitingForTurnEnd::~WaitingForTurnEnd()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~WaitingForTurnEnd"; }

sc::result WaitingForTurnEnd::react(const HostSPGame& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForTurnEnd.HostSPGame";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    boost::shared_ptr<SinglePlayerSetupData> single_player_setup_data(new SinglePlayerSetupData);
    ExtractMessageData(message, *single_player_setup_data);

    if (server.m_networking.PlayerIsHost(player_connection->PlayerID()) && !single_player_setup_data->m_new_game) {
        Empires().Clear();
        player_connection->SendMessage(HostSPAckMessage(player_connection->PlayerID()));
        player_connection->SendMessage(JoinAckMessage(player_connection->PlayerID()));
        server.m_single_player_game = true;
        context<ServerFSM>().m_single_player_setup_data = single_player_setup_data;
        return transit<WaitingForSPGameJoiners>();
    }

    if (server.m_networking.PlayerIsHost(player_connection->PlayerID())) {
        Logger().errorStream() << "WaitingForTurnEnd.HostSPGame : Player #" << message.SendingPlayer()
                               << " attempted to initiate a new game or game load, but is not the host. "
                               << "Terminating connection.";
    }
    if (single_player_setup_data->m_new_game) {
        Logger().errorStream() << "WaitingForTurnEnd.HostSPGame : Player #" << message.SendingPlayer()
                               << " attempted to start a new game without ending the current one. "
                               << "Terminating connection.";
    }
    server.m_networking.Disconnect(player_connection);

    return discard_event();
}

sc::result WaitingForTurnEnd::react(const TurnOrders& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForTurnEnd.TurnOrders";
    ServerApp& server = Server();
    const Message& message = msg.m_message;

    OrderSet* order_set = new OrderSet;
    ExtractMessageData(message, *order_set);

    // check order validity -- all orders must originate from this empire in order to be considered valid
    int player_id = message.SendingPlayer();
    Empire* empire = server.GetPlayerEmpire(player_id);
    if (!empire) {
        Logger().errorStream() << "WaitingForTurnEnd::react(TurnOrders&) couldn't get empire for player with id:" << player_id;
        server.m_networking.SendMessage(ErrorMessage(message.SendingPlayer(), "EMPIRE_NOT_FOUND_CANT_HANDLE_ORDERS"));
        return discard_event();
    }

    for (OrderSet::const_iterator it = order_set->begin(); it != order_set->end(); ++it) {
        OrderPtr order = it->second;
        if (!order) {
            Logger().errorStream() << "WaitingForTurnEnd::react(TurnOrders&) couldn't get order from order set!";
            continue;
        }
        if (empire->EmpireID() != order->EmpireID()) {
            Logger().errorStream() << "WaitingForTurnEnd::react(TurnOrders&) received orders from player " << empire->PlayerName() << "(id: "
                                   << message.SendingPlayer() << ") who controls empire " << empire->EmpireID()
                                   << " but those orders were for empire " << order->EmpireID() << ".  Orders being ignored.";
            server.m_networking.SendMessage(ErrorMessage(message.SendingPlayer(), "ORDERS_FOR_WRONG_EMPIRE"));
            return discard_event();
        }
    }

    if (TRACE_EXECUTION) Logger().debugStream() << "WaitingForTurnEnd.TurnOrders : Received orders from player " << message.SendingPlayer();

    server.SetEmpireTurnOrders(empire->EmpireID(), order_set);

    // notify other player that this player submitted orders
    for (ServerNetworking::const_established_iterator player_it = server.m_networking.established_begin();
         player_it != server.m_networking.established_end();
         ++player_it)
    {
        PlayerConnectionPtr player_ctn = *player_it;
        player_ctn->SendMessage(PlayerStatusMessage(player_ctn->PlayerID(), message.SendingPlayer(), Message::WAITING));
    }

    if (server.AllOrdersReceived()) {
        // if all players have submitted orders, proceed to turn processing
        if (TRACE_EXECUTION) Logger().debugStream() << "WaitingForTurnEnd.TurnOrders : All orders received.";
        post_event(ProcessTurn());
        return transit<ProcessingTurn>();

    } else {
        // if still waiting for other players, inform the player who just submitted orders
        server.m_networking.SendMessage(TurnProgressMessage(message.SendingPlayer(), Message::WAITING_FOR_PLAYERS));
    }

    return discard_event();
}

sc::result WaitingForTurnEnd::react(const RequestObjectID& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForTurnEnd.RequestObjectID";
    Server().m_networking.SendMessage(DispatchObjectIDMessage(msg.m_message.SendingPlayer(), GetUniverse().GenerateObjectID()));
    return discard_event();
}

sc::result WaitingForTurnEnd::react(const RequestDesignID& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForTurnEnd.RequestDesignID";
    Server().m_networking.SendMessage(DispatchDesignIDMessage(msg.m_message.SendingPlayer(), GetUniverse().GenerateDesignID()));
    return discard_event();
}


////////////////////////////////////////////////////////////
// WaitingForTurnEndIdle
////////////////////////////////////////////////////////////
WaitingForTurnEndIdle::WaitingForTurnEndIdle(my_context c) :
    my_base(c)
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForTurnEndIdle"; }

WaitingForTurnEndIdle::~WaitingForTurnEndIdle()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~WaitingForTurnEndIdle"; }

sc::result WaitingForTurnEndIdle::react(const SaveGameRequest& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForTurnEndIdle.SaveGameRequest";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    if (!server.m_networking.PlayerIsHost(player_connection->PlayerID())) {
        return discard_event();
        Logger().errorStream() << "WaitingForTurnEndIdle.SaveGameRequest : Player #" << message.SendingPlayer()
                               << " attempted to initiate a game save, but is not the host.  Ignoring request connection.";
        player_connection->SendMessage(ErrorMessage("NON_HOST_SAVE_REQUEST_IGNORED"));
        return discard_event();
    }

    context<WaitingForTurnEnd>().m_save_filename = message.Text();  // store requested save file name in Base state context so that sibling state can retreive it

    return transit<WaitingForSaveData>();
}


////////////////////////////////////////////////////////////
// WaitingForSaveData
////////////////////////////////////////////////////////////
WaitingForSaveData::WaitingForSaveData(my_context c) :
    my_base(c)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForSaveData";

    ServerApp& server = Server();
    for (ServerNetworking::const_established_iterator player_it = server.m_networking.established_begin();
         player_it != server.m_networking.established_end();
         ++player_it)
    {
        PlayerConnectionPtr player = *player_it;
        int player_id = player->PlayerID();
        bool host = server.m_networking.PlayerIsHost(player_id);
        player->SendMessage(ServerSaveGameMessage(player_id, host));
        m_needed_reponses.insert(player_id);
    }
}

WaitingForSaveData::~WaitingForSaveData()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~WaitingForSaveData"; }

sc::result WaitingForSaveData::react(const ClientSaveData& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) WaitingForSaveData.ClientSaveData";
    ServerApp& server = Server();
    const Message& message = msg.m_message;
    PlayerConnectionPtr& player_connection = msg.m_player_connection;

    int player_id = player_connection->PlayerID();

    // extract client save information in message
    OrderSet received_orders;
    boost::shared_ptr<SaveGameUIData> ui_data(new SaveGameUIData);
    bool ui_data_available = false;
    std::string save_state_string;
    bool save_state_string_available = false;

    try {
        ExtractMessageData(message, received_orders, ui_data_available, *ui_data, save_state_string_available, save_state_string);
    } catch (const std::exception& e) {
        Logger().debugStream() << "WaitingForSaveData::react(const ClientSaveData& msg) received invalid save data from player " << player_connection->PlayerName();
        player_connection->SendMessage(ErrorMessage("INVALID_CLIENT_SAVE_DATA_RECEIVED"));
        // use whatever portion of message data was extracted, and leave the rest as defaults.
    }

    // store recieved orders or already existing orders.  I'm not sure what's
    // going on here with the two possible sets of orders.  apparently the
    // received orders are ignored if there are already existing orders?
    boost::shared_ptr<OrderSet> order_set;
    if (const Empire* empire = server.GetPlayerEmpire(player_id)) {
        OrderSet* existing_orders = server.m_turn_sequence[empire->EmpireID()];
        if (existing_orders)
            order_set.reset(new OrderSet(*existing_orders));
        else
            order_set.reset(new OrderSet(received_orders));
    } else {
        Logger().errorStream() << "WaitingForSaveData::react(const ClientSaveData& msg) couldn't get empire for player " << player_id;
        order_set.reset(new OrderSet(received_orders));
    }

    // ensure ui data availability flag is consistent with ui data
    if (!ui_data_available)
        ui_data.reset();

    // what type of client is this?
    Networking::ClientType client_type = player_connection->GetClientType();


    // pack data into struct
    m_player_save_game_data.push_back(
        PlayerSaveGameData(player_connection->PlayerName(),
                           server.PlayerEmpireID(message.SendingPlayer()),
                           order_set,
                           ui_data,
                           save_state_string,
                           client_type));


    // if all players have responded, proceed with save and continue game
    m_players_responded.insert(message.SendingPlayer());
    if (m_players_responded == m_needed_reponses) {
        ServerSaveGameData server_data(server.m_current_turn, server.m_victors);

        // retreive requested save name from Base state, which should have been
        // set in WaitingForTurnEndIdle::react(const SaveGameRequest& msg)
        const std::string& save_filename = context<WaitingForTurnEnd>().m_save_filename;

        // save game...
        try {
            SaveGame(save_filename,
                     server_data,
                     m_player_save_game_data,
                     GetUniverse(),
                     Empires(),
                     GetSpeciesManager());
        } catch (const std::exception&) {
            SendMessageToAllPlayers(ErrorMessage("UNABLE_TO_WRITE_SAVE_FILE"));
        }

        context<WaitingForTurnEnd>().m_save_filename = "";
        return transit<WaitingForTurnEndIdle>();
    }

    return discard_event();
}


////////////////////////////////////////////////////////////
// ProcessingTurn
////////////////////////////////////////////////////////////
ProcessingTurn::ProcessingTurn(my_context c) :
    my_base(c),
    m_combat_system(0),
    m_combat_empire_ids()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ProcessingTurn"; }

ProcessingTurn::~ProcessingTurn()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~ProcessingTurn"; }

sc::result ProcessingTurn::react(const ProcessTurn& u)
{
    ServerApp& server = Server();
    server.PreCombatProcessTurns();
    server.ProcessCombats();
    server.PostCombatProcessTurns();

    // update players that other players are now playing their turn
    for (ServerNetworking::const_established_iterator player_it = server.m_networking.established_begin();
         player_it != server.m_networking.established_end();
         ++player_it)
    {
        PlayerConnectionPtr player_ctn = *player_it;
        if (player_ctn->GetClientType() == Networking::CLIENT_TYPE_AI_PLAYER ||
            player_ctn->GetClientType() == Networking::CLIENT_TYPE_HUMAN_PLAYER ||
            player_ctn->GetClientType() == Networking::CLIENT_TYPE_HUMAN_OBSERVER)
        {
            // inform all players that this player is playing a turn
            for (ServerNetworking::const_established_iterator recipient_player_it = server.m_networking.established_begin();
                recipient_player_it != server.m_networking.established_end();
                ++recipient_player_it)
            {
                PlayerConnectionPtr recipient_player_ctn = *recipient_player_it;
                server.m_networking.SendMessage(
                    PlayerStatusMessage(recipient_player_ctn->PlayerID(), player_ctn->PlayerID(), Message::PLAYING_TURN));
            }
        }
    }

    return transit<WaitingForTurnEnd>();
}


////////////////////////////////////////////////////////////
// ProcessingTurnIdle
////////////////////////////////////////////////////////////
ProcessingTurnIdle::ProcessingTurnIdle(my_context c) :
    my_base(c)
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ProcessingTurnIdle"; }

ProcessingTurnIdle::~ProcessingTurnIdle()
{ if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~ProcessingTurnIdle"; }

sc::result ProcessingTurnIdle::react(const ResolveCombat& u)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ProcessingTurnIdle.ResolveCombat";

    context<ProcessingTurn>().m_combat_system = u.m_system;
    ResolveCombat& mu = const_cast<ResolveCombat&>(u);
    std::swap(context<ProcessingTurn>().m_combat_empire_ids, mu.m_empire_ids);
    return transit<ResolvingCombat>();
}


////////////////////////////////////////////////////////////
// ResolvingCombat
////////////////////////////////////////////////////////////
ResolvingCombat::ResolvingCombat(my_context c) :
    my_base(c)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ResolvingCombat";

    ServerApp& server = Server();

    std::map<int, std::vector<CombatSetupGroup> > setup_groups;
    server.m_current_combat =
        new CombatData(context<ProcessingTurn>().m_combat_system, setup_groups);
    context<ProcessingTurn>().m_combat_system = 0;

    // TODO: For now, we're just sending all designs to everyone.  Reconsider
    // this later.
    std::map<int, Universe::ShipDesignMap> foreign_designs;
    for (std::map<int, std::vector<CombatSetupGroup> >::const_iterator it = setup_groups.begin();
         it != setup_groups.end();
         ++it) {
        for (std::size_t i = 0; i < it->second.size(); ++i) {
            for (std::set<int>::const_iterator ship_it = it->second[i].m_ships.begin();
                 ship_it != it->second[i].m_ships.end();
                 ++ship_it) {
                ShipDesign* design = const_cast<ShipDesign*>(GetObject<Ship>(*ship_it)->Design());
                for (std::set<int>::const_iterator empire_it =
                         context<ProcessingTurn>().m_combat_empire_ids.begin();
                     empire_it != context<ProcessingTurn>().m_combat_empire_ids.end();
                     ++empire_it) {
                    foreign_designs[*empire_it][design->ID()] = design;
                }
            }
        }
    }

    server.ClearEmpireCombatTurns();

    std::cerr << "ResolvingCombat: waiting for orders from empires ";

    for (ServerNetworking::const_established_iterator it =
             server.m_networking.established_begin();
         it != server.m_networking.established_end();
         ++it)
    {
        int player_id = (*it)->PlayerID();
        int empire_id = server.GetPlayerEmpire(player_id)->EmpireID();
        if (context<ProcessingTurn>().m_combat_empire_ids.find(empire_id) !=
            context<ProcessingTurn>().m_combat_empire_ids.end())
        {
            assert(!setup_groups[empire_id].empty());
            (*it)->SendMessage(
                ServerCombatStartMessage(
                    player_id,
                    empire_id,
                    *server.m_current_combat,
                    setup_groups[empire_id],
                    foreign_designs[empire_id]));
            server.AddEmpireCombatTurn(empire_id);
            std::cerr << empire_id << " ";
        }
    }
    std::cerr << "\n";
}

ResolvingCombat::~ResolvingCombat()
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ~ResolvingCombat";

    context<ProcessingTurn>().m_combat_empire_ids.clear();
}

sc::result ResolvingCombat::react(const CombatTurnOrders& msg)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ResolvingCombat.CombatTurnOrders";

    ServerApp& server = Server();
    const Message& message = msg.m_message;
 
    CombatOrderSet* order_set = new CombatOrderSet;

    ExtractMessageData(message, *order_set);

    // check order validity -- all orders must originate from this empire in order to be considered valid
    Empire* empire = server.GetPlayerEmpire(message.SendingPlayer());
    assert(empire);
    for (CombatOrderSet::const_iterator it = order_set->begin(); it != order_set->end(); ++it) {
        const CombatOrder& order = *it;
        int owner_id = ALL_EMPIRES;
        if (order.Type() == CombatOrder::SHIP_ORDER ||
            order.Type() == CombatOrder::SETUP_PLACEMENT_ORDER) {
            if (UniverseObject* object = GetObject(order.ID())) {
                assert(object->Owners().size() == 1u);
                owner_id = *object->Owners().begin();
            }
        } else if (order.Type() == CombatOrder::FIGHTER_ORDER) {
            CombatFighterPtr combat_fighter =
                server.m_current_combat->m_pathing_engine.FindLeader(order.ID());
            if (combat_fighter)
                owner_id = combat_fighter->Owner();
        } else {
            assert(!"Unknown CombatOrder type!");
        }
        if (owner_id == ALL_EMPIRES) {
            throw std::runtime_error(
                "ResolvingCombat.CombatTurnOrders : Player \"" + empire->PlayerName() +
                "\" attempted to issue combat orders for an object that does not exist!  "
                "Terminating...");
        } else if (owner_id != empire->EmpireID()) {
            throw std::runtime_error(
                "ResolvingCombat.CombatTurnOrders : Player \"" + empire->PlayerName() +
                "\" attempted to issue combat orders for an object it does not own!  Terminating...");
        }

        // TODO: check legality of placement orders, and check that mission
        // orders are given to the right kind of object (e.g. ship missions
        // given to ships, not fighters).
    }

    Logger().debugStream() << "ResolvingCombat.CombatTurnOrders : Received combat orders from player " << message.SendingPlayer();

    server.SetEmpireCombatTurnOrders(empire->EmpireID(), order_set);

    std::cerr << "received orderd for empire " << empire->EmpireID() << '\n';

    if (server.AllCombatOrdersReceived()) {
        std::cerr << "all orders received; processing turn\n";
        Logger().debugStream() << "ResolvingCombat.CombatTurnOrders : All orders received.  Processing combat turn....";
        server.ProcessCombatTurn();

        std::cerr << "turn complete; sending out updates to empires ";

        for (ServerNetworking::const_established_iterator it = server.m_networking.established_begin();
             it != server.m_networking.established_end();
             ++it)
        {
            int player_id = (*it)->PlayerID();
            int empire_id = server.GetPlayerEmpire(player_id)->EmpireID();
            if (context<ProcessingTurn>().m_combat_empire_ids.find(empire_id) !=
                context<ProcessingTurn>().m_combat_empire_ids.end())
            {
                (*it)->SendMessage(
                    ServerCombatUpdateMessage(player_id, empire_id, *server.m_current_combat));
                std::cerr << empire_id << " ";
            }
        }
        std::cerr << "\n";

        if (server.CombatTerminated())
        {std::cerr << "this combat is over\n";
            return transit<ProcessingTurnIdle>();
        }
    }

    return discard_event();
}

sc::result ResolvingCombat::react(const CombatComplete& cc)
{
    if (TRACE_EXECUTION) Logger().debugStream() << "(ServerFSM) ResolvingCombat.CombatComplete";
    ServerApp& server = Server();
    delete server.m_current_combat;
    server.m_current_combat = 0;
 
    for (ServerNetworking::const_established_iterator it = server.m_networking.established_begin();
         it != server.m_networking.established_end();
         ++it)
    {
        int player_id = (*it)->PlayerID();
        int empire_id = server.GetPlayerEmpire(player_id)->EmpireID();
        if (context<ProcessingTurn>().m_combat_empire_ids.find(empire_id) !=
            context<ProcessingTurn>().m_combat_empire_ids.end())
        {
            (*it)->SendMessage(ServerCombatEndMessage(player_id));
        }
    }

    return transit<ProcessingTurnIdle>();
}
