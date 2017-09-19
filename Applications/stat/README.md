# NetBSD `stat` utility, revision 1.38.

Really hacky port. Sorry.

**This tool has been only validated with the modified StdLib libraries in this distributon.**

Examples:

    fs3:\> stat -x
      File: "(stdin)"
      Size: 0
      FileType: Character Device
      Mode: (0555/cr-xr-xr-x)
    Access: Wed Dec 31 23:59:59 1969
    Modify: Wed Dec 31 23:59:59 1969

Limitations (mostly of edk2 StdLib implementation):
- No `st_dev` (%d format)
- No `st_ino` (%i format)
- No `st_nlink` (%l format)
- No `st_uid` (%u format)
- No `st_gid` (%g format)
- No `st_rdev` (%r format)
- No `st_ctime` (%c format)
- No `st_gen` (%v format)
- No symlink support (%Y format)
- No %Z format, as there is no `st_rdev`.
- No string escaping via vis(3)

...otherwise fine?