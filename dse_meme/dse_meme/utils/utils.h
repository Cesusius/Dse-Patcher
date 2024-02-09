#define in_range(x,a,b)    (x >= a && x <= b) 
#define get_bits( x )    (in_range((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xA) : (in_range(x,'0','9') ? x - '0' : 0))
#define get_byte( x )    (get_bits(x[0]) << 4 | get_bits(x[1]))
#define to_lower_i(Char) ((Char >= 'A' && Char <= 'Z') ? (Char + 32) : Char)
#define to_lower_c(Char) ((Char >= (char*)'A' && Char <= (char*)'Z') ? (Char + 32) : Char)

auto get_system_information( SYSTEM_INFORMATION_CLASS InformationClass ) -> void* {
    unsigned long size = 32;
    char buffer[ 32 ];

    ZwQuerySystemInformation( InformationClass, buffer, size, &size );

    void* info = ExAllocatePoolZero( NonPagedPool, size, 'UD' );

    if ( !info )
        return nullptr;

    if ( !NT_SUCCESS( ZwQuerySystemInformation( InformationClass, info, size, &size ) ) ) {
        ExFreePool( info );
        return nullptr;
    }

    return info;
}

auto get_kernel_module( const char* name ) -> uintptr_t {
    const auto to_lower = [ ]( char* string ) -> const char* {
        for ( char* pointer = string; *pointer != '\0'; ++pointer ) {
            *pointer = ( char )( short )tolower( *pointer );
        }

        return string;
        };

    const PRTL_PROCESS_MODULES info = ( PRTL_PROCESS_MODULES )get_system_information( SystemModuleInformation );

    if ( !info )
        return NULL;

    for ( size_t i = 0; i < info->NumberOfModules; ++i ) {
        const auto& mod = info->Modules[ i ];

        if ( crt::strcmp( to_lower_c( ( char* )mod.FullPathName + mod.OffsetToFileName ), name ) == 0 ) {
            const void* address = mod.ImageBase;
            ExFreePool( info );
            return ( uintptr_t )address;
        }
    }

    ExFreePool( info );
    return NULL;
}

uintptr_t find_pattern( uintptr_t base, size_t range, const char* pattern, const char* mask ) {
   
    const auto check_mask = [ ]( const char* base, const char* pattern, const char* mask ) -> bool {
        for ( ; *mask; ++base, ++pattern, ++mask ) {
            if ( *mask == 'x' && *base != *pattern ) {
                return false;
            }
        }

        return true;
        };

    range = range - crt::strlen( mask );

    for ( size_t i = 0; i < range; ++i ) {
        if ( check_mask( ( const char* )base + i, pattern, mask ) ) {
            return base + i;
        }
    }

    return NULL;
}

uintptr_t find_pattern( uintptr_t base, const char* pattern, const char* mask ) {
    const PIMAGE_NT_HEADERS headers = ( PIMAGE_NT_HEADERS )( base + ( ( PIMAGE_DOS_HEADER )base )->e_lfanew );

    const PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION( headers );

    for ( size_t i = 0; i < headers->FileHeader.NumberOfSections; i++ ) {
        const PIMAGE_SECTION_HEADER section = &sections[ i ];

        if ( section->Characteristics & IMAGE_SCN_MEM_EXECUTE ) {
            const auto match = find_pattern( base + section->VirtualAddress, section->Misc.VirtualSize, pattern, mask );

            if ( match ) {
                return match;
            }
        }
    }

    return 0;
}

uintptr_t find_pattern( uintptr_t module_base, const char* pattern ) {
    auto pattern_ = pattern;
    uintptr_t first_match = 0;

    if ( !module_base ) {
        return 0;
    }

    const auto nt = reinterpret_cast< IMAGE_NT_HEADERS* >( module_base + reinterpret_cast< IMAGE_DOS_HEADER* >( module_base )->e_lfanew );

    for ( uintptr_t current = module_base; current < module_base + nt->OptionalHeader.SizeOfImage; current++ ) {
        if ( !*pattern_ ) {
            return first_match;
        }

        if ( *( BYTE* )pattern_ == '\?' || *( BYTE* )current == get_byte( pattern_ ) ) {
            if ( !first_match )
                first_match = current;

            if ( !pattern_[ 2 ] )
                return first_match;

            if ( *( WORD* )pattern_ == '\?\?' || *( BYTE* )pattern_ != '\?' )
                pattern_ += 3;

            else
                pattern_ += 2;
        }
        else {
            pattern_ = pattern;
            first_match = 0;
        }
    }

    return 0;
}