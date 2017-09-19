# GopTool

List and manipulate framebuffer information.

    fs1:\> GopTool
    0: EFI_HANDLE 0xBFFE6518D8, Mode 1/5 (800x600, format 1)
    0: framebuffer is 0x80000000-0x801D4C00
    0: PCI(e) device 1:2:0:0

With `-i`, info on a specific instance can be printed, instead.
`-b` may be used to patch the framebuffer address.

    fs1:\> GopTool -i 0 -b 0x180000000
    0: EFI_HANDLE 0xBFFE6518D8, Mode 1/5 (800x600, format 1)
    0: framebuffer is 0x80000000-0x801D4C00
    0: framebuffer is 0x180000000-0x1801D4C00 [overidden]
