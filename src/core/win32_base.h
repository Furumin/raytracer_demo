#ifndef _H_WIN32_BASE_H
#define _H_WIN32_BASE_H
#if defined(__cplusplus)
extern "C" {
#endif
/*
===================================================
custom win32 defines
===================================================
*/

#define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
#define MAKEINTRESOURCE(i) (char*)((ULONG_PTR)((WORD)(i)))
#define WDEF32(r)		__declspec(dllimport) r __stdcall
#define HINSTANCE		void*
DECLARE_HANDLE(HWND);
#define HANDLE			void*
#define LONG_PTR		int64_t
#define ULONG_PTR		uint64_t
#define DWORD_PTR               uint64_t
typedef unsigned __int64 UINT_PTR, *PUINT_PTR;
#define INVALID_HANDLE_VALUE	(((HANDLE)(LONG_PTR)-1))
#define WORD uint16_t
#define SHORT int16_t
#define DWORD			uint32_t
#define INFINITE		0XFFFFFFFF
#define LONG			int32_t
#define UINT			uint32_t
#define BYTE			uint8_t
#define LPCWSTR			const wchar_t*
#define LPCSTR const char*
#define LPSTR char*
#define LPVOID			void*
#define BOOL			int32_t
#define HMONITOR		HANDLE
#define LRESULT			LONG_PTR
#define LPARAM			LONG_PTR
#define WPARAM			uint64_t
#define FALSE			(0)
#define TRUE			(1)
#define CALLBACK __stdcall
#define WINAPI __stdcall
typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef UINT_PTR (CALLBACK *LPOFNHOOKPROC) (HWND, UINT, WPARAM, LPARAM);

#define LOWORD(l)           ((WORD)(((DWORD)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD)(l)) >> 16) & 0xffff))

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

// key codes
#define VK_TAB	0x09
#define VK_SHIFT	0x10
#define VK_CONTROL	0x11
#define VK_MENU		0x12
#define VK_ESCAPE	0x1b
#define VK_SPACE	0x20
#define VK_LEFT		0x25
#define VK_UP		0x26
#define VK_RIGHT	0x27
#define VK_DOWN		0x28
#define VK_LCONTROL	0xa2
#define VK_RCONTROL	0xa3
#define VK_LMENU	0xa4
#define VK_RMENU	0xa5
#define VK_RETURN	0x0d

#define WM_KEYDOWN	0x0100
#define WM_SYSKEYDOWN	0x0104
#define WM_KEYUP	0x0101
#define WM_SYSKEYUP	0x0105
#define WM_SETCURSOR   0x0020
#define WM_MOUSEMOVE    0x0200
#define WM_MOVING 0x0216
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_QUIT 0x0012
#define WM_DESTROY 0x0002
#define WM_CLOSE 0x0010

#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_RBUTTONUP 0x0205
#define WM_MBUTTONDOWN 0x0207
#define WM_MBUTTONUP 0x0208
#define WM_RBUTTONDBLCLK 0x0206

#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_MBUTTON 0x0010
#define MK_CONTROL 0x0008

SHORT GetKeyState(int);

typedef struct _SECURITY_ATTRIBUTES {
    DWORD  nLength;
    LPVOID lpSecurityDescriptor;
    BOOL   bInheritHandle;
} SECURITY_ATTRIBUTES;
typedef struct tagRECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
} RECT;
/*https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-large_integer-r1#:~:text=Represents%20a%2064%2Dbit%20signed,included%20with%20your%20C%20compiler.*/
typedef union _LARGE_INTEGER {
  struct {
    uint32_t LowPart;
    int32_t  HighPart;
  };
  int64_t QuadPart;
  } LARGE_INTEGER;

typedef struct _OVERLAPPED {
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  union {
    struct {
      DWORD Offset;
      DWORD OffsetHigh;
    } DUMMYSTRUCTNAME;
    void* Pointer;
  } DUMMYUNIONNAME;
  HANDLE    hEvent;
} OVERLAPPED;

#define HICON void*
typedef HICON HCURSOR;

#define WNDCLASS WNDCLASSA
typedef struct tagWNDCLASSA {
  UINT      style;
  WNDPROC   lpfnWndProc;
  int       cbClsExtra;
  int       cbWndExtra;
  HINSTANCE hInstance;
  HICON     hIcon;
  HCURSOR   hCursor;
  void*    hbrBackground;
  const char*    lpszMenuName;
  const char*    lpszClassName;
} WNDCLASSA;

typedef struct tagPOINT {
  LONG x;
  LONG y;
} POINT;

typedef struct tagMSG {
  HWND   hwnd;
  UINT   message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD  time;
  POINT  pt;
  DWORD  lPrivate;
} MSG;

typedef DWORD (__stdcall *LPTHREAD_START_ROUTINE) (void*);

typedef struct _SYSTEM_INFO {
  union {
    DWORD dwOemId;
    struct {
      WORD wProcessorArchitecture;
      WORD wReserved;
    } DUMMYSTRUCTNAME;
  } DUMMYUNIONNAME;
  DWORD     dwPageSize;
  LPVOID    lpMinimumApplicationAddress;
  LPVOID    lpMaximumApplicationAddress;
  DWORD_PTR dwActiveProcessorMask;
  DWORD     dwNumberOfProcessors;
  DWORD     dwProcessorType;
  DWORD     dwAllocationGranularity;
  WORD      wProcessorLevel;
  WORD      wProcessorRevision;
} SYSTEM_INFO;

WDEF32(void) GetSystemInfo(SYSTEM_INFO*);

#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_DECOMMIT 0x00004000
#define MEM_RELEASE 0x00008000
#define PAGE_READWRITE 0x04
WDEF32(LPVOID) VirtualAlloc(LPVOID, uint64_t, DWORD, DWORD);
WDEF32(void) VirtualFree(LPVOID, uint64_t, DWORD);

WDEF32(BOOL) QueryPerformanceCounter(LARGE_INTEGER*);
WDEF32(BOOL) GetClientRect(HWND, RECT*);

#define CreateFile CreateFileA
#define GENERIC_ALL	(0x10000000L)
#define GENERIC_EXECUTE (0x20000000L)
#define GENERIC_WRITE	(0x40000000L)
#define GENERIC_READ	(0x80000000L)
#define FILE_SHARE_READ (1)
#define FILE_SHARE_WRITE (2)
#define CREATE_NEW (1)
#define CREATE_ALWAYS (2)
#define OPEN_EXISTING (3)
#define OPEN_ALWAYS (4)
#define FILE_ATTRIBUTE_NORMAL     (0x80)
#define FILE_ATTRIBUTE_READONLY   (0x1)
#define FILE_FLAG_OVERLAPPED      0x40000000
#define FILE_FLAG_NO_BUFFERING    0x20000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_RANDOM_ACCESS   0x10000000
#define FILE_MAP_ALL_ACCESS 0xF001F
WDEF32(HANDLE) CreateFileA(const void*, DWORD, DWORD, SECURITY_ATTRIBUTES*, DWORD, DWORD, HANDLE);
WDEF32(HANDLE) CreateFileMappingA(HANDLE, SECURITY_ATTRIBUTES*, DWORD, DWORD, DWORD, const char*);

#define DeleteFile DeleteFileA
WDEF32(BOOL) DeleteFileA(const char*);

WDEF32(DWORD) GetFileSize(HANDLE, DWORD*);
WDEF32(DWORD) GetFileSizeEx(HANDLE, LARGE_INTEGER*);

WDEF32(BOOL) ReadFile(HANDLE, void*, DWORD, DWORD*, OVERLAPPED*);
WDEF32(BOOL) GetOverlappedResult(HANDLE, OVERLAPPED*, DWORD*, BOOL);
WDEF32(void*) MapViewOfFile(HANDLE, DWORD, DWORD, DWORD, uint64_t);

WDEF32(BOOL) CloseHandle(HANDLE);
WDEF32(BOOL) UnmapViewOfFile(void*);

WDEF32(BOOL) WriteFile(HANDLE, const void*, DWORD, DWORD*, OVERLAPPED*);

WDEF32(void) Sleep(DWORD);
WDEF32(HANDLE) CreateThread(SECURITY_ATTRIBUTES*, uint64_t, LPTHREAD_START_ROUTINE, void*, DWORD, DWORD*);
WDEF32(DWORD) WaitForMultipleObjects(DWORD, void**, BOOL, DWORD);

WDEF32(void) PostQuitMessage(int);

#define DefWindowProc DefWindowProcA
WDEF32(LRESULT) DefWindowProcA(HWND, UINT, WPARAM, LPARAM);

#define RegisterClass RegisterClassA
WDEF32(uint16_t) RegisterClassA(const WNDCLASSA*);

#define WS_OVERLAPPED 0x0L
#define WS_CAPTION 0x00C00000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define CW_USEDEFAULT 0x80000000
#define CreateWindowEx CreateWindowExA
WDEF32(HWND) CreateWindowExA(DWORD, const char*, const char*, DWORD, int, int, int, int, HWND, void*, HINSTANCE, void*);

#define CP_UTF8 65001
WDEF32(BOOL) AllocConsole(void);
WDEF32(BOOL) SetConsoleCP(UINT);
WDEF32(BOOL) SetConsoleOutputCP(UINT);
WDEF32(BOOL) FreeConsole(void);

#define CreateEvent CreateEventA
WDEF32(HANDLE) CreateEventA(SECURITY_ATTRIBUTES*, BOOL, BOOL, char*);

#define LC_ALL 0

WDEF32(BOOL) QueryPerformanceFrequency(LARGE_INTEGER*);
WDEF32(UINT) timeBeginPeriod(UINT);
WDEF32(UINT) timeEndPeriod(UINT);

WDEF32(BOOL) ShowWindow(HWND, int);

#define PM_REMOVE 0X0001
#define PeekMessage PeekMessageA
WDEF32(BOOL) PeekMessageA(MSG*, HWND, UINT, UINT, UINT);

WDEF32(BOOL) TranslateMessage(const MSG*);
#define DispatchMessage DispatchMessageA
WDEF32(LRESULT) DispatchMessageA(const MSG*);

WDEF32(DWORD) GetLastError();

#define InterlockedCompareExchange _InterlockedCompareExchange
long _InterlockedCompareExchange(long volatile*, long, long);
#define InterlockedExchange _InterlockedExchange
long _InterlockedExchange(long volatile*, long);
WDEF32(LONG) InterlockedIncrement(LONG volatile *addend);
char* setlocale(int, const char*);

#define LoadCursor LoadCursorA
#define IDC_ARROW MAKEINTRESOURCE(32512)
WDEF32(HCURSOR) LoadCursorA(HINSTANCE hInstance, const char* lpCursorName);

typedef HINSTANCE HMODULE;
#define GetModuleHandle GetModuleHandleA
WDEF32(HMODULE) GetModuleHandleA(const char*);

typedef struct tagOFNA {
  DWORD         lStructSize;
  HWND          hwndOwner;
  HINSTANCE     hInstance;
  LPCSTR        lpstrFilter;
  LPSTR         lpstrCustomFilter;
  DWORD         nMaxCustFilter;
  DWORD         nFilterIndex;
  LPSTR         lpstrFile;
  DWORD         nMaxFile;
  LPSTR         lpstrFileTitle;
  DWORD         nMaxFileTitle;
  LPCSTR        lpstrInitialDir;
  LPCSTR        lpstrTitle;
  DWORD         Flags;
  WORD          nFileOffset;
  WORD          nFileExtension;
  LPCSTR        lpstrDefExt;
  LPARAM        lCustData;
  LPOFNHOOKPROC lpfnHook;
  LPCSTR        lpTemplateName;
  void          *pvReserved;
  DWORD         dwReserved;
  DWORD         FlagsEx;
} OPENFILENAMEA, *LPOPENFILENAMEA;
WDEF32(BOOL) GetOpenFileNameA(LPOPENFILENAMEA);
#define OFN_READONLY                 0x00000001
#define OFN_OVERWRITEPROMPT          0x00000002
#define OFN_HIDEREADONLY             0x00000004
#define OFN_NOCHANGEDIR              0x00000008
#define OFN_SHOWHELP                 0x00000010
#define OFN_ENABLEHOOK               0x00000020
#define OFN_ENABLETEMPLATE           0x00000040
#define OFN_ENABLETEMPLATEHANDLE     0x00000080
#define OFN_NOVALIDATE               0x00000100
#define OFN_ALLOWMULTISELECT         0x00000200
#define OFN_EXTENSIONDIFFERENT       0x00000400
#define OFN_PATHMUSTEXIST            0x00000800
#define OFN_FILEMUSTEXIST            0x00001000
#define OFN_CREATEPROMPT             0x00002000
#define OFN_SHAREAWARE               0x00004000
#define OFN_NOREADONLYRETURN         0x00008000
#define OFN_NOTESTFILECREATE         0x00010000
#define OFN_NONETWORKBUTTON          0x00020000
#define OFN_NOLONGNAMES              0x00040000     // force no long names for 4.x modules
#define OFN_EXPLORER                 0x00080000     // new look commdlg
#define OFN_NODEREFERENCELINKS       0x00100000
#define OFN_LONGNAMES                0x00200000     // force long names for 3.x modules

WDEF32(DWORD) GetCurrentDirectoryA(DWORD nBufferLength, char* dir);
#define GetCurrentDirectory GetCurrentDirectoryA

WDEF32(BOOL) SetCurrentDirectoryA(const char* path);
#define SetCurrentDirectory SetCurrentDirectoryA

#define MB_OK (0x0L)
int MessageBoxA(HWND, LPCSTR, LPCSTR, UINT);
#define MessageBox MessageBoxA
  
BOOL GetWindowRect(HWND, RECT*);

#if defined(__cplusplus)
}
#endif

#endif
