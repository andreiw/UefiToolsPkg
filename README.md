UefiToolsPkg
============

Various useful utilities for UEFI.

* FdtDump       - Dump system device tree to storage.
* AcpiDump      - Dump system ACPI tables to storage.
* AcpiLoader    - Load system ACPI tables from storage.
* ShellPlatVars - Set UEFI Shell environment variables
                  based on platform ACPI/SMBIOS
                  configuration.
* MemResv       - Create new memory map entries.
* gdb_uefi.py   - Load TianoCore symbols in gdb.

Various useful libraries for UEFI.

* UtilsLib      - Consumed by the above utilities.
* SoftFloatLib  - Port of SoftFloat-3d to UEFI.

Building
--------

Assuming you have EDK2 (http://www.tianocore.org/edk2/)
all configured and capable of producing builds for your
target architecture:

    $ git clone https://github.com/andreiw/UefiToolsPkg.git
    $ build -p UefiToolsPkg/UefiToolsPkg.dsc

To override architecture and/or toolchain:

    $ build -p UefiToolsPkg/UefiToolsPkg.dsc -a PPC64
    $ build -p UefiToolsPkg/UefiToolsPkg.dsc -a X64 -t GCC49

There's no architecture-specific code.

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

Contact Info
------------

Andrei Warkentin (andrey.warkentin@gmail.com).
