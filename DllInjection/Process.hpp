#pragma once

#include <Windows.h>
#include "stdafx.h"

namespace Injector
{    
    typedef std::unique_ptr<class Process> ProcessPtr;

    class Process
    {
        struct Info
        {
            bool                Success;
            STARTUPINFOA        StartupInfo;
            PROCESS_INFORMATION ProcessInfo;
        };

    public:
        ~Process();

        // Checks that the process creation was successfull 
        bool CreationSuccess() const;

        // Returns error message if launching failed
        const std::string& GetError() const;

        // Injects a dll to the process
        bool InjectDLL(const std::string& dllPath);

        // Launches a new process in suspended state
        static ProcessPtr LaunchSuspended(const std::string& executablePath);

        // Resumes the process thread
        void Resume();

        // Shuts down the process
        void Shutdown();

    private:
        Process();
        Process(const Process&) = delete;
        Process& operator=(const Process&) = delete;

        void ConstructErrorMessage();
        bool SetError(DWORD dwErrCode);     // Returns always false
    private:
        std::string m_error;
        Info        m_info;
    };    
}