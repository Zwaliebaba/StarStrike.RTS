#include "pch.h"
#include "ClientWorld.h"
#include "ClientNet.h"

namespace Neuron
{
  void ClientWorld::Startup()
  {
    m_objects.clear();
    m_interpolation.clear();
    m_pendingInputs.clear();
    m_inputSequence = 0;
    m_localPlayerId = INVALID_OBJECT_ID;
    m_lastSnapshotId = 0;
    m_hasTarget = false;
  }

  void ClientWorld::Shutdown()
  {
    m_objects.clear();
    m_interpolation.clear();
    m_pendingInputs.clear();
  }

  void ClientWorld::Update(float _deltaT)
  {
    PredictOwnedObject(_deltaT);
    InterpolateRemoteObjects(_deltaT);
  }

  void ClientWorld::OnSnapshotReceived(const Net::WorldSnapshotPacket& _snapshot)
  {
    m_lastSnapshotId = _snapshot.snapshotId;
    Reconcile(_snapshot);
  }

  uint32_t ClientWorld::IssueMoveTo(XMFLOAT3 _target)
  {
    uint32_t seq = ++m_inputSequence;

    PendingInput input;
    input.sequence = seq;
    input.type = Net::InputType::MoveTo;
    input.target = _target;
    m_pendingInputs.push_back(input);

    m_currentTarget = _target;
    m_hasTarget = true;

    // Send to server
    Net::ClientInputPacket pkt;
    pkt.header.type = Net::PacketType::ClientInput;
    pkt.header.sequence = static_cast<uint16_t>(seq & 0xFFFF);
    pkt.clientId = Client::ClientNet::GetClientId();
    pkt.inputSequence = seq;
    pkt.inputType = Net::InputType::MoveTo;
    pkt.targetPosition = _target;
    Client::ClientNet::SendInput(pkt);

    return seq;
  }

  uint32_t ClientWorld::IssueStop()
  {
    uint32_t seq = ++m_inputSequence;

    PendingInput input;
    input.sequence = seq;
    input.type = Net::InputType::Stop;
    m_pendingInputs.push_back(input);

    m_hasTarget = false;

    Net::ClientInputPacket pkt;
    pkt.header.type = Net::PacketType::ClientInput;
    pkt.header.sequence = static_cast<uint16_t>(seq & 0xFFFF);
    pkt.clientId = Client::ClientNet::GetClientId();
    pkt.inputSequence = seq;
    pkt.inputType = Net::InputType::Stop;
    Client::ClientNet::SendInput(pkt);

    return seq;
  }

  uint32_t ClientWorld::GetObjectCountByType(WorldObjectType _type) const
  {
    uint32_t count = 0;
    for (const auto& [id, obj] : m_objects)
    {
      if (obj.type == _type)
        count++;
    }
    return count;
  }

  void ClientWorld::PredictOwnedObject(float _deltaT)
  {
    if (m_localPlayerId == INVALID_OBJECT_ID)
      return;

    auto it = m_objects.find(m_localPlayerId);
    if (it == m_objects.end())
      return;

    auto& state = it->second;

    if (state.type != WorldObjectType::Ship)
      return;

    auto sc = static_cast<ShipClass>(state.subclass);
    float maxSpeed = 50.0f;
    float accel = 20.0f;
    if (static_cast<uint8_t>(sc) < static_cast<uint8_t>(ShipClass::Count))
    {
      maxSpeed = GetShipDef(sc).maxSpeed;
      accel = GetShipDef(sc).acceleration;
    }

    if (!m_hasTarget)
    {
      // Decelerate to stop (matches server UpdateShip)
      XMVECTOR vel = XMLoadFloat3(&state.velocity);
      float speed = Math::Length(vel);
      if (speed > 0.01f)
      {
        float decelAmount = accel * _deltaT;
        float newSpeed = (std::max)(0.0f, speed - decelAmount);
        vel = XMVectorScale(Math::Normalize(vel), newSpeed);
        XMStoreFloat3(&state.velocity, vel);
      }
      else
      {
        state.velocity = {0.f, 0.f, 0.f};
        state.flags &= ~static_cast<uint16_t>(ObjectFlags::Moving);
      }
    }
    else
    {
      XMVECTOR pos = XMLoadFloat3(&state.position);
      XMVECTOR target = XMLoadFloat3(&m_currentTarget);
      XMVECTOR toTarget = XMVectorSubtract(target, pos);
      float dist = Math::Length(toTarget);

      float stoppingDist = (maxSpeed * maxSpeed) / (2.0f * accel);

      if (dist < 1.0f)
      {
        m_hasTarget = false;
        state.velocity = {0.f, 0.f, 0.f};
        state.flags &= ~static_cast<uint16_t>(ObjectFlags::Moving);
      }
      else
      {
        XMVECTOR dir = Math::Normalize(toTarget);
        state.yaw = atan2f(Math::GetX(dir), Math::GetZ(dir));

        XMVECTOR vel = XMLoadFloat3(&state.velocity);
        float currentSpeed = Math::Length(vel);

        float desiredSpeed = maxSpeed;
        if (dist < stoppingDist)
          desiredSpeed = maxSpeed * (dist / stoppingDist);

        float newSpeed = currentSpeed + accel * _deltaT;
        if (newSpeed > desiredSpeed)
          newSpeed = desiredSpeed;

        vel = XMVectorScale(dir, newSpeed);
        XMStoreFloat3(&state.velocity, vel);
        state.flags |= static_cast<uint16_t>(ObjectFlags::Moving);
      }
    }

    // Integrate position
    XMVECTOR pos = XMLoadFloat3(&state.position);
    XMVECTOR vel = XMLoadFloat3(&state.velocity);
    pos = XMVectorAdd(pos, XMVectorScale(vel, _deltaT));
    XMStoreFloat3(&state.position, pos);
  }

  void ClientWorld::InterpolateRemoteObjects(float _deltaT)
  {
    constexpr float INTERP_DURATION = Net::SNAPSHOT_INTERVAL;

    for (auto& [id, interp] : m_interpolation)
    {
      if (id == m_localPlayerId)
        continue;

      interp.t += _deltaT / INTERP_DURATION;
      if (interp.t > 1.0f)
        interp.t = 1.0f;

      auto it = m_objects.find(id);
      if (it == m_objects.end())
        continue;

      XMVECTOR prevPos = XMLoadFloat3(&interp.previous.position);
      XMVECTOR targPos = XMLoadFloat3(&interp.target.position);
      XMVECTOR lerpPos = XMVectorLerp(prevPos, targPos, interp.t);
      XMStoreFloat3(&it->second.position, lerpPos);

      // Lerp yaw
      it->second.yaw = interp.previous.yaw + (interp.target.yaw - interp.previous.yaw) * interp.t;
    }
  }

  void ClientWorld::Reconcile(const Net::WorldSnapshotPacket& _snapshot)
  {
    // Remove acknowledged pending inputs
    auto removeEnd = std::remove_if(m_pendingInputs.begin(), m_pendingInputs.end(),
      [&](const PendingInput& input) { return input.sequence <= _snapshot.lastProcessedInput; });
    m_pendingInputs.erase(removeEnd, m_pendingInputs.end());

    // Track which objects are in this snapshot
    std::unordered_map<ObjectId, bool> seenObjects;

    for (uint16_t i = 0; i < _snapshot.objectCount; ++i)
    {
      const auto& sd = _snapshot.objects[i];
      seenObjects[sd.objectId] = true;

      ObjectState newState;
      newState.id        = sd.objectId;
      newState.type      = sd.objectType;
      newState.subclass  = sd.subclass;
      newState.flags     = sd.flags;
      newState.position  = sd.position;
      newState.velocity  = sd.velocity;
      newState.yaw       = sd.yaw;
      newState.hitpoints = sd.hitpoints;

      if (sd.objectId == m_localPlayerId)
      {
        // For owned object: blend toward server state to avoid hard snaps
        auto it = m_objects.find(sd.objectId);
        if (it != m_objects.end())
        {
          XMVECTOR clientPos = XMLoadFloat3(&it->second.position);
          XMVECTOR serverPos = XMLoadFloat3(&newState.position);
          float error = Math::Length(XMVectorSubtract(serverPos, clientPos));

          constexpr float SNAP_THRESHOLD = 30.0f;
          constexpr float BLEND_FACTOR = 0.3f;

          if (error > SNAP_THRESHOLD)
          {
            it->second = newState;
          }
          else
          {
            XMVECTOR blended = XMVectorLerp(clientPos, serverPos, BLEND_FACTOR);
            XMStoreFloat3(&it->second.position, blended);
            it->second.velocity = newState.velocity;
            it->second.flags = newState.flags;
            it->second.hitpoints = newState.hitpoints;
          }

          // Re-apply unacknowledged inputs
          for (const auto& input : m_pendingInputs)
          {
            if (input.type == Net::InputType::MoveTo)
            {
              m_currentTarget = input.target;
              m_hasTarget = true;
            }
            else if (input.type == Net::InputType::Stop)
            {
              m_hasTarget = false;
            }
          }
        }
        else
        {
          m_objects[sd.objectId] = newState;
        }
      }
      else
      {
        // Remote object: set up interpolation
        auto it = m_objects.find(sd.objectId);
        if (it != m_objects.end())
        {
          auto& interp = m_interpolation[sd.objectId];
          interp.previous = it->second;
          interp.target = newState;
          interp.t = 0.f;
        }
        else
        {
          // New object, snap directly
          m_objects[sd.objectId] = newState;
          auto& interp = m_interpolation[sd.objectId];
          interp.previous = newState;
          interp.target = newState;
          interp.t = 1.0f;
        }
      }
    }

    // Remove objects no longer in snapshot
    std::erase_if(m_objects, [&](const auto& pair) {
      return seenObjects.find(pair.first) == seenObjects.end();
    });
    std::erase_if(m_interpolation, [&](const auto& pair) {
      return seenObjects.find(pair.first) == seenObjects.end();
    });
  }
}
