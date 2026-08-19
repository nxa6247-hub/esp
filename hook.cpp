#pragma once
#include "hook.hpp"
#include "../../utils/debug/log.hpp"



namespace hooks {
	void Init() {

		utils::debug::log("[init] starting hooks::Init\n");

		hk::init();
		scimitar::Init();

		const auto kiero_status = kiero::init(kiero::RenderType::D3D12);
		utils::debug::log("[init] kiero::init status = %d\n", static_cast<int>(kiero_status));

		if (kiero_status == kiero::Status::Success) {
			kiero::bind(54, (void**)&d3d12hook::oExecuteCommandListsD3D12, d3d12hook::hookExecuteCommandListsD3D12);
			kiero::bind(58, (void**)&d3d12hook::oSignalD3D12, d3d12hook::hookSignalD3D12);
			kiero::bind(140, (void**)&d3d12hook::oPresentD3D12, d3d12hook::hookPresentD3D12);
			kiero::bind(84, (void**)&d3d12hook::oDrawInstancedD3D12, d3d12hook::hookkDrawInstancedD3D12);
			kiero::bind(85, (void**)&d3d12hook::oDrawIndexedInstancedD3D12, d3d12hook::hookDrawIndexedInstancedD3D12);

			utils::debug::log("[init] d3d12 hooks bound, waiting for numpad1 to unload\n");

			do {

			
				

			} while (!(GetAsyncKeyState(VK_NUMPAD1) & 0x1));


			d3d12hook::release();


			kiero::shutdown();

			inputhook::Remove(d3d12::mainWindow);

			Beep(220, 100);

			FreeLibraryAndExitThread(d3d12::mainModule, 0);
		}
		else
		{
			utils::debug::log("[init] kiero failed, entity hook may still be active\n");
		}
	}
}
