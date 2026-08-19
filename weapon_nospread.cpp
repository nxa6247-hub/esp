#include "../rainbow6.hpp"
#include "../../utils/debug/log.hpp"

namespace rainbow6
{
	auto weapon_nospread() -> void
	{
		if (!weapon::bNoSpread || !utils::memory::image_base)
			return;

		const auto spread_addr = utils::memory::image_base + weapon::k_spread_rva;
		utils::memory::write_float(spread_addr, 0.f);
	}
}
