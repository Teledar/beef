#include <efi.h>
#include <efilib.h>


struct {
    UINT32 width;
    UINT32 height;
    UINT32* pixels;
} backbuffer;


EFI_GRAPHICS_OUTPUT_PROTOCOL* get_graphics() {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics = NULL;
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    uefi_call_wrapper(ST->BootServices->LocateProtocol, 3, &guid, NULL, (VOID**)&graphics);
    return graphics;
}


void select_graphics_mode(EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics) {
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mode_info = NULL;
    UINTN mode_info_size = 0;
    UINT32 mode_count = graphics->Mode->MaxMode;
    UINT32 max_res_mode = 0;
    UINT32 max_resolution = 0;
    for (UINT32 mode = 0; mode < mode_count; mode++) {
        uefi_call_wrapper(graphics->QueryMode, 4, graphics, mode, &mode_info_size, &mode_info);
        UINT32 resolution = mode_info->HorizontalResolution * mode_info->VerticalResolution;
        if (resolution > max_resolution) {
            backbuffer.width = mode_info->HorizontalResolution;
            backbuffer.height = mode_info->VerticalResolution;
            max_res_mode = mode;
            max_resolution = resolution;
        }
    }
    uefi_call_wrapper(graphics->SetMode, 2, graphics, max_res_mode);
}


EFI_STATUS
allocate_buffer() {
    return uefi_call_wrapper(ST->BootServices->AllocatePool, 3,
        EfiLoaderData, backbuffer.width * backbuffer.height * 4, (VOID**)&backbuffer.pixels);
}


void free_buffer() {
    uefi_call_wrapper(ST->BootServices->FreePool, 1, (VOID*)backbuffer.pixels);
}


void blt_green(EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics) {
    UINT32 resolution = backbuffer.height * backbuffer.width;
    for (UINT32 y = 0; y < resolution; y+= backbuffer.width) {
        for (UINT32 x = 0; x < backbuffer.width; x++) {
            backbuffer.pixels[y + x] = 0x0000ff00;
        }
    }
    uefi_call_wrapper(graphics->Blt, 10, graphics, backbuffer.pixels, EfiBltBufferToVideo,
        0, 0, 0, 0, backbuffer.width, backbuffer.height, 0);
}


EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics = get_graphics();
    select_graphics_mode(graphics);
    if (EFI_SUCCESS == allocate_buffer()) {
        blt_green(graphics);
        free_buffer();
    }
    return EFI_SUCCESS;
}