#pragma once

#include "stdafx.h"

#define CHECK_OK if (!m_hookFuncAddr || !m_origFuncAddr) return;

namespace Intruder
{
    typedef unsigned char byte;


    // Base class for function detours
    class FunctionDetour
    {
    public:
        FunctionDetour() : m_origFuncAddr(NULL), m_hookFuncAddr(NULL) { }
        virtual ~FunctionDetour() { }

        // Enables the hook. All calls to the original function will be redirected to the hooked one
        virtual void Hook() = 0;
        
        // Disables the hook
        virtual void Unhook() = 0;

    protected:

        // Initializes the detour
        virtual void Init(LPVOID originalFuncAddress, LPVOID hookFuncAddress)
        {
            m_origFuncAddr = originalFuncAddress;
            m_hookFuncAddr = hookFuncAddress;
        }

    protected:
        LPVOID m_origFuncAddr;
        LPVOID m_hookFuncAddr;
    };


    // Class for creating detours to static functions
    class StaticFunctionDetour : public FunctionDetour
    {
    public:
        ~StaticFunctionDetour()
        {
            Unhook();
        }

        static StaticFunctionDetour Create(LPVOID targetFuncAddress, LPVOID hookFuncAddress)
        {
            // Create and initialize the detour
            StaticFunctionDetour detour;

            detour.Init(targetFuncAddress, hookFuncAddress);

            return detour;
        }

        void Hook() override
        {
            CHECK_OK;

            byte asmCodes[6] = { 0xe9, 0, 0, 0, 0, 0xc3 };		// 0xe9 = jmp, 0xc3 = ret

            // Evaluate jump distances
            DWORD jumpDistance = (DWORD)m_hookFuncAddr - (DWORD)m_origFuncAddr - 5;

            // Replace original function instructions with our jump instruction
            *((DWORD*)&asmCodes[1]) = jumpDistance;
            dist = jumpDistance;
            WriteProtected(m_origFuncAddr, asmCodes, 6);
        }

        void Unhook() override
        {
            CHECK_OK;

            // Undo hook by copying original instructions back
            WriteProtected(m_origFuncAddr, m_origInstructions, 6);
        }

    protected:

        void Init(LPVOID originalFuncAddress, LPVOID hookFuncAddress)
        {
            FunctionDetour::Init(originalFuncAddress, hookFuncAddress);

            CHECK_OK;

            // Store original instructions
            //WriteProtected(m_origInstructions, originalFuncAddress, 6);
            memcpy(m_origInstructions, originalFuncAddress, 6);
        }

    private:
        void WriteProtected(LPVOID dest, LPVOID source, int size)
        {
            DWORD oldProtect;
            VirtualProtect(dest, size, PAGE_EXECUTE_READWRITE, &oldProtect);

            // copy bytes
            memcpy(dest, source, size);

            // Restore original protection
            VirtualProtect(dest, size, oldProtect, &oldProtect);
        }

    public:
        int dist;
        byte m_origInstructions[6];     // Original instructions that are replaced by the jump code
    };


    class VirtualFunctionDetour : public FunctionDetour
    {
    public:
        VirtualFunctionDetour() : m_ptrToVtable(nullptr) { }

        // Creates a new detour object for a virtual function of the given class. Give offset in function units (0 = first function, 1 = second function)
        template <typename TClass>
        static VirtualFunctionDetour Create(TClass* object, LPVOID hookFunctionAddress, unsigned offset)
        {
            // Get pointer to the vtable (array of function pointers)
            intptr_t** vTablePtr = *(intptr_t***)object;

            // Evaluate function pointer position using the offset
            intptr_t** targetFunctionPtrPos = vTablePtr + offset;

            VirtualFunctionDetour detour;

            // Hook!
            detour.Init((LPVOID*)targetFunctionPtrPos, hookFunctionAddress);

            return detour;
        }

        void Hook() override
        {
            CHECK_OK;
            WriteMemory(m_ptrToVtable, m_hookFuncAddr);
        }

        void Unhook() override
        {
            CHECK_OK;
            WriteMemory(m_ptrToVtable, m_origFuncAddr);
        }

    protected:

        void Init(LPVOID* ptrToVTableFunc, LPVOID hookFunctionAddress)
        {
            // ptrToVTableFunc is a pointer to the vtable position. Get pointer to the original function
            LPVOID originalFunctionPtr = *ptrToVTableFunc;

            FunctionDetour::Init(originalFunctionPtr, hookFunctionAddress);

            // Store the original address
            m_ptrToVtable = (LPVOID)ptrToVTableFunc;
        }

        // Helper function for writing to protected memory
        inline void WriteMemory(LPVOID destination, LPVOID source)
        {
            DWORD oldProtect;
            VirtualProtect(destination, sizeof(intptr_t), PAGE_EXECUTE_READWRITE, &oldProtect);

            // copy bytes
            *(LPVOID*)destination = source;

            VirtualProtect(destination, sizeof(intptr_t), oldProtect, &oldProtect);
        }

    private:
        LPVOID m_ptrToVtable;        // Pointer to virtual table

    };
}

#undef CHECK_OK