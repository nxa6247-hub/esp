#pragma once

#include <cstdio>
#include <cstdarg>
#include <cstdint>

namespace utils::debug
{
	inline void log(const char* fmt, ...) noexcept
	{
		va_list args;
		va_start(args, fmt);
		std::vprintf(fmt, args);
		va_end(args);
		std::fflush(stdout);
	}

	inline void log_hex(const char* label, std::uintptr_t value) noexcept
	{
		log("%s 0x%llX\n", label, static_cast<unsigned long long>(value));
	}
}
