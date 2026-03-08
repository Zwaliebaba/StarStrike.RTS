#include "pch.h"
#include "InputSystem.h"

#include <windowsx.h>

namespace Neuron::Client
{

bool InputSystem::processMessage(HWND /*hwnd*/, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam < 256)
            m_keysDown[static_cast<uint8_t>(wParam)] = true;
        return true;

    case WM_KEYUP:
        if (wParam < 256)
            m_keysDown[static_cast<uint8_t>(wParam)] = false;
        return true;

    case WM_MOUSEMOVE:
    {
        float x = static_cast<float>(GET_X_LPARAM(lParam));
        float y = static_cast<float>(GET_Y_LPARAM(lParam));
        m_mouseDelta.x += x - m_mousePos.x;
        m_mouseDelta.y += y - m_mousePos.y;
        m_mousePos = { x, y };
        return true;
    }

    case WM_MOUSEWHEEL:
    {
        float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
        m_wheelDelta += delta;
        return true;
    }

    case WM_LBUTTONDOWN:
    {
        // Left click: generate a move command to the clicked world position.
        // For now queue a Move command with the screen-space position;
        // world-space conversion will happen once the camera is available.
        CmdInput cmd;
        cmd.action  = ActionType::Move;
        cmd.targetX = static_cast<float>(GET_X_LPARAM(lParam));
        cmd.targetY = static_cast<float>(GET_Y_LPARAM(lParam));
        cmd.targetZ = 0.0f;
        m_pendingCommands.push_back(cmd);
        return true;
    }

    default:
        break;
    }
    return false;
}

std::vector<CmdInput> InputSystem::getPendingCommands(PlayerID localPlayerId)
{
    // WASD key-hold → continuous move commands
    if (m_keysDown['W'] || m_keysDown['A'] || m_keysDown['S'] || m_keysDown['D'])
    {
        CmdInput cmd;
        cmd.playerId = localPlayerId;
        cmd.action   = ActionType::Move;
        cmd.targetX  = 0.0f;
        cmd.targetY  = 0.0f;
        cmd.targetZ  = 0.0f;

        if (m_keysDown['W']) cmd.targetY += 1.0f;
        if (m_keysDown['S']) cmd.targetY -= 1.0f;
        if (m_keysDown['D']) cmd.targetX += 1.0f;
        if (m_keysDown['A']) cmd.targetX -= 1.0f;

        m_pendingCommands.push_back(cmd);
    }

    // R key → attack
    if (m_keysDown['R'])
    {
        CmdInput cmd;
        cmd.playerId = localPlayerId;
        cmd.action   = ActionType::Attack;
        m_pendingCommands.push_back(cmd);
    }

    // M key → mine
    if (m_keysDown['M'])
    {
        CmdInput cmd;
        cmd.playerId = localPlayerId;
        cmd.action   = ActionType::Mine;
        m_pendingCommands.push_back(cmd);
    }

    // Stamp player ID on all pending commands
    for (auto& cmd : m_pendingCommands)
        cmd.playerId = localPlayerId;

    auto result = std::move(m_pendingCommands);
    m_pendingCommands.clear();
    return result;
}

void InputSystem::resetFrame()
{
    m_mouseDelta = {};
    m_wheelDelta = 0.0f;
}

} // namespace Neuron::Client
