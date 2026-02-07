#include "pch.h"
#include "WndProcManager.h"

LRESULT WndProcManager::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT ans = -1;

  for (auto i = sm_wndProcs.begin(); i != sm_wndProcs.end(); ++i)
  {
    ans = (*i)(hWnd, message, wParam, lParam);
    if (ans != -1)
      break;
  }

  return ans;
}

void WndProcManager::AddWndProc(WNDPROC _driver)
{
  std::scoped_lock lock(sm_sync);

  DEBUG_ASSERT(_driver != nullptr);
  sm_wndProcs.emplace_back(_driver);
}

void WndProcManager::RemoveWndProc(WNDPROC _driver)
{
  std::scoped_lock lock(sm_sync);

  for (auto i = sm_wndProcs.begin(); i != sm_wndProcs.end();)
    if (_driver == *i)
      i = sm_wndProcs.erase(i);
    else
      ++i;
}