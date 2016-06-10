/* Time-stamp: <2016-06-10 00:24:39 andreiw>
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

#include <IndustryStandard/Acpi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/EfiShellParameters.h>

#include <Guid/FileInfo.h>
#include <Guid/Acpi.h>

EFI_STATUS
FileSystemSave (
                IN EFI_HANDLE Handle,
                IN CHAR16 *VolSubDir,
                IN EFI_ACPI_DESCRIPTION_HEADER *Table
               )
{
  EFI_STATUS Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FsProtocol;
  EFI_FILE_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL *Dir;
  EFI_FILE_PROTOCOL *File;
  CHAR16 Path[4 + 1 + 3 + 1];
  UINTN Size;

  Print(L"Table %.4a @ %p (0x%x bytes)\n", &Table->Signature, Table, Table->Length);

  Path[0] = ((CHAR8 *) &Table->Signature)[0];
  Path[1] = ((CHAR8 *) &Table->Signature)[1];
  Path[2] = ((CHAR8 *) &Table->Signature)[2];
  Path[3] = ((CHAR8 *) &Table->Signature)[3];
  Path[4] = L'.';
  Path[5] = L'a';
  Path[6] = L'm';
  Path[7] = L'l';
  Path[8] = L'\0';

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

  Size = Table->Length;
  Status = File->Write (File, &Size, (void *) Table);
  if (Status != EFI_SUCCESS || Size != Table->Length) {
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
  UINTN i;
  EFI_STATUS Status;
  UINTN SdtEntrySize;
  EFI_PHYSICAL_ADDRESS SdtTable;
  EFI_PHYSICAL_ADDRESS SdtTableEnd;
  EFI_ACPI_DESCRIPTION_HEADER *Rsdt;
  EFI_ACPI_DESCRIPTION_HEADER *Xsdt;
  EFI_LOADED_IMAGE_PROTOCOL *ImageProtocol;
  EFI_ACPI_5_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp;
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;
  CHAR16 *VolSubDir;

  EFI_GUID AcpiGuids[2] = {
    EFI_ACPI_20_TABLE_GUID,
    ACPI_10_TABLE_GUID
  };

  VolSubDir = L".";
  Status = gBS->HandleProtocol (ImageHandle, &gEfiShellParametersProtocolGuid, (VOID **) &ShellParameters);
  if (Status == EFI_SUCCESS && ShellParameters->Argc > 1) {
    VolSubDir = ShellParameters->Argv[1];
  }
  Print(L"Dumping tables to '\\%s'\n", VolSubDir);

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (void **) &ImageProtocol);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not get loaded image device handle: %r\n", Status);
    return Status;
  }

  for (i = 0; i < 2; i++) {
    Rsdp = GetTable(&AcpiGuids[i]);
    if (Rsdp != NULL) {
      break;
    }
  }

  if (Rsdp == NULL) {
    Print(L"No ACPI support found\n");
    return Status;
  }

  if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->XsdtAddress;
  } else {
    Xsdt = NULL;
  }

  Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->RsdtAddress;
  if (Xsdt != 0) {
    SdtTable = (UINTN) Xsdt;
    SdtTableEnd = SdtTable + Xsdt->Length;
    SdtEntrySize = sizeof(UINT64);
  } else {
    SdtTable = (UINTN) Rsdt;
    SdtTableEnd = SdtTable + Rsdt->Length;
    SdtEntrySize = sizeof(UINT32);
  }

  for (SdtTable += sizeof(EFI_ACPI_DESCRIPTION_HEADER);
       SdtTable < SdtTableEnd;
       SdtTable += SdtEntrySize) {
    EFI_ACPI_DESCRIPTION_HEADER *TableHeader;

    if (SdtEntrySize == sizeof(UINT32)) {
      TableHeader = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) *(UINT32 *) SdtTable;
    } else {
      TableHeader = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) *(UINT64 *) SdtTable;
    }

    if (TableHeader == NULL) {
      Print(L"<skipping empty SDT entry>\n");
      continue;
    }

    FileSystemSave(ImageProtocol->DeviceHandle, VolSubDir, TableHeader);

    if (TableHeader->Signature == EFI_ACPI_5_1_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
      EFI_ACPI_DESCRIPTION_HEADER *DsdtHeader;
      EFI_ACPI_DESCRIPTION_HEADER *FacsHeader;

      EFI_ACPI_5_1_FIXED_ACPI_DESCRIPTION_TABLE *Fadt = (void *) TableHeader;
      if (TableHeader->Revision >= 3 && Fadt->XDsdt != 0) {
        DsdtHeader = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Fadt->XDsdt;
      } else {
        DsdtHeader = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Fadt->Dsdt;
      }

      if (TableHeader->Revision >= 3 && Fadt->XFirmwareCtrl != 0) {
        FacsHeader = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Fadt->XFirmwareCtrl;
      } else {
        FacsHeader = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Fadt->FirmwareCtrl;
      }

      if (DsdtHeader != NULL) {
        FileSystemSave(ImageProtocol->DeviceHandle, VolSubDir, DsdtHeader);
      }

      if (FacsHeader != NULL) {
        FileSystemSave(ImageProtocol->DeviceHandle, VolSubDir, FacsHeader);
      }
    }
  }

  Print(L"All done!\n");
  return Status;
}
