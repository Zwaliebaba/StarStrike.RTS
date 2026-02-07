#pragma once

#ifdef PROFILER_ENABLED

#include <pix3.h>

inline void StartProfile(const wchar_t* _itemName) { PIXBeginEvent(PIX_COLOR_DEFAULT, _itemName); }
inline void StartProfile(ID3D12GraphicsCommandList* _context, const wchar_t* _itemName) { { PIXBeginEvent(_context, PIX_COLOR_DEFAULT, _itemName); } }

inline void EndProfile(ID3D12GraphicsCommandList* _context) { PIXEndEvent(_context); }
inline void EndProfile() { PIXEndEvent(); }

template<typename CONTEXT>
class ScopedProfile : PIXScopedEventObject<CONTEXT>
{
  public:
  ScopedProfile(CONTEXT* context, const wchar_t* _itemName)
    : PIXScopedEventObject<CONTEXT>(context, PIX_COLOR_DEFAULT, _itemName)
  { }
};

#else
constexpr void StartProfile(const wchar_t* _itemName) {}
constexpr void StartProfile(ID3D12GraphicsCommandList* _context, const wchar_t* _itemName) {}

constexpr void EndProfile() {}
constexpr void EndProfile(ID3D12GraphicsCommandList* _context) {}

constexpr void ScopedProfile(ID3D12GraphicsCommandList* _context, const wchar_t* _itemName) {}

#endif
