/// @file target_selector.h
/// Target selector parsing (@a, @p, @r, @s, @n, @e) for Bedrock commands.

#pragma once

#include <endstone/endstone.hpp>
#include <string>
#include <vector>

namespace primebds { class PrimeBDS; }
namespace primebds::utils {

    std::vector<endstone::Actor *> getMatchingActors(PrimeBDS &plugin,
                                                     const std::string &selector,
                                                     endstone::CommandSender &origin);

    endstone::Player *resolvePlayerTarget(PrimeBDS &plugin,
                                          const std::string &arg,
                                          endstone::CommandSender &origin);

} // namespace primebds::utils
