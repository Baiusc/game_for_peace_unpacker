#pragma once
#include <Windows.h>
#include <d3dx9.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include <dwmapi.h>
#include <atlbase.h>
#include <atlconv.h>
#include "../Includes/Imgui/imgui_internal.h"
#include "../Includes/Imgui/imgui.h"
#include "../Includes/Imgui/imgui_impl_win32.h"
#include "../Includes/Imgui/imgui_impl_dx9.h"
#include "../game/globals.hpp"
#include "../Game/structs.hpp"

#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "Dwmapi.lib")
LPDIRECT3DTEXTURE9 d3d9_texture = nullptr;

IDirect3D9Ex* p_Object = NULL;
IDirect3DDevice9Ex* p_Device = NULL;
D3DPRESENT_PARAMETERS p_Params = { NULL };

HWND MyWnd = NULL;
HWND GameWnd = NULL;
MSG Message = { NULL };

RECT GameRect = { NULL };
D3DPRESENT_PARAMETERS d3dpp;

static ULONG Width = GetSystemMetrics(SM_CXSCREEN);
static ULONG Height = GetSystemMetrics(SM_CYSCREEN);
DWORD ScreenCenterX = Width / 2;
DWORD ScreenCenterY = Height / 2;
static ULONG linescreen = 0;


void render();


HRESULT DirectXInit(HWND hWnd)
{
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &p_Object)))
		exit(3);

	D3DPRESENT_PARAMETERS p_Params = { 0 };
	p_Params.Windowed = TRUE;
	p_Params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	p_Params.hDeviceWindow = hWnd;
	p_Params.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	p_Params.BackBufferFormat = D3DFMT_A8R8G8B8;
	p_Params.BackBufferWidth = Width;
	p_Params.BackBufferHeight = Height;
	p_Params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	p_Params.EnableAutoDepthStencil = TRUE;
	p_Params.AutoDepthStencilFormat = D3DFMT_D16;
	p_Params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	if (FAILED(p_Object->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &p_Params, 0, &p_Device)))
	{
		p_Object->Release();
		exit(4);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX9_Init(p_Device);

	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text] = ImVec4(0.31f, 0.25f, 0.24f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.74f, 0.74f, 0.94f, 1.00f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.62f, 0.70f, 0.72f, 0.56f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.95f, 0.33f, 0.14f, 0.47f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.97f, 0.31f, 0.13f, 0.81f);
	style.Colors[ImGuiCol_TitleBg] = ImColor(180,180,180);
	style.Colors[ImGuiCol_TitleBgActive] = ImColor(180, 180, 180);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImColor(180, 180, 180);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.40f, 0.62f, 0.80f, 0.15f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.39f, 0.64f, 0.80f, 0.30f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.67f, 0.80f, 0.59f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.25f, 0.48f, 0.53f, 0.67f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.48f, 0.47f, 0.47f, 0.71f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.31f, 0.47f, 0.99f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(1.00f, 0.79f, 0.18f, 0.78f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.42f, 0.82f, 1.00f, 0.81f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.72f, 1.00f, 1.00f, 0.86f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.65f, 0.78f, 0.84f, 0.80f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.88f, 0.94f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.68f, 0.74f, 0.80f);//ImVec4(0.46f, 0.84f, 0.90f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.60f, 0.60f, 0.80f, 0.30f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.99f, 0.54f, 0.43f);
	style.Colors[ImGuiCol_PopupBg] = ImColor(255,255,255);
	style.Alpha = 1.0f;
	style.FrameRounding = 4;
	style.WindowTitleAlign = { 0.5,0.5 };
	style.IndentSpacing = 12.0f;
	p_Object->Release();
	return S_OK;
}
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CleanuoD3D()
{
	if (p_Device != NULL)
	{
		p_Device->EndScene();
		p_Device->Release();
	}
	if (p_Object != NULL)
	{
		p_Object->Release();
	}
}
LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
		return true;

	switch (Message)
	{
	case WM_DESTROY:
		CleanuoD3D();
		PostQuitMessage(0);
		exit(4);
		break;
	case WM_ACTIVATEAPP:
	{
		if (p_Device)
			p_Device->Reset(&p_Params);
	} break;
	case WM_SIZE:
		if (p_Device != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_In#pragma once
#include <Windows.h>
#include <d3dx9.h>
#include <d3d9.h>
#include <d3dx9tex.h>
#include <dwmapi.h>
#include <atlbase.h>
#include <atlconv.h>
#include "../Includes/Imgui/imgui_internal.h"
#include "../Includes/Imgui/imgui.h"
#include "../Includes/Imgui/imgui_impl_win32.h"
#include "../Includes/Imgui/imgui_impl_dx9.h"
#include "../game/globals.hpp"
#include "../Game/structs.hpp"

#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "Dwmapi.lib")
LPDIRECT3DTEXTURE9 d3d9_texture = nullptr;

IDirect3D9Ex* p_Object = NULL;
IDirect3DDevice9Ex* p_Device = NULL;
D3DPRESENT_PARAMETERS p_Params = { NULL };

HWND MyWnd = NULL;
HWND GameWnd = NULL;
MSG Message = { NULL };

RECT GameRect = { NULL };
D3DPRESENT_PARAMETERS d3dpp;

static ULONG Width = GetSystemMetrics(SM_CXSCREEN);
static ULONG Height = GetSystemMetrics(SM_CYSCREEN);
DWORD ScreenCenterX = Width / 2;
DWORD ScreenCenterY = Height / 2;
static ULONG linescreen = 0;


void render();


HRESULT DirectXInit(HWND hWnd)
{
	if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &p_Object)))
		exit(3);

	D3DPRESENT_PARAMETERS p_Params = { 0 };
	p_Params.Windowed = TRUE;
	p_Params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	p_Params.hDeviceWindow = hWnd;
	p_Params.MultiSampleQuality = D3DMULTISAMPLE_NONE;
	p_Params.BackBufferFormat = D3DFMT_A8R8G8B8;
	p_Params.BackBufferWidth = Width;
	p_Params.BackBufferHeight = Height;
	p_Params.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	p_Params.EnableAutoDepthStencil = TRUE;
	p_Params.AutoDepthStencilFormat = D3DFMT_D16;
	p_Params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	if (FAILED(p_Object->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &p_Params, 0, &p_Device)))
	{
		p_Object->Release();
		exit(4);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX9_Init(p_Device);

	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text] = ImVec4(0.31f, 0.25f, 0.24f, 1.00f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.74f, 0.74f, 0.94f, 1.00f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.62f, 0.70f, 0.72f, 0.56f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.95f, 0.33f, 0.14f, 0.47f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.97f, 0.31f, 0.13f, 0.81f);
	style.Colors[ImGuiCol_TitleBg] = ImColor(180,180,180);
	style.Colors[ImGuiCol_TitleBgActive] = ImColor(180, 180, 180);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImColor(180, 180, 180);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.40f, 0.62f, 0.80f, 0.15f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.39f, 0.64f, 0.80f, 0.30f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.67f, 0.80f, 0.59f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.25f, 0.48f, 0.53f, 0.67f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.48f, 0.47f, 0.47f, 0.71f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.31f, 0.47f, 0.99f, 1.00f);
	style.Colors[ImGuiCol_Button] = ImVec4(1.00f, 0.79f, 0.18f, 0.78f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.42f, 0.82f, 1.00f, 0.81f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.72f, 1.00f, 1.00f, 0.86f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.65f, 0.78f, 0.84f, 0.80f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.88f, 0.94f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.55f, 0.68f, 0.74f, 0.80f);//ImVec4(0.46f, 0.84f, 0.90f, 1.00f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.60f, 0.60f, 0.80f, 0.30f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.60f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.90f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(1.00f, 0.99f, 0.54f, 0.43f);
	style.Colors[ImGuiCol_PopupBg] = ImColor(255,255,255);
	style.Alpha = 1.0f;
	style.FrameRounding = 4;
	style.WindowTitleAlign = { 0.5,0.5 };
	style.IndentSpacing = 12.0f;
	p_Object->Release();
	return S_OK;
}
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CleanuoD3D()
{
	if (p_Device != NULL)
	{
		p_Device->EndScene();
		p_Device->Release();
	}
	if (p_Object != NULL)
	{
		p_Object->Release();
	}
}
LRESULT CALLBACK WinProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Message, wParam, lParam))
		return true;

	switch (Message)
	{
	case WM_DESTROY:
		CleanuoD3D();
		PostQuitMessage(0);
		exit(4);
		break;
	case WM_ACTIVATEAPP:
	{
		if (p_Device)
			p_Device->Reset(&p_Params);
	} break;
	case WM_SIZE:
		if (p_Device != NULL && wParam != SIZE_MINIMIZED)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
			p_Params.BackBufferWidth = LOWORD(lParam);
			p_Params.BackBufferHeight = HIWORD(lParam);
			HRESULT hr = p_Device->Reset(&p_Params);
			if (hr == D3DERR_INVALIDCALL)
				exit(1);
			ImGui_ImplDX9_CreateDeviceObjects();
		}
		break;
	default:
		return DefWindowProc(hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}
MARGINS margin = { -1 };
std::string name = std::string(skCrypt("UnrealWindow"));

auto get_process_wnd(uint32_t pid) -> HWND
{
	std::pair<HWND, uint32_t> params = { 0, pid };
	BOOL bResult = EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
		auto pParams = (std::pair<HWND, uint32_t>*)(lParam);
		uint32_t processId = 0;

		if (GetWindowThreadProcessId(hwnd, reinterpret_cast<LPDWORD>(&processId)) && processId == pParams->second) {
			SetLastError((uint32_t)-1);
			pParams->first = hwnd;
			return FALSE;
		}

		return TRUE;

		}, (LPARAM)&params);

	if (!bResult && GetLastError() == -1 && params.first)
		return params.first;

	return NULL;
}
void SetupWindow()
{
	WNDCLASSEX wClass =
	{
		sizeof(WNDCLASSEX),
		0,
		WinProc,
		0,
		0,
		nullptr,
		LoadIcon(nullptr, IDI_APPLICATION),
		LoadCursor(nullptr, IDC_ARROW),
		nullptr,
		nullptr,
		TEXT(overlayname.c_str()),
		LoadIcon(nullptr, IDI_APPLICATION)
	};

	if (!RegisterClassEx(&wClass))
		exit(1);

	CA2W unicodeStr(name.c_str());
	GameWnd = FindWindowW(unicodeStr, NULL);
	if (GameWnd)
	{
		GetClientRect(GameWnd, &GameRect);
		POINT xy;
		ClientToScreen(GameWnd, &xy);
		GameRect.left = xy.x;
		GameRect.top = xy.y;

		Width = GameRect.right + 2;
		Height = GameRect.bottom + 1;
	}
	else exit(2);

	MyWnd = CreateWindowExA(NULL, overlayname.c_str(), overlayname2.c_str(), WS_POPUP | WS_VISIBLE, GameRect.left, GameRect.top, Width, Height, NULL, NULL, 0, NULL);
	DwmExtendFrameIntoClientArea(MyWnd, &margin);
	SetWindowLong(MyWnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
	ShowWindow(MyWnd, SW_SHOW);
	UpdateWindow(MyWnd);

	Sleep(1000);
}

using namespace ColorStructs;

static void DrawLine(int x1, int y1, int x2, int y2, D3DCOLOR color, int thickness)
{
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, thickness);
}
void DrawFilledRect(int x, int y, int w, int h, D3DCOLOR color)
{
	ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), color, 0, 0);
}

void DrawFilledRect2(int x, int y, int w, int h, ImColor color)
{
	ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), color, 0, 0);
}

void DrawCornerBox(float x, float y, float w, float h, const ImColor& color)
{
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x, y), ImVec2(x + w / 4.f, y), color, 1);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x, y), ImVec2(x, y + h / 4.f), color, 1);

	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x + w, y), ImVec2(x + w - w / 4.f, y), color, 1);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + h / 4.f), color, 1);

	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x, y + h), ImVec2(x + w / 4.f, y + h), color, 1);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - h / 4.f), color, 1);

	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - h / 4.f), color, 1);
	ImGui::GetForegroundDrawList()->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - w / 4.f, y + h), color, 1);
}

void DrawNormalBox(int x, int y, int w, int h, int borderPx, ImColor color)
{
	DrawFilledRect2(x + borderPx, y, w, borderPx, color);
	DrawFilledRect2(x + w - w + borderPx, y, w, borderPx, color);
	DrawFilledRect2(x, y, borderPx, h, color);
	DrawFilledRect2(x, y + h - h + borderPx * 2, borderPx, h, color);
	DrawFilledRect2(x + borderPx, y + h + borderPx, w, borderPx, color);
	DrawFilledRect2(x + w - w + borderPx, y + h + borderPx, w, borderPx, color);
	DrawFilledRect2(x + w + borderPx, y, borderPx, h, color);
	DrawFilledRect2(x + w + borderPx, y + h - h + borderPx * 2, borderPx, h, color);
}

using namespace UE4Structs;

auto Draw2DBox(FVector RootPosition, float Width, float Height, ImColor Colors) -> void
{
	if (Settings::Visuals::bFilled) {
		DrawFilledRect2(RootPosition.x - Width / 2, RootPosition.y - Height / 2, Width, Height, ImVec4(0,0,0, 0.50));
	}
	DrawNormalBox(RootPosition.x - Width / 2, RootPosition.y - Height / 2, Width, Height, Settings::Visuals::BoxWidth, Colors);
	DrawNormalBox(RootPosition.x - Width / 2 - 1, RootPosition.y - Height / 2 - 1, Width + 2, Height + 2, Settings::Visuals::BoxWidth, ImColor(0, 0, 0));
	DrawNormalBox(RootPosition.x - Width / 2 + 1, RootPosition.y - Height / 2 + 1 , Width - 2, Height- 2, Settings::Visuals::BoxWidth, ImColor(0, 0, 0));
}

void DrawRect(int x, int y, int w, int h, D3DCOLOR color, int thickness)
{
	ImGui::GetForegroundDrawList()->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 0, 0, thickness);
}
static std::string string_To_UTF8(const std::string& str)
{
	int nwLen = ::MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);

	wchar_t* pwBuf = new wchar_t[nwLen + 1];
	ZeroMemory(pwBuf, nwLen * 2 + 2);

	::MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.length(), pwBuf, nwLen);

	int nLen = ::WideCharToMultiByte(CP_UTF8, 0, pwBuf, -1, NULL, NULL, NULL, NULL);

	char* pBuf = new char[nLen + 1];
	ZeroMemory(pBuf, nLen + 1);

	::WideCharToMultiByte(CP_UTF8, 0, pwBuf, nwLen, pBuf, nLen, NULL, NULL);

	std::string retStr(pBuf);

	delete[]pwBuf;
	delete[]pBuf;

	pwBuf = NULL;
	pBuf = NULL;

	return retStr;
}

static void DrawStrokeText(int x, int y, ImColor Color, const char* str)
{
	ImFont a;
	std::string utf_8_1 = std::string(str);
	std::string utf_8_2 = string_To_UTF8(utf_8_1);
	ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y - 1), IM_COL32_BLACK, utf_8_2.c_str());
	ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y + 1), IM_COL32_BLACK, utf_8_2.c_str());
	ImGui::GetForegroundDrawList()->AddText(ImVec2(x - 1, y), IM_COL32_BLACK, utf_8_2.c_str());
	ImGui::GetForegroundDrawList()->AddText(ImVec2(x + 1, y), IM_COL32_BLACK, utf_8_2.c_str());
	ImGui::GetForegroundDrawList()->AddText(ImVec2(x, y), ImColor(255,255,255), utf_8_2.c_str());
}
auto DrawOutlinedBox(FVector RootPosition, float Width, float Height, ImColor Color) -> void
{
	if (Settings::Visuals::bFilled) {
		DrawFilledRect2(RootPosition.x - Width / 2, RootPosition.y - Height / 2, Width, Height, ImVec4(0, 0, 0, 0.50));
	}

	DrawCornerBox(RootPosition.x - Width / 2, RootPosition.y - Height / 2, Width, Height, Color);
	DrawCornerBox(RootPosition.x - Width / 2 - 1, RootPosition.y - Height / 2 - 1, Width + 2 , Height +2, ImColor(0,0, 0));
	DrawCornerBox(RootPosition.x - Width / 2 + 1, RootPosition.y - Height / 2 + 1, Width-2, Height - 2, ImColor(0, 0, 0));
}

auto DrawDistance(FVector Location, float Distance) -> void
{
	char dist[64];
	sprintf_s(dist, "%.fm", Distance);

	ImVec2 TextSize = ImGui::CalcTextSize(dist);
	DrawStrokeText(Location.x - TextSize.x / 2, Location.y - TextSize.y / 2, ImGui::GetColorU32({ 255, 255, 255, 255 }), dist);
}
auto DrawPNL(FVector Location,float distance, std::string asgasg) -> void
{
	ImVec2 TextSize = ImGui::CalcTextSize(std::string(" " + asgasg).c_str());
	ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(Location.x - (TextSize.x * 0.6), Location.y - (TextSize.y * 0.6) - (distance * 0.1)), ImVec2(Location.x + (TextSize.x * 0.6) , Location.y + (TextSize.y * 0.6) - (distance * 0.1)), ImColor(96, 96, 96, 200));
	ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(Location.x - (TextSize.x * 0.6), Location.y - (TextSize.y * 0.6) - (distance * 0.1)), ImVec2(Location.x - (TextSize.x * 0.6) + 3, Location.y + (TextSize.y * 0.6) - (distance * 0.1)), ImColor(255,54,54, 240));
	DrawStrokeText(Location.x - TextSize.x / 2, Location.y - TextSize.y / 2 - (distance * 0.1), ImGui::GetColorU32({ 255, 255, 255, 255 }), std::string(" " + asgasg).c_str());
}
auto DrawWPN(FVector Location, std::string asgasg) -> void
{
	ImVec2 TextSize = ImGui::CalcTextSize(std::string(" " + asgasg).c_str());
	ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(Location.x - (TextSize.x * 0.60), Location.y - (TextSize.y * 0.60)), ImVec2(Location.x + (TextSize.x * 0.60), Location.y + (TextSize.y * 0.60)), ImColor(96, 96, 96, 200));
	ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(Location.x - (TextSize.x * 0.60), Location.y - (TextSize.y * 0.60)), ImVec2(Location.x - (TextSize.x * 0.60) + 3, Location.y + (TextSize.y * 0.60)), ImColor(54, 255, 54, 240));
	DrawStrokeText(Location.x - TextSize.x / 2, Location.y - TextSize.y / 2, ImGui::GetColorU32({ 255, 255, 255, 255 }), std::string(" " + asgasg).c_str());
}

auto DrawOzel(FVector Location, float Distance, std::string asgasg) -> void
{
	char dist[64];
	sprintf_s(dist, asgasg.c_str());

	ImVec2 TextSize = ImGui::CalcTextSize(dist);
	DrawStrokeText(Location.x - TextSize.x / 2, Location.y - TextSize.y / 2, ImGui::GetColorU32({ 255, 255, 0, 0 }), asgasg.c_str());
}

auto DrawTracers(FVector Target, ImColor Color) -> void
{
	ImGui::GetForegroundDrawList()->AddLine(
		ImVec2(ScreenCenterX, linescreen),
		ImVec2(Target.x, Target.y),
		Color,
		0.1f
	);
}

static void DrawNewText(int x, int y, D3DCOLOR color, std::string str, int size)
{
	std::string utf_8_1 = std::string(str);
	std::string utf_8_2 = string_To_UTF8(utf_8_1);

	ImGui::GetForegroundDrawList()->AddText(NULL, size, ImVec2(x, y), color, utf_8_2.c_str());
}
void TextCentered(int x, int y, D3DCOLOR color, std::string str, int size) {
	int textWidth = str.length();
	std::string utf_8_1 = std::string(str);
	std::string utf_8_2 = string_To_UTF8(utf_8_1);

	ImGui::GetForegroundDrawList()->AddText(NULL, size, ImVec2(x - (textWidth * (size / 4)), y), color, utf_8_2.c_str());
}
auto DrawHealthBar(FVector RootPosition, float Width, float Height, float Health, float RelativeDistance, int health)-> void
{
	const float  multiplier = 2.55; //number we multiply our health by to get our colors(multiply our health by 2.55 to give a number we then use for the color. since 255 is the max of any color for our esp ie. 100 full health * 2.55 = 255 or max color)
	int red = 255 - (health * multiplier);	//find red value (no health = max red, full health = no red)
	int green = health * multiplier;	//find green value (full health = max green, no health = no green)
	int blue = 0;	//no blue on color scale red to green
	int alpha = 255;	//max alpha

	auto HPBoxWidth = 0.5 / RelativeDistance;

	auto HPBox_X = RootPosition.x - Width / 2 - 5 - HPBoxWidth;
	auto HPBox_Y = RootPosition.y - Height / 2 + (Height - Height * (Health / 100));

	int HPBoxHeight = Height * (Health / 100);

	DrawFilledRect(HPBox_X, HPBox_Y, HPBoxWidth, HPBoxHeight, D3DCOLOR_ARGB(alpha, blue, green, red));
	DrawRect(HPBox_X - 1, HPBox_Y - 1, HPBoxWidth + 2, HPBoxHeight + 2, D3DCOLOR_ARGB(255, 0, 0, 0), 1);
}
auto DrawOutlinedBox2(FVector RootPosition, float Width, float Height, ImColor Color) -> void
{
	float width = Width / 2;
	float height = Height / 2;
	DrawLine(RootPosition.x - width, RootPosition.y - height, RootPosition.x - width, RootPosition.y + height, Color, 1);
	DrawLine(RootPosition.x + width, RootPosition.y + height, RootPosition.x + width, RootPosition.y - height, Color, 1);
	DrawLine(RootPosition.x - width, RootPosition.y + height, RootPosition.x + width, RootPosition.y + height, Color, 1);
	DrawLine(RootPosition.x - width, RootPosition.y - height, RootPosition.x + width, RootPosition.y - height, Color, 1);
}
