#include "pch.h"
#include "Audio.h"

using namespace Neuron::Audio;

namespace
{
  struct EngineCallback : IXAudio2EngineCallback
  {
    EngineCallback()
    {
      mCriticalError = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
      if (!mCriticalError)
        throw std::exception("CreateEvent");
    };

    virtual ~EngineCallback() { CloseHandle(mCriticalError); }

    STDMETHOD_(void, OnProcessingPassStart)() override {}
    STDMETHOD_(void, OnProcessingPassEnd)() override {}

    STDMETHOD_(void, OnCriticalError)([[maybe_unused]] THIS_ HRESULT error) override
    {
      DebugTrace("ERROR: AudioEngine encountered critical error ({:08X})\n", error);
      SetEvent(mCriticalError);
    }

    HANDLE mCriticalError;
  };

  struct VoiceCallback : IXAudio2VoiceCallback
  {
    VoiceCallback()
    {
      mBufferEnd = CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
      if (!mBufferEnd)
        throw std::exception("CreateEvent");
    }

    ~VoiceCallback() { CloseHandle(mBufferEnd); }

    STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32) override {}
    STDMETHOD_(void, OnVoiceProcessingPassEnd)() override {}
    STDMETHOD_(void, OnStreamEnd)() override {}
    STDMETHOD_(void, OnBufferStart)(void*) override {}

    STDMETHOD_(void, OnBufferEnd)(void* context) override
    {
      if (context)
      {
        auto inotify = static_cast<IVoiceNotify*>(context);
        inotify->OnBufferEnd();
        SetEvent(mBufferEnd);
      }
    }

    STDMETHOD_(void, OnLoopEnd)(void*) override {}
    STDMETHOD_(void, OnVoiceError)(void*, HRESULT) override {}

    HANDLE mBufferEnd;
  };

  const XAUDIO2FX_REVERB_I3DL2_PARAMETERS gReverbPresets[] = {
    XAUDIO2FX_I3DL2_PRESET_DEFAULT,             // Reverb_Off
    XAUDIO2FX_I3DL2_PRESET_DEFAULT,             // Reverb_Default
    XAUDIO2FX_I3DL2_PRESET_GENERIC,             // Reverb_Generic
    XAUDIO2FX_I3DL2_PRESET_FOREST,              // Reverb_Forest
    XAUDIO2FX_I3DL2_PRESET_PADDEDCELL,          // Reverb_PaddedCell
    XAUDIO2FX_I3DL2_PRESET_ROOM,                // Reverb_Room
    XAUDIO2FX_I3DL2_PRESET_BATHROOM,            // Reverb_Bathroom
    XAUDIO2FX_I3DL2_PRESET_LIVINGROOM,          // Reverb_LivingRoom
    XAUDIO2FX_I3DL2_PRESET_STONEROOM,           // Reverb_StoneRoom
    XAUDIO2FX_I3DL2_PRESET_AUDITORIUM,          // Reverb_Auditorium
    XAUDIO2FX_I3DL2_PRESET_CONCERTHALL,         // Reverb_ConcertHall
    XAUDIO2FX_I3DL2_PRESET_CAVE,                // Reverb_Cave
    XAUDIO2FX_I3DL2_PRESET_ARENA,               // Reverb_Arena
    XAUDIO2FX_I3DL2_PRESET_HANGAR,              // Reverb_Hangar
    XAUDIO2FX_I3DL2_PRESET_CARPETEDHALLWAY,     // Reverb_CarpetedHallway
    XAUDIO2FX_I3DL2_PRESET_HALLWAY,             // Reverb_Hallway
    XAUDIO2FX_I3DL2_PRESET_STONECORRIDOR,       // Reverb_StoneCorridor
    XAUDIO2FX_I3DL2_PRESET_ALLEY,               // Reverb_Alley
    XAUDIO2FX_I3DL2_PRESET_CITY,                // Reverb_City
    XAUDIO2FX_I3DL2_PRESET_MOUNTAINS,           // Reverb_Mountains
    XAUDIO2FX_I3DL2_PRESET_QUARRY,              // Reverb_Quarry
    XAUDIO2FX_I3DL2_PRESET_PLAIN,               // Reverb_Plain
    XAUDIO2FX_I3DL2_PRESET_PARKINGLOT,          // Reverb_ParkingLot
    XAUDIO2FX_I3DL2_PRESET_SEWERPIPE,           // Reverb_SewerPipe
    XAUDIO2FX_I3DL2_PRESET_UNDERWATER,          // Reverb_Underwater
    XAUDIO2FX_I3DL2_PRESET_SMALLROOM,           // Reverb_SmallRoom
    XAUDIO2FX_I3DL2_PRESET_MEDIUMROOM,          // Reverb_MediumRoom
    XAUDIO2FX_I3DL2_PRESET_LARGEROOM,           // Reverb_LargeRoom
    XAUDIO2FX_I3DL2_PRESET_MEDIUMHALL,          // Reverb_MediumHall
    XAUDIO2FX_I3DL2_PRESET_LARGEHALL,           // Reverb_LargeHall
    XAUDIO2FX_I3DL2_PRESET_PLATE,               // Reverb_Plate
  };

  unsigned int makeVoiceKey(_In_ const WAVEFORMATEX* wfx)
  {
    DEBUG_ASSERT(IsValid(wfx));

    if (wfx->nChannels > 0x7F)
      return 0;

    union KeyGen
    {
      struct
      {
        unsigned int tag           : 9;
        unsigned int channels      : 7;
        unsigned int bitsPerSample : 8;
      } pcm;

      struct
      {
        unsigned int tag             : 9;
        unsigned int channels        : 7;
        unsigned int samplesPerBlock : 16;
      } adpcm;

      unsigned int key;
    } result;

    static_assert(sizeof(KeyGen) == sizeof(unsigned int), "KeyGen is invalid");

    result.key = 0;

    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
      // We reuse EXTENSIBLE only if it is equivalent to the standard form
      auto wfex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
      if (wfex->Samples.wValidBitsPerSample != 0 && wfex->Samples.wValidBitsPerSample != wfx->wBitsPerSample)
        return 0;

      if (wfex->dwChannelMask != 0 && wfex->dwChannelMask != GetDefaultChannelMask(wfx->nChannels))
        return 0;
    }

    uint32_t tag = GetFormatTag(wfx);
    switch (tag)
    {
      case WAVE_FORMAT_PCM:
        static_assert(WAVE_FORMAT_PCM < 0x1ff, "KeyGen tag is too small");
        result.pcm.tag = WAVE_FORMAT_PCM;
        result.pcm.channels = wfx->nChannels;
        result.pcm.bitsPerSample = wfx->wBitsPerSample;
        break;

      case WAVE_FORMAT_IEEE_FLOAT:
        static_assert(WAVE_FORMAT_IEEE_FLOAT < 0x1ff, "KeyGen tag is too small");

        if (wfx->wBitsPerSample != 32)
          return 0;

        result.pcm.tag = WAVE_FORMAT_IEEE_FLOAT;
        result.pcm.channels = wfx->nChannels;
        result.pcm.bitsPerSample = 32;
        break;

      case WAVE_FORMAT_ADPCM:
        static_assert(WAVE_FORMAT_ADPCM < 0x1ff, "KeyGen tag is too small");
        result.adpcm.tag = WAVE_FORMAT_ADPCM;
        result.adpcm.channels = wfx->nChannels;

        {
          auto wfadpcm = reinterpret_cast<const ADPCMWAVEFORMAT*>(wfx);
          result.adpcm.samplesPerBlock = wfadpcm->wSamplesPerBlock;
        }
        break;

      default:
        return 0;
    }

    return result.key;
  }
}

static_assert(_countof(gReverbPresets) == Reverb_MAX, "AUDIO_ENGINE_REVERB enum mismatch");

class AudioEngine::Impl
{
  public:
    Impl()
      : mMasterVoice(nullptr),
        mReverbVoice(nullptr),
        masterChannelMask(0),
        masterChannels(0),
        masterRate(0),
        defaultRate(44100),
        maxVoiceOneshots(SIZE_MAX),
        maxVoiceInstances(SIZE_MAX),
        mCriticalError(false),
        mReverbEnabled(false),
        mEngineFlags(AudioEngine_Default),
        mCategory(AudioCategory_GameEffects),
        mVoiceInstances(0) { memset(&mX3DAudio, 0, X3DAUDIO_HANDLE_BYTESIZE); };

    HRESULT Initialize(AUDIO_ENGINE_FLAGS flags, _In_opt_ const WAVEFORMATEX* wfx, _In_opt_z_ const wchar_t* deviceId,
                       AUDIO_STREAM_CATEGORY category);

    HRESULT Reset(_In_opt_ const WAVEFORMATEX* wfx, _In_opt_z_ const wchar_t* deviceId);

    void SetSilentMode();

    void Shutdown();

    bool Update();

    void SetReverb(_In_opt_ const XAUDIO2FX_REVERB_PARAMETERS* native);

    void SetMasteringLimit(int release, int loudness);

    AudioStatistics GetStatistics() const;

    void TrimVoicePool();

    void AllocateVoice(_In_ const WAVEFORMATEX* wfx, SOUND_EFFECT_INSTANCE_FLAGS flags, bool oneshot,
                       _Outptr_result_maybenull_ IXAudio2SourceVoice** voice);
    void DestroyVoice(_In_ IXAudio2SourceVoice* voice) noexcept;

    void RegisterNotify(_In_ IVoiceNotify* notify, bool usesUpdate);
    void UnregisterNotify(_In_ IVoiceNotify* notify, bool oneshots, bool usesUpdate);

    com_ptr<IXAudio2> xaudio2;
    IXAudio2MasteringVoice* mMasterVoice;
    IXAudio2SubmixVoice* mReverbVoice;

    uint32_t masterChannelMask;
    uint32_t masterChannels;
    uint32_t masterRate;

    int defaultRate;
    size_t maxVoiceOneshots;
    size_t maxVoiceInstances;

    X3DAUDIO_HANDLE mX3DAudio;

    bool mCriticalError;
    bool mReverbEnabled;

    AUDIO_ENGINE_FLAGS mEngineFlags;

  private:
    using notifylist_t = std::set<IVoiceNotify*>;
    using oneshotlist_t = std::list<std::pair<unsigned int, IXAudio2SourceVoice*>>;
    using voicepool_t = std::unordered_multimap<unsigned int, IXAudio2SourceVoice*>;

    AUDIO_STREAM_CATEGORY mCategory;
    com_ptr<IUnknown> mReverbEffect;
    com_ptr<IUnknown> mVolumeLimiter;
    oneshotlist_t mOneShots;
    voicepool_t mVoicePool;
    notifylist_t mNotifyObjects;
    notifylist_t mNotifyUpdates;
    size_t mVoiceInstances;
    VoiceCallback mVoiceCallback;
    EngineCallback mEngineCallback;
};

_Use_decl_annotations_ HRESULT AudioEngine::Impl::Initialize(AUDIO_ENGINE_FLAGS flags, const WAVEFORMATEX* wfx,
                                                             const wchar_t* deviceId, AUDIO_STREAM_CATEGORY category)
{
  mEngineFlags = flags;
  mCategory = category;

  return Reset(wfx, deviceId);
}

_Use_decl_annotations_ HRESULT AudioEngine::Impl::Reset(const WAVEFORMATEX* wfx, const wchar_t* deviceId)
{
  if (wfx)
  {
    if (wfx->wFormatTag != WAVE_FORMAT_PCM)
      return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    if (!wfx->nChannels || wfx->nChannels > XAUDIO2_MAX_AUDIO_CHANNELS)
      return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    if (wfx->nSamplesPerSec < XAUDIO2_MIN_SAMPLE_RATE || wfx->nSamplesPerSec > XAUDIO2_MAX_SAMPLE_RATE)
      return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    // We don't use other data members of WAVEFORMATEX here to describe the device format, so no need to fully validate
  }

  DEBUG_ASSERT(!xaudio2);
  DEBUG_ASSERT(!mMasterVoice);
  DEBUG_ASSERT(!mReverbVoice);

  masterChannelMask = masterChannels = masterRate = 0;

  memset(&mX3DAudio, 0, X3DAUDIO_HANDLE_BYTESIZE);

  mCriticalError = false;
  mReverbEnabled = false;

  //
  // Create XAudio2 engine
  //
  UINT32 eflags = 0;
  HRESULT hr = XAudio2Create(xaudio2.put(), eflags);
  if (FAILED(hr))
    return hr;

  if (mEngineFlags & AudioEngine_Debug)
  {
    // To see the trace output, you need to view ETW logs for this application:
    //    Go to Control Panel, Administrative Tools, Event Viewer.
    //    View->Show Analytic and Debug Logs.
    //    Applications and Services Logs / Microsoft / Windows / XAudio2. 
    //    Right click on Microsoft Windows XAudio2 debug logging, Properties, then Enable Logging, and hit OK 
    XAUDIO2_DEBUG_CONFIGURATION debug = {0};
    debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
    debug.BreakMask = XAUDIO2_LOG_ERRORS;
    xaudio2->SetDebugConfiguration(&debug, nullptr);
    DebugTrace("INFO: XAudio 2.8 debugging enabled\n");
  }

  if (mEngineFlags & AudioEngine_DisableVoiceReuse)
    DebugTrace("INFO: Voice reuse is disabled\n");

  hr = xaudio2->RegisterForCallbacks(&mEngineCallback);
  if (FAILED(hr))
  {
    xaudio2 = nullptr;
    return hr;
  }

  //
  // Create mastering voice for device
  //

  hr = xaudio2->CreateMasteringVoice(&mMasterVoice, (wfx) ? wfx->nChannels : XAUDIO2_DEFAULT_CHANNELS,
                                     (wfx) ? wfx->nSamplesPerSec : XAUDIO2_DEFAULT_SAMPLERATE, 0, deviceId, nullptr, mCategory);
  if (FAILED(hr))
  {
    xaudio2 = nullptr;
    return hr;
  }

  DWORD dwChannelMask;
  hr = mMasterVoice->GetChannelMask(&dwChannelMask);
  if (FAILED(hr))
  {
    mMasterVoice = nullptr;
    xaudio2 = nullptr;
    return hr;
  }

  XAUDIO2_VOICE_DETAILS details;
  mMasterVoice->GetVoiceDetails(&details);

  masterChannelMask = dwChannelMask;
  masterChannels = details.InputChannels;
  masterRate = details.InputSampleRate;

  DebugTrace("INFO: mastering voice has {} channels, {} sample rate, {:08X} channel mask\n", masterChannels, masterRate,
             masterChannelMask);

  //
  // Setup mastering volume limiter (optional)
  //
  if (mEngineFlags & AudioEngine_UseMasteringLimiter)
  {
    FXMASTERINGLIMITER_PARAMETERS params = {0};
    params.Release = FXMASTERINGLIMITER_DEFAULT_RELEASE;
    params.Loudness = FXMASTERINGLIMITER_DEFAULT_LOUDNESS;

    hr = CreateFX(__uuidof(FXMasteringLimiter), mVolumeLimiter.put(), &params, sizeof(params));
    if (FAILED(hr))
    {
      mMasterVoice = nullptr;
      xaudio2 = nullptr;
      return hr;
    }

    XAUDIO2_EFFECT_DESCRIPTOR desc = {nullptr};
    desc.InitialState = TRUE;
    desc.OutputChannels = masterChannels;
    desc.pEffect = mVolumeLimiter.get();

    XAUDIO2_EFFECT_CHAIN chain = {1, &desc};
    hr = mMasterVoice->SetEffectChain(&chain);
    if (FAILED(hr))
    {
      mMasterVoice = nullptr;
      mVolumeLimiter = nullptr;
      xaudio2 = nullptr;
      return hr;
    }

#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
        hr = mMasterVoice->SetEffectParameters( 0, &params, sizeof(params) );
        if ( FAILED(hr) )
        {
            mMasterVoice = nullptr;
            mVolumeLimiter = nullptr;
            xaudio2 = nullptr;
            return hr;
        }
#endif

    DebugTrace("INFO: Mastering volume limiter enabled\n");
  }

  //
  // Setup environmental reverb for 3D audio (optional)
  //
  if (mEngineFlags & AudioEngine_EnvironmentalReverb)
  {
    UINT32 rflags = 0;
    hr = XAudio2CreateReverb(mReverbEffect.put(), rflags);
    if (FAILED(hr))
    {
      mMasterVoice = nullptr;
      mVolumeLimiter = nullptr;
      xaudio2 = nullptr;
      return hr;
    }

    XAUDIO2_EFFECT_DESCRIPTOR effects[] = {{mReverbEffect.get(), TRUE, 1}};
    XAUDIO2_EFFECT_CHAIN effectChain = {1, effects};

    mReverbEnabled = true;

    hr = xaudio2->CreateSubmixVoice(&mReverbVoice, 1, masterRate,
                                    (mEngineFlags & AudioEngine_ReverbUseFilters) ? XAUDIO2_VOICE_USEFILTER : 0, 0, nullptr,
                                    &effectChain);
    if (FAILED(hr))
    {
      mMasterVoice = nullptr;
      mReverbEffect = nullptr;
      mVolumeLimiter = nullptr;
      xaudio2 = nullptr;
      return hr;
    }

    XAUDIO2FX_REVERB_PARAMETERS native;
    ReverbConvertI3DL2ToNative(&gReverbPresets[Reverb_Default], &native);
    hr = mReverbVoice->SetEffectParameters(0, &native, sizeof(XAUDIO2FX_REVERB_PARAMETERS));
    if (FAILED(hr))
    {
      mMasterVoice = nullptr;
      mReverbVoice = nullptr;
      mReverbEffect = nullptr;
      mVolumeLimiter = nullptr;
      xaudio2 = nullptr;
      return hr;
    }

    DebugTrace("INFO: I3DL2 reverb effect enabled for 3D positional audio\n");
  }

  //
  // Setup 3D audio
  //
  constexpr float SPEEDOFSOUND = X3DAUDIO_SPEED_OF_SOUND;

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
  hr = X3DAudioInitialize(masterChannelMask, SPEEDOFSOUND, mX3DAudio);
  if (FAILED(hr))
  {
    mMasterVoice = nullptr;
    mReverbVoice = nullptr;
    mReverbEffect = nullptr;
    mVolumeLimiter = nullptr;
    xaudio2 = nullptr;
    return hr;
  }
#else
    X3DAudioInitialize( masterChannelMask, SPEEDOFSOUND, mX3DAudio );
#endif

  //
  // Inform any notify objects we are ready to go again
  //
  for (auto it = mNotifyObjects.begin(); it != mNotifyObjects.end(); ++it)
  {
    DEBUG_ASSERT(*it != 0);
    (*it)->OnReset();
  }

  return S_OK;
}

void AudioEngine::Impl::SetSilentMode()
{
  for (auto it = mNotifyObjects.begin(); it != mNotifyObjects.end(); ++it)
  {
    DEBUG_ASSERT(*it != 0);
    (*it)->OnCriticalError();
  }

  mOneShots.clear();
  mVoicePool.clear();

  mVoiceInstances = 0;

  mMasterVoice = nullptr;
  mReverbVoice = nullptr;

  mReverbEffect = nullptr;
  mVolumeLimiter = nullptr;
  xaudio2 = nullptr;
}

void AudioEngine::Impl::Shutdown()
{
  for (auto it = mNotifyObjects.begin(); it != mNotifyObjects.end(); ++it)
  {
    DEBUG_ASSERT(*it != 0);
    (*it)->OnDestroyEngine();
  }

  if (xaudio2)
  {
    xaudio2->UnregisterForCallbacks(&mEngineCallback);

    xaudio2->StopEngine();

    mOneShots.clear();
    mVoicePool.clear();

    mVoiceInstances = 0;

    mMasterVoice = nullptr;
    mReverbVoice = nullptr;

    mReverbEffect = nullptr;
    mVolumeLimiter = nullptr;
    xaudio2 = nullptr;

    masterChannelMask = masterChannels = masterRate = 0;

    mCriticalError = false;
    mReverbEnabled = false;

    memset(&mX3DAudio, 0, X3DAUDIO_HANDLE_BYTESIZE);
  }
}

bool AudioEngine::Impl::Update()
{
  if (!xaudio2)
    return false;

  HANDLE events[2] = {mEngineCallback.mCriticalError, mVoiceCallback.mBufferEnd};
  DWORD result = WaitForMultipleObjectsEx(2, events, FALSE, 0, FALSE);
  switch (result)
  {
    case WAIT_TIMEOUT:
      break;

    case WAIT_OBJECT_0:     // OnCritialError
      mCriticalError = true;

      SetSilentMode();
      return false;

    case WAIT_OBJECT_0 + 1: // OnBufferEnd
      // Scan for completed one-shot voices
      for (auto it = mOneShots.begin(); it != mOneShots.end();)
      {
        DEBUG_ASSERT(it->second != 0);

        XAUDIO2_VOICE_STATE xstate;
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
        it->second->GetState(&xstate, XAUDIO2_VOICE_NOSAMPLESPLAYED);
#else
            it->second->GetState( &xstate );
#endif

        if (!xstate.BuffersQueued)
        {
          it->second->Stop(0);
          if (it->first)
          {
            // Put voice back into voice pool for reuse since it has a non-zero voiceKey
#ifdef VERBOSE_TRACE
                    DebugTrace( "INFO: One-shot voice being saved for reuse ({:08X})\n", it->first );
#endif
            voicepool_t::value_type v(it->first, it->second);
            mVoicePool.emplace(v);
          }
          else
          {
            // Voice is to be destroyed rather than reused
#ifdef VERBOSE_TRACE
                    DebugTrace( "INFO: Destroying one-shot voice\n" );
#endif
            it->second->DestroyVoice();
          }
          it = mOneShots.erase(it);
        }
        else
          ++it;
      }
      break;

    case WAIT_FAILED:
      throw std::exception("WaitForMultipleObjects");
  }

  //
  // Inform any notify objects of updates
  //
  for (auto it = mNotifyUpdates.begin(); it != mNotifyUpdates.end(); ++it)
  {
    DEBUG_ASSERT(*it != 0);
    (*it)->OnUpdate();
  }

  return true;
}

_Use_decl_annotations_ void AudioEngine::Impl::SetReverb(const XAUDIO2FX_REVERB_PARAMETERS* native)
{
  if (!mReverbVoice)
    return;

  if (native)
  {
    if (!mReverbEnabled)
    {
      mReverbEnabled = true;
      mReverbVoice->EnableEffect(0);
    }

    mReverbVoice->SetEffectParameters(0, native, sizeof(XAUDIO2FX_REVERB_PARAMETERS));
  }
  else if (mReverbEnabled)
  {
    mReverbEnabled = false;
    mReverbVoice->DisableEffect(0);
  }
}

void AudioEngine::Impl::SetMasteringLimit(int release, int loudness)
{
  if (!mVolumeLimiter || !mMasterVoice)
    return;

  if ((release < FXMASTERINGLIMITER_MIN_RELEASE) || (release > FXMASTERINGLIMITER_MAX_RELEASE))
    throw std::out_of_range("AudioEngine::SetMasteringLimit");

  if ((loudness < FXMASTERINGLIMITER_MIN_LOUDNESS) || (loudness > FXMASTERINGLIMITER_MAX_LOUDNESS))
    throw std::out_of_range("AudioEngine::SetMasteringLimit");

  FXMASTERINGLIMITER_PARAMETERS params = {0};
  params.Release = static_cast<UINT32>(release);
  params.Loudness = static_cast<UINT32>(loudness);

  HRESULT hr = mMasterVoice->SetEffectParameters(0, &params, sizeof(params));
  check_hresult(hr);
}

AudioStatistics AudioEngine::Impl::GetStatistics() const
{
  AudioStatistics stats;
  memset(&stats, 0, sizeof(stats));

  stats.allocatedVoices = stats.allocatedVoicesOneShot = mOneShots.size() + mVoicePool.size();
  stats.allocatedVoicesIdle = mVoicePool.size();

  for (auto it = mNotifyObjects.begin(); it != mNotifyObjects.end(); ++it)
  {
    DEBUG_ASSERT(*it != 0);
    (*it)->GatherStatistics(stats);
  }

  DEBUG_ASSERT(stats.allocatedVoices == ( mOneShots.size() + mVoicePool.size() + mVoiceInstances ));

  return stats;
}

void AudioEngine::Impl::TrimVoicePool()
{
  for (auto it = mNotifyObjects.begin(); it != mNotifyObjects.end(); ++it)
  {
    DEBUG_ASSERT(*it != 0);
    (*it)->OnTrim();
  }

  for (auto it = mVoicePool.begin(); it != mVoicePool.end(); ++it)
  {
    DEBUG_ASSERT(it->second != 0);
    it->second->DestroyVoice();
  }
  mVoicePool.clear();
}

_Use_decl_annotations_ void AudioEngine::Impl::AllocateVoice(const WAVEFORMATEX* wfx, SOUND_EFFECT_INSTANCE_FLAGS flags,
                                                             bool oneshot, IXAudio2SourceVoice** voice)
{
  if (!wfx)
    throw std::exception("Wave format is required\n");

  // No need to call IsValid on wfx because CreateSourceVoice will do that

  if (!voice)
    throw std::exception("Voice pointer must be non-null");

  *voice = nullptr;

  if (!xaudio2 || mCriticalError)
    return;

  unsigned int voiceKey = 0;
  if (oneshot)
  {
    if (flags & (SoundEffectInstance_Use3D | SoundEffectInstance_ReverbUseFilters | SoundEffectInstance_NoSetPitch))
    {
      DebugTrace((flags & SoundEffectInstance_NoSetPitch)
                   ? "ERROR: One-shot voices must support pitch-shifting for voice reuse\n"
                   : "ERROR: One-use voices cannot use 3D positional audio\n");
      throw std::exception("Invalid flags for one-shot voice");
    }

#ifdef VERBOSE_TRACE
        if ( wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE )
        {
            DebugTrace( "INFO: Requesting one-shot: Format Tag EXTENSIBLE {}, {} channels, {}-bit, {} blkalign, {} Hz\n", GetFormatTag( wfx ), 
                        wfx->nChannels, wfx->wBitsPerSample, wfx->nBlockAlign, wfx->nSamplesPerSec );
        }
        else
        {
            DebugTrace( "INFO: Requesting one-shot: Format Tag {}, {} channels, {}-bit, {} blkalign, {} Hz\n", wfx->wFormatTag, 
                        wfx->nChannels, wfx->wBitsPerSample, wfx->nBlockAlign, wfx->nSamplesPerSec );
        }
#endif

    if (!(mEngineFlags & AudioEngine_DisableVoiceReuse))
    {
      voiceKey = makeVoiceKey(wfx);
      if (voiceKey != 0)
      {
        auto it = mVoicePool.find(voiceKey);
        if (it != mVoicePool.end())
        {
          // Found a matching (stopped) voice to reuse
          DEBUG_ASSERT(it->second != 0);
          *voice = it->second;
          mVoicePool.erase(it);
        }
        else if ((mVoicePool.size() + mOneShots.size() + 1) >= maxVoiceOneshots)
        {
          DebugTrace("WARNING: Too many one-shot voices in use (%Iu + %Iu >= %Iu); one-shot not played\n", mVoicePool.size(),
                     mOneShots.size() + 1, maxVoiceOneshots);
          return;
        }
        else
        {
          // makeVoiceKey already constrained the supported wfx formats to those supported for reuse

          char buff[64] = {0};
          auto wfmt = reinterpret_cast<WAVEFORMATEX*>(buff);

          uint32_t tag = GetFormatTag(wfx);
          switch (tag)
          {
            case WAVE_FORMAT_PCM:
              CreateIntegerPCM(wfmt, defaultRate, wfx->nChannels, wfx->wBitsPerSample);
              break;

            case WAVE_FORMAT_IEEE_FLOAT:
              CreateFloatPCM(wfmt, defaultRate, wfx->nChannels);
              break;

            case WAVE_FORMAT_ADPCM:
            {
              auto wfadpcm = reinterpret_cast<const ADPCMWAVEFORMAT*>(wfx);
              CreateADPCM(wfmt, 64, defaultRate, wfx->nChannels, wfadpcm->wSamplesPerBlock);
            }
            break;

#if defined(_XBOX_ONE) && defined(_TITLE)
                    case WAVE_FORMAT_XMA2:
                        CreateXMA2( wfmt, 64, defaultRate, wfx->nChannels, 65536, 2, 0 );
                        break;
#endif
          }

#ifdef VERBOSE_TRACE
                    DebugTrace( "INFO: Allocate reuse voice: Format Tag {}, {} channels, {}-bit, {} blkalign, {} Hz\n", wfmt->wFormatTag,
                                wfmt->nChannels, wfmt->wBitsPerSample, wfmt->nBlockAlign, wfmt->nSamplesPerSec );
#endif

          DEBUG_ASSERT(voiceKey == makeVoiceKey( wfmt ));

          HRESULT hr = xaudio2->CreateSourceVoice(voice, wfmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &mVoiceCallback, nullptr, nullptr);
          if (FAILED(hr))
          {
            DebugTrace("ERROR: CreateSourceVoice (reuse) failed with error {:08X}\n", hr);
            throw std::exception("CreateSourceVoice");
          }
        }

        DEBUG_ASSERT(*voice != 0);
        HRESULT hr = (*voice)->SetSourceSampleRate(wfx->nSamplesPerSec);
        if (FAILED(hr))
        {
          DebugTrace("ERROR: SetSourceSampleRate failed with error {:08X}\n", hr);
          throw std::exception("SetSourceSampleRate");
        }
      }
    }
  }

  if (!*voice)
  {
    if (oneshot)
    {
      if ((mVoicePool.size() + mOneShots.size() + 1) >= maxVoiceOneshots)
      {
        DebugTrace("WARNING: Too many one-shot voices in use (%Iu + %Iu >= %Iu); one-shot not played; see TrimVoicePool\n",
                   mVoicePool.size(), mOneShots.size() + 1, maxVoiceOneshots);
        return;
      }
    }
    else if ((mVoiceInstances + 1) >= maxVoiceInstances)
    {
      DebugTrace("ERROR: Too many instance voices (%Iu >= %Iu); see TrimVoicePool\n", mVoiceInstances + 1, maxVoiceInstances);
      throw std::exception("Too many instance voices");
    }

    UINT32 vflags = (flags & SoundEffectInstance_NoSetPitch) ? XAUDIO2_VOICE_NOPITCH : 0;

    HRESULT hr;
    if (flags & SoundEffectInstance_Use3D)
    {
      XAUDIO2_SEND_DESCRIPTOR sendDescriptors[2];
      sendDescriptors[0].Flags = sendDescriptors[1].Flags = (flags & SoundEffectInstance_ReverbUseFilters)
                                                              ? XAUDIO2_SEND_USEFILTER
                                                              : 0;
      sendDescriptors[0].pOutputVoice = mMasterVoice;
      sendDescriptors[1].pOutputVoice = mReverbVoice;
      const XAUDIO2_VOICE_SENDS sendList = {mReverbVoice ? 2U : 1U, sendDescriptors};

#ifdef VERBOSE_TRACE
            DebugTrace( "INFO: Allocate voice 3D: Format Tag {}, {} channels, {}-bit, {} blkalign, {} Hz\n", wfx->wFormatTag, 
                        wfx->nChannels, wfx->wBitsPerSample, wfx->nBlockAlign, wfx->nSamplesPerSec );
#endif

      hr = xaudio2->CreateSourceVoice(voice, wfx, vflags, XAUDIO2_DEFAULT_FREQ_RATIO, &mVoiceCallback, &sendList, nullptr);
    }
    else
    {
#ifdef VERBOSE_TRACE
            DebugTrace( "INFO: Allocate voice: Format Tag {}, {} channels, {}-bit, {} blkalign, {} Hz\n", wfx->wFormatTag, 
                        wfx->nChannels, wfx->wBitsPerSample, wfx->nBlockAlign, wfx->nSamplesPerSec );
#endif

      hr = xaudio2->CreateSourceVoice(voice, wfx, vflags, XAUDIO2_DEFAULT_FREQ_RATIO, &mVoiceCallback, nullptr, nullptr);
    }

    if (FAILED(hr))
    {
      DebugTrace("ERROR: CreateSourceVoice failed with error {:08X}\n", hr);
      throw std::exception("CreateSourceVoice");
    }
    if (!oneshot)
      ++mVoiceInstances;
  }

  if (oneshot)
  {
    DEBUG_ASSERT(*voice != 0);
    mOneShots.emplace_back(std::make_pair(voiceKey, *voice));
  }
}

void AudioEngine::Impl::DestroyVoice(_In_ IXAudio2SourceVoice* voice) noexcept
{
  if (!voice)
    return;

#ifndef NDEBUG
  for (auto it = mOneShots.cbegin(); it != mOneShots.cend(); ++it)
  {
    if (it->second == voice)
    {
      DebugTrace("ERROR: DestroyVoice should not be called for a one-shot voice\n");
      return;
    }
  }

  for (auto it = mVoicePool.cbegin(); it != mVoicePool.cend(); ++it)
  {
    if (it->second == voice)
    {
      DebugTrace("ERROR: DestroyVoice should not be called for a one-shot voice; see TrimVoicePool\n");
      return;
    }
  }
#endif

  DEBUG_ASSERT(mVoiceInstances > 0);
  --mVoiceInstances;
  voice->DestroyVoice();
}

void AudioEngine::Impl::RegisterNotify(_In_ IVoiceNotify* notify, bool usesUpdate)
{
  DEBUG_ASSERT(notify != nullptr);
  mNotifyObjects.insert(notify);

  if (usesUpdate)
    mNotifyUpdates.insert(notify);
}

void AudioEngine::Impl::UnregisterNotify(_In_ IVoiceNotify* notify, bool usesOneShots, bool usesUpdate)
{
  DEBUG_ASSERT(notify != nullptr);
  mNotifyObjects.erase(notify);

  // Check for any pending one-shots for this notification object
  if (usesOneShots)
  {
    bool setevent = false;

    for (auto& shot : mOneShots | std::views::values)
    {
      DEBUG_ASSERT(shot != nullptr);

      XAUDIO2_VOICE_STATE state;
      shot->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);

      if (state.pCurrentBufferContext == notify)
      {
        std::ignore = shot->Stop(0);
        std::ignore = shot->FlushSourceBuffers();
        setevent = true;
      }
    }

    if (setevent)
    {
      // Trigger scan on next call to Update...
      SetEvent(mVoiceCallback.mBufferEnd);
    }
  }

  if (usesUpdate)
    mNotifyUpdates.erase(notify);
}

_Use_decl_annotations_ AudioEngine::AudioEngine(AUDIO_ENGINE_FLAGS flags, const WAVEFORMATEX* wfx, const wchar_t* deviceId,
                                                AUDIO_STREAM_CATEGORY category)
  : pImpl(NEW Impl())
{
  HRESULT hr = pImpl->Initialize(flags, wfx, deviceId, category);
  if (FAILED(hr))
  {
    if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
    {
      if (flags & AudioEngine_ThrowOnNoAudioHW)
      {
        DebugTrace("ERROR: AudioEngine found no default audio device\n");
        throw std::exception("AudioEngineNoAudioHW");
      }
      DebugTrace("WARNING: AudioEngine found no default audio device; running in 'silent mode'\n");
    }
    else
    {
      DebugTrace(L"ERROR: AudioEngine failed ({:08X}) to initialize using device [{}]\n", hr, (deviceId) ? deviceId : L"default");
      throw std::exception("AudioEngine");
    }
  }
}

// Public destructor.
AudioEngine::~AudioEngine()
{
  if (pImpl)
    pImpl->Shutdown();
}

// Public methods.
bool AudioEngine::Update() { return pImpl->Update(); }

_Use_decl_annotations_ bool AudioEngine::Reset(const WAVEFORMATEX* wfx, const wchar_t* deviceId)
{
  if (pImpl->xaudio2)
  {
    DebugTrace("WARNING: Called Reset for active audio graph; going silent in preparation for migration\n");
    pImpl->SetSilentMode();
  }

  HRESULT hr = pImpl->Reset(wfx, deviceId);
  if (FAILED(hr))
  {
    if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND))
    {
      if (pImpl->mEngineFlags & AudioEngine_ThrowOnNoAudioHW)
      {
        DebugTrace("ERROR: AudioEngine found no default audio device on Reset\n");
        throw std::exception("AudioEngineNoAudioHW");
      }
      DebugTrace("WARNING: AudioEngine found no default audio device on Reset; running in 'silent mode'\n");
      return false;
    }
    DebugTrace(L"ERROR: AudioEngine failed ({:08X}) to Reset using device [{}]\n", hr, (deviceId) ? deviceId : L"default");
    throw std::exception("AudioEngine::Reset");
  }

  DebugTrace(L"INFO: AudioEngine Reset using device [{}]\n", (deviceId) ? deviceId : L"default");

  return true;
}

void AudioEngine::Suspend()
{
  if (!pImpl->xaudio2)
    return;

  pImpl->xaudio2->StopEngine();
}

void AudioEngine::Resume()
{
  if (!pImpl->xaudio2)
    return;

  HRESULT hr = pImpl->xaudio2->StartEngine();
  check_hresult(hr);
}

void AudioEngine::SetReverb(AUDIO_ENGINE_REVERB reverb)
{
  if (reverb >= Reverb_MAX)
    throw std::invalid_argument("reverb parameter is invalid");

  if (reverb == Reverb_Off)
    pImpl->SetReverb(nullptr);
  else
  {
    XAUDIO2FX_REVERB_PARAMETERS native;
    ReverbConvertI3DL2ToNative(&gReverbPresets[reverb], &native);
    pImpl->SetReverb(&native);
  }
}

_Use_decl_annotations_ void AudioEngine::SetReverb(const XAUDIO2FX_REVERB_PARAMETERS* native) { pImpl->SetReverb(native); }

void AudioEngine::SetMasteringLimit(int release, int loudness) { pImpl->SetMasteringLimit(release, loudness); }

// Public accessors.
AudioStatistics AudioEngine::GetStatistics() const { return pImpl->GetStatistics(); }

WAVEFORMATEXTENSIBLE AudioEngine::GetOutputFormat() const
{
  WAVEFORMATEXTENSIBLE wfx = {};

  if (!pImpl->xaudio2)
    return wfx;

  wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
  wfx.Format.wBitsPerSample = wfx.Samples.wValidBitsPerSample = 16; // This is a guess
  wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

  wfx.Format.nChannels = static_cast<WORD>(pImpl->masterChannels);
  wfx.Format.nSamplesPerSec = pImpl->masterRate;
  wfx.dwChannelMask = pImpl->masterChannelMask;

  wfx.Format.nBlockAlign = static_cast<WORD>(wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8);
  wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;

  static const GUID s_pcm = {WAVE_FORMAT_PCM, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
  memcpy(&wfx.SubFormat, &s_pcm, sizeof(GUID));

  return wfx;
}

uint32_t AudioEngine::GetChannelMask() const { return pImpl->masterChannelMask; }

int AudioEngine::GetOutputChannels() const { return pImpl->masterChannels; }

bool AudioEngine::IsAudioDevicePresent() const { return (pImpl->xaudio2.get() != nullptr) && !pImpl->mCriticalError; }

bool AudioEngine::IsCriticalError() const { return pImpl->mCriticalError; }

// Voice management.
void AudioEngine::SetDefaultSampleRate(int sampleRate)
{
  if ((sampleRate < XAUDIO2_MIN_SAMPLE_RATE) || (sampleRate > XAUDIO2_MAX_SAMPLE_RATE))
    throw std::exception("Default sample rate is out of range");

  pImpl->defaultRate = sampleRate;
}

void AudioEngine::SetMaxVoicePool(size_t maxOneShots, size_t maxInstances)
{
  if (maxOneShots > 0)
    pImpl->maxVoiceOneshots = maxOneShots;

  if (maxInstances > 0)
    pImpl->maxVoiceInstances = maxInstances;
}

void AudioEngine::TrimVoicePool() { pImpl->TrimVoicePool(); }

_Use_decl_annotations_ void AudioEngine::AllocateVoice(const WAVEFORMATEX* wfx, SOUND_EFFECT_INSTANCE_FLAGS flags, bool oneshot,
                                                       IXAudio2SourceVoice** voice)
{
  pImpl->AllocateVoice(wfx, flags, oneshot, voice);
}

void AudioEngine::DestroyVoice(_In_ IXAudio2SourceVoice* voice) { pImpl->DestroyVoice(voice); }

void AudioEngine::RegisterNotify(_In_ IVoiceNotify* notify, bool usesUpdate) { pImpl->RegisterNotify(notify, usesUpdate); }

void AudioEngine::UnregisterNotify(_In_ IVoiceNotify* notify, bool oneshots, bool usesUpdate)
{
  pImpl->UnregisterNotify(notify, oneshots, usesUpdate);
}

IXAudio2* AudioEngine::GetInterface() const { return pImpl->xaudio2.get(); }

IXAudio2MasteringVoice* AudioEngine::GetMasterVoice() const { return pImpl->mMasterVoice; }

IXAudio2SubmixVoice* AudioEngine::GetReverbVoice() const { return pImpl->mReverbVoice; }

X3DAUDIO_HANDLE& AudioEngine::Get3DHandle() const { return pImpl->mX3DAudio; }
