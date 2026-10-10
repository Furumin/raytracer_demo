#ifdef UNICODE
#undef UNICODE
#endif

// Defines
#define APP_CPP_CODE (1)
#define VULKAN_VALIDATE (0)
#define DESC_POOL_DESC_CAPACITY (KILO(2))
#define GPU_USE_IMGUI (1)

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
#define NO_DOUBLE_PRECISION_SUPPORT
#define NO_CUSTOM_GEOMETRY
#define NO_VOXEL_SUPPORT
#define NO_THREADED_BUILDS
#include "ext/bvh/tiny_bvh.h"

// imgui
#if GPU_USE_IMGUI
#define IMGUI_IMPL_WIN32_DISABLE_GAMEPAD (1)
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_vulkan.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

// includes (app)
#include "common.h"
#include "gpu/gpu.h"
#include "gpu/gpu.cpp"
#include "tracer.cpp"

// globals
App_globals gAppGlobals;
volatile App_input gAppInput;
volatile bool gRunApp = true;
volatile bool gInputLatched = false;

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
#if GPU_USE_IMGUI
  if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
    return true;
#endif
  switch(uMsg) {
  case WM_KEYUP:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    u32 state = (lParam & 0x80000000) ? 0 : 1;
    i32 ind = _App_input_details::GetAppInputIndex((u32)wParam);
    ind = CLAMP(ind, 0, APP_KEY_COUNT);
    u8 v = (u8)gAppInput.KeyStates[ind];
    _App_input_details::SetKey(&v, state);
    gAppInput.KeyStates[ind] = v;
    break;
  };
  case WM_LBUTTONDOWN:
  case WM_RBUTTONDOWN: {
    u32 state = 1;
    u32 masks[] = {MK_LBUTTON, MK_RBUTTON};
    for (u32 i = 0; i < ARRAYCOUNT(gAppInput.MouseStates); i++) {
      i32 ind = i;
      state = (masks[i] & (u32)wParam) ? 1 : 0;
      if (!state) continue;
      u8 v = (u8)gAppInput.MouseStates[ind];
      _App_input_details::SetKey(&v, state);
      gAppInput.MouseStates[ind] = v;
    }
    break;
  };
  case WM_LBUTTONUP:
  case WM_RBUTTONUP: {
    u32 state = 0;
    u32 masks[] = {MK_LBUTTON, MK_RBUTTON};
    for (u32 i = 0; i < ARRAYCOUNT(gAppInput.MouseStates); i++) {
      i32 ind = i;
      state = (masks[i] & (u32)wParam) ? 1 : 0;
      if (state) continue; // dont register key as down
      u8 v = (u8)gAppInput.MouseStates[ind];
      _App_input_details::SetKey(&v, state);
      gAppInput.MouseStates[ind] = v;
    }
    break;
    break;
  };
  case WM_MOUSEMOVE: {
    iv2 p {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    gAppInput.MousePosition.x = p.x;
    gAppInput.MousePosition.y = p.y;
    break;
  };
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
			     320*2,
			     240*2,

			     NULL,
			     NULL,
			     instance,
			     NULL
			     );
  return hwnd;
}

void
CenterWindow(HWND hw) {
  RECT r;
  GetWindowRect(hw, &r);
  i32 w = r.right - r.left;
  i32 h = r.bottom - r.top;
  i32 x = (GetSystemMetrics(0) - w) / 2;
  i32 y = (GetSystemMetrics(1) - h) / 2;
  SetWindowPos(hw, 0, x, y, 0, 0, 5); // 5: SWP_NOSIZE | SWP_NOZORDER
}

int
AppMainProc(void *param) {
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

    // Latch input
    App_input inp;
    memcpy(V_(&inp), V_(&gAppInput), sizeof (gAppInput));
    _mm_mfence();
    gInputLatched = true;
    t.UpdateInputs(&inp);
    
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
  HWND windowHandle = CreateWindow("AppClass", "Raycast", hInstance);
  if (NULL == windowHandle) {
    return -1;
  }
  CenterWindow(windowHandle);
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
    }
    if (gInputLatched) {
      // reset all key/mouse states
      _mm_mfence();
      gAppInput.Reset();
      gInputLatched = false;
    }
    Sleep(0);
  }
  gRunApp = false;
  return 0;
}
