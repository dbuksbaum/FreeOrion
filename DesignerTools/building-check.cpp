#include "../FreeOrion/util/MultiplayerCommon.h"
#include "../FreeOrion/util/OptionsDB.h"
#include "../FreeOrion/universe/Effect.h"
#include "../FreeOrion/universe/Building.h"

#include <iostream>

using boost::lexical_cast;
using std::string;

namespace {
    // HACK! BuildingTypeManager was lifted directly from Building.cpp.
    // It is reproduced here in order to supply iterator access to the loaded BuildingTypes.
    // Ensure that the lifted code is identical to that in Building.cpp before committing chages!

    // loads and stores BuildingTypes specified in [settings-dir]/buildings.xml
    class BuildingTypeManager
    {
    public:
        typedef std::map<std::string, BuildingType*>::const_iterator iterator;

        BuildingTypeManager()
        {
            std::string settings_dir = GetOptionsDB().Get<std::string>("settings-dir");
            if (!settings_dir.empty() && settings_dir[settings_dir.size() - 1] != '/')
                settings_dir += '/';
            std::ifstream ifs((settings_dir + "buildings.xml").c_str());
            GG::XMLDoc doc;
            doc.ReadDoc(ifs);
            for (GG::XMLElement::const_child_iterator it = doc.root_node.child_begin(); it != doc.root_node.child_end(); ++it) {
                if (it->Tag() != "BuildingType")
                    throw std::runtime_error("ERROR: Encountered non-BuildingType in buildings.xml!");
                BuildingType* building_type = 0;
                try {
                    building_type = new BuildingType(*it);
                } catch (const std::runtime_error& e) {
                    std::stringstream stream;
                    it->WriteElement(stream);
                    throw std::runtime_error(std::string("ERROR: \"") + e.what() + "\" encountered when loading this Building XML code:\n" + stream.str());
                }
                if (m_building_types.find(building_type->Name()) != m_building_types.end())
                    throw std::runtime_error(("ERROR: More than one building type in buildings.xml has the name " + building_type->Name()).c_str());
                m_building_types[building_type->Name()] = building_type;
            }
            ifs.close();
        }

        iterator begin() const {return m_building_types.begin();}
        iterator end() const {return m_building_types.end();}

        BuildingType* GetBuildingType(const std::string& name) const
        {
            std::map<std::string, BuildingType*>::const_iterator it = m_building_types.find(name);
            return it != m_building_types.end() ? it->second : 0;
        }

    private:
        std::map<std::string, BuildingType*> m_building_types;
    };
}


std::ostream& operator<<(std::ostream& os, const BuildingType& building)
{
    os << string(78, '*') << "\n";
    os << UserString(building.Name()) << "\n";
    os << UserString(building.Description()) << "\n";
    os << building.BuildCost() << " PP / " << building.BuildTime() << " turns\n";
    os << building.MaintenanceCost() << " trade / turn maintenance\n";
    os << "\n";
    if (building.Effects().empty()) {
        os << "[No effects.]\n";
    } else {
        int i = 0;
        for (std::vector<boost::shared_ptr<const Effect::EffectsGroup> >::const_iterator it = building.Effects().begin();
             it != building.Effects().end();
             ++it)
        {
            os << "Effect " << lexical_cast<string>(i++ + 1) << ":\n";
            const Effect::EffectsGroup::Description& desc = (*it)->GetDescription();
            os << desc.activation_description << "\n";
            os << desc.scope_description << "\n";
            for (unsigned int j = 0; j < desc.effect_descriptions.size(); ++j) {
                os << desc.effect_descriptions[j] << "\n";
            }
            os << "\n";
        }
    }
    os << "\n\n";
    return os;
}

int main(int argc, char* argv[])
{
    try {
        GetOptionsDB().AddFlag('h', "help", "Print this help message.");
        GetOptionsDB().SetFromCommandLine(argc, argv);
        if (GetOptionsDB().Get<bool>("help")) {
            GetOptionsDB().GetUsage(std::cerr);
            return 0;
        }
    } catch (...) {
        GetOptionsDB().GetUsage(std::cerr);
        return 1;
    }

    try {
        std::string settings_dir = GetOptionsDB().Get<std::string>("settings-dir");
        if (!settings_dir.empty() && settings_dir[settings_dir.size() - 1] != '/')
            settings_dir += '/';
        std::cout << "Loading buildings file \"" << settings_dir << "buildings.xml\" ...\n";
        BuildingTypeManager manager;
        std::cout << "Buildings file loaded.  Buildings are:\n\n\n";
        for (BuildingTypeManager::iterator it = manager.begin(); it != manager.end(); ++it) {
            std::cout << *it->second;
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "main() caught exception(std::invalid_arg): " << e.what();
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "main() caught exception(std::runtime_error): " << e.what();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "main() caught exception(std::exception): " << e.what();
        return 1;
    } catch (...) {
        std::cerr << "main() caught unknown exception.";
        return 1;
    }

    return 0;
}
