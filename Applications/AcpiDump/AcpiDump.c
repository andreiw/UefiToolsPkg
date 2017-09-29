/* Time-stamp: <2017-09-28 23:31:23 andreiw>
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

#include <IndustryStandard/Acpi.h>
#include <Protocol/LoadedImage.h>

#include <Guid/Acpi.h>

static RANGE_CHECK_CONTEXT RangeCheck;

static
EFI_STATUS
TableSave (
           IN EFI_HANDLE Handle,
           IN CHAR16 *VolSubDir,
           IN EFI_ACPI_DESCRIPTION_HEADER *Table
           )
{
  EFI_STATUS Status;
  static unsigned Index = 0;
  CHAR16 Path[3 + 4 + 1 + 3 + 1];

  if (Table == NULL) {
    Print(L"<skipping empty SDT entry\n");
    return EFI_NOT_FOUND;
  }

  Status = RangeIsMapped(&RangeCheck, (UINTN) Table, sizeof(EFI_ACPI_DESCRIPTION_HEADER));
  if (Status != EFI_SUCCESS) {
    Print(L"<could not validate mapping of table header: %r>\n", Status);
    return Status;
  }

  Print(L"Table %.4a @ %p (0x%x bytes)\n", &Table->Signature, Table, Table->Length);

  Status = RangeIsMapped(&RangeCheck, (UINTN) Table, Table->Length);
  if (Status != EFI_SUCCESS) {
    Print(L"<could not validate mapping of full table: %r>\n", Status);
    return Status;
  }

  Index = Index % 100;
  Path[0] = '0' + Index / 10;
  Path[1] = '0' + Index % 10;
  Path[2] = '-';
  Path[3] = ((CHAR8 *) &Table->Signature)[0];
  Path[4] = ((CHAR8 *) &Table->Signature)[1];
  Path[5] = ((CHAR8 *) &Table->Signature)[2];
  Path[6] = ((CHAR8 *) &Table->Signature)[3];
  Path[7] = L'.';
  Path[8] = L'a';
  Path[9] = L'm';
  Path[10] = L'l';
  Path[11] = L'\0';
  Index++;

  Status = FileSystemSave(Handle, VolSubDir, Path, Table, Table->Length);
  if (Status != EFI_SUCCESS) {
    //
    // FileSystemSave already does sufficient logging. We only
    // return failure if there was something wrong with the
    // table itself.
    //
    return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
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
  UINTN SdtEntrySize;
  EFI_PHYSICAL_ADDRESS SdtTable;
  EFI_PHYSICAL_ADDRESS SdtTableEnd;
  EFI_ACPI_DESCRIPTION_HEADER *Rsdt;
  EFI_ACPI_DESCRIPTION_HEADER *Xsdt;
  EFI_LOADED_IMAGE_PROTOCOL *ImageProtocol;
  EFI_ACPI_5_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp;
  CHAR16 *VolSubDir;

  EFI_GUID AcpiGuids[2] = {
    EFI_ACPI_20_TABLE_GUID,
    ACPI_10_TABLE_GUID
  };

  VolSubDir = L".";
  Status = GetShellArgcArgv(ImageHandle, &Argc, &Argv);
  if (Status == EFI_SUCCESS && Argc > 1) {
    VolSubDir = Argv[1];
  }

  Status = InitRangeCheckContext(TRUE, TRUE, &RangeCheck);
  if (EFI_ERROR(Status)) {
    Print(L"Couldn't initialize range checking: %r\n", Status);
    return Status;
  }

  Print(L"Dumping tables to '\\%s'\n", VolSubDir);

  Status = gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid,
                                (void **) &ImageProtocol);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not get loaded image device handle: %r\n", Status);
    goto done;
  }

  for (i = 0; i < 2; i++) {
    Rsdp = GetTable(&AcpiGuids[i]);
    if (Rsdp != NULL) {
      break;
    }
  }

  if (Rsdp == NULL) {
    Print(L"No ACPI support found\n");
    goto done;
  }

  if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->XsdtAddress;
  } else {
    Xsdt = NULL;
  }

  Status = EFI_NOT_FOUND;
  Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->RsdtAddress;
  if (Xsdt != NULL &&
      (Status = RangeIsMapped(&RangeCheck, (UINTN) Xsdt,
                              sizeof(EFI_ACPI_DESCRIPTION_HEADER)))
      == EFI_SUCCESS) {
    SdtTable = (UINTN) Xsdt;
    SdtTableEnd = SdtTable + Xsdt->Length;
    SdtEntrySize = sizeof(UINT64);
  } else if (Rsdt != NULL &&
             (Status = RangeIsMapped(&RangeCheck, (UINTN) Rsdt,
                                     sizeof(EFI_ACPI_DESCRIPTION_HEADER)))
             == EFI_SUCCESS) {
    SdtTable = (UINTN) Rsdt;
    SdtTableEnd = SdtTable + Rsdt->Length;
    SdtEntrySize = sizeof(UINT32);
  } else {
    Print(L"No valid RSDT/XSDT: %r\n", Status);
    goto done;
  }

  Status = RangeIsMapped(&RangeCheck, SdtTable, SdtTableEnd - SdtTable);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not validate RSDT/XSDT mapping: %r\n", Status);
    goto done;
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

    Status = TableSave(ImageProtocol->DeviceHandle, VolSubDir, TableHeader);

    if (Status == EFI_SUCCESS &&
        TableHeader->Signature == EFI_ACPI_5_1_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE) {
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
        TableSave(ImageProtocol->DeviceHandle, VolSubDir, DsdtHeader);
      } else {
        Print(L"No DSDT\n");
      }

      if (FacsHeader != NULL) {
        TableSave(ImageProtocol->DeviceHandle, VolSubDir, FacsHeader);
      } else {
        Print(L"No FACS\n");
      }
    }
  }

  Print(L"All done!\n");

done:
  CleanRangeCheckContext(&RangeCheck);
  return Status;
}
