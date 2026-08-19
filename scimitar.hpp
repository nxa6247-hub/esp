#pragma once
#ifndef _scimitar
#define _scimitar
//
#include "../utils/spoofcall/spoofcall.h"
#include "../utils/debug/log.hpp"
#include "../config.hpp"
#include "../scimitar/engine/havok_math.hpp"
#include "../scimitar/engine/ida_defs.hpp"
#include "../External/imgui/imgui.h"


#include <iostream>
#include <thread>
#include <cmath>
#include <atomic>


namespace scimitar 
{

	class Entity;

	enum class entity_status
	{
		INVALID,
		DEAD_1,
		DEAD_2,
		TEAM,
		LOCAL,
		VALID
	};

	class view_translation;

	class view_translation {

	private:
		
		static inline uint64_t temp = 0; //<-- Store View Translation Address

	public:

		static auto init( ) -> bool {

			utils::memory::search_pattern(
				"A4 70 7D BF 00 00 00 00 00 00 00 00 00 00 A0 40 00 00 A0 C0 00 00 00 00 00 00 00 00 CD CC 4C 3F 00 00 00 3F 00 00 80 3E",
				temp,
				-0x2A4,
				false
			);

			return temp != 0;
		}


		static view_translation* get_instance( ) {

			if (!temp || !utils::memory::valid_pointer(reinterpret_cast<void*>(temp)))
				return nullptr;

			return reinterpret_cast<view_translation*>( temp );
		}

	
		auto m_ViewMatrix( ) -> const ubiViewMatrix&
		{
			static ubiViewMatrix fallback{};
			const auto matrix_ptr = reinterpret_cast<const ubiViewMatrix*>(this + 0x250);

			if (!utils::memory::valid_pointer(const_cast<ubiViewMatrix*>(matrix_ptr)))
				return fallback;

			return *matrix_ptr;
		}

		auto m_ViewOffset( ) -> ubiVector4 {

			if (!utils::memory::valid_pointer(this))
				return ubiVector4();

			const auto offset_ptr = reinterpret_cast<const ubiVector4*>(this + 0x190);
			if (!utils::memory::valid_pointer(const_cast<ubiVector4*>(offset_ptr)))
				return ubiVector4();

			return *offset_ptr;
		}

		auto world_to_screen( const ubiVector4& world , ubiVector2& screen ) -> bool
		{
			if (!utils::memory::valid_pointer(this))
				return false;

			const ubiViewMatrix& vm = this->m_ViewMatrix();

			const float clip_w = vm[0][3] * world.x + vm[1][3] * world.y + vm[2][3] * world.z + vm[3][3];
			if (clip_w < 0.001f)
				return false;

			const float clip_x = world.x * vm[0][0] + world.y * vm[1][0] + world.z * vm[2][0] + vm[3][0];
			const float clip_y = world.x * vm[0][1] + world.y * vm[1][1] + world.z * vm[2][1] + vm[3][1];

			const float inv_w = 1.0f / clip_w;
			const float ndc_x = clip_x * inv_w;
			const float ndc_y = clip_y * inv_w;

			const ImVec2 size = ImGui::GetIO().DisplaySize;
			const float half_w = size.x * 0.5f;
			const float half_h = size.y * 0.5f;

			screen.x = half_w + (ndc_x * half_w);
			screen.y = half_h - (ndc_y * half_h);
			return true;
		}

	};


	class Entity
	{
	public:
		static auto filter_byte3_from(std::uint64_t fb) noexcept -> unsigned char
		{
			return static_cast<unsigned char>((fb >> 24) & 0xFF);
		}

		static auto filter_byte4_from(std::uint64_t fb) noexcept -> unsigned char
		{
			return static_cast<unsigned char>((fb >> 32) & 0xFF);
		}

		static auto is_valid_world_pos(const ubiVector3& v) -> bool
		{
			if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
				return false;

			if (std::fabs(v.x) >= 500000.f || std::fabs(v.y) >= 500000.f || std::fabs(v.z) >= 500000.f)
				return false;

			const int significant_axes =
				(std::fabs(v.x) > 2.f ? 1 : 0)
				+ (std::fabs(v.y) > 2.f ? 1 : 0)
				+ (std::fabs(v.z) > 2.f ? 1 : 0);

			return significant_axes >= 2;
		}

		static auto is_dead_filter_byte(std::uint64_t fb) noexcept -> bool
		{
			const auto b4 = filter_byte4_from(fb);
			return b4 == 0x84 || b4 == 0x82 || b4 == 0x80;
		}

		static auto is_teammate_filter_byte(std::uint64_t fb) noexcept -> bool
		{
			const auto b4 = filter_byte4_from(fb);
			return b4 == 0x00 || b4 == 0x02;
		}

		static auto is_player_filter_byte(std::uint64_t fb) noexcept -> bool
		{
			return ((fb >> 52) & 0xFFF) == 0x2C8;
		}

		static auto read_filter_byte_at(std::uintptr_t actor_ptr) noexcept -> std::uint64_t
		{
			__try
			{
				if (!utils::memory::valid_pointer(reinterpret_cast<void*>(actor_ptr)))
					return 0;

				const auto fb_ptr = reinterpret_cast<const std::uint64_t*>(actor_ptr + 0xB8);
				if (!utils::memory::valid_pointer(const_cast<std::uint64_t*>(fb_ptr)))
					return 0;

				return *fb_ptr;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return 0;
			}
		}

		static auto reset_comp_cache() -> void
		{
			s_comp_array_offset = 0;
		}

		static auto read_actor_origin_fast(std::uintptr_t actor_ptr) noexcept -> ubiVector3
		{
			__try
			{
				const auto& pos = *reinterpret_cast<const ubiVector3*>(actor_ptr + 0x50);
				if (is_valid_world_pos(pos))
					return pos;

				return {};
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return {};
			}
		}

		static auto should_render_filter_byte(std::uint64_t fb) noexcept -> bool
		{
			if (visuals::bDeadCheck && is_dead_filter_byte(fb))
				return false;

			const auto b4 = filter_byte4_from(fb);

			if (b4 == 0x00)
				return false;

			if (visuals::bTeamCheck && is_teammate_filter_byte(fb))
				return false;

			return true;
		}

		static auto read_player_position_cached(std::uintptr_t actor_ptr, std::uint16_t& comp_index) -> ubiVector3
		{
			if (!utils::memory::valid_pointer(reinterpret_cast<void*>(actor_ptr)))
				return {};

			if (comp_index != 0xFFFF && s_comp_array_offset)
			{
				const auto list_ptr = utils::memory::Read<std::uintptr_t>(actor_ptr + s_comp_array_offset);
				if (utils::memory::valid_pointer(reinterpret_cast<void*>(list_ptr)))
				{
					const auto comp = utils::memory::Read<std::uintptr_t>(
						list_ptr + static_cast<std::uintptr_t>(comp_index) * 8);

					if (utils::memory::valid_pointer(reinterpret_cast<void*>(comp)))
					{
						const auto id = utils::memory::Read<std::uint16_t>(comp - 0x08);
						if (id == 0x0c63)
						{
							const auto pos = utils::memory::Read<ubiVector3>(comp + 0xb00);
							if (is_valid_world_pos(pos))
								return pos;
						}
					}
				}

				comp_index = 0xFFFF;
			}

			return read_player_position_at(actor_ptr, comp_index);
		}

		static auto read_player_position_at(std::uintptr_t actor_ptr, std::uint16_t& comp_index) -> ubiVector3
		{
			if (!utils::memory::valid_pointer(reinterpret_cast<void*>(actor_ptr)))
				return {};

			const auto pos50 = read_actor_origin_fast(actor_ptr);
			if (is_valid_world_pos(pos50))
				return pos50;

			const auto pos = position_from_components(actor_ptr, comp_index);
			if (is_valid_world_pos(pos))
				return pos;

			return {};
		}

		static auto world_position_at(std::uintptr_t actor_ptr, std::uint16_t* comp_index_io = nullptr) -> ubiVector3
		{
			__try
			{
				if (!utils::memory::valid_pointer(reinterpret_cast<void*>(actor_ptr)))
					return {};

				const auto fb = read_filter_byte_at(actor_ptr);
				std::uint16_t local_comp_index = 0xFFFF;
				std::uint16_t& comp_index = comp_index_io ? *comp_index_io : local_comp_index;

				if (is_player_filter_byte(fb))
				{
					const auto pos50_ptr = reinterpret_cast<const ubiVector3*>(actor_ptr + 0x50);
					if (utils::memory::valid_pointer(const_cast<ubiVector3*>(pos50_ptr)))
					{
						const ubiVector3 pos50 = *pos50_ptr;
						if (is_valid_world_pos(pos50))
							return pos50;
					}

					const auto pos = position_from_components(actor_ptr, comp_index);
					if (is_valid_world_pos(pos))
						return pos;
				}

				ubiVector3 pos50{};
				ubiVector3 pos60{};

				if (utils::memory::valid_pointer(reinterpret_cast<void*>(actor_ptr + 0x50)))
					pos50 = utils::memory::Read<ubiVector3>(actor_ptr + 0x50);

				if (utils::memory::valid_pointer(reinterpret_cast<void*>(actor_ptr + 0x60)))
					pos60 = utils::memory::Read<ubiVector3>(actor_ptr + 0x60);

				const bool v50 = is_valid_world_pos(pos50);
				const bool v60 = is_valid_world_pos(pos60);

				if (v50 && v60)
				{
					const float mag50 = pos50.x * pos50.x + pos50.y * pos50.y + pos50.z * pos50.z;
					const float mag60 = pos60.x * pos60.x + pos60.y * pos60.y + pos60.z * pos60.z;
					return mag50 >= mag60 ? pos50 : pos60;
				}

				if (v50)
					return pos50;

				if (v60)
					return pos60;

				return {};
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				if (comp_index_io)
					*comp_index_io = 0xFFFF;

				return {};
			}
		}

	private:
		static inline std::uintptr_t s_comp_array_offset = 0;
		static inline std::atomic<std::uint64_t> s_local_filter_byte{ 0 };
		static inline std::atomic<std::uint8_t> s_local_byte3{ 0 };

		static auto find_components_array(std::uintptr_t ent) -> std::uintptr_t
		{
			if (!utils::memory::valid_pointer(reinterpret_cast<void*>(ent)))
				return 0;

			if (s_comp_array_offset)
			{
				const auto list_ptr = utils::memory::Read<std::uintptr_t>(ent + s_comp_array_offset);
				if (utils::memory::valid_pointer(reinterpret_cast<void*>(list_ptr)))
					return list_ptr;

				s_comp_array_offset = 0;
			}

			for (std::uintptr_t offset = 0xA0; offset <= 0xFC; offset += 8)
			{
				const auto list_ptr = utils::memory::Read<std::uintptr_t>(ent + offset);
				if (!utils::memory::valid_pointer(reinterpret_cast<void*>(list_ptr)))
					continue;

				for (std::uintptr_t i = 0; i < 100; ++i)
				{
					const auto comp = utils::memory::Read<std::uintptr_t>(list_ptr + i * 8);
					if (!utils::memory::valid_pointer(reinterpret_cast<void*>(comp)))
						continue;

					const auto id = utils::memory::Read<std::uint16_t>(comp - 0x08);
					if (id == 0x0c63)
					{
						s_comp_array_offset = offset;
						return list_ptr;
					}
				}
			}

			return 0;
		}

		static auto position_from_components(std::uintptr_t ent, std::uint16_t& comp_index) -> ubiVector3
		{
			const auto list_ptr = find_components_array(ent);
			if (!list_ptr)
				return {};

			if (comp_index != 0xFFFF)
			{
				const auto comp = utils::memory::Read<std::uintptr_t>(list_ptr + static_cast<std::uintptr_t>(comp_index) * 8);
				if (utils::memory::valid_pointer(reinterpret_cast<void*>(comp)))
				{
					const auto id = utils::memory::Read<std::uint16_t>(comp - 0x08);
					if (id == 0x0c63)
						return utils::memory::Read<ubiVector3>(comp + 0xb00);
				}

				comp_index = 0xFFFF;
			}

			for (std::uintptr_t i = 0; i < 100; ++i)
			{
				const auto comp = utils::memory::Read<std::uintptr_t>(list_ptr + i * 8);
				if (!utils::memory::valid_pointer(reinterpret_cast<void*>(comp)))
					continue;

				const auto id = utils::memory::Read<std::uint16_t>(comp - 0x08);
				if (id == 0x0c63)
				{
					comp_index = static_cast<std::uint16_t>(i);
					return utils::memory::Read<ubiVector3>(comp + 0xb00);
				}
			}

			return {};
		}

	public:

		auto filter_byte() -> std::uint64_t
		{
			if (!utils::memory::valid_pointer(this))
				return 0;

			const auto fb_ptr = reinterpret_cast<const std::uint64_t*>(reinterpret_cast<std::uintptr_t>(this) + 0xB8);
			if (!utils::memory::valid_pointer(const_cast<std::uint64_t*>(fb_ptr)))
				return 0;

			return *fb_ptr;
		}

		auto is_actor_player() -> bool
		{
			return ((filter_byte() >> 52) & 0xFFF) == 0x2C8;
		}

		auto world_position() -> ubiVector3
		{
			if (!utils::memory::valid_pointer(this))
				return {};

			const auto ent = reinterpret_cast<std::uintptr_t>(this);

			if (is_actor_player())
			{
				std::uint16_t comp_index = 0xFFFF;
				const auto pos = position_from_components(ent, comp_index);
				if (is_valid_world_pos(pos))
					return pos;
			}

			ubiVector3 pos50{};
			ubiVector3 pos60{};

			if (utils::memory::valid_pointer(reinterpret_cast<void*>(ent + 0x50)))
				pos50 = utils::memory::Read<ubiVector3>(ent + 0x50);

			if (utils::memory::valid_pointer(reinterpret_cast<void*>(ent + 0x60)))
				pos60 = utils::memory::Read<ubiVector3>(ent + 0x60);

			const bool v50 = is_valid_world_pos(pos50);
			const bool v60 = is_valid_world_pos(pos60);

			if (v50 && v60)
			{
				const float mag50 = pos50.x * pos50.x + pos50.y * pos50.y + pos50.z * pos50.z;
				const float mag60 = pos60.x * pos60.x + pos60.y * pos60.y + pos60.z * pos60.z;
				return mag50 >= mag60 ? pos50 : pos60;
			}

			if (v50)
				return pos50;

			if (v60)
				return pos60;

			return {};
		}

		auto m_Origin() -> ubiVector4
		{
			const auto pos = world_position();
			return ubiVector4(pos.x, pos.y, pos.z, 0.f);
		}
		

		auto filter_byte4() -> unsigned char
		{
			return filter_byte4_from(filter_byte());
		}

		auto filter_byte3() -> unsigned char
		{
			return filter_byte3_from(filter_byte());
		}

		auto refresh_team_reference() -> void
		{
			const auto fb = filter_byte();

			if (filter_byte4_from(fb) == 0x00)
			{
				s_local_filter_byte.store(fb, std::memory_order_relaxed);
				s_local_byte3.store(filter_byte3_from(fb), std::memory_order_relaxed);
			}
		}

		auto is_local_player() -> bool
		{
			return filter_byte4() == 0x00;
		}

		auto is_teammate() -> bool
		{
			return is_teammate_filter_byte(filter_byte());
		}

		auto is_dead() -> bool
		{
			if (is_dead_filter_byte(filter_byte()))
				return true;

			if (!utils::memory::valid_pointer(this))
				return false;

			const auto bitfield_ptr = reinterpret_cast<const unsigned long long*>(
				reinterpret_cast<std::uintptr_t>(this) + 0xB0);

			if (!utils::memory::valid_pointer(const_cast<unsigned long long*>(bitfield_ptr)))
				return false;

			const unsigned long long bitfield = *bitfield_ptr;
			const unsigned char byte3 = static_cast<unsigned char>((bitfield >> 24) & 0xFF);
			const unsigned char byte4 = static_cast<unsigned char>((bitfield >> 32) & 0xFF);

			if (byte3 == 0xFA)
				return true;

			return byte4 == 0x80 || byte4 == 0x84 || byte4 == 0x82;
		}

		auto m_Status() -> entity_status
		{
			if (!utils::memory::valid_pointer(this))
				return entity_status::INVALID;

			const unsigned char byte4 = filter_byte4();

			if (byte4 == 0x84 || byte4 == 0x82)
				return entity_status::DEAD_2;

			if (byte4 == 0x80)
				return entity_status::DEAD_1;

			if (byte4 == 0x02)
				return entity_status::TEAM;

			if (byte4 == 0x00)
				return entity_status::LOCAL;

			return entity_status::VALID;
		}

		auto actor_id( ) -> int
		{
			if (!utils::memory::valid_pointer(this))
				return -1;

			return *reinterpret_cast<int*>(this + 0x1C);
		}

		auto has_valid_origin() -> bool
		{
			return is_valid_world_pos(world_position());
		}

		auto is_cacheable_actor() -> bool
		{
			return is_actor_player() && has_valid_origin();
		}

		auto should_render_player() -> bool
		{
			if (!is_cacheable_actor())
				return false;

			refresh_team_reference();
			return should_render_filter_byte(filter_byte());
		}

		auto is_player_entity() -> bool
		{
			return should_render_player();
		}

		auto isA() -> bool
		{
			if (!utils::memory::valid_pointer( this )) 
				return false;

			const auto id_ptr = reinterpret_cast<const int*>(this + 0x1C);
			if (!utils::memory::valid_pointer(const_cast<int*>(id_ptr)))
				return false;

			int id = *id_ptr;
			return id > 0 && id < 1000;
		}

	};

  

    inline auto Init( ) -> void
    {
		if (scimitar::view_translation::init())
			utils::debug::log_hex("[scimitar] view_translation", reinterpret_cast<std::uintptr_t>(scimitar::view_translation::get_instance()));
		else
			utils::debug::log("[scimitar] view_translation pattern not found (visuals w2s disabled)\n");
    }

}


#endif