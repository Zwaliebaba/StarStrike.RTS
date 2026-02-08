#pragma once
#include "WAVFileReader.h"

// Represents a loaded sound effect with its XAudio2 resources
class SoundEffect
{
public:
  SoundEffect() = default;

  ~SoundEffect() { Unload(); }

  SoundEffect(const SoundEffect &) = delete;
  SoundEffect &operator=(const SoundEffect &) = delete;
  SoundEffect(SoundEffect &&) = default;
  SoundEffect &operator=(SoundEffect &&) = default;

  bool Load(IXAudio2 *_engine, std::wstring_view _filename)
  {
    m_wavData = BinaryFile::ReadFile(_filename);
    if (m_wavData.empty())
    {
      DebugTrace("Failed to load sound file: {}", std::string(_filename.begin(), _filename.end()));
      return false;
    }

    const WAVEFORMATEX *wfx = nullptr;
    const uint8_t *audioData = nullptr;
    uint32_t audioBytes = 0;

    if (FAILED(Neuron::Audio::LoadWAVAudioInMemory(m_wavData, &wfx, &audioData, &audioBytes)))
    {
      DebugTrace("Failed to parse WAV file: {}", std::string(_filename.begin(), _filename.end()));
      m_wavData.clear();
      return false;
    }

    m_wfx = wfx;
    m_audioData = audioData;
    m_audioBytes = audioBytes;
    m_engine = _engine;

    return true;
  }

  void Unload()
  {
    Stop();
    if (m_sourceVoice)
    {
      m_sourceVoice->DestroyVoice();
      m_sourceVoice = nullptr;
    }
    m_wavData.clear();
    m_wfx = nullptr;
    m_audioData = nullptr;
    m_audioBytes = 0;
  }

  void Play(bool _loop = false)
  {
    if (!m_engine || !m_wfx || !m_audioData) return;

    // Create a new source voice for this playback
    if (m_sourceVoice)
    {
      m_sourceVoice->Stop();
      m_sourceVoice->FlushSourceBuffers();
    }
    else
    {
      if (FAILED(m_engine->CreateSourceVoice(&m_sourceVoice, m_wfx))) return;
    }

    XAUDIO2_BUFFER buffer = {};
    buffer.AudioBytes = m_audioBytes;
    buffer.pAudioData = m_audioData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.LoopCount = _loop ? XAUDIO2_LOOP_INFINITE : 0;

    m_sourceVoice->SubmitSourceBuffer(&buffer);
    m_sourceVoice->Start();
  }

  void Stop()
  {
    if (m_sourceVoice)
    {
      m_sourceVoice->Stop();
      m_sourceVoice->FlushSourceBuffers();
    }
  }

  bool IsLoaded() const { return !m_wavData.empty(); }

private:
  byte_buffer_t m_wavData;
  const WAVEFORMATEX *m_wfx = nullptr;
  const uint8_t *m_audioData = nullptr;
  uint32_t m_audioBytes = 0;
  IXAudio2 *m_engine = nullptr;
  IXAudio2SourceVoice *m_sourceVoice = nullptr;
};
