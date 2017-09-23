This is a fixed-up version of edk2's `StdLib/LibC/Uefi/Devices/Console`.

NOTE: [`StdExtLib`](../StdExtLib), [`StdLibDevConsole`](../StdLibDevConsole), [`StdLibInteractiveIO`](../StdLibInteractiveIO) and [`StdLibUefi`](../StdLibUefi) all go together.

NOTE: the redirection/pipe features rely on using a v2 UEFI Shell.

Differences:
- if StdErr isn't redirected via Shell to a file, redirect it out to ConOut.
  This solves the unexpected behavior of POSIX apps seemingly printing
  nothing on error on certain UEFI implementations that always redirect
  StdErr to serial or to noting.
- Set correct permissions and current time on `stdin:`/`stdout:`/`stderr:`.
- Disallow reading on `stdout:` and `stderr:`.
- Make Shell redirection via pipes work (e.g. `ls | cat`).
- Move Termio init from StdLibUefi.
- Detect Shell redirection of StdIn/StdOut/Stderr (file or 'pipe') to
  properly apply termios flag. Interactive I/O behavior is only
  applied if we detect interactive console (e.g. don't want `ECHO`
  when redirecting to a file, don't want `ICANON` if redirecting
  from a file).
- Strip UTF16 BOM tag from StdIn, which is useful when redirecting
  via pipe or file.
- Ignore 0-sized writes.
- Catch wide writes where StrLen doesn't match buffer size, to
  catch `Proto->OutputString` corruption.
- Remove gMD->StdIo usage (no one cares about it at of 2017 for sure).
- add the notion of "narrow" aliases `nstdin:`, `nstdout:` and `nstderr:`.
  These only function  when dealing with redirection (i.e. *never*
  interactive console), and are useful for dealing with binary
  data. e.g. to make a perfect copy.
- Strip UTF16 BOM tag from narrow StdOut/StdErr.
- st_blksize == 1 to make up for lack of st_blocks
  (so you can always st_physsize / st_blksize)
- Support v1 Shell mode (e.g. VMware Fusion). Redirection/pipes
  is a lost cause, but at least the basic use cases work.

Improvements to make:
- VINTR should raise signals.
- `whence` should not be ignored in da_conSeek.
