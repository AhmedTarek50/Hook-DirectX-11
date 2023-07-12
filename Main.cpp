#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <thread>
#include <TlHelp32.h>
#include <string>
#include <iostream>
#include <vector>


#include "MinHook/MinHook.h"
#if _WIN64 
#pragma comment(lib, "MinHook/libMinHook.x64.lib")
#else
#pragma comment(lib, "MinHook/libMinHook.x86.lib")
#endif

#include "GLFW\glfw3.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")


// Our State
bool open = false;
bool THealth = false;
bool THealthOF = false;
bool TSPEED = false;
bool TUNDERGROUND = false;
bool TFLY = false;
auto f = 50.f;

//  [sauerbraten] offsets::
// defult value Reference
//uintptr_t addrR;
uintptr_t moduleBase = (uintptr_t)GetModuleHandle("sauerbraten.exe");
uintptr_t value;
unsigned int localPlayer = 0x2A2560;
unsigned int RState = 0x3472D0;
unsigned int health = 0x178;
unsigned int shild = 0x70CEA0 + 0x4;


// Font State
ImFont* mainfont;
ImFont* fontNULL;


// Calculate The Addresses To The Target Value
uintptr_t FindAddress(uintptr_t ptr, std::vector<unsigned int> offsets)
{
    value = ptr;
    for (unsigned int i = 0; i < offsets.size(); i++)
    {
        value = *(uintptr_t*)value;
        value += offsets[i];
    }

    // defult value Reference
    //if (TAIMBOTOF == false)
    //{
        //addrR = *(int*)address;
    //}

    return value;
}


HINSTANCE dll_handle;
bool consoleAllocated = false;
HWND consoleHandle = NULL;

void ShowInjectionSuccess()
{
    MessageBoxW(NULL, L"DLL injected successfully!", L"Injection Success", MB_OK | MB_ICONINFORMATION);
}

void ShowUnloadSuccess()
{
    MessageBoxW(NULL, L"DLL unloaded successfully!", L"Unload Success", MB_OK | MB_ICONINFORMATION);
}


// function to cleanup when unload the dll
void Cleanup()
{
    // Perform cleanup actions here

    ShowUnloadSuccess();

    // Free the console window if it was allocated
    if (consoleAllocated)
    {
        // Restore the original console window state
        ShowWindow(consoleHandle, SW_HIDE);
        FreeConsole();
    }
}

// function for cleanup when close the console
BOOL WINAPI ConsoleCloseHandler(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_CLOSE_EVENT)
    {
        Cleanup();

        // Detach DLL before closing console
        if (dll_handle != NULL)
        {
            FreeLibrary(dll_handle);
        }
    }
    return FALSE;
}

WNDPROC oWndProc;

long __stdcall ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
long __stdcall ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (uMsg) {
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		return true;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false;
		return true;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true;
		return true;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false;
		return true;
	case WM_MBUTTONDOWN:
		io.MouseDown[2] = true;
		return true;
	case WM_MBUTTONUP:
		io.MouseDown[2] = false;
		return true;
	case WM_XBUTTONDOWN:
		if ((GET_KEYSTATE_WPARAM(wParam) & MK_XBUTTON1) == MK_XBUTTON1)
			io.MouseDown[3] = true;
		else if ((GET_KEYSTATE_WPARAM(wParam) & MK_XBUTTON2) == MK_XBUTTON2)
			io.MouseDown[4] = true;
		return true;
	case WM_XBUTTONUP:
		if ((GET_KEYSTATE_WPARAM(wParam) & MK_XBUTTON1) == MK_XBUTTON1)
			io.MouseDown[3] = false;
		else if ((GET_KEYSTATE_WPARAM(wParam) & MK_XBUTTON2) == MK_XBUTTON2)
			io.MouseDown[4] = false;
		return true;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? +1.0f : -1.0f;
		return true;
	case WM_MOUSEMOVE:
		io.MousePos.x = (signed short)(lParam);
		io.MousePos.y = (signed short)(lParam >> 16);
		return true;
	}
	return false;
}


long __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    // Toggle menu
    if (GetAsyncKeyState(VK_INSERT) & 1)
        open = !open;

    // pass messages to imgui
    if (open && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return 1L;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

typedef long(__stdcall* present)(IDXGISwapChain*, UINT, UINT);
present p_present;
present p_present_target;
bool get_present_pointer()
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	IDXGISwapChain* swap_chain;
	ID3D11Device* device;

	const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(
		NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		feature_levels,
		2,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		nullptr,
		nullptr) == S_OK)
	{
		void** p_vtable = *reinterpret_cast<void***>(swap_chain);
		swap_chain->Release();
		device->Release();
		//context->Release();
		p_present_target = (present)p_vtable[8];
		return true;
	}
	return false;
}


bool init = false;
HWND window = NULL;
ID3D11Device* p_device = NULL;
ID3D11DeviceContext* p_context = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;
static long __stdcall detour_present(IDXGISwapChain* p_swap_chain, UINT sync_interval, UINT flags) {
	if (!init) {
		if (SUCCEEDED(p_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&p_device)))
		{
			p_device->GetImmediateContext(&p_context);
			DXGI_SWAP_CHAIN_DESC sd;
			p_swap_chain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			p_device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();

			// load a custom font
			io.Fonts->AddFontDefault();
			mainfont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\ChrustyRock-ORLA.ttf", 12.8f);
			IM_ASSERT(mainfont != NULL);

			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX11_Init(p_device, p_context);
			init = true;
		}
		else
			return p_present(p_swap_chain, sync_interval, flags);
	}
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	// Setting Window Size
	ImGui::SetNextWindowSize(ImVec2(500, 500));

	ImGui::NewFrame();

	// Start Of the Custom Font
	ImGui::PushFont(mainfont);


	// Show Our Dear Menu
	ImGui::Begin("Sauerbraten Hack",
		&open
	);

	// Start Of Our UI
	ImGui::Checkbox("Unlimited Health", &THealth);

	if (THealth == true)
	{
		THealthOF = true;
		*(int*)FindAddress(moduleBase + localPlayer, { health }) = 99999999;
	}
	else if (THealth == false && THealthOF == true)
	{
		THealthOF = false;
		*(int*)value = 100;
		// defult value Reference
		//*(int*) value = addrR;
	}

	ImGui::Text("FOV");

	// End Of the Custom Font
	ImGui::PopFont();
	// Defult Font For The Slider Avoiding Font Bug
	ImGui::PushFont(fontNULL);
	ImGui::SliderFloat("##add", &f, 0.f, 100.f);
	ImGui::Text(" ");
	ImGui::PopFont();

	ImGui::PushFont(mainfont);

	ImGui::Checkbox("SPEED", &TSPEED);

	ImGui::Checkbox("UNDERGROUND", &TUNDERGROUND);

	ImGui::Checkbox("FLY", &TFLY);

	// End Of Our UI
	ImGui::PopFont();


	ImGui::End();

	ImGui::EndFrame();


	// prepare the data for rendering so we can call GetDrawData()
	ImGui::Render();

	// Only showing out Menu If "Insert" Key Is Clicked
	if (open)
	{
		p_context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
		// the real rendering
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = true;
	}
	else
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
	}

	return p_present(p_swap_chain, sync_interval, flags);
}

DWORD __stdcall EjectThread(LPVOID lpParameter) {
	Sleep(100);
	FreeLibraryAndExitThread(dll_handle, 0);
	Sleep(100);
	return 0;
}



DWORD WINAPI ThreadMain(LPVOID lpParameter)
{
    ShowInjectionSuccess();

	// Redirect standard output to the console window
	FILE* fileStream;
	if (freopen_s(&fileStream, "CONOUT$", "w", stdout) != 0)
	{
		MessageBoxW(NULL, L"Failed to redirect console output!", L"Error", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	if (!get_present_pointer())
	{
		return 1;
	}

	MH_STATUS status = MH_Initialize();
	if (status != MH_OK)
	{
		return 1;
	}

	if (MH_CreateHook(reinterpret_cast<void**>(p_present_target), &detour_present, reinterpret_cast<void**>(&p_present)) != MH_OK) {
		return 1;
	}

	if (MH_EnableHook(p_present_target) != MH_OK) {
		return 1;
	}

	while (!GetAsyncKeyState(VK_NUMPAD1))
		std::this_thread::sleep_for(std::chrono::milliseconds(50));


	//Cleanup
	if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK) {
		return 1;
	}
	if (MH_Uninitialize() != MH_OK) {
		return 1;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();


	if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = NULL; }
	if (p_context) { p_context->Release(); p_context = NULL; }
	if (p_device) { p_device->Release(); p_device = NULL; }
	SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));

    Cleanup();

    FreeLibraryAndExitThread(dll_handle, 0);

    return 0;
}


BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        dll_handle = hModule;

        // Allocate a console window for debugging
        AllocConsole();
        consoleAllocated = true;

        // Set console close handler
        SetConsoleCtrlHandler(ConsoleCloseHandler, TRUE);

        // Get the handle to the console window
        consoleHandle = GetConsoleWindow();

        // Create the main thread
        CreateThread(NULL, 0, ThreadMain, NULL, 0, NULL);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        // Check if the DLL is being unloaded due to a FreeLibrary call
        if (lpReserved == NULL)
        {
            // Perform cleanup only if the DLL is being unloaded normally
            Cleanup();
        }
    }

    return TRUE;
}
