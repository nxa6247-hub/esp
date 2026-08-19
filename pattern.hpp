#pragma once

#include <Zydis/Zydis.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>

#include "../utils.h"

namespace utils::pattern
{
	struct scan_result
	{
		std::uintptr_t call_instruction;
		std::uintptr_t call_target;
		std::uintptr_t anchor;
	};

	inline bool decode_instruction(const std::uint8_t* address, ZydisDecodedInstruction& instruction)
	{
		static ZydisDecoder decoder = [] {
			ZydisDecoder result;
			ZydisDecoderInit(&result, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
			return result;
		}();

		return ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(
			&decoder,
			nullptr,
			address,
			ZYDIS_MAX_INSTRUCTION_LENGTH,
			&instruction));
	}

	inline std::uintptr_t resolve_call_rel32(const std::uint8_t* instruction)
	{
		const auto rel = *reinterpret_cast<const std::int32_t*>(instruction + 1);
		return reinterpret_cast<std::uintptr_t>(instruction + 5 + rel);
	}

	template <std::size_t Size>
	inline std::uintptr_t find_bytes(std::uintptr_t base, std::size_t size, const std::array<std::uint8_t, Size>& bytes)
	{
		if (!base || size < Size)
			return 0;

		const auto* data = reinterpret_cast<const std::uint8_t*>(base);
		for (std::size_t i = 0; i <= size - Size; ++i)
		{
			bool matched = true;

			for (std::size_t j = 0; j < Size; ++j)
			{
				if (data[i + j] != bytes[j])
				{
					matched = false;
					break;
				}
			}

			if (matched)
				return reinterpret_cast<std::uintptr_t>(data + i);
		}

		return 0;
	}

	inline bool decode_until_anchor(
		std::uintptr_t start,
		std::uintptr_t anchor,
		std::vector<std::uintptr_t>& instruction_starts)
	{
		instruction_starts.clear();

		for (auto cursor = start; cursor < anchor;)
		{
			ZydisDecodedInstruction instruction{};
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(cursor);

			if (!decode_instruction(bytes, instruction))
				return false;

			const auto next = cursor + instruction.length;
			if (next > anchor)
				return true;

			instruction_starts.push_back(cursor);
			cursor = next;
		}

		return true;
	}


	inline std::vector<scan_result> find_all_rel32_calls_before_anchor(
		std::uintptr_t section_base,
		std::size_t section_size,
		std::uintptr_t anchor,
		std::size_t max_backtrack = 0x80)
	{
		std::vector<scan_result> results;

		if (!section_base || !section_size || !anchor)
			return results;

		const auto section_end = section_base + section_size;
		if (anchor <= section_base || anchor >= section_end)
			return results;

		const auto search_start =
			(anchor > section_base + max_backtrack) ? anchor - max_backtrack : section_base;

		std::vector<std::uintptr_t> starts;
		for (auto start = search_start; start < anchor; ++start)
		{
			if (decode_until_anchor(start, anchor, starts) && !starts.empty())
				break;
		}

		if (starts.empty())
			return results;

		
		for (const auto& addr : starts)
		{
			ZydisDecodedInstruction instruction{};
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(addr);

			if (!decode_instruction(bytes, instruction))
				continue;

			if (instruction.mnemonic == ZYDIS_MNEMONIC_CALL &&
				bytes[0] == 0xE8 &&
				instruction.length == 5)
			{
				auto target = resolve_call_rel32(bytes);

				
				bool duplicate = false;
				for (const auto& r : results)
				{
					if (r.call_target == target)
					{
						duplicate = true;
						break;
					}
				}

				if (!duplicate && target)
				{
					results.push_back({ addr, target, anchor });
				}
			}
		}

		return results;
	}


	inline std::vector<scan_result> find_all_rel32_calls_before_flag_store(
		std::uintptr_t section_base,
		std::size_t section_size,
		std::size_t max_backtrack = 0x80)
	{
		constexpr std::array<std::uint8_t, 5> tail = { 0xC7, 0x05, 0x00, 0x00, 0x01 };

		auto cursor = section_base;
		auto remaining = section_size;

		while (remaining >= tail.size())
		{
			const auto anchor = find_bytes(cursor, remaining, tail);
			if (!anchor)
				break;

			auto results = find_all_rel32_calls_before_anchor(
				section_base, section_size, anchor, max_backtrack
			);

			if (!results.empty())
				return results;

			const auto next = anchor + 1;
			remaining = (section_base + section_size) - next;
			cursor = next;
		}

		return {};
	}


	inline scan_result find_rel32_call_before_anchor(
		std::uintptr_t section_base,
		std::size_t section_size,
		std::uintptr_t anchor,
		std::size_t max_backtrack = 0x40)
	{
		if (!section_base || !section_size || !anchor)
			return {};

		const auto section_end = section_base + section_size;
		if (anchor <= section_base || anchor >= section_end)
			return {};

		const auto search_start =
			(anchor > section_base + max_backtrack) ? anchor - max_backtrack : section_base;

		std::vector<std::uintptr_t> starts;
		for (auto start = search_start; start < anchor; ++start)
		{
			if (decode_until_anchor(start, anchor, starts) && !starts.empty())
				break;
		}

		if (starts.empty())
			return {};

		for (auto it = starts.rbegin(); it != starts.rend(); ++it)
		{
			ZydisDecodedInstruction instruction{};
			const auto* bytes = reinterpret_cast<const std::uint8_t*>(*it);

			if (!decode_instruction(bytes, instruction))
				continue;

			if (instruction.mnemonic == ZYDIS_MNEMONIC_CALL && bytes[0] == 0xE8 && instruction.length == 5)
			{
				return {
					*it,
					resolve_call_rel32(bytes),
					anchor
				};
			}
		}

		return {};
	}

	inline scan_result find_rel32_call_before_flag_store(
		std::uintptr_t section_base,
		std::size_t section_size,
		std::size_t max_backtrack = 0x40)
	{
		constexpr std::array<std::uint8_t, 5> tail = { 0xC7, 0x05, 0x00, 0x00, 0x01 };

		auto cursor = section_base;
		auto remaining = section_size;

		while (remaining >= tail.size())
		{
			const auto anchor = find_bytes(cursor, remaining, tail);
			if (!anchor)
				break;

			const auto result = find_rel32_call_before_anchor(section_base, section_size, anchor, max_backtrack);
			if (result.call_target)
				return result;

			const auto next = anchor + 1;
			remaining = (section_base + section_size) - next;
			cursor = next;
		}

		return {};
	}

	inline std::vector<scan_result> collect_all_rel32_calls_before_flag_store(
		std::uintptr_t section_base,
		std::size_t section_size,
		std::size_t max_backtrack = 0x40)
	{
		std::vector<scan_result> all_results;
		constexpr std::array<std::uint8_t, 5> tail = { 0xC7, 0x05, 0x00, 0x00, 0x01 };

		auto cursor = section_base;
		auto remaining = section_size;

		while (remaining >= tail.size())
		{
			const auto anchor = find_bytes(cursor, remaining, tail);
			if (!anchor)
				break;

			auto results = find_all_rel32_calls_before_anchor(
				section_base, section_size, anchor, max_backtrack
			);

			for (const auto& result : results)
			{
				bool duplicate = false;
				for (const auto& existing : all_results)
				{
					if (existing.call_target == result.call_target)
					{
						duplicate = true;
						break;
					}
				}

				if (!duplicate && result.call_target)
					all_results.push_back(result);
			}

			const auto next = anchor + 1;
			remaining = (section_base + section_size) - next;
			cursor = next;
		}

		return all_results;
	}

	inline std::uintptr_t find_function_start(std::uintptr_t address, std::uintptr_t module_base)
	{
		if (!address || !module_base)
			return 0;

		const auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
		if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
			return 0;

		const auto* nt_headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(module_base + dos_header->e_lfanew);
		if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
			return 0;

		const auto& exception_dir =
			nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];

		if (!exception_dir.Size)
			return 0;

		const auto* function_table = reinterpret_cast<const RUNTIME_FUNCTION*>(
			module_base + exception_dir.VirtualAddress);

		const auto function_count = exception_dir.Size / sizeof(RUNTIME_FUNCTION);

		for (DWORD i = 0; i < function_count; ++i)
		{
			const auto function_start = module_base + function_table[i].BeginAddress;
			const auto function_end = module_base + function_table[i].EndAddress;

			if (address >= function_start && address < function_end)
				return function_start;
		}

		return 0;
	}

	inline std::uintptr_t find_entity_actor_function(
		std::uintptr_t section_base,
		std::size_t section_size,
		std::uintptr_t module_base)
	{
		if (!section_base || !section_size || !module_base)
			return 0;

		static const char* signatures[] = {
			"10 0F ? ? 20 0F ? ? 30 0F ? ? 50 0F ? ? 40 0F ? ? 30 0F ? ? 20 ? 01",
			"10 0F ? ? 20 0F ? ? 30 0F ? ? 50 0F ? ? 40 0F ? ? 30 0F ? ? 20 ? ? 01",
			"10 0F ? ? 20 0F ? ? 30 0F ? ? 50 0F ? ? 40 0F ? ? 30 0F ? ? 20 ? ? ? 01",
			"10 0F ? ? 20 0F ? ? 30 0F ? ? 50 0F ? ? 40 0F ? ? 30 0F ? ? 20 ? ? ? ? 01",
		};

		std::vector<std::uintptr_t> matches;

		for (const auto* signature : signatures)
		{
			auto search_start = section_base;

			while (search_start < section_base + section_size)
			{
				const auto remaining = (section_base + section_size) - search_start;
				const auto found = utils::memory::scan_pattern(search_start, static_cast<std::uint32_t>(remaining), signature);

				if (!found)
					break;

				matches.push_back(found);
				search_start = found + 1;
			}
		}

		if (matches.empty())
			return 0;

		std::sort(matches.begin(), matches.end());
		matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

		for (const auto match : matches)
		{
			const auto function_start = find_function_start(match, module_base);

			if (function_start
				&& function_start >= section_base
				&& function_start < section_base + section_size)
			{
				return function_start;
			}
		}

		return 0;
	}
}
