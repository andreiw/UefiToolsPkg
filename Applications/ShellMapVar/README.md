# ShellMapVar

This tool will set the `mapvar` Shell environment
variable to the device mapping the tool is run
from.

Usage:

    Shell> ShellMapVar.efi
    Shell> %mapvar%
    fs0:\>

This is primarily interesting for writing
robust UEFI Shell startup scripts, since many
commands and tools don't work correctly without
a mapping and the actual mapping depends on the
mappings present.

Example:

    ShellMapVar.efi
    %mapvar%
    load path\to\my\driver.efi
