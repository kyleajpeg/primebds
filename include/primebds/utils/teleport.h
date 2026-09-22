#pragma once
#include <string>
namespace endstone { class Player; }
namespace primebds { class PrimeBDS; }
namespace primebds::utils {
bool teleportSaved(PrimeBDS &plugin, endstone::Player &player, const std::string &serialized);
bool teleportLogout(PrimeBDS &plugin, endstone::Player &player, const std::string &position, const std::string &dimension);
bool teleportHere(PrimeBDS &plugin, endstone::Player &player, double x, double y, double z);
}
