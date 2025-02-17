// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors
#pragma once

#include "logging.hpp"

#include <filesystem>
#include <string_view>
#include <system_error>

namespace crow
{
namespace ibm_utils
{

inline bool createDirectory(std::string_view path)
{
    // Create persistent directory
    std::error_code ec;

    BMCWEB_LOG_DEBUG("Creating persistent directory : {}", path);

    bool dirCreated = std::filesystem::create_directories(path, ec);

    if (ec)
    {
        BMCWEB_LOG_ERROR("Failed to create persistent directory : {}", path);
        return false;
    }

    if (dirCreated)
    {
        // set the permission of the directory to 700
        BMCWEB_LOG_DEBUG("Setting the permission to 700");
        std::filesystem::perms permission = std::filesystem::perms::owner_all;
        std::filesystem::permissions(path, permission);
    }
    else
    {
        BMCWEB_LOG_DEBUG("{} already exists", path);
    }
    return true;
}

} // namespace ibm_utils
} // namespace crow
