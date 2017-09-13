This is a fixed-up version of StdLib/LibC/Uefi.

NOTE: StdLibDevConsole, StdLibInteractiveIO and StdLibUefi all go together.

Differences:
- ^D is the VEOF character, allowing to break out of input.
  Note: you'll need the new LibIIO, too.
- Termios init is moved to Library/StdLibDevConsole/daConsole.c,
  where it belongs.
