#define WIN32_LEAN_AND_MEAN




#include "scimitar/scimitar.hpp"
#include "scimitar/hooks/hooks.hpp"
#include "utils/debug/log.hpp"

#include "directx/Hook/hook.hpp"



auto on_attach() -> unsigned long __stdcall
{
	SetupConsole();
	utils::debug::log("[init] console ready\n");
	utils::debug::log("[init] waiting for game modules to settle...\n");
	Sleep(5000);

	utils::debug::log_hex("[init] RainbowSix.exe", utils::memory::image_base);
	utils::debug::log_hex("[init] .text size", utils::memory::text_size);
	utils::debug::log_hex("[init] trampoline_jmp", reinterpret_cast<std::uintptr_t>(utils::trampoline_jmp));

	if (!utils::memory::image_base)
	{
		utils::debug::log("[init] abort: RainbowSix.exe module not found\n");
		return 0;
	}

	Beep(750, 300);

	hooks::Init();

	return 0;
}



auto __stdcall DllMain(HMODULE dll_instance, std::int32_t reason, [[maybe_unused]] void* r8) -> bool
{
	if (reason == 1) {


		utils::memory::image_base = utils::importer::get_safe_module(_x(L"RainbowSix.exe"));
		utils::memory::text_size = utils::importer::get_section_size(utils::memory::image_base, _x(".text"));
		utils::memory::text_virtualaddress = static_cast<std::uint32_t>(
			utils::importer::get_virtual_address(utils::memory::image_base, _x(".text")));
		utils::memory::rdata_size = utils::importer::get_section_size(utils::memory::image_base, _x(".rdata"));
		utils::memory::rdata_virtualaddress = static_cast<std::uint32_t>(
			utils::importer::get_virtual_address(utils::memory::image_base, _x(".rdata")));

		utils::trampoline_jmp = (void*)utils::memory::scan_pattern(
			utils::memory::text_start(),
			utils::memory::text_size,
			_x("FF 27"));

		d3d12::mainModule = dll_instance;

		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)on_attach, 0, 0, 0);
	}
	else if (reason == 2) {




	}
	return true;
}