#pragma once

#include "WorldTypes.h"
#include "NetProtocol.h"
#include "ShipDefs.h"
#include <unordered_map>
#include <vector>

namespace Neuron
{
  class ClientWorld
  {
  public:
    void Startup();
    void Shutdown();
    void Update(float _deltaT);

    void OnSnapshotReceived(const Net::WorldSnapshotPacket& _snapshot);
    uint32_t IssueMoveTo(XMFLOAT3 _target);
    uint32_t IssueStop();

    [[nodiscard]] const std::unordered_map<ObjectId, ObjectState>& GetObjects() const noexcept { return m_objects; }
    [[nodiscard]] ObjectId GetLocalPlayerId() const noexcept { return m_localPlayerId; }
    void SetLocalPlayerId(ObjectId _id) noexcept { m_localPlayerId = _id; }

    [[nodiscard]] uint32_t GetObjectCountByType(WorldObjectType _type) const;
    [[nodiscard]] uint32_t GetTotalObjectCount() const noexcept { return static_cast<uint32_t>(m_objects.size()); }

  private:
    void PredictOwnedObject(float _deltaT);
    void InterpolateRemoteObjects(float _deltaT);
    void Reconcile(const Net::WorldSnapshotPacket& _snapshot);
    void ApplyInput(ObjectState& _state, const struct PendingInput& _input, float _deltaT);

    struct PendingInput
    {
      uint32_t          sequence = 0;
      Net::InputType    type     = Net::InputType::None;
      XMFLOAT3          target   = {0.f, 0.f, 0.f};
    };

    std::unordered_map<ObjectId, ObjectState> m_objects;

    struct InterpolationState
    {
      ObjectState previous;
      ObjectState target;
      float       t = 0.f;
    };
    std::unordered_map<ObjectId, InterpolationState> m_interpolation;

    std::vector<PendingInput> m_pendingInputs;
    uint32_t m_inputSequence   = 0;
    ObjectId m_localPlayerId   = INVALID_OBJECT_ID;
    uint32_t m_lastSnapshotId  = 0;
    XMFLOAT3 m_currentTarget   = {0.f, 0.f, 0.f};
    bool     m_hasTarget       = false;
  };
}
