#pragma once
namespace primebds::utils {
template <typename HasPermission>
bool mayKeepGameMode(int mode, HasPermission has) {
    if (mode == 0) return true;
    if (has("minecraft.command.gamemode")) return true;
    if (mode == 1) return has("primebds.command.gmc") || has("primebds.command.gmt");
    if (mode == 2) return has("primebds.command.gma");
    if (mode == 3) return has("primebds.command.gmsp") || has("primebds.command.spectate");
    return false;
}
} // namespace primebds::utils
