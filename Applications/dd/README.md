This is the NetBSD `dd` utility, revision 1.51

Really hacky port. Sorry.

Examples:

    fs3:\> dd if=stdin: of=nstdout: < text.utf16 > text.ascii err 2> dd.errs

Other limitations (mostly of edk2 StdLib implementation):
- No ftruncate - Will not truncate the output file.
- No alt_oio.
- No async.
- No cloexec.
- No direct.
- No dsync.
- No exlock.
- No nofollow.
- No nosigpipe.
- No rsync.
- No search.
- No shlock.
- No sync.
- No MTIO.
