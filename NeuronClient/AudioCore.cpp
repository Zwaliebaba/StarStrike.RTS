#include "pch.h"
#include "AudioCore.h"

using namespace Neuron::Audio;

void Core::Startup()
{
  CreateDeviceIndependentResources();
}

void Core::Shutdown()
{
  ReleaseDeviceIndependentResources();
}

void Core::CreateDeviceIndependentResources()
{
  UINT32 flags = 0;

  check_hresult(XAudio2Create(m_musicEngine.put(), flags));

#if defined(_DEBUG)
  XAUDIO2_DEBUG_CONFIGURATION debugConfiguration = {0};
  debugConfiguration.BreakMask = XAUDIO2_LOG_ERRORS;
  debugConfiguration.TraceMask = XAUDIO2_LOG_ERRORS;
  m_musicEngine->SetDebugConfiguration(&debugConfiguration);
#endif

  HRESULT hr = m_musicEngine->CreateMasteringVoice(&m_musicMasteringVoice);
  if (FAILED(hr))
  {
    // Unable to create an audio device
    m_audioAvailable = false;
    return;
  }

  check_hresult(XAudio2Create(m_soundEffectEngine.put(), flags));

#if defined(_DEBUG)
  m_soundEffectEngine->SetDebugConfiguration(&debugConfiguration);
#endif

  check_hresult(m_soundEffectEngine->CreateMasteringVoice(&m_soundEffectMasteringVoice));

  m_audioAvailable = true;
}

void Core::ReleaseDeviceIndependentResources()
{
  if (m_musicMasteringVoice)
  {
    m_musicMasteringVoice->DestroyVoice();
    m_musicMasteringVoice = nullptr;
  }

  if (m_soundEffectMasteringVoice)
  {
    m_soundEffectMasteringVoice->DestroyVoice();
    m_soundEffectMasteringVoice = nullptr;
  }

  m_musicEngine = nullptr;
  m_soundEffectEngine = nullptr;
}

IXAudio2 *Core::MusicEngine() { return m_musicEngine.get(); }

IXAudio2 *Core::SoundEffectEngine() { return m_soundEffectEngine.get(); }

void Core::SuspendAudio()
{
  if (m_audioAvailable)
  {
    m_musicEngine->StopEngine();
    m_soundEffectEngine->StopEngine();
  }
}

void Core::ResumeAudio()
{
  if (m_audioAvailable)
  {
    check_hresult(m_musicEngine->StartEngine());
    check_hresult(m_soundEffectEngine->StartEngine());
  }
}