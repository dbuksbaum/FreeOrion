#include "../../util/Version.h"

namespace {
    static const std::string retval = "post-v0.3.15 [SVN 3777]";
}

const std::string& FreeOrionVersionString()
{
    return retval;
}
