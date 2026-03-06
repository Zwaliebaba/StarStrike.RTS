#include "pch.h"
#include "GameApp.h"

int WINAPI wWinMain(HINSTANCE _hInstance, HINSTANCE _hPrevInstance, LPWSTR _cmdLine, int _iCmdShow)
{
#if defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  wchar_t filename[MAX_PATH];
  GetModuleFileNameW(nullptr, filename, MAX_PATH);
  auto path = std::wstring(filename);
  path = path.substr(0, path.find_last_of('\\'));

  FileSys::SetHomeDirectory(path);

  ClientEngine::Startup(L"Deep Space Outpost", {}, _hInstance, _iCmdShow);

  auto main = winrt::make_self<GameApp>();
  ClientEngine::StartGame(std::move(main));

  ClientEngine::Run();

  ClientEngine::Shutdown();

  return WM_QUIT;
}
