#pragma once

#include "Types.h"
#include "PacketTypes.h"

#include <cstdint>
#include <vector>

namespace Neuron::Client
{

/// Tracks keyboard and mouse state from Win32 messages.
/// Call processMessage() from the window procedure, then
/// getPendingCommands() once per frame to harvest queued commands.
class InputSystem
{
public:
    InputSystem() = default;

    /// Process a Win32 window message. Returns true if the message was consumed.
    bool processMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    /// Harvest all commands generated since the last call.
    /// The returned vector is moved out; the internal queue is cleared.
    std::vector<CmdInput> getPendingCommands(PlayerID localPlayerId);

    /// Query instantaneous key state.
    [[nodiscard]] bool isKeyDown(uint8_t vk) const noexcept { return m_keysDown[vk]; }

    /// Current mouse position in client coordinates.
    [[nodiscard]] Vec2 mousePos() const noexcept { return m_mousePos; }

    /// Mouse movement since last frame.
    [[nodiscard]] Vec2 mouseDelta() const noexcept { return m_mouseDelta; }

    /// Accumulated mouse wheel delta (positive = forward/up).
    [[nodiscard]] float wheelDelta() const noexcept { return m_wheelDelta; }

    /// Reset per-frame accumulators (call once at the start of each frame).
    void resetFrame();

private:
    bool  m_keysDown[256]{};
    Vec2  m_mousePos  = {};
    Vec2  m_prevMouse = {};
    Vec2  m_mouseDelta = {};
    float m_wheelDelta = 0.0f;

    // Queued commands for this frame
    std::vector<CmdInput> m_pendingCommands;
};

} // namespace Neuron::Client
