# NetBSD `ls` utility, revision 1.75.4.1.

Really hacky port. Sorry.

**This tool has been only validated with the modified StdLib libraries in this distributon.**

Limitations (mostly of edk2 StdLib implementation):
- Terminal columns always 80.
- No `-o` option (show flags)
- No `-i` option (show inode)
- No `-q` option (don't hide unprintable chars as `?`)
- No `-B` option (escape non-printing chars as octal)
- No `-b` option (escape non-printing chars as C escapes)
- `-X` is useless (there are no mount points)
- `-W` is useless (there are no whiteouts)
- `-A` is useless (always root)
- uid/gid is 0/0 (none/none)
- block sizes are always 512 or 1024 (via `-k`), UEFI-derived `st_blksize` is not honored
- no minor/major info for devices
- no ctime
- no link counts (all link counts are 1)
- No way to detect directory cycles (so uh, be careful with third-part file system drivers)
- No symlinks support
- Locale decimal delimiter is always `.`