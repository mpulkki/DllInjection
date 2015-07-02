// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "Detours.hpp"

#pragma comment(lib, "d3d9.lib")
#include <d3d9.h>

using namespace std;

//void CreateConsole()
//{
//    AllocConsole();
//
//    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
//    int hCrt = _open_osfhandle((long)handle_out, _O_TEXT);
//    FILE* hf_out = _fdopen(hCrt, "w");
//    setvbuf(hf_out, NULL, _IONBF, 1);
//    *stdout = *hf_out;
//
//    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
//    hCrt = _open_osfhandle((long)handle_in, _O_TEXT);
//    FILE* hf_in = _fdopen(hCrt, "r");
//    setvbuf(hf_in, NULL, _IONBF, 128);
//    *stdin = *hf_in;
//}

#define LOGFILE_LENGTH (21 + 1) 
#define CREATEDEVICE_OFFSET   (16)
#define SETRENDERSTATE_OFFSET (57)

static const char* logFileName = "InjectorDllOutput.txt";
static std::ofstream logOutput;

void WriteLogEntry(const std::string& message)
{
    // Open file if not yet
    if (!logOutput.is_open())
    {
        // Get module location
        char filePath[MAX_PATH];

        DWORD fileNameLength = GetModuleFileNameA(
            NULL,
            filePath,
            MAX_PATH);

        // split path
        char drive[_MAX_DRIVE];
        char dir[_MAX_DIR];

        _splitpath_s(
            filePath,
            drive, _MAX_DRIVE,
            dir, _MAX_DIR,
            NULL, 0,
            NULL, 0);

        // Get directory path
        char directoryPath[_MAX_DRIVE + _MAX_DIR + LOGFILE_LENGTH];
        memset(directoryPath, 0, _MAX_DRIVE + _MAX_DIR);

        strcat_s(directoryPath, drive);
        strcat_s(directoryPath, dir);
        strcat_s(directoryPath, logFileName);

        // Open file to this location
        logOutput.open(directoryPath, std::ios::out | std::ios::app);

        // Write the message
        if (logOutput.is_open())
        {
            logOutput << message.c_str() << std::endl;
            logOutput.close();
        }
    }
}

// Detour declarations
static Intruder::StaticFunctionDetour detourLoadLibraryA;
static Intruder::StaticFunctionDetour detourLoadLibraryW;

static Intruder::StaticFunctionDetour  detourDirect3DCreate9;
static Intruder::VirtualFunctionDetour createD3D9DeviceDetour;
static Intruder::VirtualFunctionDetour setRenderStateDetour;

IDirect3DDevice9* d3d9Device = NULL;
bool wireframeEnabled = true;

HRESULT STDMETHODCALLTYPE SetRenderStateCustom(LPDIRECT3DDEVICE9 _this, D3DRENDERSTATETYPE State, DWORD Value)
{
    setRenderStateDetour.Unhook();

    DWORD modifiedValue = Value;

    // draw in wireframe
    if (State == D3DRS_FILLMODE && wireframeEnabled)
        modifiedValue = D3DFILL_WIREFRAME;

    auto result = _this->SetRenderState(State, modifiedValue);

    setRenderStateDetour.Hook();

    return result;
}

HRESULT WINAPI CreateDeviceCustom(LPDIRECT3D9 _this, UINT adapter, D3DDEVTYPE deviceType, HWND hFocusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
{
    WriteLogEntry(string("   - Direct3D9 device created!"));

    createD3D9DeviceDetour.Unhook();

    HRESULT result = _this->CreateDevice(adapter, deviceType, hFocusWindow, behaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    // Store the device pointer
    d3d9Device = *ppReturnedDeviceInterface;

    // Hook to different device functions
    setRenderStateDetour = Intruder::VirtualFunctionDetour::Create(d3d9Device, &SetRenderStateCustom, SETRENDERSTATE_OFFSET);
    setRenderStateDetour.Hook();

    createD3D9DeviceDetour.Hook();

    return result;
}

static HHOOK keyboardHook = 0;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && ((DWORD)lParam & 0x80000000) == 0)
    {
        if (wParam == VK_RETURN)
        {
            wireframeEnabled = !wireframeEnabled;
        }
    }

    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

IDirect3D9* STDMETHODCALLTYPE Direct3DCreate9Custom(UINT SDKVersion)
{
    WriteLogEntry(string("   - Direct3D9 interface (SDK version ") + to_string(SDKVersion) + ") created!");

    detourDirect3DCreate9.Unhook();

    // Get the device pointer using the original function
    IDirect3D9* device = Direct3DCreate9(SDKVersion);

    // Hook to the device creation function (offset 16)
    createD3D9DeviceDetour = Intruder::VirtualFunctionDetour::Create(device, &CreateDeviceCustom, CREATEDEVICE_OFFSET);
    createD3D9DeviceDetour.Hook();

    detourDirect3DCreate9.Hook();

    return device;
}

// Our own loadlibrary-function
HMODULE WINAPI LoadLibraryACustom(_In_ LPCSTR lpFileName)
{    
    WriteLogEntry(string(" - LibraryA loaded: ") + lpFileName);
    
    detourLoadLibraryA.Unhook();

    // Forward the function call to the original function
    HMODULE hModule = LoadLibraryA(lpFileName);

    if (strcmp(lpFileName, "d3d9.dll") == 0)
    {
        // Next we want access to the direct3d device
        LPVOID createDeviceAddress = GetProcAddress(hModule, "Direct3DCreate9");
        
        // Init hooks!
        detourDirect3DCreate9 = Intruder::StaticFunctionDetour::Create(createDeviceAddress, &Direct3DCreate9Custom);
        detourDirect3DCreate9.Hook();

        WriteLogEntry("  - d3d9.dll found! ");
    }

    detourLoadLibraryA.Hook();

    if (keyboardHook == NULL)
    {
        // Listen keyboard input
        keyboardHook = SetWindowsHookEx(WH_KEYBOARD, (HOOKPROC)KeyboardProc, hModule, 0);

        if (keyboardHook == NULL)
            WriteLogEntry("Failed to set keyboard hook");
    }

    return hModule;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
    case DLL_PROCESS_ATTACH:
        {
            cout << "DllIntruder injected into the target process!" << endl;

            // Hook to the loadlibrarya-function
            LPVOID loadLibraryAAddress = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

            detourLoadLibraryA = Intruder::StaticFunctionDetour::Create(loadLibraryAAddress, &LoadLibraryACustom);
            detourLoadLibraryA.Hook();
        }
        break;
	case DLL_THREAD_ATTACH:
        break;
	case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        {
            cout << "Detaching dll from the process" << endl;
            UnhookWindowsHookEx(keyboardHook);
        }
		break;
	}
	return TRUE;
}

