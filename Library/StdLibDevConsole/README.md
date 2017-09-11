This is a fixed-up version of StdLib/LibC/Uefi/Devices/Console.

Differences:
- If EFI ConOut is graphical, but StdErr is redirected to a
  non-graphical console, stderr will always send to ConOut, not StdErr.
  This solves the unexpected behavior of POSIX apps seemingly printing
  nothing on error on certain UEFI implementations that always redirect
  StdErr to serial.
- Set correct permissions and current time on stdin:/stdout:/stderr:.
- Disallow reading on stdout: and stderr:.