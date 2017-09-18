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

Name | Description
---|---
[FdtDump](#fdtdump) | dump system device tree to storage
[AcpiDump](#acpidump) | dump system ACPI tables to storage
[AcpiLoader](#acpiloader) | load system ACPI tables from storage
[ShellPlatVars](#shellplatvars) | set UEFI Shell variables based on platform configuration
[ShellErrVars](#shellerrvars) | set UEFI Shell variables to look up EFI RETURN_STATUS values
[MemResv](#memresv) | create new memory map entries
[RangeIsMapped](#rangeismapped) | validates ranges in the memory map
[GopTool](#goptool) | Check and manipulate EFI_GRAPHICS_OUTPUT_PROTOCOL instances
[tinycc](#tinycc) | port of TinyCC to UEFI

### FdtDump

FdtDump will dump the device tree blob to the same volume
the tool is located on. This is sometimes useful, but keep
in mind that some UEFI implementations can patch the DTB
on the way out during ExitBootServices, meaning you might
not get the same (correct) data if you dumped the DTB from
the OS proper.

Note: this tool is not available for IA32, X64, IPF and
EBC targets.

An optional parameter specifies the path, relative to the
volume root, where to place the tables. E.g.:

    fs16:> FdtDump.efi
    fs16:> FdtDump.efi MyFunkySystem

### AcpiDump

AcpiDump will dump all of the ACPI tables to the same volume
the tool is located on. This is sometimes useful, but keep
in mind that some UEFI implementations can patch ACPI tables
on the way out during ExitBootServices, meaning you might
not get the same (correct) data if you dumped tables from
the OS proper.

An optional parameter specifies the path, relative to the
volume root, where to place the tables. E.g.:

    fs16:> AcpiDump.efi
    fs16:> AcpiDump.efi MyFunkySystem

The files produced can be loaded by [AcpiLoader](#acpiloader).

### AcpiLoader

AcpiLoader will remove all of the existing ACPI tables
and install tables loaded from the same volume the
tool is on. Every file with an AML (`.aml`) extension
is loaded.

An optional parameter specifies the path, relative to
the volume root, where the tables are loaded from. E.g.:

    fs16:> AcpiLoader.efi
    fs16:> AcpiLoader.efi MyFunkySystem

Features (or limitations, depending on how you look):
* XSDT only.
* No ACPI 1.0.
* No special ordering (i.e. FADT can be in any XSDT slot).
* No 32-bit restrictions on placement.
* 32-bit DSDT and FACS pointers are not supported.

### ShellPlatVars

This tool is meant to be run from the UEFI Shell,
and will set the shell environment variables based
on SMBIOS/ACPI/FDT configuration.

For SMBIOS:
* `pvar-have-smbios` for SMBIOS < 3.0.
* `pvar-have-smbios64` for SMBIOS >= 3.0.
* `pvar-smbios-bios-vendor` for firmware vendor.
* `pvar-smbios-bios-ver` for firmware version.
* `pvar-smbios-bios-data` for firmware release data.
* `pvar-smbios-manufacturer` for system manufacturer.
* `pvar-smbios-product` for product name.
* `pvar-smbios-product-ver` for product version.
* `pvar-smbios-product-sn` for product serial number.
* `pvar-smbios-product-family` for product family.

For ACPI:
* `pvar-have-acpi` for ACPI presence.
* `pvar-acpi-<OemId> = True`
* `pvar-acpi-<OemTableId> = True`
* `pvar-acpi-<Signature>-rev = <Revision>`
* `pvar-acpi-<Signature>-oem-id = <OemId>`
* `pvar-acpi-<Signature>-oem-rev = <OemRevision>`
* `pvar-acpi-<Signature>-tab-id = <OemTableId>`

Note: `<OemId>` and `<OemTableId>` are sanitized, replacing
all occurances of ` ` with `_`.

For FDT (not available on IA32, X64, IPF and EBC):
* `pvar-have-fdt` for FDT presence.
* `pvar-fdt-compat-0 = <root node compatible element 0>`
* ...
* `pvar-fdt-compat-9 = <root node compatible element 9>`

Usage:

    fs16:> ShellPlatVars.efi
    fs16:> set
        path = .\;FS0:\efi\tools\;FS0:\efi\boot\;FS0:\
        profiles = ;Driver1;Install;Debug1;network1;
        uefishellsupport = 3
        uefishellversion = 2.0
        uefiversion = 2.40
        pvar-have-smbios = False
        pvar-have-smbios64 = False
        pvar-have-acpi = False
        pvar-have-fdt = False

This is meant to be used from a shell script. E.g.:

    if x%pvar-have-fdt% eq xTrue then
        echo I am running on a %pvar-fdt-compat-0%
    endif

    if x%pvar-have-acpi% eq xFalse then
        AcpiLoader MyFunkySystem
    endif

    if x%pvar-acpi-DSDT-tab-id% eq xPOWER_NV then
        load fs16:\PowerNV\IODA2HostBridge.efi
    endif

    if x"%pvar-smbios-product-family%" eq x"Whatever This Is" then
        echo I am running on Whatever
    endif

### ShellErrVars

This tool is meant to be run from the UEFI Shell,
and will set shell environment variables to
quickly test for and look up EFI return statuses
and error codes.

Usage:

    fs16:> ShellErrVars.efi [-i]

A few examples of what gets set:

    evar_RETURN_SUCCESS = 0x0
    evar_0x0 = RETURN_SUCCESS
    svar_RETURN_SUCCESS = 0x0
    svar_0x0 = RETURN_SUCCESS
    ...
    evar_RETURN_HTTP_ERROR = 0x23
    evar_0x23 = RETURN_HTTP_ERROR
    svar_RETURN_HTTP_ERROR = 0x8000000000000023
    svar_0x8000000000000023 = RETURN_HTTP_ERROR
    ...
    wvar_RETURN_WARN_WRITE_FAILURE = 0x3
    wvar_0x3 = RETURN_WARN_WRITE_FAILURE
    svar_RETURN_WARN_WRITE_FAILURE = 0x3
    svar_0x3 = RETURN_WARN_WRITE_FAILURE

`svar_` refers to the raw RETURN_STATUS value, while
`evar_` will strip the high error bit, since that's
the way the UEFI Shell `%lasterror%` reports status.

Note: `svar_`, `wvar_` and reverse (value to name) variables
are only set with the `-i` (interactive) flag, as they
are mostly useless in a script setting. Sadly, the UEFI shell is too
dumb to do double expansion.

Examples of use:

    FS1:\> echo %evar_0x0%
    RETURN_SUCCESS
    FS1:\> echo %evar_0xE%
    RETURN_NOT_FOUND
    FS1:\> echo %svar_RETURN_NOT_FOUND%
    0x800000000000000E

In a script:

    if x%lasterror% eq x%evar_RETURN_NOT_FOUND%
        echo Something wasn't found.
    endif

### MemResv

Allows creating memory map entries of type **reserved** or
**MMIO**. This is useful to steal some memory for
OS low-memory testing, or to create some missing
MMIO entries.

The range is specified as base address and number of pages.
All values are hexadecimal, and may be provided with or
without the leading `0x`. The type parameter is optional.

For example, to reserve memory in the range 0x90000000-0x90004FFF:

    fs16:> MemResv.efi 0x90000000 5

To reserve MMIO range 0x91000000-0x910FEFFF:

    fs16:> MemResv.efi 0x91000000 ff mmio

Note: This tool requires Tiano Core, as it uses the DXE services.

### RangeIsMapped

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

### GopTool

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

### tinycc

Yes, you can now build UEFI applications **in UEFI itself**. You
will need the EDK2 headers, or be content with buidling precompiled
sources only. X64 and AArch64 only.

    fs16:> tcc hello.c
    fs16:> hello.efi
    Hello, World!

See https://github.com/andreiw/tinycc and TCC documentation for more.

UNIX Tools
----------

Name | Description
---|---
[ls](#ls) | Port of NetBSD directory lister
[stat](#stat) | Port of NetBSD stat
[cat](#cat) | Port of NetBSD cat
[dd](#d) | Port of NetBSD dd

### ls

This is exactly what you think this is. Limitations are highlighted
in [README.md](Applications/ls/README.md).

    fs3:\> ls.efi -l -h tools
    total 632K
    -rwxrwxrwx  1 none  none   62K Mar 14  2017 AcpiDump.efi
    -rwxrwxrwx  1 none  none   72K Mar 14  2017 AcpiLoader.efi

### stat

Display file status. Limitations are highlighted
in [README.md](Applications/stat/README.md).

    fs3:\> stat -x
      File: "(stdin)"
      Size: 0
      FileType: Character Device
      Mode: (0555/cr-xr-xr-x)
    Access: Wed Dec 31 23:59:59 1969
    Modify: Wed Dec 31 23:59:59 1969

### cat

Catenate and print files. Examples of use and limitations are highlighted
in [README.md](Applications/cat/README.md).

    fs3:\> cat
    ^D

### dd

Convert and copy files. Examples of use and limitations are highlighted
in [README.md](Applications/dd/README.md).

    fs3:\> dd
    ^D
    0+0 records in
    0+0 records out
    0 bytes transferred in 0.347366 secs (0 bytes/sec)

Libraries
---------

Name | Description
---|---
[UtilsLib](#utilslib) | consumed by the above utilities.
[SoftFloatLib](#softfloatlib) | port of SoftFloat-3d to UEFI
[FTSLib](#ftslib) | port of FTS(3) routines, file hierarchy traversal
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

### StdExtLib

This provides functionality that should be in the edk2 StdLib, but isn't, and that
isn't large enough to be a library on its own. This also overrides
some broken behavior in StdLib, so be sure to include
[`Library/StdExtLib.h`](Include/Library/StdExtLib.h) last.

There also a few overrides for StdLib that fix broken StdLib
behavior or add features. These are highly suggested to be used,
and so effectively should be treated like a part of StdExtLib.

What | Replaces | Notes
--- | --- | ---
StdLibUefi | DevUefi | [README.md](Library/StdLibUefi/README.md)
StdLibDevConsole | DevConsole | [README.md](Library/StdLibDevConsole/README.md)
StdLibInteractiveIO | LibIIO | [README.md](Library/StdLibInteractiveIO/README.md)

I ought to upstream at least some of these fixes. Without these fixes, the
NetBSD tools in this collection will work even worse than they do now ;-).

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
