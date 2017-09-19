UefiToolsPkg
============

This is a Tiano Core (edk2) package with various goodies. The
goal was to make the UEFI environment much more useful
to system hackers. It may be a reduced environment, but
there's no need for it to remain a crippled one. People
make the analogy of UEFI being the 21st century equivalent
of DOS, yet DOS was a vastly more useful environment than
UEFI is today.

Hopefully, one day this will grow into a veritable
distribution of software to be productive even
without a "real OS" around.

* Useful [utilities](#utilities) for developers and admins.
* Ported UNIX [tools](#unix-tools).
* Useful [libraries](#libraries) for developers.
* [Drivers](#drivers) for UEFI.
* [Development tools](#development-tools) for Windows/Linux.

Other tools [around the Web](OTHER.md).

Building
--------

Assuming you have EDK2 (http://www.tianocore.org/edk2/)
all configured and capable of producing builds for your
target architecture:

    $ git clone --recursive https://github.com/andreiw/UefiToolsPkg.git
    $ build -p UefiToolsPkg/UefiToolsPkg.dsc

To override architecture and/or toolchain:

    $ build -p UefiToolsPkg/UefiToolsPkg.dsc -a PPC64
    $ build -p UefiToolsPkg/UefiToolsPkg.dsc -a X64 -t GCC49

Utilities
---------

Name | Description | Notes
---|---|---
[FdtDump](Applications/FdtDump) | dump system device tree to storage | [`README.md`](Applications/FdtDump/README.md)
[AcpiDump](Applications/AcpiDump) | dump system ACPI tables to storage | [`README.md`](Applications/AcpiDump/README.md)
[AcpiLoader](Applications/AcpiLoader) | load system ACPI tables from storage | [`README.md`](Applications/AcpiLoader/README.md)
[ShellPlatVars](Applications/ShellPlatVars) | set UEFI Shell variables based on platform configuration | [`README.md`](Applications/ShellPlatVars/README.md)
[ShellErrVars](Applications/ShellErrVars) | set UEFI Shell variables to look up EFI RETURN_STATUS values | [`README.md`](Applications/ShellErrVars/README.md)
[MemResv](Applications/MemResv) | create new memory map entries | [`README.md`](Applications/MemResv/README.md)
[RangeIsMapped](Applications/RangeIsMapped) | validates ranges in the memory map | [`README.md`](Applications/RangeIsMapped/README.md)
[GopTool](Applications/GopTool) | check and manipulate EFI_GRAPHICS_OUTPUT_PROTOCOL instances | [`README.md`](Applications/GopTool/README.md)
[tinycc](https://github.com/andreiw/tinycc) | port of TinyCC to UEFI | [`README.md`](https://github.com/andreiw/tinycc/blob/mob/README.md#tcc-in-uefi)

UNIX Tools
----------

Name | Description | Notes
---|---|---
[cat](Applications/cat) | Port of NetBSD cat | [`README.md`](Applications/cat/README.md)
[dd](Applications/dd) | Port of NetBSD dd | [`README.md`](Applications/dd/README.md)
[grep](Applications/grep) | Port of NetBSD grep | [`README.md`](Applications/grep/README.md)
[ls](Applications/ls) | Port of NetBSD directory lister | [`README.md`](Applications/ls/README.md)
[stat](Applications/stat) | Port of NetBSD stat | [`README.md`](Applications/stat/README.md)

Libraries
---------

Name | Description
---|---
[UtilsLib](#utilslib) | consumed by the above utilities.
[SoftFloatLib](#softfloatlib) | port of SoftFloat-3d to UEFI
[FTSLib](#ftslib) | port of FTS(3) routines, file hierarchy traversal
[RegexLib](#regexlib) | port of REGEX(3) IEEE Std 1003.2-1992 regular expression routines
[StdExtLib](#stdextlib) | fixes and functionality on top of StdLib

### UtilsLib

Various useful routines.

### SoftFloatLib

This gives you an alternate software floating point implementation that doesn't
rely on compiler intrinsics or arch-specific behavior. On
certain architectures, this library also provides an implementation
for certain missing compiler intrinsics (e.g. `__floatunditf` and
`__fixunstfdi` on AArch64).

See [`README.md`](Library/SoftFloatLib/README.md).

### FTSLib

Port of FTS(3) file traversal routines. These include
`fts_open`, `fts_read`, `fts_children`, and so on. As something
introduced in 4.4BSD, this should have been part of StdLib,
but isn't.

Don't forget to include [`Library/FTSLib.h`](Include/Library/FTSLib.h) instead of `fts.h`.

Limitations are highlighted in [`README.md`](Library/FTSLib/README.md).

### RegexLib

Port of REGEX(3) POSIX.2 regular expression routines.

Don't forget to include [`Library/RegexLib.h`](Include/Library/RegexLib.h) instead of `regex.h`.

Notes are in [`README.md`](Library/RegexLib/README.md).

### StdExtLib

This provides functionality that should be in the edk2 StdLib, but isn't, and that
isn't large enough to be a library on its own. This also overrides
some broken behavior in StdLib, so be sure to include
[`Library/StdExtLib.h`](Include/Library/StdExtLib.h) last.

There also a few overrides for StdLib that fix broken StdLib
behavior or add features. These are effectively to be treated
as part of StdExtLib.

What | Replaces | Notes
--- | --- | ---
StdLibUefi | DevUefi | [README.md](Library/StdLibUefi/README.md)
StdLibDevConsole | DevConsole | [README.md](Library/StdLibDevConsole/README.md)
StdLibInteractiveIO | LibIIO | [README.md](Library/StdLibInteractiveIO/README.md)

I ought to upstream at least some of these fixes. Without these fixes, the
NetBSD tools in this collection will work even worse than they do now ;-).

Drivers
-------

What | Description | Platform | Notes
--- | --- | --- | ---
QemuVideoDxe | GOP driver for QEMU | OVMF, ArmVirtPkg | [README.md](Drivers/QemuVideoDxe/README.md)

Development Tools
-----------------

Name | Description
---|---
[gdb_uefi.py](#gdb_uefipy) | load TianoCore symbols in gdb

### gdb_uefi.py

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

Contact Info
------------

Andrei Warkentin (andrey.warkentin@gmail.com).
