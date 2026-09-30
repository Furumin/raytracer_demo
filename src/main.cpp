#ifdef UNICODE
#undef UNICODE
#endif

// Defines
#define APP_CPP_CODE (1)
#define VULKAN_VALIDATE (0)
#define DESC_POOL_DESC_CAPACITY (KILO(32))

// Permanent defines
#define VK_USE_PLATFORM_WIN32_KHR (1)

// includes (core)
#include <cstdint>
#include <cstring>
#include "core/win32_base.h"
#include <cstdio>
#include "core/def.h"
#include "core/math.h"
#include "core/memory.h"
#include "core/util.h"

// includes (ext)
// This include alone increases debug compile times by 0.6 seconds... :(
#include "ext/bvh/tiny_bvh.h"

// includes (app)
#include "common.h"
#include "gpu/gpu.h"
#include "gpu/gpu.cpp"
#include "tracer.cpp"

// globals
App_globals gAppGlobals;
volatile bool gRunApp = true;

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch(uMsg) {
  case WM_QUIT:
  case WM_DESTROY:
  case WM_CLOSE: {
    PostQuitMessage(0);
    break;
  }
  default:
    break;
  };

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

HWND
CreateWindow(const char *className, const char *windowName, HINSTANCE instance) {
  WNDCLASS wc = {};
  wc.lpszClassName = className;
  wc.lpfnWndProc   = WindowProc;
  wc.hInstance     = instance;
  wc.hCursor = LoadCursor(0, IDC_ARROW);
  RegisterClass(&wc);
  HWND hwnd = CreateWindowEx(
			     0,
			     className,
			     windowName,
			     WS_OVERLAPPEDWINDOW,

			     // pos & size
			     CW_USEDEFAULT,
			     CW_USEDEFAULT,
			     600,
			     400,

			     NULL,
			     NULL,
			     instance,
			     NULL
			     );
  return hwnd;
}

int AppMainProc(void *param) {
  GPU::Ctx_init_params p = {};
  p.VulkanVersion = VK_MAKE_VERSION(1, 3, 0);
  p.WindowHandle = gAppGlobals.WindowHandle;
  p.WindowInstance = gAppGlobals.Instance;
  p.BufferMaxAllocationCount = 64000;
  p.ImageMaxAllocationCount = 1000;
  p.SampleImageDescriptorCount = DESC_POOL_DESC_CAPACITY;
  p.StorageImageDescriptorCount = DESC_POOL_DESC_CAPACITY;
  i32 res = GPU::Init(p);
  if (res) {
    return -1;
  }
  Tracer::World t;
  t.Init();
  while (gRunApp) {
    Sleep(10);
    i32 imageIndex = GPU::AcquireSwapchain();
    // Make sure to poll AFTER acquiring swapchain!
    GPU::PollResources();
    if (imageIndex < 0) {
      if (imageIndex == -1) {
	GPU::ReinitSwapchain();
	uv2 swapchainDim = GPU::GetSwapchainDim();
	t.OnResize(swapchainDim);
      }
      continue;
    }
    // Make sure to begin command buffer AFTER acquiring swapchain!
    GPU::Cmd_buf cmdBuf = GPU::BeginCmdBuf(GPU::GENERAL);
    t.Tick(&cmdBuf, imageIndex);
    
    // Frame end
    i32 res = GPU::PresentSwapchain(imageIndex, &cmdBuf, 1);
    if (res < 0) {
      if (res == -1) {
	GPU::ReinitSwapchain();
	uv2 swapchainDim = GPU::GetSwapchainDim();
	t.OnResize(swapchainDim);
      }
      continue;
    }
  }
  GPU::Free();
  return 0;
};

int CALLBACK
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char* pCmdLineA, int nCmdShow) {
  setlocale(LC_ALL, ".UTF8");
  HWND windowHandle = CreateWindow("AppClass", "App", hInstance);
  if (NULL == windowHandle) {
    return -1;
  }
  ShowWindow(windowHandle, nCmdShow);
  gAppGlobals.Set(windowHandle, hInstance);
  gRunApp = true;
  Util::LaunchThread(AppMainProc, NULL);
  bool run = true;
  MSG msg = {};
  while (run) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (WM_QUIT == msg.message)
	run = false;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      Sleep(2);
    }
  }
  gRunApp = false;
  return 0;
}
