This is the NetBSD 'cat' utility, revision 1.47.20.1

Really hacky port. Sorry.

Limitations (mostly of edk2 StdLib implementation):
- '-l' flag is useless (no file locking)
- UEFI Shell pipes don't do anything to StdLib
  programs, so don't bother doing stuff like `stat | cat`
- UEFI Shell redirection is just broken, don't use it.
