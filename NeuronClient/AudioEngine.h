#pragma once

namespace Neuron::Audio
{
  class SoundEffectInstance;

  class AudioEngine
  {
    public:
      explicit AudioEngine(AUDIO_ENGINE_FLAGS flags = AudioEngine_Default, _In_opt_ const WAVEFORMATEX* wfx = nullptr,
                           _In_opt_z_ const wchar_t* deviceId = nullptr,
                           AUDIO_STREAM_CATEGORY category = AudioCategory_GameEffects);

      virtual ~AudioEngine();

      bool Update();
      // Performs per-frame processing for the audio engine, returns false if in 'silent mode'

      bool Reset(_In_opt_ const WAVEFORMATEX* wfx = nullptr, _In_opt_z_ const wchar_t* deviceId = nullptr);
      // Reset audio engine from critical error/silent mode using a new device; can also 'migrate' the graph
      // Returns true if succesfully reset, false if in 'silent mode' due to no default device
      // Note: One shots are lost, all SoundEffectInstances are in the STOPPED state after successful reset

      void Suspend();
      void Resume();
      // Suspend/resumes audio processing (i.e. global pause/resume)

      void SetReverb(AUDIO_ENGINE_REVERB reverb);
      void SetReverb(_In_opt_ const XAUDIO2FX_REVERB_PARAMETERS* native);
      // Sets environmental reverb for 3D positional audio (if active)

      void SetMasteringLimit(int release, int loudness);
      // Sets the mastering volume limiter properties (if active)

      AudioStatistics GetStatistics() const;
      // Gathers audio engine statistics

      WAVEFORMATEXTENSIBLE GetOutputFormat() const;
      // Returns the format consumed by the mastering voice (which is the same as the device output if defaults are used)

      uint32_t GetChannelMask() const;
      // Returns the output channel mask

      int GetOutputChannels() const;
      // Returns the number of output channels

      bool IsAudioDevicePresent() const;
      // Returns true if the audio graph is operating normally, false if in 'silent mode'

      bool IsCriticalError() const;
      // Returns true if the audio graph is halted due to a critical error (which also places the engine into 'silent mode')

      // Voice pool management.
      void SetDefaultSampleRate(int sampleRate);
      // Sample rate for voices in the reuse pool (defaults to 44100)

      void SetMaxVoicePool(size_t maxOneShots, size_t maxInstances);
      // Maximum number of voices to allocate for one-shots and instances
      // Note: one-shots over this limit are ignored; too many instance voices throws an exception

      void TrimVoicePool();
      // Releases any currently unused voices

      void AllocateVoice(_In_ const WAVEFORMATEX* wfx, SOUND_EFFECT_INSTANCE_FLAGS flags, bool oneshot,
                         _Outptr_result_maybenull_ IXAudio2SourceVoice** voice);

      void DestroyVoice(_In_ IXAudio2SourceVoice* voice);
      // Should only be called for instance voices, not one-shots

      void RegisterNotify(_In_ IVoiceNotify* notify, bool usesUpdate);
      void UnregisterNotify(_In_ IVoiceNotify* notify, bool usesOneShots, bool usesUpdate);

      // XAudio2 interface access
      IXAudio2* GetInterface() const;
      IXAudio2MasteringVoice* GetMasterVoice() const;
      IXAudio2SubmixVoice* GetReverbVoice() const;
      X3DAUDIO_HANDLE& Get3DHandle() const;

    private:
      // Private implementation.
      class Impl;
      std::unique_ptr<Impl> pImpl;
  };
}
