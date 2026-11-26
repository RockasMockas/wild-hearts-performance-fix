typedef unsigned char BYTE;
typedef unsigned short WORD, WCHAR;
typedef unsigned int UINT, DWORD;
typedef int BOOL, LONG, HRESULT;
typedef unsigned long long SIZE_T;
typedef void *HANDLE, *HMODULE;
typedef struct { DWORD d1; WORD d2, d3; BYTE d4[8]; } GUID;
typedef struct { DWORD dwSize, cntUsage, th32ThreadID, th32OwnerProcessID; LONG tpBasePri, tpDeltaPri; DWORD dwFlags; } THREADENTRY32;

#define API __declspec(dllimport)
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define DLL_PROCESS_ATTACH 1
#define PAGE_READWRITE 0x04
#define HIGH_PRIORITY_CLASS 0x80
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_SET_LIMITED_INFORMATION 0x0400
#define THREAD_QUERY_LIMITED_INFORMATION 0x0800
#define TH32CS_SNAPTHREAD 0x4

API HMODULE LoadLibraryW(const WCHAR *);
API void *GetProcAddress(HMODULE, const char *);
API UINT GetSystemDirectoryW(WCHAR *, UINT);
API UINT GetModuleFileNameW(HMODULE, WCHAR *, UINT);
API BOOL DisableThreadLibraryCalls(HMODULE);
API HANDLE CreateThread(void *, SIZE_T, DWORD (*)(void *), void *, DWORD, DWORD *);
API void Sleep(DWORD);
API DWORD GetCurrentThreadId(void);
API DWORD GetCurrentProcessId(void);
API HANDLE GetCurrentProcess(void);
API HANDLE GetCurrentThread(void);
API BOOL SetPriorityClass(HANDLE, DWORD);
API BOOL SetThreadPriority(HANDLE, int);
API BOOL SetThreadPriorityBoost(HANDLE, BOOL);
API BOOL GetThreadPriorityBoost(HANDLE, BOOL *);
API BOOL SetProcessPriorityBoost(HANDLE, BOOL);
API HANDLE OpenThread(DWORD, BOOL, DWORD);
API BOOL CloseHandle(HANDLE);
API HANDLE CreateToolhelp32Snapshot(DWORD, DWORD);
API BOOL Thread32First(HANDLE, THREADENTRY32 *);
API BOOL Thread32Next(HANDLE, THREADENTRY32 *);
API BOOL VirtualProtect(void *, SIZE_T, DWORD, DWORD *);

#define FORWARDED(X) \
    X(ApplyCompatResolutionQuirking) X(CompatString) X(CompatValue) X(DXGID3D10CreateDevice) \
    X(DXGID3D10CreateLayeredDevice) X(DXGID3D10GetLayeredDeviceSize) X(DXGID3D10RegisterLayers) \
    X(DXGID3D10ETWRundown) X(DXGIDeclareAdapterRemovalSupport) X(DXGIDisableVBlankVirtualization) \
    X(DXGIDumpJournal) X(DXGIGetDebugInterface1) \
    X(DXGIReportAdapterConfiguration) X(PIXBeginCapture) X(PIXEndCapture) X(PIXGetCaptureState) \
    X(SetAppCompatStringPointer) X(UpdateHMDEmulationStatus)
#define HOOKED(X) X(CreateDXGIFactory) X(CreateDXGIFactory1) X(CreateDXGIFactory2)

#define DECLARE(name) void *p_##name;
#define RESOLVE(name) p_##name = GetProcAddress(dxgi, #name);
#define REQUIRE(name) if (!(p_##name = GetProcAddress(dxgi, #name))) return FALSE;
#define STUB(name) __asm__(".intel_syntax noprefix\n.globl Stub_" #name "\nStub_" #name ":\n jmp qword ptr [rip + p_" #name "]\n.att_syntax prefix\n");

FORWARDED(DECLARE)
HOOKED(DECLARE)
FORWARDED(STUB)

enum {
    QUERY_INTERFACE = 0,
    RELEASE = 2,
    PRESENT = 8,
    CREATE_SWAP_CHAIN = 10,
    CREATE_SWAP_CHAIN_FOR_HWND = 15,
    CREATE_SWAP_CHAIN_FOR_CORE_WINDOW = 16,
    PRESENT1 = 22,
    CREATE_SWAP_CHAIN_FOR_COMPOSITION = 24,
};

static const GUID IID_IDXGIFactory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID IID_IDXGISwapChain1 = {0x790a45f7, 0x0d42, 0x4876, {0x98, 0x3a, 0x0a, 0x55, 0xcf, 0xe6, 0xf4, 0xaa}};

typedef HRESULT (*QueryInterface_t)(void *, const GUID *, void **);
typedef UINT (*Release_t)(void *);
typedef HRESULT (*Present_t)(void *, UINT, UINT);
typedef HRESULT (*Present1_t)(void *, UINT, UINT, const void *);
typedef HRESULT (*CreateSwapChain_t)(void *, void *, void *, void **);
typedef HRESULT (*CreateSwapChainForHwnd_t)(void *, void *, void *, const void *, const void *, void *, void **);
typedef HRESULT (*CreateSwapChainForCoreWindow_t)(void *, void *, void *, const void *, void *, void **);
typedef HRESULT (*CreateSwapChainForComposition_t)(void *, void *, const void *, void *, void **);
typedef HRESULT (*CreateDXGIFactory_t)(const GUID *, void **);
typedef HRESULT (*CreateDXGIFactory2_t)(UINT, const GUID *, void **);

static Present_t o_Present;
static Present1_t o_Present1;
static CreateSwapChain_t o_CreateSwapChain;
static CreateSwapChainForHwnd_t o_CreateSwapChainForHwnd;
static CreateSwapChainForCoreWindow_t o_CreateSwapChainForCoreWindow;
static CreateSwapChainForComposition_t o_CreateSwapChainForComposition;

static volatile DWORD g_renderThread;

static void **vtable(void *object) { return *(void ***)object; }

static void *query(void *object, const GUID *iid) {
    void *result = 0;
    return ((QueryInterface_t)vtable(object)[QUERY_INTERFACE])(object, iid, &result) >= 0 ? result : 0;
}

static void release(void *object) { ((Release_t)vtable(object)[RELEASE])(object); }

static void patch(void *object, int index, void *hook, void **original) {
    void **slot = &vtable(object)[index];
    DWORD protect;
    if (*slot == hook || !VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &protect)) return;
    if (!*original) *original = *slot;
    __atomic_store_n(slot, hook, __ATOMIC_SEQ_CST);
    VirtualProtect(slot, sizeof *slot, protect, &protect);
}

static void on_present(void) {
    DWORD thread = GetCurrentThreadId();
    if (thread == g_renderThread) return;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    SetThreadPriorityBoost(GetCurrentThread(), FALSE);
    g_renderThread = thread;
}

static HRESULT Hook_Present(void *swapChain, UINT sync, UINT flags) {
    on_present();
    return o_Present(swapChain, sync, flags);
}

static HRESULT Hook_Present1(void *swapChain, UINT sync, UINT flags, const void *params) {
    on_present();
    return o_Present1(swapChain, sync, flags, params);
}

static HRESULT hook_swap_chain(HRESULT hr, void **swapChain) {
    if (hr < 0 || !swapChain || !*swapChain) return hr;
    patch(*swapChain, PRESENT, Hook_Present, (void **)&o_Present);
    void *swapChain1 = query(*swapChain, &IID_IDXGISwapChain1);
    if (swapChain1) {
        patch(swapChain1, PRESENT1, Hook_Present1, (void **)&o_Present1);
        release(swapChain1);
    }
    return hr;
}

static HRESULT Hook_CreateSwapChain(void *factory, void *device, void *desc, void **swapChain) {
    return hook_swap_chain(o_CreateSwapChain(factory, device, desc, swapChain), swapChain);
}

static HRESULT Hook_CreateSwapChainForHwnd(void *factory, void *device, void *hwnd, const void *desc,
                                           const void *fullscreen, void *output, void **swapChain) {
    return hook_swap_chain(o_CreateSwapChainForHwnd(factory, device, hwnd, desc, fullscreen, output, swapChain), swapChain);
}

static HRESULT Hook_CreateSwapChainForCoreWindow(void *factory, void *device, void *window, const void *desc,
                                                 void *output, void **swapChain) {
    return hook_swap_chain(o_CreateSwapChainForCoreWindow(factory, device, window, desc, output, swapChain), swapChain);
}

static HRESULT Hook_CreateSwapChainForComposition(void *factory, void *device, const void *desc, void *output,
                                                  void **swapChain) {
    return hook_swap_chain(o_CreateSwapChainForComposition(factory, device, desc, output, swapChain), swapChain);
}

static HRESULT hook_factory(HRESULT hr, void **factory) {
    if (hr < 0 || !factory || !*factory) return hr;
    patch(*factory, CREATE_SWAP_CHAIN, Hook_CreateSwapChain, (void **)&o_CreateSwapChain);
    void *factory2 = query(*factory, &IID_IDXGIFactory2);
    if (factory2) {
        patch(factory2, CREATE_SWAP_CHAIN_FOR_HWND, Hook_CreateSwapChainForHwnd, (void **)&o_CreateSwapChainForHwnd);
        patch(factory2, CREATE_SWAP_CHAIN_FOR_CORE_WINDOW, Hook_CreateSwapChainForCoreWindow, (void **)&o_CreateSwapChainForCoreWindow);
        patch(factory2, CREATE_SWAP_CHAIN_FOR_COMPOSITION, Hook_CreateSwapChainForComposition, (void **)&o_CreateSwapChainForComposition);
        release(factory2);
    }
    return hr;
}

HRESULT My_CreateDXGIFactory(const GUID *iid, void **factory) {
    return hook_factory(((CreateDXGIFactory_t)p_CreateDXGIFactory)(iid, factory), factory);
}

HRESULT My_CreateDXGIFactory1(const GUID *iid, void **factory) {
    return hook_factory(((CreateDXGIFactory_t)p_CreateDXGIFactory1)(iid, factory), factory);
}

HRESULT My_CreateDXGIFactory2(UINT flags, const GUID *iid, void **factory) {
    return hook_factory(((CreateDXGIFactory2_t)p_CreateDXGIFactory2)(flags, iid, factory), factory);
}

static void update_priority_boost(void) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    DWORD process = GetCurrentProcessId();
    THREADENTRY32 entry;
    entry.dwSize = sizeof entry;
    for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != process) continue;
        HANDLE thread = OpenThread(THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
        if (!thread) continue;
        BOOL disable = entry.th32ThreadID != g_renderThread, disabled;
        if (!GetThreadPriorityBoost(thread, &disabled) || !disabled != !disable) SetThreadPriorityBoost(thread, disable);
        CloseHandle(thread);
    }
    CloseHandle(snapshot);
}

static DWORD worker(void *unused) {
    /* Process wide fixes apply even if the Present chain never forms, e.g. when
       loaded as an OptiScaler plugin and another mod owns the swap chain. */
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetProcessPriorityBoost(GetCurrentProcess(), TRUE);
    while (!g_renderThread) Sleep(500);
    for (;;) {
        update_priority_boost();
        Sleep(3000);
    }
}

static BOOL path_in_directory(const WCHAR *file, const WCHAR *directory, UINT length) {
    for (UINT i = 0; i < length; i++) {
        WCHAR a = file[i], b = directory[i];
        if (a >= 'a' && a <= 'z') a -= 'a' - 'A';
        if (b >= 'a' && b <= 'z') b -= 'a' - 'A';
        if (a != b) return FALSE;
    }
    return file[length] == '\\';
}

static BOOL load_real_dxgi(void) {
    WCHAR path[MAX_PATH + 16], actual[MAX_PATH];
    UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (!length || length >= MAX_PATH) return FALSE;
    for (const WCHAR *name = L"\\dxgi.dll"; (path[length] = *name); length++, name++);
    HMODULE dxgi = LoadLibraryW(path);
    if (!dxgi) return FALSE;
    if (!GetModuleFileNameW(dxgi, actual, MAX_PATH) || !path_in_directory(actual, path, length - 9)) return FALSE;
    FORWARDED(RESOLVE)
    HOOKED(REQUIRE)
    return TRUE;
}

BOOL DllMain(HMODULE module, DWORD reason, void *reserved) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(module);
    if (!load_real_dxgi()) return FALSE;
    HANDLE thread = CreateThread(0, 0, worker, 0, 0, 0);
    if (thread) CloseHandle(thread);
    return TRUE;
}
