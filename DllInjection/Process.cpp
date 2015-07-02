#include "stdafx.h"
#include "Process.hpp"

using namespace std;

#define CHECK_OK if (!m_info.Success) return;
#define CHECK_OK_B if (!m_info.Success) return false;

namespace Injector
{

    Process::Process()
    {
        ZeroMemory(&m_info, sizeof(Process::Info));
    }

    Process::~Process()
    {
    }

    void Process::ConstructErrorMessage()
    {
        m_error.clear();

        // Get id of the previous error
        auto errorId = GetLastError();

        LPSTR errorMessage = NULL;

        // And get the error message
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            errorId,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&errorMessage,
            0,
            NULL);

        if (errorMessage != nullptr)
        {
            m_error = std::string(errorMessage);

            // free reserved memory
            LocalFree(errorMessage);
        }
    }

    bool Process::CreationSuccess() const
    {
        return m_info.Success;
    }

    const std::string& Process::GetError() const
    {
        return m_error;
    }

    bool Process::InjectDLL(const std::string& dllPath)
    {
        CHECK_OK_B;

        // Validate path
        if (dllPath.length() == 0)
            return SetError(ERROR_BAD_PATHNAME);

        // Conversion to LPCTSTR required
        wstring wDllPath = wstring(dllPath.begin(), dllPath.end());

        auto fileAttributes = GetFileAttributes(wDllPath.c_str());

        if (fileAttributes == INVALID_FILE_ATTRIBUTES || fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return SetError(ERROR_BAD_PATHNAME);

        const char* cPath = dllPath.c_str();
        auto        cLen  = strlen(cPath);

        // (As it turns out, Kernel32.dll is loaded to the same memory location in every process so the LoadLibrary-address is valid across processes
        LPVOID loadLibraryAaddress = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

        if (!loadLibraryAaddress)
            return false;

        // Path to this dll has to be written to the address space of the target process
        // Allocate memory for this
        LPVOID allocatedMemPtr = VirtualAllocEx(
            m_info.ProcessInfo.hProcess,    // Process
            NULL,                           // Address
            cLen,
            MEM_COMMIT,                     // Allocation type
            PAGE_READWRITE);                // Protect

        if (!allocatedMemPtr)
            return false;

        // Write dll path to this address
        if (!WriteProcessMemory(
            m_info.ProcessInfo.hProcess,
            allocatedMemPtr,                // Address
            cPath,                          // Buffer
            cLen,                           // Buffer size
            NULL))                          // Out: how many bytes were written
        {
            return false;
        }

        // Dll path is in the memory now. Launch a remote thread that will actually execute the dll
        HANDLE thread = CreateRemoteThread(
            m_info.ProcessInfo.hProcess,
            NULL,                           // Thread attributes
            0,                              // Stack size
            (LPTHREAD_START_ROUTINE)loadLibraryAaddress,
            allocatedMemPtr,
            NULL,                           // Creation flags
            NULL);                          // Thread id

        bool success = false;

        if (!thread)
            return false;
        else
        {
            // Wait for the thread to finish
            DWORD threadResult = WaitForSingleObject(thread, 50000);

            success = threadResult == WAIT_OBJECT_0;
        }

        CloseHandle(thread);
        return success;
    }

    ProcessPtr Process::LaunchSuspended(const string& executablePath)
    {
        // Create and start a new process
        ProcessPtr process(new Process);
        
        auto path = executablePath;

        auto result = CreateProcessA(
            NULL,                               // Application name
            const_cast<LPSTR>(path.c_str()),    // CommandLine
            0,                                  // ProcessAttributes
            0,                                  // ThreadAttributes
            false,                              // InheritHandles
            CREATE_SUSPENDED,                   // CreationFlags
            0,                                  // Environment
            0,                                  // Current directory
            &process->m_info.StartupInfo,
            &process->m_info.ProcessInfo
            );

        if (FAILED(result))
        {
            process->ConstructErrorMessage();
            process->m_info.Success = false;
        }
        else
            process->m_info.Success = true;

        return process;
    }

    void Process::Resume()
    {
        ResumeThread(m_info.ProcessInfo.hThread);
    }

    bool Process::SetError(DWORD dwErrCode)
    {
        SetLastError(dwErrCode);
        ConstructErrorMessage();
        return false;
    }

    void Process::Shutdown()
    {
        CHECK_OK;

        auto hProcess = m_info.ProcessInfo.hProcess;

        if (hProcess == 0)
            return;

        // Terminate the process
        auto result = TerminateProcess(hProcess, 0);

        // And the handle also
        CloseHandle(hProcess);

        ZeroMemory(&m_info, sizeof(Process::Info));
    }

}