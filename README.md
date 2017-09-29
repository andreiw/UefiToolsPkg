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
[ShellMapVar](Applications/ShellMapVar) | set `%mapvar%` with own current device mapping | [`README.md`](Applications/ShellMapVar/README.md)
[MemResv](Applications/MemResv) | create new memory map entries | [`README.md`](Applications/MemResv/README.md)
[RangeIsMapped](Applications/RangeIsMapped) | validates ranges in the memory map | [`README.md`](Applications/RangeIsMapped/README.md)
[GopTool](Applications/GopTool) | check and manipulate EFI_GRAPHICS_OUTPUT_PROTOCOL instances | [`README.md`](Applications/GopTool/README.md)
[SetCon](Applications/SetCon) | check and manipulate console device variables | [`README.md`](Applications/SetCon/README.md)
[PciRom](Applications/PciRom) | list and save PCI ROM images | [`README.md`](Applications/RomPci/README.md)
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

Name | Description | Notes
---|---|---
[UtilsLib](Library/UtilsLib) | consumed by the above utilities | None
[SoftFloatLib](Library/SoftFloatLib) | port of SoftFloat-3d to UEFI | [`README.md`](Library/SoftFloatLib/README.md)
[FTSLib](Library/FTSLib) | port of FTS(3) routines, file hierarchy traversal | [`README.md`](Library/FTSLib/README.md)
[RegexLib](Library/RegexLib) | port of REGEX(3) IEEE Std 1003.2-1992 regular expression routines | [`README.md`](Library/RegexLib/README.md)
[StdExtLib](Library/StdExtLib) | fixes and functionality on top of StdLib |  [`README.md`](Library/StdExtLib/README.md)

Drivers
-------

What | Description | Platform | Notes
--- | --- | --- | ---
[QemuVideoDxe](Drivers/QemuVideoDxe) | GOP driver for QEMU | OVMF, ArmVirtPkg | [`README.md`](Drivers/QemuVideoDxe/README.md)

Development Tools
-----------------

Name | Description | Notes
---|---|---
[gdb_uefi.py](Scripts/GdbUefi) | load TianoCore symbols in gdb | [`README.md`](Scripts/GdbUefi/README.md)

Contact Info
------------

Andrei Warkentin (andrey.warkentin@gmail.com).
