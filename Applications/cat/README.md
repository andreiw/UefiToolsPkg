# NetBSD `cat` utility, revision 1.47.20.1

Really hacky port. Sorry.

**This tool has been only validated with the modified StdLib libraries in this distributon.**

UEFI Shell Redirection and pipes do work. E.g.:

    fs3:\> ls.efi -l | cat.efi
    fs3:\> ls.efi > ls.uni
    fs3:\> ls.efi >a ls.txt
    fs3:\> cat.efi ls.txt
    fs3:\> cat.efi <a ls.txt
    fs3:\> cat.efi < ls.uni

Opened files are expected to be ASCII, while files created with the UEFI Shell are UTF-16.
Either use the `<a` redirector or operate on ASCII text files (which you could create using the `>a`
redirector).

Also, the regular `stdin:`/`stdout:`/`stderr:` devices read and write UTF16 data, and
while the Shell `>a`, `<a` and `|a` redirectors sort-of exist to support ASCII,
these expect the data to be printable NUL-terminated text. Do not use these
to deal with binary data.

To deal with binary data use `-o` for specifying where the `cat` output will go,
and the "narrow" character device aliases `nstdin:`, `nstdout:` and `nstderr:`
with `>`, `<` and `|` redirectors. Never use `>a`, `<a` and `|a`!

Examples:

    fs3:\> cat hello.efi -o out.efi
    fs3:\> cat.efi -o nstdout: nstdin: < hello.efi > out.efi
    fs3:\> cat.efi nstdin: < hello.efi | cat.efi -o nstdout: > out2.efi
    fs3:\> cat.efi -o nstdout: stdin: < text.utf16 > text.ascii

Differences:
- `-o` allow specifying an output file.

Other limitations (mostly of edk2 StdLib implementation):
- `-l` flag is useless (no file locking)
- has a slow memory leak since it can malloc, but never frees.
