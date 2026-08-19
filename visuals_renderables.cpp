#include "../rainbow6.hpp"
#include <vector>
#include <cmath>

namespace
{
	constexpr float k_min_player_distance = 0.1f;
	constexpr float k_max_player_distance = 150.f;
	constexpr float k_head_height = 1.72f;
	constexpr auto k_entity_max_age = std::chrono::seconds(3);
	constexpr std::uint64_t k_hook_idle_clear_ms = 2000;

	struct render_target
	{
		ubiVector3 position{};
	};

	void draw_box_2d(
		scimitar::view_translation* view,
		ImDrawList* draw_list,
		const ubiVector3& feet,
		ImU32 color)
	{
		const ubiVector4 feet_world(feet.x, feet.y, feet.z, 0.f);
		const ubiVector4 head_world(feet.x, feet.y, feet.z + k_head_height, 0.f);

		ubiVector2 feet_screen{};
		ubiVector2 head_screen{};

		if (!view->world_to_screen(feet_world, feet_screen))
			return;

		if (!view->world_to_screen(head_world, head_screen))
			return;

		const float box_height = std::fabs(feet_screen.y - head_screen.y);
		if (box_height < 4.f)
			return;

		const float box_width = box_height * 0.45f;
		const float left = feet_screen.x - box_width * 0.5f;
		const float top = (feet_screen.y < head_screen.y) ? feet_screen.y : head_screen.y;
		const float right = left + box_width;
		const float bottom = (feet_screen.y > head_screen.y) ? feet_screen.y : head_screen.y;

		switch (visuals::iBoxype)
		{
		case 0:
		case 1:
			draw_list->AddRect(ImVec2(left, top), ImVec2(right, bottom), color);
			break;

		case 2:
		{
			const float w = right - left;
			const float h = bottom - top;
			const float line_frac = 0.25f;

			draw_list->AddLine({ left, top }, { left + w * line_frac, top }, color);
			draw_list->AddLine({ left, top }, { left, top + h * line_frac }, color);
			draw_list->AddLine({ right, top }, { right - w * line_frac, top }, color);
			draw_list->AddLine({ right, top }, { right, top + h * line_frac }, color);
			draw_list->AddLine({ left, bottom }, { left + w * line_frac, bottom }, color);
			draw_list->AddLine({ left, bottom }, { left, bottom - h * line_frac }, color);
			draw_list->AddLine({ right, bottom }, { right - w * line_frac, bottom }, color);
			draw_list->AddLine({ right, bottom }, { right, bottom - h * line_frac }, color);
			break;
		}
		default:
			break;
		}
	}
}

auto rainbow6::visuals_renderables(bool enabled) -> void
{
	if (!enabled)
		return;

	auto* view = scimitar::view_translation::get_instance();
	if (!view)
		return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float width = display.x;
	const float height = display.y;
	const float frame_dt = ImGui::GetIO().DeltaTime > 0.f ? ImGui::GetIO().DeltaTime : (1.f / 60.f);

	const auto now = std::chrono::steady_clock::now();

	const auto hook_tick = entity_last_hook_tick.load(std::memory_order_relaxed);
	const auto now_tick = GetTickCount64();
	if (hook_tick > 0 && (now_tick - hook_tick) > k_hook_idle_clear_ms)
	{
		clear_entity_cache();
		return;
	}

	std::vector<render_target> targets;
	targets.reserve(16);

	{
		std::lock_guard<std::mutex> lock(entity_mutex);

		for (auto it = cached_entities.begin(); it != cached_entities.end(); )
		{
			const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.last_seen);

			if (age > k_entity_max_age)
			{
				it = cached_entities.erase(it);
				continue;
			}

			if (!scimitar::Entity::should_render_filter_byte(it->second.filter_byte))
			{
				++it;
				continue;
			}

			const auto fresh_pos = scimitar::Entity::read_player_position_cached(it->first, it->second.comp_index);
			if (scimitar::Entity::is_valid_world_pos(fresh_pos))
				apply_entity_position(it->second, fresh_pos, now);

			const auto draw_pos = smooth_entity_draw_position(it->second, frame_dt);
			if (!scimitar::Entity::is_valid_world_pos(draw_pos))
			{
				++it;
				continue;
			}

			targets.push_back(render_target{ draw_pos });
			++it;
		}
	}

	if (targets.empty())
		return;

	const auto camera = view->m_ViewOffset();
	ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
	const ImU32 box_color = ImGui::ColorConvertFloat4ToU32(visuals::BoxRGB);
	const ImU32 snap_color = ImGui::ColorConvertFloat4ToU32(visuals::SnaplineRGB);

	for (const auto& target : targets)
	{
		const ubiVector4 origin(target.position.x, target.position.y, target.position.z, 0.f);

		const float dx = target.position.x - camera.x;
		const float dy = target.position.y - camera.y;
		const float dz = target.position.z - camera.z;
		const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

		if (distance < k_min_player_distance || distance > k_max_player_distance)
			continue;

		if (visuals::bSnaplines)
		{
			ubiVector2 screen{};
			if (view->world_to_screen(origin, screen))
			{
				draw_list->AddLine(
					ImVec2(width * 0.5f, height),
					ImVec2(screen.x, screen.y),
					snap_color,
					1.0f
				);
			}
		}

		if (visuals::bBox)
			draw_box_2d(view, draw_list, target.position, box_color);
	}
}
