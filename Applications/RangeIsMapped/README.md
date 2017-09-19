# RangeIsMapped

Easily check if a range is described in the UEFI memory map,
either interactively or in a script.

    fs1:\> RangeIsMapped.efi 10540000 10000
    0x10540000-0x1054FFFF is in the memory map
    fs1:\> set lasterror
    lasterror = 0x0
    fs1:\> RangeIsMapped.efi 10540000 10001
    0x10540000-0x10550000 not in memory map (starting at 0x10550000)
    fs1:\> set lasterror
    lasterror = 0xE

With `-q`, the tool can be used quietly in a script.

    fs1:\> RangeIsMapped.efi -q 10540000 10001
    fs1:\> echo %lasterror%
    0xE
