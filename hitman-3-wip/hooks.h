#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>


#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx12.h"
#include "ImGui/imgui_impl_win32.h"

#include "Minhook/MinHook.h"
#include "HookManager.h"

#include "Logger.h"
#include "Process.h"
#include "singleton.h"

extern Process myProcess;

struct FrameContext {
    ID3D12CommandAllocator *CommandAllocator;
    UINT64 FenceValue;
    ID3D12Resource *Resource;
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle;
};

class DX12Hook {
  private:
    typedef HRESULT(__stdcall *present)(IDXGISwapChain3 *, UINT, UINT);
    typedef HRESULT(APIENTRY *Signal)(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value);


    static present p_present;
    static present p_present_target;
    static Signal p_signalD3D12;
    static Signal p_signalD3D12_target;
    static ID3D12Device *p_device;

    static bool hookInited;
    static bool unHooking;

    static FrameContext *g_frameContext;
    static UINT g_frameIndex;

    static int const NUM_BACK_BUFFERS = 3;
    static ID3D12DescriptorHeap *g_rtvHeap;
    static ID3D12DescriptorHeap *g_srvHeap;
    static ID3D12CommandQueue *g_commandQueue;
    static ID3D12CommandAllocator *g_commandAllocator;
    static ID3D12GraphicsCommandList *g_commandList;
    static ID3D12Fence *g_fence;
    static HANDLE g_fenceEvent;
    static UINT64 g_fenceLastSignaledValue;
    static IDXGISwapChain3 *g_swapChain;
    static bool swapChainInited;
    static HANDLE g_hSwapChainWaitableObject;
    static ID3D12Resource *g_mainRenderTargetResource[NUM_BACK_BUFFERS];
    static D3D12_CPU_DESCRIPTOR_HANDLE g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS];

    // Event used to synchronize unhooking with the Present hook
    static HANDLE g_unhookEvent;

    static UINT bufferCount;

    // dummy window
    static WNDCLASSEX wc;
    static HWND hwnd;

    bool initWindow();
    bool deleteWindow();

  public:
    static DX12Hook &getInstance() { return Singleton<DX12Hook>::getInstance(); }

    bool getPresentPointer();
    static HRESULT detourPresent(IDXGISwapChain3 *p_swap_chain, UINT sync_interval, UINT flags);
    static HRESULT signalD3D12Hook(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value);

    bool hook();
    void unHook();

    bool isUnhooking() const;


};
