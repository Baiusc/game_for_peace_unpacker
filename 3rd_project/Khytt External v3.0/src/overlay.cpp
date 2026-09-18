#include "overlay.h"
#include <Windows.h>

#include "features/bsp_parser.h"
#include "features/esp.h"
#include "features/spectatorlist.h"
#include "resource.h"
#include "sdk/config.h"
#include "sdk/memory.h"
#include <chrono>
#include <dwmapi.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <thread>
#include <wincodec.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "windowscodecs.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU)
      return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

namespace Overlay {
HWND Window = nullptr;
ID3D11Device *Device = nullptr;
ID3D11DeviceContext *DeviceContext = nullptr;
IDXGISwapChain *SwapChain = nullptr;
ID3D11RenderTargetView *RenderTargetView = nullptr;

// Assets
ID3D11ShaderResourceView *SilhouetteTexture = nullptr;
int SilhouetteWidth = 0;
int SilhouetteHeight = 0;

void CreateOverlay(LPCWSTR name) {
  WNDCLASSEXW wc = {sizeof(WNDCLASSEXW),
                    CS_CLASSDC,
                    WindowProc,
                    0L,
                    0L,
                    GetModuleHandle(NULL),
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    L"Overlay",
                    NULL};
  RegisterClassExW(&wc);

  Window = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, wc.lpszClassName, name,
      WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
      GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);

  SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 0, LWA_COLORKEY);

  MARGINS margins = {-1};
  DwmExtendFrameIntoClientArea(Window, &margins);

  ShowWindow(Window, SW_SHOWDEFAULT);
  UpdateWindow(Window);
}

bool Initialize() {
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 0;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = Window;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT createDeviceFlags = 0;
  D3D_FEATURE_LEVEL featureLevel;
  const D3D_FEATURE_LEVEL featureLevelArray[2] = {
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_0,
  };
  if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                    createDeviceFlags, featureLevelArray, 2,
                                    D3D11_SDK_VERSION, &sd, &SwapChain, &Device,
                                    &featureLevel, &DeviceContext) != S_OK)
    return false;

  ID3D11Texture2D *backBuffer;
  SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
  Device->CreateRenderTargetView(backBuffer, NULL, &RenderTargetView);
  backBuffer->Release();

  return true;
}

void Update() {
  // Toggle menu visibility with Insert key
  static bool was_pressed = false;
  bool is_pressed = GetAsyncKeyState(VK_INSERT) & 0x8000;
  if (is_pressed && !was_pressed) {
    Settings::menu_visible = !Settings::menu_visible;
  }
  was_pressed = is_pressed;

  // Toggle input transparency based on menu visibility
  LONG exStyle = GetWindowLong(Window, GWL_EXSTYLE);
  if (Settings::menu_visible) {
    if (exStyle & WS_EX_TRANSPARENT) {
      SetWindowLong(Window, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
    }
  } else {
    if (!(exStyle & WS_EX_TRANSPARENT)) {
      SetWindowLong(Window, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
    }
  }

  // Track CS2 window foreground status
  static HWND cs2Window = FindWindowW(L"SDL_app", L"Counter-Strike 2");
  if (!cs2Window) {
    cs2Window = FindWindowW(L"SDL_app", L"Counter-Strike 2");
  }

  HWND foregroundWindow = GetForegroundWindow();
  if (foregroundWindow == cs2Window || foregroundWindow == Window) {
    ShowWindow(Window, SW_SHOW);
  } else {
    ShowWindow(Window, SW_HIDE);
  }
}

void SetupStyle() {
  auto &style = ImGui::GetStyle();

  style.WindowPadding = ImVec2(0, 0);
  style.WindowRounding = 12.0f;
  style.FrameRounding = 6.0f;
  style.ItemSpacing = ImVec2(8, 8);
  style.ScrollbarSize = 12.0f;
  style.ScrollbarRounding = 12.0f;
  style.GrabRounding = 6.0f;
  style.WindowBorderSize = 1.0f;

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.09f, 0.94f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
  colors[ImGuiCol_Border] = ImVec4(0.25f, 0.15f, 0.45f, 0.50f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 0.54f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.18f, 0.35f, 0.40f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.25f, 0.55f, 0.67f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.07f, 0.23f, 1.00f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.60f, 0.40f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.45f, 0.35f, 0.75f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.40f, 1.00f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.18f, 0.54f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.18f, 0.35f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.25f, 0.55f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.25f, 0.15f, 0.45f, 0.50f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.25f, 0.55f, 0.80f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.35f, 0.75f, 1.00f);
  colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.15f, 0.45f, 0.50f);
}

void Run(void (*draw_callback)()) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  // Load Inter font from resources
  HRSRC res = FindResourceW(GetModuleHandle(NULL),
                            MAKEINTRESOURCEW(IDR_INTER_FONT), RT_RCDATA);
  if (res) {
    HGLOBAL res_data = LoadResource(GetModuleHandle(NULL), res);
    if (res_data) {
      void *font_ptr = LockResource(res_data);
      size_t font_size = SizeofResource(GetModuleHandle(NULL), res);
      if (font_ptr && font_size > 0) {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(font_ptr, (int)font_size, 16.0f, &cfg);
        io.Fonts->AddFontFromMemoryTTF(font_ptr, (int)font_size, 18.0f, &cfg);
        io.Fonts->AddFontFromMemoryTTF(font_ptr, (int)font_size, 64.0f,
                                       &cfg); // Large Banner Font
      }
    }
  }

  // Load Silhouette Texture
  LoadTextureFromFile("silhouette.png", &SilhouetteTexture, &SilhouetteWidth,
                      &SilhouetteHeight);

  SetupStyle();

  ImGui_ImplWin32_Init(Window);
  ImGui_ImplDX11_Init(Device, DeviceContext);

  MSG msg;
  while (true) {
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        break;
    }

    Update();

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (draw_callback)
      draw_callback();

    Visuals::DrawOverlay();
    Visuals::DrawSpectators();

    ImGui::Render();
    const float clear_color_with_alpha[4] = {0, 0, 0, 0};
    DeviceContext->OMSetRenderTargets(1, &RenderTargetView, NULL);
    DeviceContext->ClearRenderTargetView(RenderTargetView,
                                         clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // Low-Latency Engine: Disable VSync & Cap at 240Hz
    SwapChain->Present(0, 0);

    // Explicit 240Hz frame limiter to prevent 100% CPU thread burn
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
  }
}

void Cleanup() {
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  if (RenderTargetView)
    RenderTargetView->Release();
  if (SwapChain)
    SwapChain->Release();
  if (DeviceContext)
    DeviceContext->Release();
  if (Device)
    Device->Release();
  if (Window)
    DestroyWindow(Window);
}
bool LoadTextureFromFile(const char *filename,
                         ID3D11ShaderResourceView **out_srv, int *out_width,
                         int *out_height) {
  // Use WIC (Windows Imaging Component) to load the PNG
  IWICImagingFactory *wFactory = nullptr;
  CoInitialize(NULL);
  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL,
                                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wFactory));
  if (FAILED(hr))
    return false;

  IWICBitmapDecoder *wDecoder = nullptr;
  // Convert char* to wchar_t*
  wchar_t wFilename[MAX_PATH];
  MultiByteToWideChar(CP_ACP, 0, filename, -1, wFilename, MAX_PATH);

  hr = wFactory->CreateDecoderFromFilename(
      wFilename, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &wDecoder);
  if (FAILED(hr)) {
    wFactory->Release();
    return false;
  }

  IWICBitmapFrameDecode *wFrame = nullptr;
  hr = wDecoder->GetFrame(0, &wFrame);
  if (FAILED(hr)) {
    wDecoder->Release();
    wFactory->Release();
    return false;
  }

  IWICFormatConverter *wConverter = nullptr;
  hr = wFactory->CreateFormatConverter(&wConverter);
  hr = wConverter->Initialize(wFrame, GUID_WICPixelFormat32bppPBGRA,
                              WICBitmapDitherTypeNone, NULL, 0.0,
                              WICBitmapPaletteTypeCustom);

  UINT width, height;
  wConverter->GetSize(&width, &height);

  D3D11_TEXTURE2D_DESC desc;
  ZeroMemory(&desc, sizeof(desc));
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

  ID3D11Texture2D *pTexture = nullptr;
  D3D11_SUBRESOURCE_DATA subResource;
  subResource.pSysMem = new BYTE[width * height * 4];
  subResource.SysMemPitch = width * 4;
  subResource.SysMemSlicePitch = 0;
  wConverter->CopyPixels(NULL, width * 4, width * height * 4,
                         (BYTE *)subResource.pSysMem);

  Device->CreateTexture2D(&desc, &subResource, &pTexture);

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  ZeroMemory(&srvDesc, sizeof(srvDesc));
  srvDesc.Format = desc.Format;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MostDetailedMip = 0;
  srvDesc.Texture2D.MipLevels = 1;

  Device->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
  pTexture->Release();

  delete[] (BYTE *)subResource.pSysMem;
  wConverter->Release();
  wFrame->Release();
  wDecoder->Release();
  wFactory->Release();
  CoUninitialize();

  *out_width = width;
  *out_height = height;

  return true;
}
} // namespace Overlay
