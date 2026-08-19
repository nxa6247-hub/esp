#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>


#include "xorstr/xorstr.h"

#include <winternl.h>


#include <Zydis/Zydis.h>

class Engine {
public:
	static Engine* Get() noexcept {
		static Engine instance;
		return &instance;
	}

	template<typename DefaultTy = uintptr_t>
	DefaultTy GetModule() noexcept {
		return (DefaultTy)NtCurrentTeb()->ProcessEnvironmentBlock->Reserved3[1];
	};

private:
	Engine() noexcept = default;

	Engine(const Engine&) = delete;
	Engine& operator=(const Engine&) = delete;

};

#include <cstdio>

inline void SetupConsole() noexcept
{
	AllocConsole();
	SetConsoleTitleA("R6 Internal Debug");
	FILE* file = nullptr;
	freopen_s(&file, "CONOUT$", "w", stdout);
	freopen_s(&file, "CONOUT$", "w", stderr);
	freopen_s(&file, "CONIN$", "r", stdin);
}

namespace Update
{
	inline const auto ImageBase = Engine::Get()->GetModule();
	inline auto retaddr = ImageBase;

}

using Update::ImageBase, Update::retaddr;


namespace utils
{
	namespace syscall
	{
		extern "C"
		{
			//	Syscall ( NtProtectVirtualMemory )
			__forceinline auto __vm_protect( void*, void*, size_t*, std::uint32_t, std::uint32_t* ) -> uint32_t;

			//	Wrapper ( does the same job as VirtualProtect )
			__forceinline auto vm_protect( std::uint64_t address, size_t size, std::uint32_t protect, std::uint32_t* old_protect ) -> uint32_t
			{
				return __vm_protect(
					reinterpret_cast< void* >( -1 ),
					&address,
					&size,
					protect,
					old_protect
				) == 0 ? *old_protect : 0;
			}

			__forceinline auto get_async_key_state( int vKey ) -> std::uint16_t;
		}
	}

	namespace memory
	{

		
		template<typename T>
		T Read(uintptr_t address) {
			if (IsBadReadPtr((void*)address, sizeof(T))) {
				if constexpr (std::is_pointer_v<T>) {
					return nullptr;
				}
				else {
					return T{};
				}
			}
			return *reinterpret_cast<T*>(address);
		}

		template<typename T>
		void Write(uintptr_t address, T data) {
			if (IsBadReadPtr((void*)address, sizeof(T))) return;
			if (IsBadWritePtr((LPVOID)address, sizeof(T))) return;
			*(T*)address = data;
		}

		inline bool write_float(std::uintptr_t address, float value)
		{
			__try
			{
				DWORD old_protect = 0;
				if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(float), PAGE_READWRITE, &old_protect))
					return false;

				*reinterpret_cast<float*>(address) = value;

				DWORD ignored = 0;
				VirtualProtect(reinterpret_cast<void*>(address), sizeof(float), old_protect, &ignored);
				return true;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				return false;
			}
		}


		template<typename T>
		T read_pointer(uintptr_t base, std::vector<uintptr_t> offsets) {
			for (int i = 0; i < offsets.size(); i++) {
				base = Read<uintptr_t>(base);
				base += offsets[i];
			}
			return Read<T>(base);
		}


		inline uintptr_t get_address(uintptr_t base, std::vector<uintptr_t> offsets) {
			for (int i = 0; i < offsets.size(); i++) {
				base = Read<uintptr_t>(base);
				if (!base) return 0;
				base += offsets[i];
				if (!base) return 0;
			}
			return base;
		}

		template<typename T>
		void write_pointer(uintptr_t base, std::vector<uintptr_t> offsets, T data) {
			base = get_address(base, offsets);
			if (IsBadWritePtr((LPVOID)base, sizeof(T))) return;
			if (IsBadReadPtr((void*)base, sizeof(T))) return;
			Write<T>(base, data);
		}



		inline std::uint64_t image_base{ };
		inline std::uint32_t text_size{ };
		inline std::uint32_t text_virtualaddress{ };

		inline std::uint32_t rdata_size{ };
		inline std::uint32_t rdata_virtualaddress{ };

		inline std::uintptr_t text_start() noexcept
		{
			return image_base + text_virtualaddress;
		}

		inline std::uintptr_t text_end() noexcept
		{
			return text_start() + text_size;
		}

		inline bool is_in_text_section(std::uintptr_t address) noexcept
		{
			if (!address || !image_base || !text_size)
				return false;

			return address >= text_start() && address < text_end();
		}

		inline bool is_executable(std::uintptr_t address) noexcept
		{
			MEMORY_BASIC_INFORMATION mbi{};
			if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)))
				return false;

			if (mbi.State != MEM_COMMIT)
				return false;

			const DWORD executable_mask =
				PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;

			return (mbi.Protect & executable_mask) != 0;
		}


		inline size_t get_instruction_length_safe(void* address)
		{
			ZydisDecoder decoder;
			ZydisDecoderInit(&decoder,
				ZYDIS_MACHINE_MODE_LONG_64,
				ZYDIS_STACK_WIDTH_64);

			ZydisDecodedInstruction instruction;

			ZyanStatus status = ZydisDecoderDecodeInstruction(
				&decoder,
				nullptr,              
				address,
				15,                   
				&instruction);

			if (ZYAN_SUCCESS(status))
				return instruction.length;

			return 0;
		}

		inline size_t calculate_hook_size(void* address, size_t min_bytes)
		{
			uint8_t* ptr = static_cast<uint8_t*>(address);
			size_t total = 0;

			while (total < min_bytes)
			{
				size_t len = get_instruction_length_safe(ptr + total);

				if (len == 0)
					return 0;

				total += len;

				if (total > 64)
					return 0;
			}

			return total;
		}

		inline void* create_trampoline(void* destination, size_t patchLen)
		{
			uint8_t* original = static_cast<uint8_t*>(destination);

			void* trampoline = VirtualAlloc(
				nullptr,
				patchLen + 12,
				MEM_COMMIT | MEM_RESERVE,
				PAGE_EXECUTE_READWRITE);

			if (!trampoline)
				return nullptr;

			std::memcpy(trampoline, original, patchLen);

			uint8_t* t = static_cast<uint8_t*>(trampoline);

			// mov rax, imm64
			t[patchLen + 0] = 0x48;
			t[patchLen + 1] = 0xB8;

			*reinterpret_cast<void**>(&t[patchLen + 2]) =
				original + patchLen;

			// jmp rax
			t[patchLen + 10] = 0xFF;
			t[patchLen + 11] = 0xE0;

			return trampoline;
		}

		inline bool write_hook(void* destination, void* detour, size_t patchLen)
		{
			if (patchLen < 12)
				return false;

			uint32_t oldProtect;
			utils::syscall::vm_protect(
				reinterpret_cast<std::uintptr_t>(destination),
				patchLen,
				PAGE_EXECUTE_READWRITE,
				&oldProtect);

			uint8_t* dst = static_cast<uint8_t*>(destination);

			dst[0] = 0x48;
			dst[1] = 0xB8;

			*reinterpret_cast<void**>(&dst[2]) = detour;

			dst[10] = 0xFF;
			dst[11] = 0xE0;

			for (size_t i = 12; i < patchLen; i++)
				dst[i] = 0x90;

			utils::syscall::vm_protect(
				reinterpret_cast<std::uintptr_t>(destination),
				patchLen,
				oldProtect,
				&oldProtect);

			return true;
		}

		inline void* swap_inline(void* destination, void* detour)
		{
			size_t patchLen = calculate_hook_size(destination, 12);

			if (patchLen == 0)
				return nullptr;

			void* trampoline = create_trampoline(destination, patchLen);

			if (!trampoline)
				return nullptr;

			if (!write_hook(destination, detour, patchLen))
			{
				VirtualFree(trampoline, 0, MEM_RELEASE);
				return nullptr;
			}

			return trampoline;
		}

		template <typename T>
		__forceinline bool valid_pointer( T ptr )
		{
			__try
			{
				volatile auto result = *( uintptr_t* ) ptr;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER )
			{
				return false;
			}

			return true;
		}


		inline std::uintptr_t scan_pattern(std::uint64_t base, std::uint32_t size, const char* signature)
		{
			static auto patternToByte = [](const char* pattern)
				{
					auto       bytes = std::vector<int>{ };
					const auto start = const_cast<char*>(pattern);
					const auto end = const_cast<char*>(pattern) + strlen(pattern);

					for (auto current = start; current < end; ++current)
					{
						if (*current == '?')
						{
							++current;
							if (*current == '?')
								++current;
							bytes.push_back(-1);
						}
						else { bytes.push_back(strtoul(current, &current, 16)); }
					}
					return bytes;
				};
			auto       patternBytes = patternToByte(signature);
			const auto scanBytes = reinterpret_cast<std::uint8_t*>(base);

			const auto s = patternBytes.size();
			const auto d = patternBytes.data();

			for (auto i = 0ul; i < size - s; ++i)
			{
				bool found = true;
				for (auto j = 0ul; j < s; ++j)
				{

					if (scanBytes[i + j] != d[j] && d[j] != -1)
					{
						found = false;
						break;
					}
				}
				if (found) { return reinterpret_cast<std::uintptr_t>(&scanBytes[i]); }
			}
			return 0;
		}

		inline void search_pattern(const char* pattern_str, uint64_t& out,
			int offset = 0, bool read_only = false, uint64_t must_end_with = UINT64_MAX)
		{
			struct pb { uint8_t v, m; };
			std::vector<pb> pattern;

			std::istringstream s(pattern_str);
			std::string b;

			while (s >> b) {
				uint8_t v = 0, m = 0;
				for (int i = 0; i < 2; ++i) {
					v <<= 4; m <<= 4;
					if (b[i] != '?') {
						v |= (uint8_t)strtoul(std::string(1, b[i]).c_str(), 0, 16);
						m |= 0xF;
					}
				}
				pattern.push_back({ v, m });
			}

			if (pattern.empty()) return;

			SYSTEM_INFO si;
			GetSystemInfo(&si);

			MEMORY_BASIC_INFORMATION mbi{};
			uint8_t* addr = 0;

			auto valid = [&](DWORD p) {
				if (read_only)
					return p == PAGE_READONLY || p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE;
				else
					return p == PAGE_READWRITE || p == PAGE_EXECUTE_READWRITE;
				};

			while (addr < (uint8_t*)si.lpMaximumApplicationAddress) {
				if (!VirtualQuery(addr, &mbi, sizeof(mbi))) break;

				if (mbi.State == MEM_COMMIT &&
					!(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
					valid(mbi.Protect))
				{
					uint8_t* start = (uint8_t*)mbi.BaseAddress;
					uint8_t* end = start + mbi.RegionSize;

					if (end < start) {
						addr += mbi.RegionSize;
						continue;
					}

					end -= pattern.size();

					for (uint8_t* cur = start; cur < end; ++cur) {
						bool match = true;

						for (size_t i = 0; i < pattern.size(); ++i) {
							if ((cur[i] & pattern[i].m) != (pattern[i].v & pattern[i].m)) {
								match = false;
								break;
							}
						}

						if (!match) continue;

						uint64_t found = (uint64_t)cur + offset;

						if (read_only && (found >> 44) != 0x7FF) continue;
						if (must_end_with != UINT64_MAX && (found & 0xFF) != (must_end_with & 0xFF)) continue;

						out = found;
						return;
					}
				}

				addr += mbi.RegionSize;
			}
		}

		inline std::uint64_t find_string(std::uint64_t base, std::string_view str) {
			if (!base || str.empty())
				return 0;

			const auto dos_header = (PIMAGE_DOS_HEADER)base;
			const auto nt_headers = (PIMAGE_NT_HEADERS)((std::uint8_t*)base + dos_header->e_lfanew);
			const auto module_size = nt_headers->OptionalHeader.SizeOfImage;

			char* scanBytes = (char*)base;
			const std::size_t s = str.size();

			for (std::uint32_t i = 0; i < module_size - s; i++) {
				bool found = true;
				for (std::uint32_t j = 0; j < s; j++) {
					if (scanBytes[i + j] != str[j]) {
						found = false;
						break;
					}
				}

				if (found) {
					char next_char = scanBytes[i + s];
					if (next_char == '\0' || next_char == ' ' || next_char == '\n' || next_char == '\r') {
						return (std::uint64_t)&scanBytes[i];
					}
				}
			}

			return 0;
		}

	
		inline std::uint64_t current_peb( )
		{
			return __readgsqword( 0x60 );
		}



	
	}

	namespace str
	{
		inline std::string format_str( const char* fmt, ... )
		{
			va_list args;
			va_start( args, fmt );

			const int size = _vscprintf( fmt, args ) + 1;
			std::vector<char> buffer( size );

			vsprintf_s( buffer.data( ), size, fmt, args );
			va_end( args );

			return std::string( buffer.data( ) );
		}

		inline void write_str( void* address, const char* string )
		{
			memcpy( address, string, strlen( string ) + 1 );
		}

		inline std::string parse_str( const std::string& jsonString, const std::string& key, size_t startPos = 0 )
		{
			std::string searchKey = "\"" + key + "\":\"";
			auto pos = jsonString.find( searchKey, startPos );
			if ( pos == std::string::npos )
			{
				return "";
			}
			pos += searchKey.length( );
			auto endPos = jsonString.find( "\"", pos );
			if ( endPos == std::string::npos )
			{
				return "";
			}
			startPos = endPos + 1;  
			return jsonString.substr( pos, endPos - pos );
		}
	}



	namespace nt
	{
		typedef struct _UNICODE_STRING
		{
			unsigned short Length;
			unsigned short MaximumLength;
			wchar_t* Buffer;
		} UNICODE_STRING, * PUNICODE_STRING;

		typedef struct _PEB_LDR_DATA
		{
			unsigned char       Reserved1[ 8 ];
			void* Reserved2[ 3 ];
			LIST_ENTRY InMemoryOrderModuleList;
		} PEB_LDR_DATA, * PPEB_LDR_DATA;

		typedef struct _PEB
		{
			unsigned char                          Reserved1[ 2 ];
			unsigned char                          BeingDebugged;
			unsigned char                          Reserved2[ 1 ];
			void* Reserved3[ 2 ];
			PPEB_LDR_DATA                 Ldr;
		} PEB, * PPEB;

		typedef struct _LIST_ENTRY
		{
			struct _LIST_ENTRY* Flink;
			struct _LIST_ENTRY* Blink;
		} LIST_ENTRY, * PLIST_ENTRY;

		typedef struct _LDR_DATA_TABLE_ENTRY
		{
			LIST_ENTRY InLoadOrderLinks;//0x0
			void* Reserved1[ 2 ];//0x10
			void* DllBase;//0x20
			void* EntryPoint;//0x28
			unsigned long SizeOfImage;//0x30
			UNICODE_STRING FullDllName;//0x34
			UNICODE_STRING BaseDllName;//0x44

		} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

		typedef struct _IMAGE_DOS_HEADER
		{      // DOS .EXE header
			unsigned short   e_magic;                     // Magic number
			unsigned short   e_cblp;                      // unsigned chars on last page of file
			unsigned short   e_cp;                        // Pages in file
			unsigned short   e_crlc;                      // Relocations
			unsigned short   e_cparhdr;                   // Size of header in paragraphs
			unsigned short   e_minalloc;                  // Minimum extra paragraphs needed
			unsigned short   e_maxalloc;                  // Maximum extra paragraphs needed
			unsigned short   e_ss;                        // Initial (relative) SS value
			unsigned short   e_sp;                        // Initial SP value
			unsigned short   e_csum;                      // Checksum
			unsigned short   e_ip;                        // Initial IP value
			unsigned short   e_cs;                        // Initial (relative) CS value
			unsigned short   e_lfarlc;                    // File address of relocation table
			unsigned short   e_ovno;                      // Overlay number
			unsigned short   e_res[ 4 ];                    // Reserved unsigned shorts
			unsigned short   e_oemid;                     // OEM identifier (for e_oeminfo)
			unsigned short   e_oeminfo;                   // OEM information; e_oemid specific
			unsigned short   e_res2[ 10 ];                  // Reserved unsigned shorts
			long   e_lfanew;                    // File address of new exe header
		} IMAGE_DOS_HEADER, * PIMAGE_DOS_HEADER;

		typedef struct _IMAGE_OPTIONAL_HEADER64
		{
			unsigned short                 Magic;
			unsigned char                 MajorLinkerVersion;
			unsigned char                 MinorLinkerVersion;
			unsigned long                SizeOfCode;
			unsigned long                SizeOfInitializedData;
			unsigned long                SizeOfUninitializedData;
			unsigned long                AddressOfEntryPoint;
			unsigned long                BaseOfCode;
			unsigned long long            ImageBase;
			unsigned long                SectionAlignment;
			unsigned long                FileAlignment;
			unsigned short                 MajorOperatingSystemVersion;
			unsigned short                 MinorOperatingSystemVersion;
			unsigned short                 MajorImageVersion;
			unsigned short                 MinorImageVersion;
			unsigned short                 MajorSubsystemVersion;
			unsigned short                 MinorSubsystemVersion;
			unsigned long                Win32VersionValue;
			unsigned long                SizeOfImage;
			unsigned long                SizeOfHeaders;
			unsigned long                CheckSum;
			unsigned short                 Subsystem;
			unsigned short                 DllCharacteristics;
			unsigned long long            SizeOfStackReserve;
			unsigned long long            SizeOfStackCommit;
			unsigned long long            SizeOfHeapReserve;
			unsigned long long            SizeOfHeapCommit;
			unsigned long                LoaderFlags;
			unsigned long                NumberOfRvaAndSizes;
			IMAGE_DATA_DIRECTORY DataDirectory[ 16 ];
		} IMAGE_OPTIONAL_HEADER64, * PIMAGE_OPTIONAL_HEADER64;

		typedef struct _IMAGE_FILE_HEADER
		{
			unsigned short  Machine;
			unsigned short  NumberOfSections;
			unsigned long TimeDateStamp;
			unsigned long PointerToSymbolTable;
			unsigned long NumberOfSymbols;
			unsigned short  SizeOfOptionalHeader;
			unsigned short  Characteristics;
		} IMAGE_FILE_HEADER, * PIMAGE_FILE_HEADER;

		typedef struct _IMAGE_NT_HEADERS64
		{
			unsigned long Signature;
			IMAGE_FILE_HEADER FileHeader;
			IMAGE_OPTIONAL_HEADER64 OptionalHeader;
		} IMAGE_NT_HEADERS64, * PIMAGE_NT_HEADERS64;

		typedef struct _IMAGE_EXPORT_DIRECTORY
		{
			unsigned long   Characteristics;
			unsigned long   TimeDateStamp;
			unsigned short    MajorVersion;
			unsigned short    MinorVersion;
			unsigned long   Name;
			unsigned long   Base;
			unsigned long   NumberOfFunctions;
			unsigned long   NumberOfNames;
			unsigned long   AddressOfFunctions;     // RVA from base of image
			unsigned long   AddressOfNames;         // RVA from base of image
			unsigned long   AddressOfNameOrdinals;  // RVA from base of image
		} IMAGE_EXPORT_DIRECTORY, * PIMAGE_EXPORT_DIRECTORY;

		typedef struct _IMAGE_DATA_DIRECTORY
		{
			unsigned long   VirtualAddress;
			unsigned long   Size;
		} IMAGE_DATA_DIRECTORY, * PIMAGE_DATA_DIRECTORY;

		typedef struct _IMAGE_SECTION_HEADER
		{
			std::uint8_t Name[ 8 ]; // An 8-byte, null-padded UTF-8 string. If the string is exactly 8 characters long, there is no terminating null. For longer names, this is a pointer to a string in the string table.
			union
			{
				std::uint32_t PhysicalAddress;           // The physical address of the section, used when the section is loaded into memory.
				std::uint32_t VirtualSize;               // The actual size of the section when loaded into memory. If this value is greater than SizeOfRawData, the section is zero-padded. This field is valid only if the section is non-empty.
			} Misc;
			std::uint32_t VirtualAddress;                // The address of the first byte of the section when loaded into memory, relative to the image base.
			std::uint32_t SizeOfRawData;                 // The size of the section (in bytes) or the size of the initialized data on disk.
			std::uint32_t PointerToRawData;              // A file pointer to the first page of the section within the COFF file.
			std::uint32_t PointerToRelocations;          // A file pointer to the beginning of the relocation entries for the section. This is set to zero for executable images or if there are no relocations.
			std::uint32_t PointerToLinenumbers;          // A file pointer to the beginning of the line-number entries for the section. This is set to zero if there are no line numbers.
			std::uint16_t  NumberOfRelocations;           // The number of relocation entries for the section. This is set to zero for executable images or if there are no relocations.
			std::uint16_t  NumberOfLinenumbers;           // The number of line-number entries for the section.
			std::uint32_t Characteristics;               // The flags that describe the characteristics of the section.
		} IMAGE_SECTION_HEADER, * PIMAGE_SECTION_HEADER;
	}

	namespace importer
	{
		inline auto get_exported_function( const std::uintptr_t module, const char* function ) -> void*
		{
			if ( !module )
				return 0;

			const auto dos_header = reinterpret_cast< nt::IMAGE_DOS_HEADER* >( module );
			const auto nt_header = reinterpret_cast< nt::IMAGE_NT_HEADERS64* >( module + dos_header->e_lfanew );

			const auto data_directory = reinterpret_cast< nt::IMAGE_DATA_DIRECTORY* >( &nt_header->OptionalHeader.DataDirectory[ 0 ] );
			const auto image_export_directory = reinterpret_cast< nt::IMAGE_EXPORT_DIRECTORY* >( module + data_directory->VirtualAddress );
			if ( !image_export_directory )
				return 0;

			const auto* const rva_table = reinterpret_cast< const unsigned long* >( module + image_export_directory->AddressOfFunctions );
			const auto* const export_table = reinterpret_cast< const unsigned long* >( module + image_export_directory->AddressOfNames );
			const auto* const ord_table = reinterpret_cast< const unsigned short* >( module + image_export_directory->AddressOfNameOrdinals );

			for ( unsigned int idx = 0; idx < image_export_directory->NumberOfFunctions; idx++ )
			{
				const auto fn_name = reinterpret_cast< const char* >( module + export_table[ idx ] );

				if ( !strcmp( fn_name, function ) )
					return reinterpret_cast< void* >( module + rva_table[ ord_table[ idx ] ] );
			}

			return 0;
		}

		inline auto get_safe_module( const wchar_t* name ) -> uintptr_t
		{
			auto peb = reinterpret_cast< nt::_PEB* >( __readgsqword( 0x60 ) );
			if ( !peb )
				return 0;

			auto list_entry = peb->Ldr->InMemoryOrderModuleList;

			for ( auto m = list_entry; m.Flink != &peb->Ldr->InMemoryOrderModuleList; m = *m.Flink )
			{
				auto module = reinterpret_cast< nt::LDR_DATA_TABLE_ENTRY* >( m.Flink );

				if ( !module->BaseDllName.Buffer )
					continue;

				if ( !_wcsicmp( module->BaseDllName.Buffer, name ) )
					return reinterpret_cast< uintptr_t >( module->DllBase );
			}

			return 0;
		}

		inline std::uint64_t get_section_size( const uintptr_t module, const char* section )
		{
			const HMODULE GameModule = ( HMODULE ) module;
			const IMAGE_DOS_HEADER* DOSHeader = reinterpret_cast< IMAGE_DOS_HEADER* >( GameModule );
			const IMAGE_NT_HEADERS* NtHeaders = reinterpret_cast< IMAGE_NT_HEADERS* >( reinterpret_cast< long long >( GameModule ) + DOSHeader->e_lfanew );
			auto size = NtHeaders->OptionalHeader.SizeOfImage;

			IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION( NtHeaders );

			for ( int i = 0; i < NtHeaders->FileHeader.NumberOfSections; ++i )
			{
				if ( strcmp( reinterpret_cast< char* >( sectionHeader[ i ].Name ), section ) == 0 )
				{
					return ( std::uint64_t ) sectionHeader[ i ].Misc.VirtualSize;
				}
			}

			return 0;
		}

		inline std::uint64_t get_virtual_address( const uintptr_t module, const char* section )
		{
			const HMODULE GameModule = ( HMODULE ) module;
			const IMAGE_DOS_HEADER* DOSHeader = reinterpret_cast< IMAGE_DOS_HEADER* >( GameModule );
			const IMAGE_NT_HEADERS* NtHeaders = reinterpret_cast< IMAGE_NT_HEADERS* >( reinterpret_cast< long long >( GameModule ) + DOSHeader->e_lfanew );
			auto size = NtHeaders->OptionalHeader.SizeOfImage;

			IMAGE_SECTION_HEADER* sectionHeader = IMAGE_FIRST_SECTION( NtHeaders );

			for ( int i = 0; i < NtHeaders->FileHeader.NumberOfSections; ++i )
			{
				if ( strcmp( reinterpret_cast< char* >( sectionHeader[ i ].Name ), section ) == 0 )
				{
					return ( std::uint64_t ) sectionHeader[ i ].VirtualAddress;
				}
			}

			return 0;
		}
	}
}