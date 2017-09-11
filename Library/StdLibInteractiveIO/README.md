This is a fixed-up version of StdLib/LibC/Uefi/InteractiveIO.

Differences:
- Honor c_cc[VEOF] as WEOF, breaking out of input.
- Do not treat errors (inside IIO_GetInChar) as WEOF.
- Properly percolate IIO_GetInChar errors through IIO_CanonRead.
- Properly percolate IIO_CanonRead/IIO_NonCanonRead return values through IIO_Read,
  making errors and 0 (e.g. VEOF) work correctly.
- Default VQUIT and VINTR to abort ongoing I/O with EIO.
