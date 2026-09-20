#pragma once
#include <string>
namespace primebds::utils {
enum StateReset { CreativeReset = 1, AdventureReset = 2, SpectatorReset = 4,
                  FlightReset = 8, SpeedReset = 16, NicknameReset = 32, GodReset = 64 };
template <typename HasPermission>
bool mayKeepGameMode(int mode, HasPermission has) {
    if (mode == 0) return true;
    if (has("minecraft.command.gamemode")) return true;
    if (mode == 1) return has("primebds.command.gmc") || has("primebds.command.gmt");
    if (mode == 2) return has("primebds.command.gma");
    if (mode == 3) return has("primebds.command.gmsp") || has("primebds.command.spectate");
    return false;
}
template <typename HasPermission>
int unavailableState(HasPermission has) {
    int reset = 0;
    if (!mayKeepGameMode(1, has)) reset |= CreativeReset;
    if (!mayKeepGameMode(2, has)) reset |= AdventureReset;
    if (!mayKeepGameMode(3, has)) reset |= SpectatorReset;
    if (!has("primebds.command.fly")) reset |= FlightReset;
    if (!has("primebds.command.speed")) reset |= SpeedReset;
    if (!has("primebds.command.nickname") && !has("primebds.command.nickname.other")) reset |= NicknameReset;
    if (!has("primebds.command.god") && !has("primebds.command.god.other")) reset |= GodReset;
    return reset;
}
inline bool modeWasRevoked(int mode, int resets) {
    return (mode == 1 && (resets & CreativeReset)) || (mode == 2 && (resets & AdventureReset)) ||
           (mode == 3 && (resets & SpectatorReset));
}
} // namespace primebds::utils
