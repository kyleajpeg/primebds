#pragma once

// Endstone's PermissibleBase only needs has_value() for the server locator.
// No permission behavior is replaced by this test-only dependency shim.
namespace entt {
template <typename T> struct locator {
    inline static bool available = false;
    static bool has_value() { return available; }
};
} // namespace entt
