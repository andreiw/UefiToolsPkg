/* Time-stamp: <2017-09-23 00:12:39 andreiw>
 * Copyright (C) 2016 Andrei Evgenievich Warkentin
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UtilsLib.h>
#include <libfdt.h>

#include <Protocol/LoadedImage.h>
#include <Guid/Fdt.h>

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  UINTN Argc;
  CHAR16 **Argv;
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL *ImageProtocol;
  CHAR16 *VolSubDir;
  VOID *Fdt;

  VolSubDir = L".";
  Status = GetShellArgcArgv(ImageHandle, &Argc, &Argv);
  if (Status == EFI_SUCCESS && Argc > 1) {
    VolSubDir = Argv[1];
  }
  Print(L"Dumping FDT to '\\%s'\n", VolSubDir);

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (void **) &ImageProtocol);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not get loaded image device handle: %r\n", Status);
    return Status;
  }

  Fdt = GetTable(&gFdtTableGuid);
  if (Fdt == NULL) {
    Print(L"No Device Tree support found\n");
    return Status;
  }

  Status = RangeIsMapped((UINTN) Fdt, sizeof(struct fdt_header), TRUE);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not validate mapping of FDT header: %r\n", Status);
    return Status;
  }

  if (fdt_check_header(Fdt) != 0) {
    Print(L"Warning: FDT header not valid\n");
  }

  Print(L"FDT 0x%x bytes\n", fdt_totalsize(Fdt));

  FileSystemSave(ImageProtocol->DeviceHandle, VolSubDir,
                 L"fdt.dtb", Fdt, fdt_totalsize(Fdt));

  Print(L"All done!\n");
  return Status;
}
