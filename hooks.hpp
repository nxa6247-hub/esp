#pragma once
#include <vector>
#include <atomic>

#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cmath>

#include "../../utils/utils.h"
#include "../../utils/pattern/pattern.hpp"
#include "../../utils/debug/log.hpp"
#include "../../scimitar/scimitar.hpp"


inline std::vector < std::pair<void*, void*>> orig_funcs;
inline std::unordered_map<void*, void*> trampolines;
inline void* original_entity_hook = nullptr;

struct cached_entity_entry
{
	ubiVector3 position{};
	ubiVector3 velocity{};
	ubiVector3 smoothed_position{};
	std::uint64_t filter_byte = 0;
	std::chrono::steady_clock::time_point last_seen{};
	std::chrono::steady_clock::time_point position_time{};
	std::uint16_t comp_index = 0xFFFF;
};

inline void apply_entity_position(cached_entity_entry& entry, const ubiVector3& new_pos, std::chrono::steady_clock::time_point now)
{
	if (!scimitar::Entity::is_valid_world_pos(new_pos))
		return;

	const float dt = std::chrono::duration<float>(now - entry.position_time).count();
	if (dt > 0.0001f && dt < 0.25f && scimitar::Entity::is_valid_world_pos(entry.position))
	{
		entry.velocity.x = (new_pos.x - entry.position.x) / dt;
		entry.velocity.y = (new_pos.y - entry.position.y) / dt;
		entry.velocity.z = (new_pos.z - entry.position.z) / dt;

		const float speed_sq = entry.velocity.x * entry.velocity.x
			+ entry.velocity.y * entry.velocity.y
			+ entry.velocity.z * entry.velocity.z;

		constexpr float k_max_speed = 25.f;
		if (speed_sq > k_max_speed * k_max_speed)
		{
			const float scale = k_max_speed / std::sqrt(speed_sq);
			entry.velocity.x *= scale;
			entry.velocity.y *= scale;
			entry.velocity.z *= scale;
		}
	}

	entry.position = new_pos;
	entry.position_time = now;

	if (!scimitar::Entity::is_valid_world_pos(entry.smoothed_position))
		entry.smoothed_position = new_pos;
}

inline ubiVector3 smooth_entity_draw_position(cached_entity_entry& entry, float frame_dt)
{
	if (!scimitar::Entity::is_valid_world_pos(entry.position))
		return {};

	ubiVector3 target = entry.position;
	target.x += entry.velocity.x * frame_dt;
	target.y += entry.velocity.y * frame_dt;
	target.z += entry.velocity.z * frame_dt;

	constexpr float k_smooth = 0.90f;
	entry.smoothed_position.x += (target.x - entry.smoothed_position.x) * k_smooth;
	entry.smoothed_position.y += (target.y - entry.smoothed_position.y) * k_smooth;
	entry.smoothed_position.z += (target.z - entry.smoothed_position.z) * k_smooth;

	return entry.smoothed_position;
}


namespace hk {



	__int64 entity(__int64 a1);


}


inline void hook_fn(void* destination, void* detour)
{
	if (!destination)
		return;

	orig_funcs.push_back({ destination, *reinterpret_cast<void**>(destination) });

	void* trampoline = utils::memory::swap_inline(destination, detour);

	if (trampoline) {



		trampolines[destination] = trampoline;


		if (detour == reinterpret_cast<void*>(hk::entity))
			original_entity_hook = trampoline;

	}
}


inline std::unordered_map<std::uintptr_t, cached_entity_entry> cached_entities;
inline std::mutex entity_mutex;
inline std::atomic<std::uint64_t> entity_last_hook_tick{ 0 };

inline void touch_entity_hook_activity()
{
	entity_last_hook_tick.store(GetTickCount64(), std::memory_order_relaxed);
}

inline void clear_entity_cache()
{
	std::lock_guard<std::mutex> lock(entity_mutex);
	cached_entities.clear();
	scimitar::Entity::reset_comp_cache();
}

inline void refresh_entity_positions()
{
	std::lock_guard<std::mutex> lock(entity_mutex);
	const auto now = std::chrono::steady_clock::now();

	for (auto& [actor_ptr, entry] : cached_entities)
	{
		const auto pos = scimitar::Entity::read_player_position_cached(actor_ptr, entry.comp_index);
		if (scimitar::Entity::is_valid_world_pos(pos))
			apply_entity_position(entry, pos, now);
	}
}

namespace hk
{
	inline bool looks_like_function_entry(std::uintptr_t address)
	{
		if (!utils::memory::is_in_text_section(address))
			return false;

		ZydisDecodedInstruction instruction{};
		const auto* bytes = reinterpret_cast<const std::uint8_t*>(address);
		if (!utils::pattern::decode_instruction(bytes, instruction))
			return false;

		switch (instruction.mnemonic)
		{
		case ZYDIS_MNEMONIC_PUSH:
		case ZYDIS_MNEMONIC_SUB:
		case ZYDIS_MNEMONIC_MOV:
		case ZYDIS_MNEMONIC_LEA:
		case ZYDIS_MNEMONIC_XOR:
		case ZYDIS_MNEMONIC_TEST:
			return true;
		default:
			return false;
		}
	}

	inline void init()
	{
		const auto section_base = utils::memory::text_start();
		const auto section_size = utils::memory::text_size;

		utils::debug::log_hex("[entity] image_base", utils::memory::image_base);
		utils::debug::log_hex("[entity] text_start", section_base);
		utils::debug::log_hex("[entity] text_size", section_size);
		utils::debug::log_hex("[entity] trampoline_jmp", reinterpret_cast<std::uintptr_t>(utils::trampoline_jmp));

		if (!utils::memory::image_base || !section_base || !section_size)
		{
			utils::debug::log("[entity] abort: missing module base or .text section\n");
			return;
		}

		if (!utils::trampoline_jmp)
		{
			utils::debug::log("[entity] abort: trampoline_jmp not found (FF 27 scan failed)\n");
			return;
		}

		void* entity_call = nullptr;

		const auto actor_function = utils::pattern::find_entity_actor_function(
			section_base,
			section_size,
			utils::memory::image_base
		);

		if (actor_function)
		{
			entity_call = reinterpret_cast<void*>(actor_function);
			utils::debug::log("[entity] using actor update function from SSE signature scan\n");
			utils::debug::log_hex("[entity] actor_function", actor_function);
		}
		else
		{
			utils::debug::log("[entity] SSE signature scan failed, falling back to rel32 call scan\n");

			const auto candidates = utils::pattern::collect_all_rel32_calls_before_flag_store(
				section_base,
				section_size
			);

			utils::debug::log("[entity] found %zu pattern candidate(s)\n", candidates.size());

			utils::pattern::scan_result chosen{};
			std::vector<utils::pattern::scan_result> valid_candidates;

			for (const auto& candidate : candidates)
			{
				const auto in_text = utils::memory::is_in_text_section(candidate.call_target);
				const auto executable = utils::memory::is_executable(candidate.call_target);
				const auto prologue = looks_like_function_entry(candidate.call_target);

				utils::debug::log(
					"[entity] candidate call=0x%llX target=0x%llX (rva 0x%llX) in_text=%d exec=%d prologue=%d\n",
					static_cast<unsigned long long>(candidate.call_instruction),
					static_cast<unsigned long long>(candidate.call_target),
					static_cast<unsigned long long>(candidate.call_target - utils::memory::image_base),
					in_text ? 1 : 0,
					executable ? 1 : 0,
					prologue ? 1 : 0
				);

				if (in_text && executable && prologue)
					valid_candidates.push_back(candidate);
			}

			if (!valid_candidates.empty())
			{
				chosen = valid_candidates.front();
				utils::debug::log("[entity] using first in-text candidate (%zu/%zu)\n",
					valid_candidates.size(), candidates.size());
			}

			if (chosen.call_target)
			{
				entity_call = reinterpret_cast<void*>(chosen.call_target);
				utils::debug::log_hex("[entity] chosen call_target", chosen.call_target);
				utils::debug::log_hex("[entity] chosen call_instruction", chosen.call_instruction);
			}
		}

		if (!entity_call)
		{
			utils::debug::log("[entity] abort: no safe entity function found\n");
			return;
		}

		utils::debug::log("[entity] installing inline hook...\n");
		hook_fn(entity_call, hk::entity);

		if (original_entity_hook)
			utils::debug::log_hex("[entity] hook installed, trampoline", reinterpret_cast<std::uintptr_t>(original_entity_hook));
		else
			utils::debug::log("[entity] hook failed: swap_inline returned no trampoline\n");
	}
}