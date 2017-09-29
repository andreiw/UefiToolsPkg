# PciRom

List PCI ROM images for a device given by SBDF.

    fs1:\> PciRom 0 0 2 0
    ROM 0x00009800 bytes
    --------------------
    +0x0: BIOS image (0x9800 bytes)

With the `-s` option and a volume path
(relative to the volume root), will save all images.

    fs1:\> PciRom -s MyFunkySystem 0 0 2 0
    ROM 0x00009800 bytes
    --------------------
    +0x0: BIOS image (0x9800 bytes)
    Saving .\00-BIOS.rom
    fs:1\>
    fs1:\> dir MyFunkySystem
    Directory of: fs1:\MyFunkySystem\
    09/29/2017  03:38 <DIR>         8,192  .
    09/29/2017  03:38 <DIR>             0  ..
    09/29/2017  03:38              38,912  00-BIOS.rom
              1 File(s)      38,912 bytes
                        2 Dir(s)

And yes, it even knows about HP-PA and OF 1275 images.

It would be nice to encode the UEFI image architecture
type, but...meh.
