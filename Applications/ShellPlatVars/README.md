# ShellPlatVars

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
