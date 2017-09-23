/* Time-stamp: <2017-09-23 00:04:55 andreiw>
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
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UtilsLib.h>

#include <IndustryStandard/Acpi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>

#include <Guid/FileInfo.h>
#include <Guid/Acpi.h>

#include "AcpiSupport.h"

EFI_ACPI_SUPPORT_INSTANCE AcpiPrivate;

EFI_STATUS
LoadTable(
          IN CHAR16 *VolSubDir,
          IN EFI_FILE_PROTOCOL *Dir,
          IN EFI_FILE_INFO *Info,
          IN EFI_ACPI_SUPPORT_INSTANCE *AcpiPrivate
          )
{
  UINTN Size;
  VOID *Data;
  EFI_STATUS Status;
  EFI_FILE_PROTOCOL *File;
  EFI_ACPI_DESCRIPTION_HEADER *Header;

  Status = Dir->Open (Dir, &File, Info->FileName, EFI_FILE_MODE_READ, 0);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open '\\%s\\': %r\n", VolSubDir, Info->FileName, Status);
    return Status;
  }

  Data = AllocatePool(Info->FileSize);
  if (Data == NULL) {
    Print(L"Could not allocate to read '\\%s\\%s': %r\n",
          VolSubDir, Info->FileName, Status);
    Status = EFI_OUT_OF_RESOURCES;
    goto close;
  }

  Size = Info->FileSize;
  Status = File->Read (File, &Size, Data);
  Header = Data;

  if (Status != EFI_SUCCESS || Size != Info->FileSize) {
    Print(L"Could not read '\\%s\\%s'\n", VolSubDir, Info->FileName);
  } else if (Header->Length == Size) {
    UINTN Key;

    Print(L"'\\%s\\%s' -> %.4a\n", VolSubDir, Info->FileName, &Header->Signature);
    Status = InstallAcpiTable(AcpiPrivate, Header, Header->Length, &Key);
    if (Status != EFI_SUCCESS) {
      Print(L"Could not install table from '\\%s\\%s': %r\n",
            VolSubDir, Info->FileName, Status);
    }
  } else {
    Print(L"Skipping '\\%s\\%s': corrupt ACPI table\n", VolSubDir, Info->FileName);
  }

  FreePool (Data);
 close:
  Dir->Close (File);
  return Status;
}

VOID
LoadTables (
            IN EFI_HANDLE Handle,
            IN CHAR16 *VolSubDir,
            IN EFI_ACPI_SUPPORT_INSTANCE *AcpiPrivate
            )
{
  EFI_STATUS Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FsProtocol;
  EFI_FILE_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Dir;
  EFI_FILE_INFO *Info;
  UINTN Size;

  Status = gBS->HandleProtocol (Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID **) &FsProtocol);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open filesystem: %r\n", Status);
    return;
  }

  Status = FsProtocol->OpenVolume (FsProtocol, &Fs);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open volume: %r\n", Status);
    return;
  }

  Status = Fs->Open (Fs, &Dir, VolSubDir, EFI_FILE_MODE_READ, 0);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open '\\%s': %r\n", VolSubDir, Status);
    return;
  }

  do {
    Size = 0;
    Status = Dir->Read (Dir, &Size, &Size);
    ASSERT (Status == EFI_BUFFER_TOO_SMALL || Size == 0);
    if (Size == 0) {
      /*
       * Done.
       */
      Status = EFI_SUCCESS;
      goto close;
    }

    Info = AllocatePool(Size);
    if (Info == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto close;
    }

    Status = Dir->Read (Dir, &Size, Info);
    if (Status == EFI_SUCCESS) {
      if (StrStr (Info->FileName, L".aml") ||
          StrStr (Info->FileName, L".AML")) {
        Status = LoadTable(VolSubDir, Dir, Info, AcpiPrivate);
      }
    } else {
      Print(L"Could not read '\\%s' direntry: %r\n", VolSubDir, Status);
    }

    FreePool(Info);
  } while (Status == EFI_SUCCESS);

 close:
  Fs->Close (Dir);
}

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  UINTN i;
  UINTN Argc;
  CHAR16 **Argv;
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL *ImageProtocol;
  CHAR16 *VolSubDir;

  EFI_GUID AcpiGuids[2] = {
    EFI_ACPI_20_TABLE_GUID,
    ACPI_10_TABLE_GUID,
  };

  VolSubDir = L".";
  Status = GetShellArgcArgv(ImageHandle, &Argc, &Argv);
  if (Status == EFI_SUCCESS && Argc > 1) {
    VolSubDir = Argv[1];
  }
  Print(L"Loading tables from '\\%s'\n", VolSubDir);

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **) &ImageProtocol);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not get loaded image device handle: %r\n", Status);
    return Status;
  }

  //
  // Uninstall existing tables.
  //
  for (i = 0; i < 2; i++) {
    gBS->InstallConfigurationTable (&AcpiGuids[i], NULL);
  }

  Status = AcpiSupportConstructor(&AcpiPrivate);
  if (Status != EFI_SUCCESS) {
    Print(L"AcpiSupportConstructor failed\n");
    return Status;
  }

  LoadTables(ImageProtocol->DeviceHandle, VolSubDir, &AcpiPrivate);

  Print(L"All done!\n");
  return Status;
}
