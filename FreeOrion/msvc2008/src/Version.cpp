#include "../../util/Version.h"

namespace {
    static const std::string retval = "after v0.3.14 [Rev 3662]";
}

const std::string& FreeOrionVersionString()
{
    return retval;
}
