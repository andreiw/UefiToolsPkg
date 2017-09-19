# FdtDump

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
