#include <efi.h>
#include <efilib.h>

#include "ascii_bitmap.c"

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
    if (output == NULL) return EFI_INVALID_PARAMETER;
    return uefi_call_wrapper(ST->BootServices->AllocatePool, 3,
        EfiLoaderData, output->width * output->height * 4, (VOID**)&output->backbuffer);
}


void graphics_free_backbuffer(GraphicsOutput* output) {
    if (output == NULL || output->backbuffer == NULL) return;
    uefi_call_wrapper(ST->BootServices->FreePool, 1, (VOID*)output->backbuffer);
}


void graphics_fill_solid_color(GraphicsOutput* output, UINT32 color) {
    if (output == NULL || output->backbuffer == NULL) return;
    UINT32 resolution = output->height * output->width;
    for (UINT32 y = 0; y < resolution; y+= output->width) {
        for (UINT32 x = 0; x < output->width; x++) {
            output->backbuffer[y + x] = color;
        }
    }
}


void graphics_blt(GraphicsOutput* output) {
    if (output == NULL || output->protocol == NULL || output->backbuffer == NULL) return;
    uefi_call_wrapper(output->protocol->Blt, 10, output->protocol, output->backbuffer, EfiBltBufferToVideo,
        0, 0, 0, 0, output->width, output->height, 0);
}


void graphics_draw_char(GraphicsOutput* output, UINT8 character, UINT32 color, INT32 x, INT32 y, UINT8 scale) {
    if (output == NULL || output->protocol == NULL || output->backbuffer == NULL) return;
    if (x + 8 >= output->width || y + 16 >= output->height) return;

    if (character < 32) character = 32;
    character -= 32;

    UINT8 red_offset = output->red_offset;
    UINT8 green_offset = output->green_offset;
    UINT8 blue_offset = output->blue_offset;
    UINT32* backbuffer = output->backbuffer;
    UINT32 color_red = (color >> red_offset) & 255;
    UINT32 color_green = (color >> green_offset) & 255;
    UINT32 color_blue = (color >> blue_offset) & 255;
    UINT32 screen_width = output->width;
    UINT32 screen_height = output->height;
    UINT32 screen_resolution = screen_height * screen_width;
    INT32 bitmap_x_offset = character * 8;
    INT32 bitmap_x_max = bitmap_x_offset + 8;
    INT32 screen_y = y * screen_width;
    INT32 screen_x = x;
    for (UINT32 bitmap_y = 0; bitmap_y < 1792 * 16; bitmap_y += 1792) {
        for (UINT32 bitmap_x = bitmap_x_offset; bitmap_x < bitmap_x_max; bitmap_x++) {
            UINT8 bitmap_value = ascii_bitmap[bitmap_y + bitmap_x];
            INT32 scaled_y = screen_y;
            for (UINT8 i = 0; i < scale; i++) {
                INT32 scaled_x = screen_x;
                for (UINT8 j = 0; j < scale; j++) {
                    if (scaled_y >= 0 && scaled_y < screen_resolution && scaled_x >= 0 && scaled_x < screen_width) {
                        UINT32 pixel = backbuffer[scaled_y + scaled_x];
                        UINT32 pixel_red = (((pixel >> red_offset) & 255) * (256 - bitmap_value) + color_red * bitmap_value) >> 8;
                        UINT32 pixel_green = (((pixel >> green_offset) & 255) * (256 - bitmap_value) + color_green * bitmap_value) >> 8;
                        UINT32 pixel_blue = (((pixel >> blue_offset) & 255) * (256 - bitmap_value) + color_blue * bitmap_value) >> 8;
                        backbuffer[scaled_y + scaled_x] = (pixel_red << red_offset) + (pixel_green << green_offset) + (pixel_blue << blue_offset);
                    }
                    scaled_x++;
                }
                scaled_y += screen_width;
            }
            screen_x += scale;
        }
        screen_y += screen_width * scale;
        screen_x = x;
    }
}