#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <imgui.h>

namespace Overlay {
extern HWND Window;
extern ID3D11Device *Device;
extern ID3D11DeviceContext *DeviceContext;
extern IDXGISwapChain *SwapChain;
extern ID3D11RenderTargetView *RenderTargetView;

void CreateOverlay(LPCWSTR name);
extern ID3D11ShaderResourceView *SilhouetteTexture;
extern int SilhouetteWidth;
extern int SilhouetteHeight;

bool Initialize();
void Run(void (*draw_callback)());
void Update();
void Cleanup();

// Texture System
bool LoadTextureFromFile(const char *filename,
                         ID3D11ShaderResourceView **out_srv, int *out_width,
                         int *out_height);
} // namespace Overlay
