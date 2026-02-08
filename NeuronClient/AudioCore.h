#pragma once

#include <xaudio2.h>

namespace Neuron::Audio
{
  class Core
  {
  public:
    static void Startup();
    static void Shutdown();

    static void CreateDeviceIndependentResources();
    static void ReleaseDeviceIndependentResources();

    static IXAudio2 *MusicEngine();
    static IXAudio2 *SoundEffectEngine();
    
    static void SuspendAudio();
    static void ResumeAudio();

  private:
    inline static bool m_audioAvailable;
    inline static com_ptr<IXAudio2> m_musicEngine;
    inline static com_ptr<IXAudio2> m_soundEffectEngine;
    inline static IXAudio2MasteringVoice *m_musicMasteringVoice;
    inline static IXAudio2MasteringVoice *m_soundEffectMasteringVoice;
  };
}