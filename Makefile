# Developed fromcp  https://www.rodsbooks.com/efi-programming/hello.html

ARCH            = $(shell uname -m | sed s,i[3456789]86,ia32,)

OBJS            = beef.o
TARGET          = beef.efi

EFIINC          = ./gnu-efi/inc
EFIINCS         = -I$(EFIINC) -I$(EFIINC)/$(ARCH) -I$(EFIINC)/protocol
LIB             = ./gnu-efi/$(ARCH)/lib
EFILIB          = ./gnu-efi/$(ARCH)/gnuefi
EFI_CRT_OBJS    = $(EFILIB)/crt0-efi-$(ARCH).o
EFI_LDS         = ./gnu-efi/gnuefi/elf_$(ARCH)_efi.lds

CFLAGS          = $(EFIINCS) -fno-stack-protector -fpic \
		  -fshort-wchar -mno-red-zone -Wall
ifeq ($(ARCH),x86_64)
  CFLAGS += -DEFI_FUNCTION_WRAPPER
endif

LDFLAGS         = -nostdlib -znocombreloc -T $(EFI_LDS) -shared \
		  -Bsymbolic -L $(EFILIB) -L $(LIB) $(EFI_CRT_OBJS)

all: $(TARGET)

beef.so: $(OBJS)
	ld $(LDFLAGS) $(OBJS) -o $@ -l efi -l gnuefi

%.efi: %.so
	objcopy -j .text -j .sdata -j .data -j .dynamic \
		-j .dynsym  -j .rel -j .rela -j .reloc \
		--target=efi-app-$(ARCH) $^ $@

clean:
	rm *.efi *.o *.so