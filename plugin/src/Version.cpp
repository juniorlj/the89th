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
    static char text[96];
    std::snprintf (text, sizeof (text), "%s  ·  %s  ·  %s",
                   THE89TH_VERSION, THE89TH_BUILD_STAMP, THE89TH_GIT_HASH);
    return text;
}
} // namespace the89th_version
