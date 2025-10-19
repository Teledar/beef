#include <efi.h>
#include <efilib.h>

#include "graphics.c"

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    EFI_GRAPHICS_OUTPUT_PROTOCOL* protocol = graphics_get_protocol();
    GraphicsOutput graphics = graphics_select_resolution(protocol);
    if (EFI_SUCCESS != graphics_allocate_backbuffer(&graphics)) return EFI_SUCCESS;

    graphics_fill_solid_color(&graphics, 0x0000ff00);

    EFI_GUID guid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL* keyboard;
    // TODO: Should we use an attribute other than GET_PROTOCOL?
    uefi_call_wrapper(ST->BootServices->OpenProtocol, 6, ST->ConsoleInHandle, &guid, 
        (VOID**)&keyboard, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    EFI_KEY_DATA keystroke;
    
    graphics_draw_char(&graphics, 65, 0xffffff, -8, 16, 3);
    graphics_blt(&graphics);

    while (keystroke.Key.UnicodeChar != 0x17) {
        uefi_call_wrapper(keyboard->ReadKeyStrokeEx, 2, keyboard, &keystroke);
        if (keystroke.Key.UnicodeChar != 0) {
            graphics_fill_solid_color(&graphics, (UINT32)keystroke.Key.UnicodeChar);
            graphics_blt(&graphics);
        }
    }

    graphics_free_backbuffer(&graphics);
    
    return EFI_SUCCESS;
}