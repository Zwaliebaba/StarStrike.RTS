#include "pch.h"
#include "SoundCommon.h"

using namespace Neuron::Audio;

namespace
{
  template <typename T>
  WORD ChannelsSpecifiedInMask(T x)
  {
    WORD bitCount = 0;
    while (x)
    {
      ++bitCount;
      x &= (x - 1);
    }
    return bitCount;
  }
}

bool Audio::IsValid(_In_ const WAVEFORMATEX* wfx)
{
  if (!wfx)
    return false;

  if (!wfx->nChannels)
  {
    DebugTrace("ERROR: Wave format must have at least 1 channel\n");
    return false;
  }

  if (wfx->nChannels > XAUDIO2_MAX_AUDIO_CHANNELS)
  {
    DebugTrace("ERROR: Wave format must have less than {} channels ({})\n", XAUDIO2_MAX_AUDIO_CHANNELS, wfx->nChannels);
    return false;
  }

  if (!wfx->nSamplesPerSec)
  {
    DebugTrace("ERROR: Wave format cannot have a sample rate of 0\n");
    return false;
  }

  if ((wfx->nSamplesPerSec < XAUDIO2_MIN_SAMPLE_RATE) || (wfx->nSamplesPerSec > XAUDIO2_MAX_SAMPLE_RATE))
  {
    DebugTrace("ERROR: Wave format channel count must be in range {}..{} ({})\n", XAUDIO2_MIN_SAMPLE_RATE, XAUDIO2_MAX_SAMPLE_RATE,
               wfx->nSamplesPerSec);
    return false;
  }

  switch (wfx->wFormatTag)
  {
    case WAVE_FORMAT_PCM:

      switch (wfx->wBitsPerSample)
      {
        case 8:
        case 16:
        case 24:
        case 32:
          break;

        default:
          DebugTrace("ERROR: Wave format integer PCM must have 8, 16, 24, or 32 bits per sample ({})\n", wfx->wBitsPerSample);
          return false;
      }

      if (wfx->nBlockAlign != (wfx->nChannels * wfx->wBitsPerSample / 8))
      {
        DebugTrace("ERROR: Wave format integer PCM - nBlockAlign ({}) != nChannels ({}) * wBitsPerSample ({}) / 8\n",
                   wfx->nBlockAlign, wfx->nChannels, wfx->wBitsPerSample);
        return false;
      }

      if (wfx->nAvgBytesPerSec != (wfx->nSamplesPerSec * wfx->nBlockAlign))
      {
        DebugTrace("ERROR: Wave format integer PCM - nAvgBytesPerSec (%lu) != nSamplesPerSec (%lu) * nBlockAlign ({})\n",
                   wfx->nAvgBytesPerSec, wfx->nSamplesPerSec, wfx->nBlockAlign);
        return false;
      }

      return true;

    case WAVE_FORMAT_IEEE_FLOAT:

      if (wfx->wBitsPerSample != 32)
      {
        DebugTrace("ERROR: Wave format float PCM must have 32-bits per sample ({})\n", wfx->wBitsPerSample);
        return false;
      }

      if (wfx->nBlockAlign != (wfx->nChannels * wfx->wBitsPerSample / 8))
      {
        DebugTrace("ERROR: Wave format float PCM - nBlockAlign ({}) != nChannels ({}) * wBitsPerSample ({}) / 8\n",
                   wfx->nBlockAlign, wfx->nChannels, wfx->wBitsPerSample);
        return false;
      }

      if (wfx->nAvgBytesPerSec != (wfx->nSamplesPerSec * wfx->nBlockAlign))
      {
        DebugTrace("ERROR: Wave format float PCM - nAvgBytesPerSec (%lu) != nSamplesPerSec (%lu) * nBlockAlign ({})\n",
                   wfx->nAvgBytesPerSec, wfx->nSamplesPerSec, wfx->nBlockAlign);
        return false;
      }

      return true;

    case WAVE_FORMAT_ADPCM:
    {
      if ((wfx->nChannels != 1) && (wfx->nChannels != 2))
      {
        DebugTrace("ERROR: Wave format ADPCM must have 1 or 2 channels ({})\n", wfx->nChannels);
        return false;
      }

      if (wfx->wBitsPerSample != 4 /*MSADPCM_BITS_PER_SAMPLE*/)
      {
        DebugTrace("ERROR: Wave format ADPCM must have 4 bits per sample ({})\n", wfx->wBitsPerSample);
        return false;
      }

      if (wfx->cbSize != 32 /*MSADPCM_FORMAT_EXTRA_BYTES*/)
      {
        DebugTrace("ERROR: Wave format ADPCM must have cbSize = 32 ({})\n", wfx->cbSize);
        return false;
      }
      auto wfadpcm = reinterpret_cast<const ADPCMWAVEFORMAT*>(wfx);

      if (wfadpcm->wNumCoef != 7 /*MSADPCM_NUM_COEFFICIENTS*/)
      {
        DebugTrace("ERROR: Wave format ADPCM must have 7 coefficients ({})\n", wfadpcm->wNumCoef);
        return false;
      }

      bool valid = true;
      for (int j = 0; j < 7 /*MSADPCM_NUM_COEFFICIENTS*/; ++j)
      {
        // Microsoft ADPCM standard encoding coefficients
        static const short g_pAdpcmCoefficients1[] = {256, 512, 0, 192, 240, 460, 392};
        static constexpr short g_pAdpcmCoefficients2[] = {0, -256, 0, 64, 0, -208, -232};

        if (wfadpcm->aCoef[j].iCoef1 != g_pAdpcmCoefficients1[j] || wfadpcm->aCoef[j].iCoef2 != g_pAdpcmCoefficients2[j])
          valid = false;
      }

      if (!valid)
      {
        DebugTrace("ERROR: Wave formt ADPCM found non-standard coefficients\n");
        return false;
      }

      if ((wfadpcm->wSamplesPerBlock < 4 /*MSADPCM_MIN_SAMPLES_PER_BLOCK*/) || (wfadpcm->wSamplesPerBlock > 64000
        /*MSADPCM_MAX_SAMPLES_PER_BLOCK*/))
      {
        DebugTrace("ERROR: Wave format ADPCM wSamplesPerBlock must be 4..64000 ({})\n", wfadpcm->wSamplesPerBlock);
        return false;
      }

      if (wfadpcm->wfx.nChannels == 1 && (wfadpcm->wSamplesPerBlock % 2))
      {
        DebugTrace("ERROR: Wave format ADPCM mono files must have even wSamplesPerBlock\n");
        return false;
      }

      int nHeaderBytes = 7 /*MSADPCM_HEADER_LENGTH*/ * wfx->nChannels;
      int nBitsPerFrame = 4 /*MSADPCM_BITS_PER_SAMPLE*/ * wfx->nChannels;
      int nPcmFramesPerBlock = (wfx->nBlockAlign - nHeaderBytes) * 8 / nBitsPerFrame + 2;

      if (wfadpcm->wSamplesPerBlock != nPcmFramesPerBlock)
      {
        DebugTrace("ERROR: Wave format ADPCM {}-channel with nBlockAlign = {} must have wSamplesPerBlock = {} ({})\n",
                   wfx->nChannels, wfx->nBlockAlign, nPcmFramesPerBlock, wfadpcm->wSamplesPerBlock);
        return false;
      }
      return true;
    }

    case WAVE_FORMAT_WMAUDIO2:
    case WAVE_FORMAT_WMAUDIO3:
      DebugTrace("ERROR: Wave format xWMA not supported by this version of Neuron for Audio\n");
      return false;

    case 0x166 /* WAVE_FORMAT_XMA2 */:
      DebugTrace("ERROR: Wave format XMA2 not supported by this version of Neuron for Audio\n");
      return false;

    case WAVE_FORMAT_EXTENSIBLE:
    {
      if (wfx->cbSize < (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)))
      {
        DebugTrace("ERROR: Wave format WAVE_FORMAT_EXTENSIBLE - cbSize must be %Iu ({})\n",
                   (sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)), wfx->cbSize);
        return false;
      }
      static const GUID s_wfexBase = {0x00000000, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

      auto wfex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);

      if (memcmp(reinterpret_cast<const BYTE*>(&wfex->SubFormat) + sizeof(DWORD),
                 reinterpret_cast<const BYTE*>(&s_wfexBase) + sizeof(DWORD), sizeof(GUID) - sizeof(DWORD)) != 0)
      {
        DebugTrace(
          "ERROR: Wave format WAVEFORMATEXTENSIBLE encountered with unknown GUID ({%8.8lX-%4.4X-%4.4X-%2.2X%2.2X-%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X})\n",
          wfex->SubFormat.Data1, wfex->SubFormat.Data2, wfex->SubFormat.Data3, wfex->SubFormat.Data4[0], wfex->SubFormat.Data4[1],
          wfex->SubFormat.Data4[2], wfex->SubFormat.Data4[3], wfex->SubFormat.Data4[4], wfex->SubFormat.Data4[5],
          wfex->SubFormat.Data4[6], wfex->SubFormat.Data4[7]);
        return false;
      }

      switch (wfex->SubFormat.Data1)
      {
        case WAVE_FORMAT_PCM:

          switch (wfx->wBitsPerSample)
          {
            case 8:
            case 16:
            case 24:
            case 32:
              break;

            default:
              DebugTrace("ERROR: Wave format integer PCM must have 8, 16, 24, or 32 bits per sample ({})\n", wfx->wBitsPerSample);
              return false;
          }

          switch (wfex->Samples.wValidBitsPerSample)
          {
            case 0:
            case 8:
            case 16:
            case 20:
            case 24:
            case 32:
              break;

            default:
              DebugTrace("ERROR: Wave format integer PCM must have 8, 16, 20, 24, or 32 valid bits per sample ({})\n",
                         wfex->Samples.wValidBitsPerSample);
              return false;
          }

          if (wfex->Samples.wValidBitsPerSample && (wfex->Samples.wValidBitsPerSample > wfx->wBitsPerSample))
          {
            DebugTrace("ERROR: Wave format ingter PCM wValidBitsPerSample ({}) is greater than wBitsPerSample ({})\n",
                       wfex->Samples.wValidBitsPerSample, wfx->wBitsPerSample);
            return false;
          }

          if (wfx->nBlockAlign != (wfx->nChannels * wfx->wBitsPerSample / 8))
          {
            DebugTrace("ERROR: Wave format integer PCM - nBlockAlign ({}) != nChannels ({}) * wBitsPerSample ({}) / 8\n",
                       wfx->nBlockAlign, wfx->nChannels, wfx->wBitsPerSample);
            return false;
          }

          if (wfx->nAvgBytesPerSec != (wfx->nSamplesPerSec * wfx->nBlockAlign))
          {
            DebugTrace("ERROR: Wave format integer PCM - nAvgBytesPerSec (%lu) != nSamplesPerSec (%lu) * nBlockAlign ({})\n",
                       wfx->nAvgBytesPerSec, wfx->nSamplesPerSec, wfx->nBlockAlign);
            return false;
          }

          break;

        case WAVE_FORMAT_IEEE_FLOAT:

          if (wfx->wBitsPerSample != 32)
          {
            DebugTrace("ERROR: Wave format float PCM must have 32-bits per sample ({})\n", wfx->wBitsPerSample);
            return false;
          }

          switch (wfex->Samples.wValidBitsPerSample)
          {
            case 0:
            case 32:
              break;

            default:
              DebugTrace("ERROR: Wave format float PCM must have 32 valid bits per sample ({})\n",
                         wfex->Samples.wValidBitsPerSample);
              return false;
          }

          if (wfx->nBlockAlign != (wfx->nChannels * wfx->wBitsPerSample / 8))
          {
            DebugTrace("ERROR: Wave format float PCM - nBlockAlign ({}) != nChannels ({}) * wBitsPerSample ({}) / 8\n",
                       wfx->nBlockAlign, wfx->nChannels, wfx->wBitsPerSample);
            return false;
          }

          if (wfx->nAvgBytesPerSec != (wfx->nSamplesPerSec * wfx->nBlockAlign))
          {
            DebugTrace("ERROR: Wave format float PCM - nAvgBytesPerSec (%lu) != nSamplesPerSec (%lu) * nBlockAlign ({})\n",
                       wfx->nAvgBytesPerSec, wfx->nSamplesPerSec, wfx->nBlockAlign);
            return false;
          }

          break;

        case WAVE_FORMAT_ADPCM:
          DebugTrace("ERROR: Wave format ADPCM is not supported as a WAVEFORMATEXTENSIBLE\n");
          return false;

        case WAVE_FORMAT_WMAUDIO2:
        case WAVE_FORMAT_WMAUDIO3:
          DebugTrace("ERROR: Wave format xWMA not supported by this version of Neuron for Audio\n");
          return false;

        case 0x166 /* WAVE_FORMAT_XMA2 */:
          DebugTrace("ERROR: Wave format XMA2 is not supported as a WAVEFORMATEXTENSIBLE\n");
          return false;

        default:
          DebugTrace("ERROR: Unknown WAVEFORMATEXTENSIBLE format tag ({})\n", wfex->SubFormat.Data1);
          return false;
      }

      if (wfex->dwChannelMask)
      {
        auto channelBits = ChannelsSpecifiedInMask(wfex->dwChannelMask);
        if (channelBits != wfx->nChannels)
        {
          DebugTrace("ERROR: WAVEFORMATEXTENSIBLE: nChannels={} but ChannelMask has {} bits set\n", wfx->nChannels, channelBits);
          return false;
        }
      }

      return true;
    }

    default:
      DebugTrace("ERROR: Unknown WAVEFORMATEX format tag ({})\n", wfx->wFormatTag);
      return false;
  }
}

uint32_t Audio::GetDefaultChannelMask(int channels)
{
  switch (channels)
  {
    case 1:
      return SPEAKER_MONO;
    case 2:
      return SPEAKER_STEREO;
    case 3:
      return SPEAKER_2POINT1;
    case 4:
      return SPEAKER_QUAD;
    case 5:
      return SPEAKER_4POINT1;
    case 6:
      return SPEAKER_5POINT1;
    case 7:
      return SPEAKER_5POINT1 | SPEAKER_BACK_CENTER;
    case 8:
      return SPEAKER_7POINT1;
    default:
      return 0;
  }
}

_Use_decl_annotations_ void Audio::CreateIntegerPCM(WAVEFORMATEX* wfx, int sampleRate, int channels, int sampleBits)
{
  int blockAlign = channels * sampleBits / 8;

  wfx->wFormatTag = WAVE_FORMAT_PCM;
  wfx->nChannels = static_cast<WORD>(channels);
  wfx->nSamplesPerSec = static_cast<DWORD>(sampleRate);
  wfx->nAvgBytesPerSec = static_cast<DWORD>(blockAlign * sampleRate);
  wfx->nBlockAlign = static_cast<WORD>(blockAlign);
  wfx->wBitsPerSample = static_cast<WORD>(sampleBits);
  wfx->cbSize = 0;

  DEBUG_ASSERT(IsValid( wfx ));
}

_Use_decl_annotations_ void Audio::CreateFloatPCM(WAVEFORMATEX* wfx, int sampleRate, int channels)
{
  int blockAlign = channels * 4;

  wfx->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
  wfx->nChannels = static_cast<WORD>(channels);
  wfx->nSamplesPerSec = static_cast<DWORD>(sampleRate);
  wfx->nAvgBytesPerSec = static_cast<DWORD>(blockAlign * sampleRate);
  wfx->nBlockAlign = static_cast<WORD>(blockAlign);
  wfx->wBitsPerSample = 32;
  wfx->cbSize = 0;

  DEBUG_ASSERT(IsValid( wfx ));
}

_Use_decl_annotations_ void Audio::CreateADPCM(WAVEFORMATEX* wfx, size_t wfxSize, int sampleRate, int channels, int samplesPerBlock)
{
  if (wfxSize < (sizeof(WAVEFORMATEX) + 32 /*MSADPCM_FORMAT_EXTRA_BYTES*/))
  {
    DebugTrace("CreateADPCM needs at least %Iu bytes for the result\n", (sizeof(WAVEFORMATEX) + 32 /*MSADPCM_FORMAT_EXTRA_BYTES*/));
    throw std::invalid_argument("ADPCMWAVEFORMAT");
  }

  if (!samplesPerBlock)
  {
    DebugTrace("CreateADPCM needs a non-zero samples per block count\n");
    throw std::invalid_argument("ADPCMWAVEFORMAT");
  }

  int blockAlign = (7 /*MSADPCM_HEADER_LENGTH*/) * channels + (samplesPerBlock - 2) * (4 /* MSADPCM_BITS_PER_SAMPLE */) * channels /
    8;

  wfx->wFormatTag = WAVE_FORMAT_ADPCM;
  wfx->nChannels = static_cast<WORD>(channels);
  wfx->nSamplesPerSec = static_cast<DWORD>(sampleRate);
  wfx->nAvgBytesPerSec = static_cast<DWORD>(blockAlign * sampleRate / samplesPerBlock);
  wfx->nBlockAlign = static_cast<WORD>(blockAlign);
  wfx->wBitsPerSample = 4 /* MSADPCM_BITS_PER_SAMPLE */;
  wfx->cbSize = 32 /*MSADPCM_FORMAT_EXTRA_BYTES*/;

  auto adpcm = reinterpret_cast<ADPCMWAVEFORMAT*>(wfx);
  adpcm->wSamplesPerBlock = static_cast<WORD>(samplesPerBlock);
  adpcm->wNumCoef = 7 /* MSADPCM_NUM_COEFFICIENTS */;

  static ADPCMCOEFSET aCoef[7] = {{256, 0}, {512, -256}, {0, 0}, {192, 64}, {240, 0}, {460, -208}, {392, -232}};
  memcpy(&adpcm->aCoef, aCoef, sizeof(aCoef));

  DEBUG_ASSERT(IsValid( wfx ));
}

void SoundEffectInstanceBase::SetPan(float pan)
{
  if (mDSPSettings.SrcChannelCount != 1)
  {
    DebugTrace("ERROR: SetPan only supports panning on mono source data\n");
    throw std::exception("SetPan");
  }

  pan = std::min<float>(1.f, pan);
  pan = std::max<float>(-1.f, pan);

  mPan = pan;

  if (!voice)
    return;

  float left = (pan >= 0) ? (1.f - pan) : 1.f;
  float right = (pan <= 0) ? (-pan - 1.f) : 1.f;

  float matrix[8];
  for (size_t j = 0; j < 8; ++j)
    matrix[j] = 1.f;

  DEBUG_ASSERT(engine != 0);
  switch (engine->GetChannelMask())
  {
    case SPEAKER_STEREO:
    case SPEAKER_2POINT1:
    case SPEAKER_SURROUND:
      matrix[0] = left;
      matrix[1] = right;
      break;

    case SPEAKER_QUAD:
      matrix[0] = matrix[2] = left;
      matrix[1] = matrix[3] = right;
      break;

    case SPEAKER_4POINT1:
      matrix[0] = matrix[3] = left;
      matrix[1] = matrix[4] = right;
      break;

    case SPEAKER_5POINT1:
    case SPEAKER_7POINT1:
    case SPEAKER_5POINT1_SURROUND:
      matrix[0] = matrix[4] = left;
      matrix[1] = matrix[5] = right;
      break;

    case SPEAKER_7POINT1_SURROUND:
      matrix[0] = matrix[4] = matrix[6] = left;
      matrix[1] = matrix[5] = matrix[7] = right;
      break;

    case SPEAKER_MONO: default:
      // No panning...
      return;
  }

  HRESULT hr = voice->SetOutputMatrix(nullptr, 1, mDSPSettings.DstChannelCount, matrix);
  check_hresult(hr);
}

void SoundEffectInstanceBase::Apply3D(const AudioListener& listener, const AudioEmitter& emitter)
{
  if (!voice)
    return;

  if (!(mFlags & SoundEffectInstance_Use3D))
  {
    DebugTrace("ERROR: Apply3D called for an instance created without SoundEffectInstance_Use3D set\n");
    throw std::exception("Apply3D");
  }

  DWORD dwCalcFlags = X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_LPF_DIRECT;

  if (mFlags & SoundEffectInstance_UseRedirectLFE)
  {
    // On devices with an LFE channel, allow the mono source data to be routed to the LFE destination channel.
    dwCalcFlags |= X3DAUDIO_CALCULATE_REDIRECT_TO_LFE;
  }

  auto reverb = mReverbVoice;
  if (reverb)
    dwCalcFlags |= X3DAUDIO_CALCULATE_LPF_REVERB | X3DAUDIO_CALCULATE_REVERB;

  float matrix[XAUDIO2_MAX_AUDIO_CHANNELS * 8];
  DEBUG_ASSERT(mDSPSettings.SrcChannelCount <= XAUDIO2_MAX_AUDIO_CHANNELS);
  DEBUG_ASSERT(mDSPSettings.DstChannelCount <= 8);
  mDSPSettings.pMatrixCoefficients = matrix;

  DEBUG_ASSERT(engine != 0);
  X3DAudioCalculate(engine->Get3DHandle(), &listener, &emitter, dwCalcFlags, &mDSPSettings);

  mDSPSettings.pMatrixCoefficients = nullptr;

  voice->SetFrequencyRatio(mDSPSettings.DopplerFactor);

  auto direct = mDirectVoice;
  DEBUG_ASSERT(direct != 0);
  voice->SetOutputMatrix(direct, mDSPSettings.SrcChannelCount, mDSPSettings.DstChannelCount, matrix);

  if (reverb)
    voice->SetOutputMatrix(reverb, 1, 1, &mDSPSettings.ReverbLevel);

  if (mFlags & SoundEffectInstance_ReverbUseFilters)
  {
    XAUDIO2_FILTER_PARAMETERS filterDirect = {
      LowPassFilter,
      2.0f * sinf(X3DAUDIO_PI / 6.0f * mDSPSettings.LPFDirectCoefficient),
      1.0f
    };
    // see XAudio2CutoffFrequencyToRadians() in XAudio2.h for more information on the formula used here
    voice->SetOutputFilterParameters(direct, &filterDirect);

    if (reverb)
    {
      XAUDIO2_FILTER_PARAMETERS filterReverb = {
        LowPassFilter,
        2.0f * sinf(X3DAUDIO_PI / 6.0f * mDSPSettings.LPFReverbCoefficient),
        1.0f
      };
      // see XAudio2CutoffFrequencyToRadians() in XAudio2.h for more information on the formula used here
      voice->SetOutputFilterParameters(reverb, &filterReverb);
    }
  }
}
