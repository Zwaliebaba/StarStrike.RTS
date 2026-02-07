#pragma once

#include <x3daudio.h>
#include <xapofx.h>
#include <xaudio2.h>
#include <xaudio2fx.h>

#include "AudioDefs.h"
#include "AudioEngine.h"
#include "FileSys.h"
#include "SoundCommon.h"

#pragma comment(lib,"xaudio2.lib")

namespace Neuron::Audio
{
  class SoundEffectInstance;

  class SoundEffect : public IVoiceNotify
  {
    public:
      SoundEffect(_In_ AudioEngine* _engine, _In_z_ const byte_buffer_t& _wavData);

      SoundEffect(_In_ const AudioEngine* _engine, _In_ const byte_buffer_t& wavData, _In_ const WAVEFORMATEX* _wfx,
                  _In_reads_bytes_(audioBytes) const uint8_t* startAudio, size_t audioBytes);

      SoundEffect(_In_ const AudioEngine* engine, _In_ const byte_buffer_t& wavData, _In_ const WAVEFORMATEX* wfx,
                  _In_reads_bytes_(audioBytes) const uint8_t* startAudio, size_t audioBytes, uint32_t loopStart,
                  uint32_t loopLength);

      ~SoundEffect() override;

      // IVoiceNotify
      void OnBufferEnd() override
      {
        InterlockedDecrement(&mOneShots);
      }

      void OnCriticalError() override { mOneShots = 0; }

      void Play();

      std::unique_ptr<SoundEffectInstance> CreateInstance(SOUND_EFFECT_INSTANCE_FLAGS flags = SoundEffectInstance_Default);

      bool IsInUse() const;

      size_t GetSampleSizeInBytes() const;
      // Returns size of wave audio data

      size_t GetSampleDuration() const;
      // Returns the duration in samples

      size_t GetSampleDurationMS() const;
      // Returns the duration in milliseconds

      const WAVEFORMATEX* GetFormat() const;

      void FillSubmitBuffer(_Out_ XAUDIO2_BUFFER& _buffer) const;

      void OnReset() override
      {
        // No action required
      }

      void OnUpdate() override
      {
        // We do not register for update notification
        DEBUG_ASSERT(false);
      }

      void OnDestroyEngine() override
      {
        mEngine = nullptr;
        mOneShots = 0;
      }

      void OnTrim() override
      {
        // No action required
      }

      void GatherStatistics(AudioStatistics& stats) const override
      {
        stats.playingOneShots += mOneShots;
        stats.audioBytes += mAudioBytes;
      }

      const WAVEFORMATEX* mWaveFormat;
      const uint8_t* mStartAudio;
      uint32_t mAudioBytes;
      uint32_t mLoopStart;
      uint32_t mLoopLength;
      AudioEngine* mEngine;
      std::list<SoundEffectInstance*> mInstances;
      uint32_t mOneShots;

    private:
      HRESULT Initialize(_In_ const AudioEngine* _engine, _Inout_ const byte_buffer_t& _wavData, _In_ const WAVEFORMATEX* _wfx,
                         _In_reads_bytes_(audioBytes) const uint8_t* _startAudio, size_t _audioBytes, uint32_t _loopStart,
                         uint32_t _loopLength);

      // Private interface
      void UnregisterInstance(_In_ const SoundEffectInstance* _instance);

      byte_buffer_t mWavData;

      friend class SoundEffectInstance;
  };

  class SoundEffectInstance : public IVoiceNotify, NonCopyable
  {
    public:
      ~SoundEffectInstance() override;

      void Play(bool loop = false);
      void Stop(bool immediate = true);
      void Pause();
      void Resume();

      void SetVolume(float volume);
      void SetPitch(float pitch);
      void SetPan(float pan);

      void Apply3D(const AudioListener& listener, const AudioEmitter& emitter);

      bool IsLooped() const;

      SoundState GetState();

      // Notifications.
      void OnDestroyParent();

      // IVoiceNotify
      void OnBufferEnd() override
      {
        // We don't register for this notification for SoundEffectInstances, so this should not be invoked
        DEBUG_ASSERT(false);
      }

      void OnCriticalError() override { mBase.OnCriticalError(); }

      void OnReset() override { mBase.OnReset(); }

      void OnUpdate() override
      {
        // We do not register for update notification
        DEBUG_ASSERT(false);
      }

      void OnDestroyEngine() override { mBase.OnDestroy(); }

      void OnTrim() override { mBase.OnTrim(); }

      void GatherStatistics(AudioStatistics& stats) const override { mBase.GatherStatistics(stats); }

      SoundEffectInstanceBase mBase;
      SoundEffect* mEffect;
      uint32_t mIndex;
      bool mLooped;

    private:
      friend std::unique_ptr<SoundEffectInstance> SoundEffect::CreateInstance(SOUND_EFFECT_INSTANCE_FLAGS);

      // Private constructors
      SoundEffectInstance(_In_ AudioEngine* engine, _In_ SoundEffect* effect, SOUND_EFFECT_INSTANCE_FLAGS flags);
  };
}
