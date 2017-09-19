This is a fixed-up version of edk2's `StdLib/LibC/Uefi`.

NOTE: [`StdExtLib`](../StdExtLib), [`StdLibDevConsole`](../StdLibDevConsole), [`StdLibInteractiveIO`](../StdLibInteractiveIO) and [`StdLibUefi`](../StdLibUefi) all go together.

Differences:
- `^D` is the `VEOF` character, allowing to break out of input.
- Termios init is moved to StdLibDevConsole, where it belongs.
- getopt is now in StdExtLib (sharing the backing implementation for getopt_long).
