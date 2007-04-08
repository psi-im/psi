////////////////////////////////////////////////////////////////
// 2000 Microsoft Systems Journal.
// If this program works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
// This program compiles with Visual C++ 6.0 on Windows 98
//
// See IdleUI.h
//

#include <windows.h>
#include <winuser.h>
#include <assert.h>

#define DLLEXPORT __declspec(dllexport)

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

////////////////
// The following global data is SHARED among all instances of the DLL
// (processes); i.e., these are system-wide globals.
//
#pragma data_seg (".IdleUI")  // you must define as SHARED in .def

HHOOK g_keyboardHook = NULL;      // one instance for all processes
HHOOK g_mouseHook = NULL;    // one instance for all processes
DWORD g_lastInputTick = 0;  // tick time of last input event

/**
  Last mouse position.
*/
POINT g_lastMousePos;

#pragma data_seg ()

/**
  Flag indicating whether the DLL's owning process is the loading DLL.
*/
bool g_isHandleOwner = false;

//
// Public interface
//

//////////////////
// Initialize DLL: install kbd/mouse hooks.
//
DLLEXPORT BOOL IdleUIInit()
{
  return TRUE;
}

//////////////////
// Terminate DLL: remove hooks.
//
DLLEXPORT void IdleUITerm()
{
}

/////////////////
// Get tick count of last keyboard or mouse event
//
DLLEXPORT DWORD IdleUIGetLastInputTime()
{
  return g_lastInputTick;
}

//
// Internals
//

/////////////////
// Keyboard hook: record tick count
//
LRESULT CALLBACK keyboardHookCallback(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
  {
    g_lastInputTick = GetTickCount();
	}
  return ::CallNextHookEx(g_keyboardHook, code, wParam, lParam);
}

/////////////////
// Mouse hook: record tick count
//
LRESULT CALLBACK mouseHookCallback(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION)
  {
    // Update timestamp if event indicates mouse action
    bool change = false;
    if (wParam == WM_MOUSEMOVE && lParam != 0)
    {
      PMOUSEHOOKSTRUCT mhs = (PMOUSEHOOKSTRUCT) lParam;
      if (mhs->pt.x != g_lastMousePos.x ||
        mhs->pt.y != g_lastMousePos.y)
      {
        change = true;
        g_lastMousePos = mhs->pt;
      }
    }
    else
    {
      change = true;
    }
    if (change)
    {
      g_lastInputTick = GetTickCount();
    }
	}
  return ::CallNextHookEx(g_mouseHook, code, wParam, lParam);
}

void initialize(HINSTANCE module)
{
  if (g_keyboardHook == 0)
  {
    g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD, keyboardHookCallback, module, 0);
    g_mouseHook = SetWindowsHookEx(WH_MOUSE, mouseHookCallback, module, 0);
    g_lastInputTick = GetTickCount();
    g_isHandleOwner = true;
  }
  assert(g_keyboardHook);
  assert(g_mouseHook);
}

void shutdown()
{
  // Only handle-owning process may unhook
  if (g_isHandleOwner)
  {
    if (g_keyboardHook != 0)
    {
      UnhookWindowsHookEx(g_keyboardHook);
      g_keyboardHook = 0;
    }
    if (g_mouseHook != 0)
    {
      UnhookWindowsHookEx(g_mouseHook);
      g_mouseHook = 0;
    }
  }
}

//
// DLL entry point
//

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
  switch (reason)
  {
    case DLL_PROCESS_ATTACH:
    {
      initialize(module);
      break;
    }
    case DLL_PROCESS_DETACH:
    {
      shutdown();
      break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    {
      // Ignore
      break;
    }
  }
  return TRUE;
}
