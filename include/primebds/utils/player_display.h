#pragma once
#include <string>
#include <vector>
#include <cstddef>
namespace endstone { class CommandSender; }
namespace primebds { class PrimeBDS; }
namespace primebds::utils {
inline std::string rankedPlayerName(const std::string &prefix, const std::string &gamertag) {
    return prefix + gamertag + "§r";
}
inline std::vector<std::string> onlineListMessages(const std::vector<std::string> &names, std::size_t capacity) {
    std::vector<std::string> messages{"§aPlayers online: " + std::to_string(names.size()) + "/" + std::to_string(capacity) + "§r"};
    std::string line;
    for (const auto &name : names) {
        if (!line.empty()) line += "§7, §r";
        line += name;
    }
    if (!line.empty()) messages.push_back(line);
    return messages;
}
void sendRankedPlayerList(PrimeBDS &plugin, endstone::CommandSender &sender);
}
