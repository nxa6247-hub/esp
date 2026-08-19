#include "d3d12hook.hpp"
#include "../utils/debug/log.hpp"


namespace d3d12hook {

    HANDLE hEvent = nullptr;

    ID3D12Device* d3d12Device = nullptr;
    ID3D12DescriptorHeap* d3d12DescriptorHeapBackBuffers = nullptr;
    ID3D12DescriptorHeap* d3d12DescriptorHeapImGuiRender = nullptr;
    ID3D12GraphicsCommandList* d3d12CommandList = nullptr;
    ID3D12Fence* d3d12Fence = nullptr;
    UINT64 d3d12FenceValue = 0;
    ID3D12CommandQueue* d3d12CommandQueue = nullptr;

    PresentD3D12 oPresentD3D12;
    DrawInstancedD3D12 oDrawInstancedD3D12;
    DrawIndexedInstancedD3D12 oDrawIndexedInstancedD3D12;

    void(*oExecuteCommandListsD3D12)(ID3D12CommandQueue*, UINT, ID3D12CommandList*);
    HRESULT(*oSignalD3D12)(ID3D12CommandQueue*, ID3D12Fence*, UINT64);

    struct FrameContext {
        ID3D12CommandAllocator* commandAllocator = nullptr;
        ID3D12Resource* main_render_target_resource = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE main_render_target_descriptor;
    };

    uintx_t buffersCounts = -1;
    FrameContext* frameContext;

    bool shutdown = false;
    bool imgui_ready = false;

    long __fastcall hookPresentD3D12(IDXGISwapChain3* pSwapChain, UINT SyncInterval, UINT Flags) {
        static bool init = false;
		static bool show_menu = true;

        if (GetAsyncKeyState(VK_INSERT) & 0x1) {
			show_menu = !show_menu;
        }

        if (!init) {
            if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device))) {
                ImGui::CreateContext();

                unsigned char* pixels;
                int width, height;
                ImGuiIO& io = ImGui::GetIO(); (void)io;
                ImGui::StyleColorsDark();
                ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 16);
                io.Fonts->AddFontDefault();
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
                io.IniFilename = NULL;

                hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                if (!hEvent)
                {
                    utils::debug::log("[d3d12] CreateEvent failed\n");
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }

                DXGI_SWAP_CHAIN_DESC sdesc;
                pSwapChain->GetDesc(&sdesc);
                d3d12::mainWindow = sdesc.OutputWindow;
                sdesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
                sdesc.Windowed = TRUE;
                buffersCounts = sdesc.BufferCount;
                frameContext = new FrameContext[buffersCounts];

                D3D12_DESCRIPTOR_HEAP_DESC descriptorImGuiRender = {};
                descriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                descriptorImGuiRender.NumDescriptors = buffersCounts;
                descriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

                if (d3d12Device->CreateDescriptorHeap(&descriptorImGuiRender, IID_PPV_ARGS(&d3d12DescriptorHeapImGuiRender)) != S_OK)
                {
                    utils::debug::log("[d3d12] CreateDescriptorHeap (imgui) failed\n");
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }

                for (size_t i = 0; i < buffersCounts; i++) {
                    if (d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frameContext[i].commandAllocator)) != S_OK)
                    {
                        utils::debug::log("[d3d12] CreateCommandAllocator failed for frame %zu\n", i);
                        return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                    }
                }

                if (d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frameContext[0].commandAllocator, NULL, IID_PPV_ARGS(&d3d12CommandList)) != S_OK ||
                    d3d12CommandList->Close() != S_OK)
                {
                    utils::debug::log("[d3d12] CreateCommandList failed\n");
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }

                D3D12_DESCRIPTOR_HEAP_DESC descriptorBackBuffers;
                descriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                descriptorBackBuffers.NumDescriptors = buffersCounts;
                descriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                descriptorBackBuffers.NodeMask = 1;

                if (d3d12Device->CreateDescriptorHeap(&descriptorBackBuffers, IID_PPV_ARGS(&d3d12DescriptorHeapBackBuffers)) != S_OK)
                {
                    utils::debug::log("[d3d12] CreateDescriptorHeap (rtv) failed\n");
                    return oPresentD3D12(pSwapChain, SyncInterval, Flags);
                }

                const auto rtvDescriptorSize = d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3d12DescriptorHeapBackBuffers->GetCPUDescriptorHandleForHeapStart();

                for (size_t i = 0; i < buffersCounts; i++) {
                    ID3D12Resource* pBackBuffer = nullptr;

                    frameContext[i].main_render_target_descriptor = rtvHandle;
                    pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
                    d3d12Device->CreateRenderTargetView(pBackBuffer, nullptr, rtvHandle);
                    frameContext[i].main_render_target_resource = pBackBuffer;
                    rtvHandle.ptr += rtvDescriptorSize;
                }

                ImGui_ImplWin32_Init(d3d12::mainWindow);
                ImGui_ImplDX12_Init(d3d12Device, buffersCounts,
                    DXGI_FORMAT_R8G8B8A8_UNORM, d3d12DescriptorHeapImGuiRender,
                    d3d12DescriptorHeapImGuiRender->GetCPUDescriptorHandleForHeapStart(),
                    d3d12DescriptorHeapImGuiRender->GetGPUDescriptorHandleForHeapStart());

                ImGui_ImplDX12_CreateDeviceObjects();
                inputhook::Init(d3d12::mainWindow);

                imgui_ready = true;
                utils::debug::log("[d3d12] imgui init ok, buffers=%llu\n", static_cast<unsigned long long>(buffersCounts));
            }
            else
            {
                utils::debug::log("[d3d12] GetDevice failed on present\n");
            }
            init = true;
        }

        if (!shutdown && imgui_ready && d3d12CommandQueue != nullptr && d3d12CommandList != nullptr && d3d12Fence != nullptr && hEvent != nullptr) {
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            if (show_menu) {
                Render::user_interface();
            }

			rainbow6::Run();

            FrameContext& currentFrameContext = frameContext[pSwapChain->GetCurrentBackBufferIndex()];
            currentFrameContext.commandAllocator->Reset();

            D3D12_RESOURCE_BARRIER barrier;
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = currentFrameContext.main_render_target_resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            d3d12CommandList->Reset(currentFrameContext.commandAllocator, nullptr);
            d3d12CommandList->ResourceBarrier(1, &barrier);
            d3d12CommandList->OMSetRenderTargets(1, &currentFrameContext.main_render_target_descriptor, FALSE, nullptr);
            d3d12CommandList->SetDescriptorHeaps(1, &d3d12DescriptorHeapImGuiRender);

            ImGui::Render();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12CommandList);

            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

            d3d12CommandList->ResourceBarrier(1, &barrier);
            d3d12CommandList->Close();

            d3d12CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&d3d12CommandList));

            if (d3d12Fence->GetCompletedValue() < d3d12FenceValue) {
                d3d12Fence->SetEventOnCompletion(d3d12FenceValue, hEvent);
                WaitForSingleObject(hEvent, 0);
            }

            d3d12FenceValue++;
        }

        return oPresentD3D12(pSwapChain, SyncInterval, Flags);
    }

    void __fastcall hookkDrawInstancedD3D12(ID3D12GraphicsCommandList* dCommandList, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) {
        return oDrawInstancedD3D12(dCommandList, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
    }

    void __fastcall hookDrawIndexedInstancedD3D12(ID3D12GraphicsCommandList* dCommandList, UINT IndexCount, UINT InstanceCount, UINT StartIndex, INT BaseVertex) {
        return oDrawIndexedInstancedD3D12(dCommandList, IndexCount, InstanceCount, StartIndex, BaseVertex);
    }

    void hookExecuteCommandListsD3D12(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists) {
        if (!d3d12CommandQueue)
        {
            d3d12CommandQueue = queue;
            utils::debug::log("[d3d12] captured command queue %p\n", queue);
        }

        oExecuteCommandListsD3D12(queue, NumCommandLists, ppCommandLists);
    }

    HRESULT hookSignalD3D12(ID3D12CommandQueue* queue, ID3D12Fence* fence, UINT64 value) {
        if (d3d12CommandQueue != nullptr && queue == d3d12CommandQueue) {
            d3d12Fence = fence;
            d3d12FenceValue = value;
        }

        return oSignalD3D12(queue, fence, value);
    }

    void release() {
        shutdown = true;
        imgui_ready = false;
        if (d3d12Device) d3d12Device->Release();
        if (d3d12DescriptorHeapBackBuffers) d3d12DescriptorHeapBackBuffers->Release();
        if (d3d12DescriptorHeapImGuiRender) d3d12DescriptorHeapImGuiRender->Release();
        if (d3d12CommandList) d3d12CommandList->Release();
        if (d3d12Fence) d3d12Fence->Release();
        if (d3d12CommandQueue) d3d12CommandQueue->Release();
        if (hEvent) CloseHandle(hEvent);
    }
}