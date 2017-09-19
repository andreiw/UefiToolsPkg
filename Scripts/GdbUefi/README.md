# gdb_uefi.py

Allows loading TianoCore symbols into a GDB remote
debugging session.

Last validated with GDB 7.10.

This is how it works: `GdbSyms.efi` is a dummy binary that
contains the relevant symbols needed by the script
to find and load image symbols. `GdbSyms.efi` does not
need to be present on the target!

    $ gdb /path/to/GdbSyms.dll
    (gdb) target remote ....
    (gdb) source Scripts/gdb_uefi.py
    (gdb) reload-uefi -o /path/to/GdbSyms.dll

N.B: it was noticed that GDB for certain targets behaves strangely
when run without any binary - like assuming a certain physical
address space size and endianness. To avoid this madness and
seing strange bugs, make sure to pass `/path/to/GdbSyms.dll`
when starting gdb.

The `-o` option should be used if you've debugging EFI, where the PE
images were converted from MACH-O or ELF binaries.
