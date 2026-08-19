#pragma once
#include <atomic>
#include "../../hooks/hooks.hpp"
#include "../../scimitar.hpp"
#include "../../../utils/debug/log.hpp"

namespace
{
	bool try_cache_actor(std::uintptr_t actor_ptr)
	{
		const auto fb = scimitar::Entity::read_filter_byte_at(actor_ptr);
		if (!fb || !scimitar::Entity::is_player_filter_byte(fb))
		{
			std::lock_guard<std::mutex> lock(entity_mutex);
			cached_entities.erase(actor_ptr);
			return false;
		}

		std::uint16_t comp_index = 0xFFFF;
		{
			std::lock_guard<std::mutex> lock(entity_mutex);
			const auto it = cached_entities.find(actor_ptr);
			if (it != cached_entities.end())
				comp_index = it->second.comp_index;
		}

		const auto position = scimitar::Entity::read_player_position_cached(actor_ptr, comp_index);
		const auto now = std::chrono::steady_clock::now();

		static std::atomic<int> cache_log_count{ 0 };
		const int log_idx = ++cache_log_count;

		if (log_idx <= 10)
		{
			utils::debug::log(
				"[entity] cached player #%d ptr=0x%llX fb=0x%llX b3=0x%02X b4=0x%02X dead=%d render=%d\n",
				log_idx,
				static_cast<unsigned long long>(actor_ptr),
				static_cast<unsigned long long>(fb),
				scimitar::Entity::filter_byte3_from(fb),
				scimitar::Entity::filter_byte4_from(fb),
				scimitar::Entity::is_dead_filter_byte(fb) ? 1 : 0,
				scimitar::Entity::should_render_filter_byte(fb) ? 1 : 0
			);
		}

		std::lock_guard<std::mutex> lock(entity_mutex);
		auto& entry = cached_entities[actor_ptr];
		entry.filter_byte = fb;
		entry.comp_index = comp_index;
		apply_entity_position(entry, position, now);
		entry.last_seen = now;
		return true;
	}
}

auto hk::entity(__int64 rcx) -> __int64
{
	if (!original_entity_hook)
		return 0;

	using func_t = __int64(__fastcall*)(__int64);

	const auto result = utils::spoof_call(reinterpret_cast<func_t>(original_entity_hook), rcx);

	touch_entity_hook_activity();

	if (utils::memory::valid_pointer(rcx))
		try_cache_actor(static_cast<std::uintptr_t>(rcx));

	return result;
}
