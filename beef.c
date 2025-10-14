#include <efi.h>
#include <efilib.h>

#include "graphics.c"

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    EFI_GRAPHICS_OUTPUT_PROTOCOL* protocol = graphics_get_protocol();
    GraphicsOutput graphics = graphics_select_resolution(protocol);
    if (EFI_SUCCESS == graphics_allocate_backbuffer(&graphics)) {
        graphics_blt_green(&graphics);
        graphics_free_backbuffer(&graphics);
    }
    return EFI_SUCCESS;
}