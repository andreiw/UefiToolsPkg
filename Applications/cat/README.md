This is the NetBSD `cat` utility, revision 1.47.20.1

Really hacky port. Sorry.

UEFI Shell Redirection and pipes do work. E.g.:

    fs3:\> ls.efi -l | cat.efi
    fs3:\> ls.efi > ls.uni
    fs3:\> ls.efi >a ls.txt
    fs3:\> cat.efi ls.txt
    fs3:\> cat.efi <a ls.txt
    fs3:\> cat.efi < ls.uni

Opened files are expected to be ASCII, while files created with the UEFI Shell are UTF-16.
Either use the `<a` redirector or operate on ASCII files (which you could create using the `>a`
redirector). Also, don't redirect binary data: `>` will give you a widened-byte version, while
`<`, <a` and `>a` won't work. The bug is in the way UEFI Shell handles I/O to redirected
console. Use `-o` instead.

Differences:
- `-o` allow specifying an output file, since redirection via UEFI Shell
  will produce wide-character output, and ASCII redirection won't deal
  with non-printable characters (rendering it useless for binary data).

Other limitations (mostly of edk2 StdLib implementation):
- `-l` flag is useless (no file locking)
- has a slow memory leak since it can malloc, but never frees.
