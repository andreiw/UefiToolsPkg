UefiToolsPkg
============

Various useful utilities for UEFI.

Name | Description
---|---
[FdtDump](#fdtdump) | dump system device tree to storage
[AcpiDump](#acpidump) | dump system ACPI tables to storage
[AcpiLoader](#acpiloader) | load system ACPI tables from storage
[ShellPlatVars](#shellplatvars) | set UEFI Shell environment variables based on platform configuration
[MemResv](#memresv) | create new memory map entries
[RangeIsMapped](#rangeismapped) | validates ranges in the memory map
[tinycc](#tinycc) | port of TinyCC to UEFI (X64/AArch64)
[ls](#ls) | Port of NetBSD directory lister
[gdb_uefi.py](#gdb_uefipy) | load TianoCore symbols in gdb

Various useful libraries for UEFI.

Name | Description
---|---
[UtilsLib](#utilslib) | consumed by the above utilities.
[StdLibDevConsole](#stdlibdevconsole) | replacement for StdLib/LibC/Uefi/Devices/Console
[SoftFloatLib](#softfloatlib) | port of SoftFloat-3d to UEFI
[FTSLib](#ftslib) | port of FTS(3) routines, file hierarchy traversal

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

FdtDump
-------

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

AcpiDump
--------

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

The files produced can be loaded by AcpiLoader.

AcpiLoader
----------

AcpiLoader will remove all of the existing ACPI tables
and install tables loaded from the same volume the
tool is on. Every file with an AML (aml) extension
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

ShellPlatVars
-------------

This tool is meant to be run from the UEFI Shell,
and will set the shell environment variables based
on SMBIOS/ACPI/FDT configuration.

For SMBIOS:
* pvar-have-smbios for SMBIOS < 3.0.
* pvar-have-smbios64 for SMBIOS >= 3.0.
* pvar-smbios-bios-vendor for firmware vendor.
* pvar-smbios-bios-ver for firmware version.
* pvar-smbios-bios-data for firmware release data.
* pvar-smbios-manufacturer for system manufacturer.
* pvar-smbios-product for product name.
* pvar-smbios-product-ver for product version.
* pvar-smbios-product-sn for product serial number.
* pvar-smbios-product-familty for product family.

For ACPI:
* pvar-have-acpi for ACPI presence.
* pvar-acpi-\<OemId\> = True
* pvar-acpi-\<OemTableId\> = True
* pvar-acpi-\<Signature\>-rev = \<Revision\>
* pvar-acpi-\<Signature\>-oem-id = \<OemId\>
* pvar-acpi-\<Signature\>-oem-rev = \<OemRevision\>
* pvar-acpi-\<Signature\>-tab-id = \<OemTableId\>

Note: \<OemId\> and \<OemTableId\> are sanitized, replacing
all occurances of ' ' with '_'.

For FDT (not available on IA32, X64, IPF and EBC):
* pvar-have-fdt for FDT presence.
* pvar-fdt-compat-0 = \<root node compatible element 0\>
* ...
* pvar-fdt-compat-9 = \<root node compatible element 9\>

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

MemResv
-------

Allows creating memory map entries of type "Reserved" or
"MMIO". This is useful to "steal" some memory for
OS low-memory testing, or to create some missing
MMIO entries.

The range is specified as base address and number of pages.
All values are hexadecimal, and may be provided with or
without the leading '0x'. The type parameter is optional.

For example, to reserve memory in the range 0x90000000-0x90004FFF:

    fs16:> MemResv.efi 0x90000000 5

To reserve MMIO range 0x91000000-0x910FEFFF:

    fs16:> MemResv.efi 0x91000000 ff mmio

Note: This tool requires Tiano Core, as it leverages
the DXE services.

RangeIsMapped
-------------

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

With '-q', the tool can be used quietly in a script.

    fs1:\> RangeIsMapped.efi -q 10540000 10001
    fs1:\> echo %lasterror%
    0xE

tinycc
------

Yes, you can now build UEFI applications **in UEFI itself**. You
will need the EDK2 headers, or be content with buidling precompiled
sources only.

    fs16:> tcc hello.c
    fs16:> hello.efi
    Hello, World!

See https://github.com/andreiw/tinycc and TCC documentation for more.

ls
--

This is exactly what you think this is. Limitations are highlighted
in [README.md](Applications/ls/README.md).

    fs3:\> ls.efi -l -h tools
    total 632K
    -rwxrwxrwx  1 none  none   62K Mar 14  2017 AcpiDump.efi
    -rwxrwxrwx  1 none  none   72K Mar 14  2017 AcpiLoader.efi

gdb_uefi.py
-----------

Allows loading TianoCore symbols into a GDB remote
debugging session.

Last validated with GDB 7.10.

This is how it works: GdbSyms.efi is a dummy binary that
contains the relevant symbols needed by the script
to find and load image symbols. GdbSyms.efi does not
need to be present on the target!

    $ gdb /path/to/GdbSyms.dll
    (gdb) target remote ....
    (gdb) source Scripts/gdb_uefi.py
    (gdb) reload-uefi -o /path/to/GdbSyms.dll

N.B: it was noticed that GDB for certain targets behaves strangely
when run without any binary - like assuming a certain physical
address space size and endianness. To avoid this madness and
seing strange bugs, make sure to pass /path/to/GdbSyms.dll
when starting gdb.

The -o option should be used if you've debugging EFI, where the PE
images were converted from MACH-O or ELF binaries.

Libraries
---------

### UtilsLib

Various useful routines.

### StdLibDevConsole

This is a replacement for StdLib/LibC/Uefi/Devices/Console, for
linking with "POSIX" applications. If EFI ConOut is graphical, but
StdErr is redirected to a non-graphical console, stderr will always
send to ConOut, not StdErr. This solves the unexpected
behavior of POSIX apps seemingly printing nothing on error on
certain UEFI implementations that always redirect StdErr to serial.

### SoftFloatLib

This gives you an alternate soft-fp implementation that doesn't
rely on compiler intrinsics or arch-specific behavior. On
certain architectures, this library also provides an implementation
for certain missing compiler intrinsics (e.g. __floatunditf and
__fixunstfdi on AArch64).

### FTSLib

Port of fts.c, FTS(3) file traversal routines. These include
fts_open, fts_read, fts_children, and so on. As something
introduced in 4.4BSD, this should have been part of StdLib,
but isn't.

Don't forget to include [Library/FTSLib.h](Include/Library/FTSLib.h) instead of fts.h.

Limitations are highlighted in [README.md](Library/FTSLib/README.md).

Other UEFI Utilities on the Web
-------------------------------

- https://github.com/LongSoft/CrScreenshotDxe
- https://github.com/fpmurphy/UEFI-Utilities-2016

Contact Info
------------

Andrei Warkentin (andrey.warkentin@gmail.com).
