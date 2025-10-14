#include <efi.h>
#include <efilib.h>

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* protocol;
    UINT32 width;
    UINT32 height;
    UINT8 red_offset;
    UINT8 green_offset;
    UINT8 blue_offset;
    UINT32* backbuffer;
} GraphicsOutput;


EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics_get_protocol() {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* protocol = NULL;
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    uefi_call_wrapper(ST->BootServices->LocateProtocol, 3, &guid, NULL, (VOID**)&protocol);
    return protocol;
}


static UINT8 find_pixel_offset(UINT32 mask) {
    UINT8 offset = 0;
    mask >>= 8;
    while (mask > 0) {
        offset += 8;
        mask >>= 8;
    }
    return offset;
}


GraphicsOutput graphics_select_resolution(EFI_GRAPHICS_OUTPUT_PROTOCOL* protocol) {
    GraphicsOutput output;
    output.protocol = protocol;
    if (protocol == NULL) return output;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* mode_info = NULL;
    UINTN mode_info_size = 0;
    UINT32 mode_count = protocol->Mode->MaxMode;
    UINT32 max_res_mode = 0;
    UINT32 max_resolution = 0;
    for (UINT32 mode = 0; mode < mode_count; mode++) {
        uefi_call_wrapper(protocol->QueryMode, 4, protocol, mode, &mode_info_size, &mode_info);
        UINT32 resolution = mode_info->PixelsPerScanLine * mode_info->VerticalResolution;
        if (resolution > max_resolution) {
            output.width = mode_info->PixelsPerScanLine;
            output.height = mode_info->VerticalResolution;
            switch (mode_info->PixelFormat) {
                case PixelBlueGreenRedReserved8BitPerColor: {
                    output.red_offset = 16;
                    output.green_offset = 8;
                    output.blue_offset = 0;
                    break;
                }
                case PixelBitMask: {
                    output.red_offset = find_pixel_offset(mode_info->PixelInformation.RedMask);
                    output.green_offset = find_pixel_offset(mode_info->PixelInformation.GreenMask);
                    output.blue_offset = find_pixel_offset(mode_info->PixelInformation.BlueMask);
                    break;
                }
                default: {
                    output.red_offset = 0;
                    output.green_offset = 8;
                    output.blue_offset = 16;
                }
            }
            max_res_mode = mode;
            max_resolution = resolution;
        }
    }
    uefi_call_wrapper(protocol->SetMode, 2, protocol, max_res_mode);
    return output;
}


EFI_STATUS
graphics_allocate_backbuffer(GraphicsOutput* output) {
    return uefi_call_wrapper(ST->BootServices->AllocatePool, 3,
        EfiLoaderData, output->width * output->height * 4, (VOID**)&output->backbuffer);
}


void graphics_free_backbuffer(GraphicsOutput* output) {
    uefi_call_wrapper(ST->BootServices->FreePool, 1, (VOID*)output->backbuffer);
}


void graphics_blt_green(GraphicsOutput* output) {
    UINT32 resolution = output->height * output->width;
    for (UINT32 y = 0; y < resolution; y+= output->width) {
        for (UINT32 x = 0; x < output->width; x++) {
            output->backbuffer[y + x] = 0x0000ff00;
        }
    }
    if (output->protocol == NULL) return;
    uefi_call_wrapper(output->protocol->Blt, 10, output->protocol, output->backbuffer, EfiBltBufferToVideo,
        0, 0, 0, 0, output->width, output->height, 0);
}