This is a fixed-up version of StdLib/LibC/Uefi/InteractiveIO, the
line discipline "driver" porition of StdLib.

NOTE: StdLibDevConsole, StdLibInteractiveIO and StdLibUefi all go together.

Differences:
- Honor c_cc[VEOF] as WEOF, breaking out of input on ^D.
- Do not treat errors (inside IIO_GetInChar) as WEOF.
- Properly percolate IIO_GetInChar errors through IIO_CanonRead.
- Properly percolate IIO_CanonRead/IIO_NonCanonRead return values through IIO_Read,
  making errors and 0 (e.g. VEOF) work correctly.
- Default VQUIT and VINTR to abort ongoing I/O with EIO.
- No default Termios initialization.
- Fix IIO_Echo calling fo_write with 0 bytes.
- Fix IIO_Write callling fo_write with 0 bytes (not seen in the wild, but...)

It seem InteractiveIO is its own ad-hoc, and very buggy implementation
of a line discipline. That seems like a mistake.
