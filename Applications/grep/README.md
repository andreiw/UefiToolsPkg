# NetBSD `grep` utility, revision 1.7.

Really hacky port. Sorry.

**This tool has been only validated with the modified StdLib libraries in this distributon.**

Examples:

    fs3:\> memmap | grep RT_Code
    ...

Limitations (mostly of edk2 StdLib implementation):
- No explicit line buffering (`--line-buffered`). Interactive console is implicitly line buffered.
- No gzip or bzip2 support.
