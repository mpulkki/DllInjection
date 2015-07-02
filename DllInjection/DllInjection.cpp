// DllInjection.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Process.hpp"
#include <iostream>

using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
    const char* path    = "D:\\Apps\\Steam\\steamapps\\common\\Space Hulk\\game.exe";
    const char* dllPath = "D:\\Apps\\Steam\\steamapps\\common\\Space Hulk\\DllIntruder.dll";

    // Launch a new process
    auto process = Injector::Process::LaunchSuspended(path);

    if (process->CreationSuccess())
    {
        // Try to inject the dll
        if (process->InjectDLL(dllPath))
        {
            // Success! Resume the process
            process->Resume();
            cout << "Dll " << dllPath << " injected successfully!" << endl;
        }
        else
        {
            process->Shutdown();
            cout << process->GetError() << endl;
        }
    }
    else
        cout << process->GetError() << endl;

	return 0;
}

