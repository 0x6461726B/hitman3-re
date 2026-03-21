#include "hooks.h"
#include "Process.h"
#include "comdef.h"
#include "drawing.h"
#include "utils.h"
#include <format>
#include <iostream>
#include <unordered_map>

#include "Menu.h"
#include <codecvt>
#include <locale>
#include <string>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


// Global objects
Menu menu;
Process myProcess;
Drawings drawer;

static HookManager g_hookManager;

// --- Function pointers ---
DX12Hook::present DX12Hook::p_present = nullptr;
DX12Hook::present DX12Hook::p_present_target = nullptr;
DX12Hook::Signal  DX12Hook::p_signalD3D12 = nullptr;
DX12Hook::Signal  DX12Hook::p_signalD3D12_target = nullptr;

void (*oExecuteCommandListsD3D12)(ID3D12CommandQueue*, UINT, ID3D12CommandList*) = nullptr;
void (*oExecuteCommandListsD3D12_target)(ID3D12CommandQueue*, UINT, ID3D12CommandList*) = nullptr;

// --- Device / state ---
ID3D12Device* DX12Hook::p_device = nullptr;
bool             DX12Hook::hookInited = false;
bool             DX12Hook::unHooking = false;
HANDLE           DX12Hook::g_unhookEvent = nullptr;
UINT             DX12Hook::bufferCount = 0;

// --- Frame context ---
FrameContext* DX12Hook::g_frameContext = nullptr;
UINT             DX12Hook::g_frameIndex = 0;

// --- DX12 resources ---
ID3D12DescriptorHeap* DX12Hook::g_rtvHeap = nullptr;
ID3D12DescriptorHeap* DX12Hook::g_srvHeap = nullptr;
ID3D12CommandQueue* DX12Hook::g_commandQueue = nullptr;
ID3D12CommandAllocator* DX12Hook::g_commandAllocator = nullptr;
ID3D12GraphicsCommandList* DX12Hook::g_commandList = nullptr;
ID3D12Fence* DX12Hook::g_fence = nullptr;
HANDLE                      DX12Hook::g_fenceEvent = nullptr;
UINT64                      DX12Hook::g_fenceLastSignaledValue = 0;

// --- Swap chain ---
IDXGISwapChain3* DX12Hook::g_swapChain = nullptr;
bool             DX12Hook::swapChainInited = false;
HANDLE           DX12Hook::g_hSwapChainWaitableObject = nullptr;
ID3D12Resource* DX12Hook::g_mainRenderTargetResource[NUM_BACK_BUFFERS] = { nullptr };
D3D12_CPU_DESCRIPTOR_HANDLE DX12Hook::g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = { 0 };

// --- Dummy window ---
WNDCLASSEX DX12Hook::wc = {};
HWND       DX12Hook::hwnd = nullptr;


bool DX12Hook::getPresentPointer() {
    bool isWindowed = true;
    auto window = FindWindowA(NULL, myProcess.windowTitle.c_str());
    myProcess.targetHwnd = window;

    if (initWindow() != true) {
        Logger::instance().error("Failed to initialize dummy window");
        return false;
    }
    Logger::instance().info("Initialized dummy window");

    ID3D12Device *device;
    IDXGIFactory1 *factory;
    if (CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&factory) != S_OK) {
        Logger::instance().error("Failed to create DXGI factory");
        deleteWindow();
        return false;
    }
    Logger::instance().info("Created DXGI factory");

    IDXGIAdapter *adapter;
    if (factory->EnumAdapters(0, &adapter) != S_OK) {
        Logger::instance().error("Failed to enumerate adapters");
        deleteWindow();
        return false;
    }
    Logger::instance().info("Enumerated adapters");

    HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void **)&device);

    if (FAILED(hr)) {
        _com_error err(hr);
        Logger::instance().error("Failed to create device: {}", err.ErrorMessage());
        deleteWindow();
        return false;
    }
    Logger::instance().info("Created device");

    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    QueueDesc.Priority = 0;
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    QueueDesc.NodeMask = 0;

    ID3D12CommandQueue *CommandQueue;
    hr = device->CreateCommandQueue(&QueueDesc, __uuidof(ID3D12CommandQueue), (void **)&CommandQueue);
    if (FAILED(hr)) {
        _com_error err(hr);
        Logger::instance().error("Failed to create command queue: {}", err.ErrorMessage());
        device->Release();
        factory->Release();
        deleteWindow();
        return false;
    }
    Logger::instance().info("Created commandQueue");

    DXGI_RATIONAL RefreshRate = {60, 1};
    DXGI_MODE_DESC BufferDesc = {};
    BufferDesc.Width = 100;
    BufferDesc.Height = 100;
    BufferDesc.RefreshRate = RefreshRate;
    BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    DXGI_SAMPLE_DESC SampleDesc = {};
    SampleDesc.Count = 1;
    SampleDesc.Quality = 0;

    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    SwapChainDesc.BufferDesc = BufferDesc;
    SwapChainDesc.SampleDesc = SampleDesc;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.OutputWindow = hwnd;
    SwapChainDesc.Windowed = 1;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    IDXGISwapChain *swapChain;
    hr = factory->CreateSwapChain(CommandQueue, &SwapChainDesc, &swapChain);
    if (FAILED(hr)) {
        _com_error err(hr);
        Logger::instance().error("Failed to create swap chain: {}", err.ErrorMessage());
        CommandQueue->Release();
        device->Release();
        factory->Release();
        deleteWindow();
        return false;
    }
    Logger::instance().info("Created swap chain");

    // Get function pointers from the vtables
    void **p_vtable = *reinterpret_cast<void ***>(swapChain);
    void **q_vtable = *reinterpret_cast<void ***>(CommandQueue);
    swapChain->Release();
    device->Release();
    factory->Release();
    CommandQueue->Release();
    deleteWindow();

    p_present_target = reinterpret_cast<present>(p_vtable[8]);
    p_signalD3D12_target = reinterpret_cast<Signal>(q_vtable[14]);
    oExecuteCommandListsD3D12_target =
        reinterpret_cast<void (*)(ID3D12CommandQueue *, UINT, ID3D12CommandList *)>(q_vtable[10]);

    Logger::instance().info("Present address: 0x%llx", p_present_target);
    Logger::instance().info("Signal address: 0x%llx", p_signalD3D12_target);
    Logger::instance().info("ExecuteCommandLists address: 0x%llx", oExecuteCommandListsD3D12_target);

    return true;
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Forward input to ImGui then call the original window proc.
    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
    return CallWindowProc(myProcess.targetWndProc, hWnd, uMsg, wParam, lParam);
};


//
// Hooking functions
//
bool DX12Hook::hook() {
    // Create event used to coordinate unhook with the present hook
    g_unhookEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_unhookEvent) {
        Logger::instance().error("[DX12] Failed to create unhook event");
        return false;
    }
    if (!getPresentPointer()) {
        Logger::instance().error("[DX12] Failed to get present pointer");
        return false;
    }

    if (!g_hookManager.create(p_signalD3D12_target, &signalD3D12Hook, reinterpret_cast<void **>(&p_signalD3D12)))
        return false;

    if (!g_hookManager.create(p_present_target, &detourPresent, reinterpret_cast<void **>(&p_present)))
        return false;

    /*if (!g_hookManager.create(reinterpret_cast<void *>(MAKE_RVA(0x72A8A0)), &Hooked_UpdateTraversalHUD, reinterpret_cast<void **>(&oUpdateTraversalHUD)))
        return false;*/
    

    //if (!g_hookManager.create(reinterpret_cast<void *>(MAKE_RVA(0x214980)), &hookedWorldToScreen,
    //                          reinterpret_cast<void **>(&oWorldToScreen)))
    //    return false;

    if (!g_hookManager.enableAll()) {
        Logger::instance().error("[MH] Couldn't enable hooks");
        return false;
    }

    return true;
}

void DX12Hook::unHook() {

    unHooking = true;

    // Wait for the present hook to finish processing before releasing resources
    if (g_unhookEvent) {
        WaitForSingleObject(g_unhookEvent, 1000);
    }

    g_hookManager.disableAll();
    g_hookManager.remove(p_present_target);
    g_hookManager.remove(p_signalD3D12_target);
    g_hookManager.uninitialize();

    if (g_commandQueue && g_fenceLastSignaledValue > 0) {
        if (g_fence->GetCompletedValue() < g_fenceLastSignaledValue) {
            g_fence->SetEventOnCompletion(g_fenceLastSignaledValue, g_fenceEvent);
            WaitForSingleObject(g_fenceEvent, 5000);
        }
    }

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        Logger::instance().info("[IG] Destroyed ImGui.");
    }

    SetWindowLongPtr(myProcess.targetHwnd, GWLP_WNDPROC, (LONG_PTR)myProcess.targetWndProc);

    if (g_commandList) {
        g_commandList->Release();
        g_commandList = nullptr;
    }

    for (UINT i = 0; i < bufferCount; i++) {
        if (g_frameContext[i].CommandAllocator) {
            g_frameContext[i].CommandAllocator->Release();
            g_frameContext[i].CommandAllocator = nullptr;
        }
        if (g_frameContext[i].Resource) {
            g_frameContext[i].Resource->Release();
            g_frameContext[i].Resource = nullptr;
        }
    }

    if (g_srvHeap) {
        g_srvHeap->Release();
        g_srvHeap = nullptr;
    }

    if (g_rtvHeap) {
        g_rtvHeap->Release();
        g_rtvHeap = nullptr;
    }

    if (g_commandQueue) {
        g_commandQueue->Release();
        g_commandQueue = nullptr;
    }

    if (p_device) {
        p_device->Release();
        p_device = nullptr;
    }

    if (g_frameContext) {
        delete[] g_frameContext;
        g_frameContext = nullptr;
    }

    if (g_fence) {
        g_fence = nullptr; // cant release since its not created by us
    }

    if (g_unhookEvent) {
        CloseHandle(g_unhookEvent);
        g_unhookEvent = nullptr;
    }

    FreeConsole();
    FreeLibraryAndExitThread(myProcess.hModule, 0);
}


//
// Simple free list based allocator
//
struct ExampleDescriptorHeapAllocator {
    ID3D12DescriptorHeap *Heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE HeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT HeapHandleIncrement;
    ImVector<int> FreeIndices;

    void Create(ID3D12Device *device, ID3D12DescriptorHeap *heap) {
        IM_ASSERT(Heap == nullptr && FreeIndices.empty());
        Heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        HeapType = desc.Type;
        HeapStartCpu = Heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = Heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = device->GetDescriptorHandleIncrementSize(HeapType);
        FreeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            FreeIndices.push_back(n);
    }
    void Destroy() {
        Heap = nullptr;
        FreeIndices.clear();
    }
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_desc_handle) {
        IM_ASSERT(FreeIndices.Size > 0);
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle) {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        IM_ASSERT(cpu_idx == gpu_idx);
        FreeIndices.push_back(cpu_idx);
    }
};

static ExampleDescriptorHeapAllocator g_pd3dSrvDescHeapAlloc;


static int shutdownFrameDelay = 5;
HRESULT DX12Hook::detourPresent(IDXGISwapChain3 *p_swap_chain, UINT sync_interval, UINT flags) {

    if (unHooking) {
        if (shutdownFrameDelay > 0) {
            shutdownFrameDelay--;
            return p_present(p_swap_chain, sync_interval, flags);
        } else {
            if (g_unhookEvent) {
                SetEvent(g_unhookEvent);
            }
            return p_present(p_swap_chain, sync_interval, flags);
        }
    }
    if (!hookInited) {
        HRESULT hr = p_swap_chain->GetDevice(__uuidof(ID3D12Device), (void **)&p_device);
        if (SUCCEEDED(hr)) {

            Logger::instance().info("[DX12] Hooked present, initializing...");

            g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (!g_fenceEvent) {
                Logger::instance().error("[DX12] Failed to create fence event.");
                return p_present(p_swap_chain, sync_interval, flags);
            }

            auto realSwapChain = *(uintptr_t *)((uintptr_t)p_swap_chain + 0x10);
            g_commandQueue = *(ID3D12CommandQueue **)(realSwapChain + 0x168); // theres 2 different command queues, so instead of getting it from Signal, we just manually get the one we need

            DXGI_SWAP_CHAIN_DESC Desc = {};
            p_swap_chain->GetDesc(&Desc);
            myProcess.targetWndProc = (WNDPROC)SetWindowLongPtr(myProcess.targetHwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
            bufferCount = Desc.BufferCount;

            Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            Desc.OutputWindow = myProcess.targetHwnd;
            Desc.Windowed = ((GetWindowLongPtr(myProcess.targetHwnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

            D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
            srvHeapDesc.NumDescriptors = bufferCount;
            srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = p_device->CreateDescriptorHeap(&srvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&g_srvHeap);
            if (FAILED(hr)) {
                Logger::instance().error("[DX12] Failed to create SRV descriptor heap.");
                return p_present(p_swap_chain, sync_interval, flags);
            }

            g_frameContext = new FrameContext[bufferCount];
            for (UINT i = 0; i < bufferCount; i++) {
                hr = p_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                      IID_PPV_ARGS(&g_frameContext[i].CommandAllocator));
                if (FAILED(hr)) {
                    Logger::instance().error("[DX12] Failed to create command allocator for frame %u.", i);
                }
                g_frameContext[i].FenceValue = 1; // Initialize fence value
            }

            hr = p_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator,
                                             nullptr, IID_PPV_ARGS(&g_commandList));
            if (FAILED(hr)) {
                Logger::instance().error("[DX12] Failed to create command list.");
            }
            g_commandList->Close();

            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
            rtvHeapDesc.NumDescriptors = bufferCount;
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            rtvHeapDesc.NodeMask = 1;
            hr = p_device->CreateDescriptorHeap(&rtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void **)&g_rtvHeap);
            if (FAILED(hr)) {
                Logger::instance().error("[DX12] Failed to create RTV descriptor heap.");
            }

            SIZE_T rtvDescriptorSize = p_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();

            for (UINT i = 0; i < bufferCount; i++) {
                ID3D12Resource *pBackBuffer = nullptr;
                g_frameContext[i].DescriptorHandle = rtvHandle;
                hr = p_swap_chain->GetBuffer(i, __uuidof(ID3D12Resource), (void **)&pBackBuffer);
                if (FAILED(hr)) {
                    Logger::instance().error("[DX12] Failed to get back buffer %u.", i);
                }
                p_device->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
                g_frameContext[i].Resource = pBackBuffer;
                rtvHandle.ptr += rtvDescriptorSize;
            }

            
          

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(myProcess.targetHwnd);

            ImGui_ImplDX12_InitInfo initInfo = {};
            initInfo.Device = p_device;
            initInfo.CommandQueue = g_commandQueue;
            initInfo.NumFramesInFlight = bufferCount;
            initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
            initInfo.SrvDescriptorHeap = g_srvHeap;

            g_pd3dSrvDescHeapAlloc.Create(p_device, g_srvHeap);
            initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *out_cpu_handle,
                                               D3D12_GPU_DESCRIPTOR_HANDLE *out_gpu_handle) {
                g_pd3dSrvDescHeapAlloc.Alloc(out_cpu_handle, out_gpu_handle);
            };
            initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
                                              D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle) {
                g_pd3dSrvDescHeapAlloc.Free(cpu_handle, gpu_desc_handle);
            };

            ImGui_ImplDX12_Init(&initInfo);
            menu.initStyles();
            drawer.initFonts();

            hookInited = true;
            Logger::instance().info("[DX12] Initialized successfully");
        } else {
            _com_error err(hr);
            Logger::instance().error("[DX12] Failed to get device: {}", err.ErrorMessage());
            return p_present(p_swap_chain, sync_interval, flags);
        }
    }

    if (g_commandQueue == nullptr) {
        Logger::instance().error("[DX12] CommandQueue is nullptr");
        return p_present(p_swap_chain, sync_interval, flags);
    }

    if (g_fence == nullptr) {
       // Logger::instance().error("[DX12] Fence is nullptr");
        return p_present(p_swap_chain, sync_interval, flags);
    }

    UINT currentBackBufferIndex = p_swap_chain->GetCurrentBackBufferIndex();

    menu.beginNewFrame();
    menu.Render();
    drawer.Begin(currentBackBufferIndex);
    drawer.Draw();
    drawer.End();
    menu.endFrame();
    ImGui::Render();


    FrameContext &currFrameContext = g_frameContext[currentBackBufferIndex];

    if (g_fence && g_fenceLastSignaledValue > 0 && g_fence->GetCompletedValue() < g_fenceLastSignaledValue) {
        g_fence->SetEventOnCompletion(g_fenceLastSignaledValue, g_fenceEvent);
        WaitForSingleObject(g_fenceEvent, 5000);
    }


    HRESULT hr = currFrameContext.CommandAllocator->Reset();
    if (FAILED(hr)) {
        Logger::instance().error("[DX12] Failed to reset command allocator for frame {}. HRESULT: {}",
                                 currentBackBufferIndex, hr);
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = currFrameContext.Resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // Logger::instance().info("Resetting command list using current frame's
    // command allocator.");
    hr = g_commandList->Reset(currFrameContext.CommandAllocator, nullptr);
    if (FAILED(hr)) {
        Logger::instance().error("[DX12] Failed to reset command list. HRESULT: {}", hr);
    }

    g_commandList->ResourceBarrier(1, &barrier);

    g_commandList->OMSetRenderTargets(1, &currFrameContext.DescriptorHandle, FALSE, nullptr);

    g_commandList->SetDescriptorHeaps(1, &g_srvHeap);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_commandList->ResourceBarrier(1, &barrier);

    g_commandList->Close();

    g_commandQueue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&g_commandList);

    return p_present(p_swap_chain, sync_interval, flags);
}

HRESULT DX12Hook::signalD3D12Hook(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value) {
    if (g_commandQueue != nullptr && queue == g_commandQueue) {
        if (g_fence != fence)
            g_fence = fence;
        g_fenceLastSignaledValue = value;
    }
    return p_signalD3D12(queue, fence, value);
}

//
// Window initialization
//
bool DX12Hook::initWindow() {
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"Dummy";
    wc.hIconSm = NULL;

    if (!RegisterClassEx(&wc)) {
        Logger::instance().error("Failed to register dummy window class");
        return false;
    }

    hwnd = CreateWindow(L"Dummy", L"Dummy2", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) {
        Logger::instance().error("Failed to create dummy window");
        return false;
    }
    return true;
}

bool DX12Hook::deleteWindow() {
    if (!DestroyWindow(hwnd)) {
        Logger::instance().error("Failed to destroy dummy window");
        return false;
    }
    if (!UnregisterClass(wc.lpszClassName, wc.hInstance)) {
        Logger::instance().error("Failed to unregister dummy window class");
        return false;
    }
    return true;
}

bool DX12Hook::isUnhooking() const { return unHooking; }