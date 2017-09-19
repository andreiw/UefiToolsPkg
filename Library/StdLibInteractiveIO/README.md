This is a fixed-up version of edk2's `StdLib/LibC/Uefi/InteractiveIO`, the
line discipline porition of StdLib.

NOTE: [`StdExtLib`](../StdExtLib), [`StdLibDevConsole`](../StdLibDevConsole), [`StdLibInteractiveIO`](../StdLibInteractiveIO) and [`StdLibUefi`](../StdLibUefi) all go together.

Differences:
- Honor `c_cc[VEOF]` as `WEOF`, breaking out of input on `^D`.
- Do not treat errors (inside `IIO_GetInChar`) as `WEOF`.
- Properly percolate `IIO_GetInChar` errors through `IIO_CanonRead`.
- Properly percolate `IIO_CanonRead`/`IIO_NonCanonRead` return values through `IIO_Read`,
  making errors and 0 (e.g. `VEOF`) work correctly.
- Default `VQUIT` and `VINTR` to abort ongoing I/O with `EIO`.
- No default Termios initialization.
- Fix `IIO_Echo` calling `fo_write` with 0 bytes.
- Fix `IIO_Write` callling `fo_write` with 0 bytes (not seen in the wild, but...)
- Fix `IIO_Write` incorrectly treating CharLen as signed size, leading to crashes
  and unpredictable behavior on writes of binary data through IIO.
- Fix `IIO_Write` to allow printing wide NULs.
- Drop UTF8 support entirely. "POSIX" side is taken to be single-byte only. This
  may seem like loss of functionality, but in practice it will make, when
  combined with special "narrow" stdin/stdout variants and redirection, the UNIX
  tools work as expected on binary data (without corrupting it).

It seem InteractiveIO is its own ad-hoc, and very buggy implementation
of a line discipline. That seems like a mistake. Many issues exist
because I/O is done through the UEFI Shell and that expects UTF16-coded
data (the weird Shell 'narrow' `|a` `<a` and `>a` are just broken as they
expect _printable_ NUL-terminated UTF16).
