#include "../FreeOrion/util/MultiplayerCommon.h"
#include "../FreeOrion/util/OptionsDB.h"
#include "../FreeOrion/universe/Effect.h"
#include "../FreeOrion/universe/Tech.h"

#include <iostream>

using boost::lexical_cast;
using std::string;

std::ostream& operator<<(std::ostream& os, const Tech& tech)
{
    os << string(78, '*') << "\n";
    os << UserString(tech.Name()) << "\n";
    os << "[" << UserString(lexical_cast<string>(tech.Type())) << " tech]\n";
    os << UserString(tech.Description()) << "\n";
    os << UserString(tech.Category()) << "\n";
    os << tech.ResearchCost() << " RP / " << tech.ResearchTurns() << " turns\n";
    os << "\n";
    if (tech.Prerequisites().empty()) {
        os << "[No prerequisites.]\n";
    } else {
        os << "Prerequisites:\n";
        for (std::set<string>::const_iterator it = tech.Prerequisites().begin();
             it != tech.Prerequisites().end();
             ++it)
        {
            os << "    " << UserString(*it) << "\n";
        }
    }
    os << "\n";
    if (tech.UnlockedItems().empty()) {
        os << "[No unlocked items.]\n";
    } else {
        os << "Unlocked items:\n";
        for (std::vector<Tech::ItemSpec>::const_iterator it = tech.UnlockedItems().begin();
             it != tech.UnlockedItems().end();
             ++it)
        {
            os << "    " << UserString(lexical_cast<string>(it->type))
               << ": " << UserString(it->name) << "\n";
        }
    }
    os << "\n";
    if (tech.Effects().empty()) {
        os << "[No effects.]\n";
    } else {
        int i = 0;
        for (std::vector<const Effect::EffectsGroup*>::const_iterator it = tech.Effects().begin();
             it != tech.Effects().end();
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
        std::cout << "Loading tech tree file \"" << settings_dir << "techs.xml\" ...\n";
        TechManager& manager = GetTechManager();
        std::cout << "Tech tree loaded.  Techs are:\n\n\n";
        for (TechManager::iterator it = manager.begin(); it != manager.end(); ++it) {
            std::cout << **it;
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
