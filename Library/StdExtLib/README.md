# StdExtLib

This provides functionality that should be in the edk2 StdLib, but isn't, and that
isn't large enough to be a library on its own. This also overrides
some broken behavior in StdLib, so be sure to include
[`Library/StdExtLib.h`](../../Include/Library/StdExtLib.h) last.

There also a few overrides for StdLib that fix broken StdLib
behavior or add features. These are effectively to be treated
as part of StdExtLib.

What | Replaces | Notes
--- | --- | ---
StdLibUefi | DevUefi | [README.md](../StdLibUefi/README.md)
StdLibDevConsole | DevConsole | [README.md](../StdLibDevConsole/README.md)
StdLibInteractiveIO | LibIIO | [README.md](../StdLibInteractiveIO/README.md)

I ought to upstream at least some of these fixes. Without these fixes, the
NetBSD tools in this collection will work even worse than they do now ;-).
