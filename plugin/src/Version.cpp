#include "Version.h"
#include "BuildStamp.h"

#include <cstdio>

namespace the89th_version
{
const char* version() noexcept    { return THE89TH_VERSION; }
const char* buildStamp() noexcept { return THE89TH_BUILD_STAMP; }
const char* gitHash() noexcept    { return THE89TH_GIT_HASH; }

const char* banner() noexcept
{
    static char text[64];
    // ASCII only — hosts often mis-decode UTF-8 middle dots as "Â·".
    std::snprintf (text, sizeof (text), "%s  |  %s",
                   THE89TH_VERSION, THE89TH_GIT_HASH);
    return text;
}
} // namespace the89th_version
