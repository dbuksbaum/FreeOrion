#include "MultiplayerCommon.h"


#include <fstream>

#include "OptionsDB.h"

namespace {
    // command-line options
    void AddOptions(OptionsDB& db)
    {
        db.Add<std::string>("settings-dir", "Sets the root directory for the settings and data files.", "default/");
    }
    bool temp_bool = RegisterOptions(&AddOptions);
}

/////////////////////////////////////////////////////
// Free Function(s)
/////////////////////////////////////////////////////
const std::vector<GG::Clr>& EmpireColors()
{
    static std::vector<GG::Clr> colors;
    if (colors.empty()) {
        GG::XMLDoc doc;
	std::string settings_dir = GetOptionsDB().Get<std::string>("settings-dir");
	if (!settings_dir.empty() && settings_dir[settings_dir.size() - 1] != '/')
	    settings_dir += '/';
        std::ifstream ifs((settings_dir + "empire_colors.xml").c_str());
        doc.ReadDoc(ifs);
        ifs.close();
        for (int i = 0; i < doc.root_node.NumChildren(); ++i) {
            colors.push_back(GG::Clr(doc.root_node.Child(i)));
        }
    }
    return colors;
}


/////////////////////////////////////////////////////
// SaveGameEmpireData
/////////////////////////////////////////////////////
SaveGameEmpireData::SaveGameEmpireData() 
{
}

SaveGameEmpireData::SaveGameEmpireData(const GG::XMLElement& elem)
{
    id = boost::lexical_cast<int>(elem.Child("id").Text());
    name = elem.Child("name").Text();
    player_name = elem.Child("player_name").Text();
    color = GG::Clr(elem.Child("color").Child("GG::Clr"));
}

GG::XMLElement SaveGameEmpireData::XMLEncode()
{
    using GG::XMLElement;
    using boost::lexical_cast;

    XMLElement retval("SaveGameEmpireData");
    retval.AppendChild(XMLElement("id", lexical_cast<std::string>(id)));
    retval.AppendChild(XMLElement("name", name));
    retval.AppendChild(XMLElement("player_name", player_name));
    retval.AppendChild(XMLElement("color", color.XMLEncode()));
    return retval;
}


/////////////////////////////////////////////////////
// PlayerSetupData
/////////////////////////////////////////////////////
PlayerSetupData::PlayerSetupData() :
    empire_name("Humans"),
    empire_color(GG::CLR_GRAY),
    save_game_empire_id(-1)
{
}

PlayerSetupData::PlayerSetupData(const GG::XMLElement& elem)
{
    empire_name = elem.Child("empire_name").Text();
    empire_color = GG::Clr(elem.Child("empire_color").Child("GG::Clr"));
    save_game_empire_id = boost::lexical_cast<int>(elem.Child("save_game_empire_id").Text());
}

GG::XMLElement PlayerSetupData::XMLEncode() const
{
    using GG::XMLElement;
    using boost::lexical_cast;

    XMLElement retval("PlayerSetupData");
    retval.AppendChild(XMLElement("empire_name", empire_name));
    retval.AppendChild(XMLElement("empire_color", empire_color.XMLEncode()));
    retval.AppendChild(XMLElement("save_game_empire_id", lexical_cast<std::string>(save_game_empire_id)));
    return retval;
}
