#include "../util/Version.h"

namespace {
    static const std::string retval = "v0.3.16 [SVN 4047] XCode";
}

const std::string& FreeOrionVersionString()
{
    return retval;
}
