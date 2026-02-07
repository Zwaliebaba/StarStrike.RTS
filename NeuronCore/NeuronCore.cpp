#include "pch.h"

void CoreEngine::Startup()
{
  init_apartment();

  if (!XMVerifyCPUSupport())
    Fatal(L"CPU does not support the right technology");

  Timer::Core::Startup();
}

void CoreEngine::Shutdown()
{
  uninit_apartment();
}
