#include "pch.h"

using namespace Neuron::Audio;

_Use_decl_annotations_ SoundEffectInstance::SoundEffectInstance(AudioEngine* engine, SoundEffect* effect,
                                                                SOUND_EFFECT_INSTANCE_FLAGS flags)
  : mEffect(effect),
    mIndex(0),
    mLooped(false)
{
  DEBUG_ASSERT(engine != nullptr);
  engine->RegisterNotify(this, false);

  DEBUG_ASSERT(mEffect != nullptr);
  mBase.Initialize(engine, effect->GetFormat(), flags);
}

// Public destructor.
SoundEffectInstance::~SoundEffectInstance()
{
  if (mEffect)
  {
    mEffect->UnregisterInstance(this);
    mEffect = nullptr;
  }

  mBase.DestroyVoice();

  if (mBase.engine)
  {
    mBase.engine->UnregisterNotify(this, false, false);
    mBase.engine = nullptr;
  }
}

// Public methods.
void SoundEffectInstance::Play(bool loop)
{
  if (!mBase.voice)
  {
    DEBUG_ASSERT(mEffect != nullptr);
    mBase.AllocateVoice(mEffect->GetFormat());
  }

  if (!mBase.Play())
    return;

  // Submit audio data for STOPPED -> PLAYING state transition
  XAUDIO2_BUFFER buffer;

  DEBUG_ASSERT(mEffect != nullptr);
  mEffect->FillSubmitBuffer(buffer);

  buffer.Flags = XAUDIO2_END_OF_STREAM;
  if (loop)
  {
    mLooped = true;
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
  }
  else
  {
    mLooped = false;
    buffer.LoopCount = buffer.LoopBegin = buffer.LoopLength = 0;
  }
  buffer.pContext = nullptr;

  HRESULT hr;
  {
    hr = mBase.voice->SubmitSourceBuffer(&buffer, nullptr);
  }

  if (FAILED(hr))
  {
#ifdef _DEBUG
    DebugTrace("ERROR: SoundEffectInstance failed ({:08X}) when submitting buffer:\n", hr);

    auto wfx = mEffect->GetFormat();

    size_t length = mEffect->GetSampleSizeInBytes();

    DebugTrace("\tFormat Tag {}, {} channels, {}-bit, {} Hz, %Iu bytes\n", wfx->wFormatTag, wfx->nChannels, wfx->wBitsPerSample,
               wfx->nSamplesPerSec, length);
#endif
    mBase.Stop(true, mLooped);
    throw std::exception("SubmitSourceBuffer");
  }
}

void SoundEffectInstance::Stop(bool immediate) { mBase.Stop(immediate, mLooped); }

void SoundEffectInstance::Pause() { mBase.Pause(); }

void SoundEffectInstance::Resume() { mBase.Resume(); }

void SoundEffectInstance::SetVolume(float volume) { mBase.SetVolume(volume); }

void SoundEffectInstance::SetPitch(float pitch) { mBase.SetPitch(pitch); }

void SoundEffectInstance::SetPan(float pan) { mBase.SetPan(pan); }

void SoundEffectInstance::Apply3D(const AudioListener& listener, const AudioEmitter& emitter) { mBase.Apply3D(listener, emitter); }

// Public accessors.
bool SoundEffectInstance::IsLooped() const { return mLooped; }

SoundState SoundEffectInstance::GetState() { return mBase.GetState(true); }

// Notifications.
void SoundEffectInstance::OnDestroyParent()
{
  mBase.OnDestroy();
  mEffect = nullptr;
}
