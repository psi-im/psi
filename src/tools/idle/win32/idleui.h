////////////////////////////////////////////////////////////////
// 2000 Microsoft Systems Journal. 
// If this program works, it was written by Paul DiLascia.
// If not, I don't know who wrote it.
// This program compiles with Visual C++ 6.0 on Windows 98
//
#define DLLIMPORT __declspec(dllimport)

// IdleUI is a DLL that lets you tell when the user interface has been idle
// for a specified amount of time. The DLL works by installing windows keyboard
// and mouse hooks. The DLL records the tick count whenever input is received.
//
// To use, you must
// - call IdleUIInit when your app starts up
// - call IdleUITerm when your app terminates
// - call IdleUIGetLastInputTime to get the time, and compare this with
//   the current GetTickCount();
//
// See TestIdleUI.cpp for an example of how to use IdleUI
// 
DLLIMPORT BOOL IdleUIInit();
DLLIMPORT void IdleUITerm();
DLLIMPORT DWORD IdleUIGetLastInputTime();
