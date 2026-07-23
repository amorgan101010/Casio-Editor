#pragma once

#include <string>

namespace casioxw
{
    // Single project-wide semantic version, covering both casioxw_core and the app -- there is
    // one release train, not separate core/app version tracks. Bump this for any change that
    // warrants a release (see .github/workflows/ci.yml's version-policy job).
    inline constexpr const char* kVersion = "0.29.1";

    std::string version();
}
