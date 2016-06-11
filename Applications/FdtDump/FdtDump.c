/* Time-stamp: <2016-06-11 01:02:34 andreiw>
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
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <libfdt.h>

#include <IndustryStandard/Acpi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/EfiShellParameters.h>

#include <Guid/FileInfo.h>
#include <Guid/Acpi.h>
#include <Guid/Fdt.h>

EFI_STATUS
FileSystemSave (
                IN EFI_HANDLE Handle,
                IN CHAR16 *VolSubDir,
                IN CHAR16 *Path,
                IN VOID *Table,
                IN UINTN TableSize
               )
{
  EFI_STATUS Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FsProtocol;
  EFI_FILE_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Dir;
  EFI_FILE_PROTOCOL *File;
  UINTN Size;

  Status = gBS->HandleProtocol (Handle, &gEfiSimpleFileSystemProtocolGuid, (void **) &FsProtocol);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open filesystem: %r\n", Status);
    return Status;
  }

  Status = FsProtocol->OpenVolume (FsProtocol, &Fs);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open volume: %r\n", Status);
    return Status;
  }

  Status = Fs->Open (Fs, &Dir, VolSubDir, EFI_FILE_MODE_CREATE |
                     EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                     EFI_FILE_DIRECTORY);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open '\\%s': %r\n", VolSubDir, Status);
    return Status;
  }

  if (Dir->Open (Dir, &File, Path, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0) == EFI_SUCCESS) {
    /*
     * Delete existing file.
     */
    Print(L"Overwriting existing'\\%s\\%s': %r\n", VolSubDir, Path);
    Status = Dir->Delete (File);
    File = NULL;
    if (Status != EFI_SUCCESS) {
      Print(L"Could not delete existing '\\%s\\%s': %r\n", VolSubDir, Path, Status);
      goto closeDir;
    }
  }

  Status = Dir->Open (Dir, &File, Path, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ |
                      EFI_FILE_MODE_WRITE, 0);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open '\\%s\\%s': %r\n", VolSubDir, Path, Status);
    goto closeDir;
    return Status;
  }

  Size = TableSize;
  Status = File->Write (File, &Size, (void *) Table);
  if (Status != EFI_SUCCESS || Size != TableSize) {
    Print(L"Writing '\\%s\\%s' failed: %r\n", VolSubDir, Path, Status);
  } else {
    File->Flush (File);
  }

  Dir->Close (File);
 closeDir:
  Fs->Close (Dir);
  return Status;
}

VOID *
GetTable (
          IN EFI_GUID *Guid
          )
{
  UINTN i;

  for (i = 0; i < gST->NumberOfTableEntries; i++) {
    if (CompareGuid (&gST->ConfigurationTable[i].VendorGuid, Guid)) {
      return gST->ConfigurationTable[i].VendorTable;
    }
  }

  return NULL;
}

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL *ImageProtocol;
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;
  CHAR16 *VolSubDir;
  VOID *Fdt;

  VolSubDir = L".";
  Status = gBS->HandleProtocol (ImageHandle, &gEfiShellParametersProtocolGuid, (VOID **) &ShellParameters);
  if (Status == EFI_SUCCESS && ShellParameters->Argc > 1) {
    VolSubDir = ShellParameters->Argv[1];
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

  if (fdt_check_header(Fdt) != 0) {
     Print(L"Warning: FDT header not valid\n");
  }

  Print(L"FDT 0x%x bytes\n", fdt_totalsize(Fdt));
  FileSystemSave(ImageProtocol->DeviceHandle, VolSubDir,
                 L"fdt.dtb", Fdt, fdt_totalsize(Fdt));

  Print(L"All done!\n");
  return Status;
}
