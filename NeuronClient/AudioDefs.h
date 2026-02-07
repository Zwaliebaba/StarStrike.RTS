#pragma once

namespace Neuron::Audio
{
  class SoundEffectInstance;

  struct AudioStatistics
  {
    size_t playingOneShots;        // Number of one-shot sounds currently playing
    size_t playingInstances;       // Number of sound effect instances currently playing
    size_t allocatedInstances;     // Number of SoundEffectInstance allocated
    size_t allocatedVoices;        // Number of XAudio2 voices allocated (standard, 3D, one-shots, and idle one-shots) 
    size_t allocatedVoices3d;      // Number of XAudio2 voices allocated for 3D
    size_t allocatedVoicesOneShot; // Number of XAudio2 voices allocated for one-shot sounds
    size_t allocatedVoicesIdle;    // Number of XAudio2 voices allocated for one-shot sounds but not currently in use
    size_t audioBytes;             // Total wave data (in bytes) in SoundEffects and in-memory WaveBanks
  };

  class IVoiceNotify
  {
    public:
      virtual ~IVoiceNotify() = default;
      virtual void OnBufferEnd() = 0;
      // Notfication that a voice buffer has finished
      // Note this is called from XAudio2's worker thread, so it should perform very minimal and thread-safe operations

      virtual void OnCriticalError() = 0;
      // Notification that the audio engine encountered a critical error

      virtual void OnReset() = 0;
      // Notification of an audio engine reset

      virtual void OnUpdate() = 0;
      // Notification of an audio engine per-frame update (opt-in)

      virtual void OnDestroyEngine() = 0;
      // Notification that the audio engine is being destroyed

      virtual void OnTrim() = 0;
      // Notification of a request to trim the voice pool

      virtual void GatherStatistics(AudioStatistics& stats) const = 0;
      // Contribute to statistics request
  };

  enum AUDIO_ENGINE_FLAGS
  {
    AudioEngine_Default             = 0x0,
    AudioEngine_EnvironmentalReverb = 0x1,
    AudioEngine_ReverbUseFilters    = 0x2,
    AudioEngine_UseMasteringLimiter = 0x4,
    AudioEngine_Debug               = 0x10000,
    AudioEngine_ThrowOnNoAudioHW    = 0x20000,
    AudioEngine_DisableVoiceReuse   = 0x40000,
  };

  DEFINE_ENUM_FLAG_OPERATORS(AUDIO_ENGINE_FLAGS);

  enum SOUND_EFFECT_INSTANCE_FLAGS
  {
    SoundEffectInstance_Default          = 0x0,
    SoundEffectInstance_Use3D            = 0x1,
    SoundEffectInstance_ReverbUseFilters = 0x2,
    SoundEffectInstance_NoSetPitch       = 0x4,
    SoundEffectInstance_UseRedirectLFE   = 0x10000,
  };

  DEFINE_ENUM_FLAG_OPERATORS(SOUND_EFFECT_INSTANCE_FLAGS);

  enum AUDIO_ENGINE_REVERB
  {
    Reverb_Off,
    Reverb_Default,
    Reverb_Generic,
    Reverb_Forest,
    Reverb_PaddedCell,
    Reverb_Room,
    Reverb_Bathroom,
    Reverb_LivingRoom,
    Reverb_StoneRoom,
    Reverb_Auditorium,
    Reverb_ConcertHall,
    Reverb_Cave,
    Reverb_Arena,
    Reverb_Hangar,
    Reverb_CarpetedHallway,
    Reverb_Hallway,
    Reverb_StoneCorridor,
    Reverb_Alley,
    Reverb_City,
    Reverb_Mountains,
    Reverb_Quarry,
    Reverb_Plain,
    Reverb_ParkingLot,
    Reverb_SewerPipe,
    Reverb_Underwater,
    Reverb_SmallRoom,
    Reverb_MediumRoom,
    Reverb_LargeRoom,
    Reverb_MediumHall,
    Reverb_LargeHall,
    Reverb_Plate,
    Reverb_MAX
  };

  enum SoundState : uint8_t
  {
    STOPPED = 0,
    PLAYING,
    PAUSED
  };

  struct AudioListener : X3DAUDIO_LISTENER
  {
    X3DAUDIO_CONE ListenerCone;

    AudioListener() noexcept
      : X3DAUDIO_LISTENER{},
        ListenerCone{}
    {
      OrientFront.z = -1.f;
      OrientTop.y = 1.f;
    }

    void XM_CALLCONV SetPosition(const XMVECTOR _v) noexcept { XMStoreFloat3(&Position, _v); }

    void XM_CALLCONV SetVelocity(const XMVECTOR _v) { XMStoreFloat3(&Velocity, _v); }

    void XM_CALLCONV SetOrientation(const XMVECTOR _forward, const XMVECTOR _up)
    {
      XMStoreFloat3(&OrientFront, _forward);
      XMStoreFloat3(&OrientTop, _up);
    }

    void XM_CALLCONV SetOrientationFromQuaternion(XMVECTOR quat)
    {
      XMVECTOR forward = XMVector3Rotate(g_XMIdentityR2, quat);
      XMStoreFloat3(&OrientFront, forward);

      XMVECTOR up = XMVector3Rotate(g_XMIdentityR1, quat);
      XMStoreFloat3(&OrientTop, up);
    }

    void XM_CALLCONV Update(XMVECTOR newPos, XMVECTOR upDir, float dt)
    {
      if (dt > 0.f)
      {
        XMVECTOR lastPos = XMLoadFloat3(&Position);

        XMVECTOR vDelta = (newPos - lastPos);
        XMVECTOR v = vDelta / dt;
        XMStoreFloat3(&Velocity, v);

        vDelta = XMVector3Normalize(vDelta);
        XMStoreFloat3(&OrientFront, vDelta);

        v = XMVector3Cross(upDir, vDelta);
        v = XMVector3Normalize(v);

        v = XMVector3Cross(vDelta, v);
        v = XMVector3Normalize(v);
        XMStoreFloat3(&OrientTop, v);

        XMStoreFloat3(&Position, newPos);
      }
    }

    void SetOmnidirectional() noexcept { pCone = nullptr; }

    void SetCone(const X3DAUDIO_CONE& listenerCone)
    {
      ListenerCone = listenerCone;
      pCone = &ListenerCone;
    }
  };

  struct AudioEmitter : X3DAUDIO_EMITTER
  {
    X3DAUDIO_CONE EmitterCone;
    float EmitterAzimuths[XAUDIO2_MAX_AUDIO_CHANNELS];

    AudioEmitter() noexcept
      : X3DAUDIO_EMITTER{},
        EmitterCone{},
        EmitterAzimuths{}
    {
      OrientFront.z = -1.f;

      OrientTop.y = ChannelRadius = CurveDistanceScaler = DopplerScaler = 1.f;

      ChannelCount = 1;
      pChannelAzimuths = EmitterAzimuths;

      InnerRadiusAngle = X3DAUDIO_PI / 4.0f;
    }

    void XM_CALLCONV SetPosition(XMVECTOR v) { XMStoreFloat3(&Position, v); }

    void XM_CALLCONV SetVelocity(XMVECTOR v) { XMStoreFloat3(&Velocity, v); }

    void XM_CALLCONV SetOrientation(XMVECTOR forward, XMVECTOR up)
    {
      XMStoreFloat3(&OrientFront, forward);
      XMStoreFloat3(&OrientTop, up);
    }

    void XM_CALLCONV SetOrientationFromQuaternion(XMVECTOR quat)
    {
      XMVECTOR forward = XMVector3Rotate(g_XMIdentityR2, quat);
      XMStoreFloat3(&OrientFront, forward);

      XMVECTOR up = XMVector3Rotate(g_XMIdentityR1, quat);
      XMStoreFloat3(&OrientTop, up);
    }

    void XM_CALLCONV Update(XMVECTOR newPos, XMVECTOR upDir, float dt)
    {
      if (dt > 0.f)
      {
        XMVECTOR lastPos = XMLoadFloat3(&Position);

        XMVECTOR vDelta = (newPos - lastPos);
        XMVECTOR v = vDelta / dt;
        XMStoreFloat3(&Velocity, v);

        vDelta = XMVector3Normalize(vDelta);
        XMStoreFloat3(&OrientFront, vDelta);

        v = XMVector3Cross(upDir, vDelta);
        v = XMVector3Normalize(v);

        v = XMVector3Cross(vDelta, v);
        v = XMVector3Normalize(v);
        XMStoreFloat3(&OrientTop, v);

        XMStoreFloat3(&Position, newPos);
      }
    }

    void SetOmnidirectional() noexcept { pCone = nullptr; }

    // Only used for single-channel emitters.
    void SetCone(const X3DAUDIO_CONE& emitterCone)
    {
      EmitterCone = emitterCone;
      pCone = &EmitterCone;
    }

    // Set multi-channel emitter azimuths based on speaker configuration geometry.
    void EnableDefaultMultiChannel(unsigned int channels, float radius = 1.f);

    // Set default volume, LFE, LPF, and reverb curves.
    void EnableDefaultCurves() noexcept;
    void EnableLinearCurves() noexcept;

    void EnableInverseSquareCurves() noexcept
    {
      pVolumeCurve = nullptr;
      pLFECurve = nullptr;
      pLPFDirectCurve = nullptr;
      pLPFReverbCurve = nullptr;
      pReverbCurve = nullptr;
    }
  };
}
