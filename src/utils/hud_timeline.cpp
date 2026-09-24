#include "primebds/plugin.h"

namespace primebds {

std::string PrimeBDS::hudStamp(const endstone::Player *player) const {
    const auto session = player ? hud_timeline.session(player->getUniqueId().str()) : 0;
    return "[HUDTest] t_ms=" + std::to_string(hud_timeline.elapsedMs()) +
           " session=" + std::to_string(session) + " player=\"" +
           (player ? player->getName() : "unassociated") + "\"";
}

void EventListener::onHudPacket(endstone::PacketReceiveEvent &event) {
    plugin_.observeHudPacket(event);
}

void PrimeBDS::observeHudPacket(endstone::PacketReceiveEvent &event) {
    const auto packet_id = event.getPacketId();
    if (packet_id != 113 && packet_id != 312) return;

    auto *player = event.getPlayer();
    const auto address = event.getAddress();
    const auto host = address.getHostname();
    const auto port = address.getPort();
    const auto subclient = event.getSubClientId();
    const auto payload = event.getPayload();
    const auto hex = utils::hudPayloadHex(payload);
    const auto stamp = hudStamp(player);
    if (packet_id == 113) {
        getLogger().info("{} event=packet.initialized.received stage=before-native-handling packet=113 address={}:{} subclient={} cancelled={} payload_bytes={} hex={} hex_truncated={}",
            stamp, host, port, subclient, event.isCancelled(), payload.size(), hex, payload.size() > 32);
        return;
    }

    const auto decoded = utils::decodeHudLoadingPacket(payload);
    const auto screen_id = decoded.screen_id ? std::to_string(*decoded.screen_id) : "absent";
    getLogger().info("{} event=packet.loading.received stage=before-native-handling packet=312 address={}:{} subclient={} cancelled={} decode={} type={} screen_id={} payload_bytes={} hex={} hex_truncated={} error={}",
        stamp, host, port, subclient, event.isCancelled(), decoded.decoded ? "expected-layout" : "undecoded",
        decoded.typeName(), decoded.decoded ? screen_id : "undecoded", payload.size(), hex, payload.size() > 32,
        decoded.error.empty() ? "none" : decoded.error);

    // Receipt is not proof of native processing or client readiness. This task
    // prints a scheduling milestone only; it never reads or writes player state.
    if (!decoded.decoded || decoded.type != 2) return;
    const auto uuid = player ? player->getUniqueId().str() : std::string{};
    const auto name = player ? player->getName() : std::string{"unassociated"};
    const auto session = hud_timeline.session(uuid);
    const auto generation = hud_timeline.generation();
    const auto received_ms = hud_timeline.elapsedMs();
    const auto cancelled = event.isCancelled();
    const auto task_id = std::make_shared<int>(-1);
    auto task = getServer().getScheduler().runTask(*this,
        [this, uuid, name, session, generation, received_ms, host, port, subclient, screen_id, cancelled, task_id]() {
            hud_marker_tasks.erase(*task_id);
            if (hud_timeline.generation() != generation) return;
            // An unassociated observation stays unassociated. Never re-resolve
            // its player and accidentally label a later login with an old packet.
            if (session && !hud_timeline.active(uuid, session)) return;
            getLogger().info("[HUDTest] t_ms={} session={} player=\"{}\" event=packet.loading.end.next-tick received_ms={} address={}:{} subclient={} screen_id={} cancelled={} marker-only=true",
                hud_timeline.elapsedMs(), session, name, received_ms, host, port, subclient, screen_id, cancelled);
        });
    if (task) {
        *task_id = task->getTaskId();
        hud_marker_tasks.insert(*task_id);
    } else {
        getLogger().warn("{} event=packet.loading.end.marker-not-scheduled", stamp);
    }
}

} // namespace primebds
