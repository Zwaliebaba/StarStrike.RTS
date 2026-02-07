#include "pch.h"
#include "SoundCommon.h"
#include "WAVFileReader.h"

using namespace Neuron::Audio;

_Use_decl_annotations_ SoundEffect::SoundEffect(AudioEngine* _engine, const byte_buffer_t& _wavData)
  : mWaveFormat(nullptr),
    mStartAudio(nullptr),
    mAudioBytes(0),
    mLoopStart(0),
    mLoopLength(0),
    mEngine(_engine),
    mOneShots(0)
{
  DEBUG_ASSERT(mEngine != nullptr);
  mEngine->RegisterNotify(this, false);

  WAVData wavInfo;
  HRESULT hr = LoadWAVAudioInMemoryEx(_wavData, wavInfo);
  if (FAILED(hr))
  {
    DebugTrace(L"ERROR: SoundEffect failed ({:08X}) to load from buffer\n", hr);
    throw std::exception("SoundEffect");
  }

  hr = Initialize(_engine, _wavData, wavInfo.m_wfx, wavInfo.startAudio, wavInfo.audioBytes, wavInfo.loopStart, wavInfo.loopLength);

  if (FAILED(hr))
  {
    DebugTrace(L"ERROR: SoundEffect failed ({:08X}) to intialize from buffer\n", hr);
    throw std::exception("SoundEffect");
  }
}

_Use_decl_annotations_ SoundEffect::SoundEffect(const AudioEngine* _engine, const byte_buffer_t& wavData, const WAVEFORMATEX* _wfx,
                                                const uint8_t* startAudio, size_t audioBytes)
{
  HRESULT hr = Initialize(_engine, wavData, _wfx, startAudio, audioBytes, 0, 0);
  if (FAILED(hr))
  {
    DebugTrace("ERROR: SoundEffect failed ({:08X}) to intialize\n", hr);
    throw std::exception("SoundEffect");
  }
}

_Use_decl_annotations_ SoundEffect::SoundEffect(const AudioEngine* engine, const byte_buffer_t& wavData, const WAVEFORMATEX* wfx,
                                                const uint8_t* startAudio, size_t audioBytes, uint32_t loopStart,
                                                uint32_t loopLength)
{
  HRESULT hr = Initialize(engine, wavData, wfx, startAudio, audioBytes, loopStart, loopLength);
  if (FAILED(hr))
  {
    DebugTrace("ERROR: SoundEffect failed ({:08X}) to intialize\n", hr);
    throw std::exception("SoundEffect");
  }
}

SoundEffect::~SoundEffect()
{
  if (!mInstances.empty())
  {
    DebugTrace("WARNING: Destroying SoundEffect with {} outstanding SoundEffectInstances\n", mInstances.size());

    for (auto it = mInstances.begin(); it != mInstances.end(); ++it)
    {
      DEBUG_ASSERT(*it != 0);
      (*it)->OnDestroyParent();
    }

    mInstances.clear();
  }

  if (mOneShots > 0)
    DebugTrace("WARNING: Destroying SoundEffect with {} outstanding one shot effects\n", mOneShots);

  if (mEngine)
  {
    mEngine->UnregisterNotify(this, true, false);
    mEngine = nullptr;
  }
}

void SoundEffect::Play()
{
  IXAudio2SourceVoice* voice = nullptr;
  mEngine->AllocateVoice(mWaveFormat, SoundEffectInstance_Default, true, &voice);

  if (!voice)
    return;

  HRESULT hr = voice->Start(0);
  check_hresult(hr);

  XAUDIO2_BUFFER buffer = {};
  buffer.AudioBytes = mAudioBytes;
  buffer.pAudioData = mStartAudio;
  buffer.Flags = XAUDIO2_END_OF_STREAM;
  buffer.pContext = this;

  hr = voice->SubmitSourceBuffer(&buffer, nullptr);
  if (FAILED(hr))
  {
    DebugTrace("ERROR: SoundEffect failed ({:08X}) when submitting buffer:\n", hr);
    DebugTrace("\tFormat Tag {}, {} channels, {}-bit, {} Hz, {} bytes\n", mWaveFormat->wFormatTag, mWaveFormat->nChannels,
               mWaveFormat->wBitsPerSample, mWaveFormat->nSamplesPerSec, mAudioBytes);
    throw std::exception("SubmitSourceBuffer");
  }

  InterlockedIncrement(&mOneShots);
}

std::unique_ptr<SoundEffectInstance> SoundEffect::CreateInstance(SOUND_EFFECT_INSTANCE_FLAGS flags)
{
  auto effect = NEW SoundEffectInstance(mEngine, this, flags);
  DEBUG_ASSERT(effect != nullptr);
  mInstances.emplace_back(effect);
  return std::unique_ptr<SoundEffectInstance>(effect);
}

HRESULT SoundEffect::Initialize(const AudioEngine* _engine, const byte_buffer_t& _wavData, const WAVEFORMATEX* _wfx,
                                const uint8_t* _startAudio, const size_t _audioBytes, const uint32_t _loopStart,
                                const uint32_t _loopLength)
{
  if (!_engine || !IsValid(_wfx) || !_startAudio || !_audioBytes)
    return E_INVALIDARG;

  if (_audioBytes > 0xFFFFFFFF)
    return E_INVALIDARG;

  switch (GetFormatTag(_wfx))
  {
    case WAVE_FORMAT_PCM:
    case WAVE_FORMAT_IEEE_FLOAT:
    case WAVE_FORMAT_ADPCM:
      // Take ownership of the buffer
      mWavData = _wavData;

      // WARNING: We assume the wfx and startAudio parameters are pointers into the wavData memory buffer
      mWaveFormat = _wfx;
      mStartAudio = _startAudio;
      break;

    default:
    {
      DebugTrace("ERROR: SoundEffect encountered an unsupported format tag ({})\n", _wfx->wFormatTag);
      return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }
  }

  mAudioBytes = static_cast<uint32_t>(_audioBytes);
  mLoopStart = _loopStart;
  mLoopLength = _loopLength;

  return S_OK;
}

void SoundEffect::UnregisterInstance(_In_ const SoundEffectInstance* _instance)
{
  const auto it = std::ranges::find(mInstances, _instance);
  if (it == mInstances.end())
    return;

  mInstances.erase(it);
}

// Public accessors.
bool SoundEffect::IsInUse() const { return (mOneShots > 0) || !mInstances.empty(); }

size_t SoundEffect::GetSampleSizeInBytes() const { return mAudioBytes; }

size_t SoundEffect::GetSampleDuration() const
{
  if (!mWaveFormat || !mWaveFormat->nChannels)
    return 0;

  switch (GetFormatTag(mWaveFormat))
  {
    case WAVE_FORMAT_ADPCM:
    {
      auto adpcmFmt = reinterpret_cast<const ADPCMWAVEFORMAT*>(mWaveFormat);

      uint64_t duration = static_cast<uint64_t>(mAudioBytes / adpcmFmt->wfx.nBlockAlign) * adpcmFmt->wSamplesPerBlock;
      if (int partial = mAudioBytes % adpcmFmt->wfx.nBlockAlign)
      {
        if (partial >= (7 * adpcmFmt->wfx.nChannels))
          duration += (partial * 2 / adpcmFmt->wfx.nChannels - 12);
      }
      return duration;
    }

    default:
      if (mWaveFormat->wBitsPerSample > 0)
      {
        return static_cast<uint64_t>(mAudioBytes) * 8 / static_cast<uint64_t>(mWaveFormat->wBitsPerSample * mWaveFormat->nChannels);
      }
  }

  return 0;
}

size_t SoundEffect::GetSampleDurationMS() const
{
  if (!mWaveFormat || !mWaveFormat->nSamplesPerSec)
    return 0;

  uint64_t samples = GetSampleDuration();
  return samples * 1000 / mWaveFormat->nSamplesPerSec;
}

const WAVEFORMATEX* SoundEffect::GetFormat() const { return mWaveFormat; }

void SoundEffect::FillSubmitBuffer(_Out_ XAUDIO2_BUFFER& _buffer) const
{
  _buffer = {};
  _buffer.AudioBytes = mAudioBytes;
  _buffer.pAudioData = mStartAudio;
  _buffer.LoopBegin = mLoopStart;
  _buffer.LoopLength = mLoopLength;
}
