#pragma once

class WndProcManager
{
  public:
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
  static void AddWndProc(WNDPROC _driver);
  static void RemoveWndProc(WNDPROC _driver);

  protected:
  inline static std::vector<WNDPROC> sm_wndProcs;
  inline static std::mutex sm_sync;
};
