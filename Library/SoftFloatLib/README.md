# Softfloat-3d for UEFI

This gives you an alternate software floating point implementation that doesn't
rely on compiler intrinsics or arch-specific behavior. On
certain architectures, this library also provides an implementation
for certain missing compiler intrinsics (e.g. `__floatunditf` and
`__fixunstfdi` on AArch64).

The way this is put together, the Softfloat-3d/ directory contains a
completely unmodified Release 3d of the Berkeley SoftFloat package.
This should make you feel better about bugs introduced in the UEFI
packaging, but also allow reasonably simple updates.

[`SoftFloatLib.inf`](SoftFloatLib.inf) is derived from the `SoftFloat-3d/build/Linux-x86_64-GCC`
configuration. [`Include/SoftFloatLib.h`](../../Include/Library/SoftFloatLib.h) is the header
file, and is derived from [`SoftFloat-3d/source/include/softfloat.h`](SoftFloat-3d/source/include/softfloat.h).

See http://www.jhauser.us/arithmetic/SoftFloat.html
