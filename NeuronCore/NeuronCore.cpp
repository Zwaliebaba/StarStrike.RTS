#include "pch.h"

void CoreEngine::Startup()
{
  winrt::init_apartment();

  if (!XMVerifyCPUSupport())
    Fatal(L"CPU does not support the right technology");

  Timer::Core::Startup();
}

void CoreEngine::Shutdown()
{
  winrt::uninit_apartment();
}
