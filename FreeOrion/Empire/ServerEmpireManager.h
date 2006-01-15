// -*- C++ -*-

#ifndef _ServerEmpireManager_h_
#define _ServerEmpireManager_h_

#ifndef _EmpireManager_h_
#include "EmpireManager.h"
#endif

/**
* ServerEmpire is the Server version of the EmpireManager.
*  It supports methods for generating turn updates to send to clients.
*
*/
class ServerEmpireManager : public EmpireManager
{
public:
   
    /** \name Constructors */ //@{
    /// Default Constructor
    /** Initializes the empire ID counter */
    ServerEmpireManager();
    //@}
   
    /** \name Mutators */ //@{
   
    /// Creates a new Empire with the specified name, color, and homeworld.
    /** 
        Creates an empire with the specified properties and
        returns a pointer to it, after setting it up.
        homeID is the ID of the planet which is the empire's homeworld
        the empire will be created, and the given planetID added to its
        list of owned planets.  This will only set up the data in Empire.  It is the caller's 
        responsibility to make sure that universe updates planet ownership.
        I do this because GameCore may want to call this under a variety of
        circumstances, and I do not want it to be too specific.
    */
    Empire* CreateEmpire(int id, const std::string& name, const std::string& player_name, const GG::Clr& color, int planetID);

    /// Removes all traces of the Empire with the specified ID.
    /**
       Removes all traces of the empire with the given ID.
       and deallocates that empire.  Pointers, references, and iterators
       to that empire will be invalidated.
      
       Nothing happens if an empire with the specified ID does not exist.
      
       Again, this method does not do anything to the universe,
       that is GameCore's responsibility.
      
       This method returns true if the empire was removed, false if it
       doesn't exist.
    */
    bool EliminateEmpire(int id);
    
    
    
    /// Creates an XML update of the Empire states to send to a client.
    /**
     * Creates an XMLElement containing the present Empire state, as visible
     * to the empire with the given ID.   The information that is 
     * included in the element is the information which the Empire with the given
     * ID would have available to it.  For v0.2, all information in the
     * server manager is included in the update element, but this may change in subsequent
     * versions in which the player's knowledge of other empires is limited.
     *  (For example, in future versions, the update generated for an empire will completely exclude empires with which the client does not have contact.)
     *
     * The returned element can be passed to the 
     * ClientEmpire::HandleEmpireUpdate() method to bring the ClientManager
     * in sync with the server manager.  
     *
     *
     * An std::runtime_error is thrown if no empire exists with the given ID
     */
	XMLElement CreateClientEmpireUpdate(int empire_id);
    
    /// Creates an XML encoding of an Empire's sitrep, to send to a Client.
    /**
     * Creates an XMLElement representing the list of sitrep events
     * for the empire with the given ID.  The returned element can be 
     * sent to the client and decoded with ClientEmpire::HandleSitrepUpdate()
     * 
     * An std::runtime_error is thrown if no empire exists with the given ID
     *
     *  This method is depracated.  It does nothing.
     */
	//XMLElement CreateClientSitrepUpdate(int empire_id);
    
    //@}
private:

    /// List of empire updates from previous turn
    /**
     * A set of XMLElements for the empires, which contain all information 
     * that the empire had at the beginning of the last turn. These are used by 
     * CreateClientEmpireUpdate() to produce an update XMLElement to
     * send off to the client. (a new one is made and diffed against the current one)
     *
     * This map is a map of EmpireID to the empire's update from lst turn and 
     * the corresponding entry for each empire is repopulated whenever CreateClientEmpireUpdate()
     * is called for that empire.
     */
    std::map<int, XMLElement> m_last_turn_empire_states;
};

inline std::pair<std::string, std::string> ServerEmpireManagerRevision()
{return std::pair<std::string, std::string>("$RCSfile$", "$Revision$");}

#endif // _ServerEmpireManager_h_
