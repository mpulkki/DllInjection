DllInjection
------------

Description:

Simple injector program that hijacks Directx9 SetRenderState-function and allows user to switch between solid and wireframe modes. Uses dll injection and function hooking. Implementation is really simple and naive and has been tested only with Space Hulk -game. USE AT YOUR OWN RISK!

Solution contains two projects: DllIntruder implements the hijacking logic and compiles into a dll. DllInjection launches the target process and handles the dll injection itself.

Points of interests:
 - DllInjection.cpp. Modify path variables "path" and "dllPath".
 - dllmain.cpp. Contains the hijacking logic itself
 - Detours.hpp. Contains classes for hooking both static and virtual functions. DANGEROUS!