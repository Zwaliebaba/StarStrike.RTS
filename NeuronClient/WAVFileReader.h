#pragma once

namespace Neuron::Audio
{
  HRESULT LoadWAVAudioInMemory(_In_ const byte_buffer_t& wavData, _Outptr_ const WAVEFORMATEX** wfx,
                               _Outptr_ const uint8_t** startAudio, _Out_ uint32_t* audioBytes);

  struct WAVData
  {
    const WAVEFORMATEX* m_wfx;
    const uint8_t* startAudio;
    uint32_t audioBytes;
    uint32_t loopStart;
    uint32_t loopLength;
    const uint32_t* seek;       // Note: XMA Seek data is Big-Endian
    uint32_t seekCount;
  };

  HRESULT LoadWAVAudioInMemoryEx(_In_ const byte_buffer_t& _wavData, _Out_ WAVData& result);
}
