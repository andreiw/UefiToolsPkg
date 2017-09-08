This is a port of FreeBSD fts.c, 05d8276  on Feb 10, 2013 (https://github.com/lattera/freebsd/)

Really hacky port. Sorry.

Limitations (mostly of edk2 StdLib implementation):
- FTS_NOCHDIR is forced (no fchdir)
- No smartness around avoiding extra stat calls (no st_nlink)
- No st_dev, st_ino (no cycle detection, among other things)
- No graceful dealing with non-ASCII file names (d_name is wide).
