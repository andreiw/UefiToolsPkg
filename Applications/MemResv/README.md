# MemResv

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
