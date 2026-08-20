
#include "anticrack.h"
#include <windows.h>


typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef ULONG_PTR* PULONG_PTR;


bool IsDebugged() {
    BOOL dbg = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &dbg);
    return dbg ||
        FindWindowA(NULL, "x64dbg") || FindWindowA(NULL, "Cheat Engine") ||
        FindWindowA(NULL, "IDA") || IsDebuggerPresent();
}


void Crash_NtRaiseHardError() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {

        using NtRaise_t = LONG(NTAPI*)(
            NTSTATUS ErrorStatus,
            ULONG NumberOfParameters,
            PUNICODE_STRING UnicodeStringParameterMask,
            PULONG_PTR Parameters,
            ULONG ValidResponseOptions,
            PULONG_PTR Response
            );

        auto NtRaiseHardError = (NtRaise_t)GetProcAddress(ntdll, "NtRaiseHardError");
        if (NtRaiseHardError) {
            NTSTATUS status = 0xC0000022;
            ULONG_PTR parameters[1] = { 0xC0000005 };
            ULONG_PTR response = 0;

            NtRaiseHardError(status, 1, NULL, parameters, 6, &response);
        }
    }
}


void Crash_KeBugCheck() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
        using RtlFailFast_t = VOID(NTAPI*)(USHORT, PVOID, PVOID);
        auto RtlFailFast = (RtlFailFast_t)GetProcAddress(ntdll, "RtlFailFast");
        if (RtlFailFast) RtlFailFast(0xF2, NULL, NULL);
    }
}


void Crash_NullDereference() {
    volatile char* ptr = NULL;
    while (true) {
        *ptr = 0xCC;
        ptr++;
    }
}


void Crash_StackOverflow() {
    char stack[1];
    Crash_StackOverflow();
}


void Crash_HeapCorruption() {
    HANDLE heap = GetProcessHeap();
    void* mem = HeapAlloc(heap, HEAP_ZERO_MEMORY, 16);
    if (mem) {

        *(DWORD*)mem = 0x41414141;
        *(DWORD*)((BYTE*)mem + 4) = 0xDEADBEEF;

        HeapFree(heap, 0, mem);
        HeapFree(heap, 0, mem);
    }
}


void ForceBSOD() {
    if (!IsDebugged()) return;

    MessageBoxA(NULL, "DEBUGGER DETECTED",
        "ANTI-DEBUG", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
    Sleep(2000);


    Crash_NtRaiseHardError();
    Sleep(50);
    Crash_KeBugCheck();
    Sleep(50);
    Crash_NullDereference();
}


#pragma warning(disable : 4073 4100)
#pragma init_seg(".CRT$XCU")

static void AntiDebugThread() {
    while (true) {
        if (IsDebugged()) {
            ForceBSOD();
            break;
        }
        Sleep(100);
    }
}

static struct AutoInit {
    AutoInit() {
        CloseHandle(CreateThread(NULL, 0, [](LPVOID)->DWORD {
            AntiDebugThread();
            return 0;
            }, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL));
    }
} g_antiDebugInit;