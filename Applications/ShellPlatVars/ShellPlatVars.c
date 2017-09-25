/* Time-stamp: <2017-09-24 23:01:33 andreiw>
 * Copyright (C) 2017 Andrei Evgenievich Warkentin
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
#include <Library/UtilsLib.h>
#include <Library/ShellLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>

#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/SmBios.h>
#include <Guid/Acpi.h>
#include <Guid/SmBios.h>

#ifdef WITH_FDT
#include <Guid/Fdt.h>
#include <libfdt.h>
#endif

static RANGE_CHECK_CONTEXT RangeCheck;

static
VOID
SetChar8KeyVal(
               IN CHAR16 *Key,
               IN CONST CHAR8 *Val
               )
{
  UINTN Length;
  UINT16 Val16Length;
  CHAR16 *Val16;

  if (Val == NULL) {
    return;
  }

  Length = AsciiStrLen (Val) + 1;
  if (Length == 0) {
    return;
  }

  Val16Length = Length * sizeof(CHAR16);
  Val16 = AllocatePool (Val16Length);
  if (Val16 == NULL) {
    return;
  }

  UnicodeSPrint(Val16, Val16Length, L"%a", Val);
  ShellSetEnvironmentVariable(Key, Val16, TRUE);
  FreePool (Val16);
}

/*
 * Pulled and modified from ShellPkg LibSmBiosView.c.
 */
static
CHAR8 *
GetSmBiosString (
                 IN SMBIOS_STRUCTURE_POINTER *SmBios,
                 IN UINT16 StringNumber
                 )
{
  UINT16 Index;
  CHAR8 *String;

  //
  // Skip over formatted section.
  //
  String = (CHAR8 *) (SmBios->Raw + SmBios->Hdr->Length);

  //
  // Look through unformated section.
  //
  for (Index = 1; Index <= StringNumber; Index++) {
    if (StringNumber == Index) {
      return String;
    }
    //
    // Skip string.
    //
    for (; *String != 0; String++);
    String++;

    if (*String == 0) {
      //
      // If double NULL then we are done.
      //
      //  Return pointer to next structure in SmBios.
      //  if you pass in a -1 you will always get here.
      //
      SmBios->Raw = (UINT8 *) ++String;
      return NULL;
    }
  }

  return NULL;
}

static
VOID
ParseSmBios (
             IN UINT8 *SmBios, // IN
             IN UINTN Length  // IN
             )
{
  SMBIOS_STRUCTURE_POINTER Index;
  SMBIOS_STRUCTURE_POINTER SmBiosEnd;

  Index.Raw = SmBios;
  SmBiosEnd.Raw = SmBios + Length;

  while (Index.Raw < SmBiosEnd.Raw) {
    if (Index.Hdr->Type == 0) {
      SetChar8KeyVal(L"pvar-smbios-bios-vendor", GetSmBiosString (&Index, 1));
      SetChar8KeyVal(L"pvar-smbios-bios-ver", GetSmBiosString (&Index, 2));
      SetChar8KeyVal(L"pvar-smbios-bios-date", GetSmBiosString (&Index, 3));
    } else if (Index.Hdr->Type == 1) {
      SetChar8KeyVal(L"pvar-smbios-manufacturer", GetSmBiosString (&Index, 1));
      SetChar8KeyVal(L"pvar-smbios-product", GetSmBiosString (&Index, 2));
      SetChar8KeyVal(L"pvar-smbios-product-ver", GetSmBiosString (&Index, 3));
      SetChar8KeyVal(L"pvar-smbios-product-sn", GetSmBiosString (&Index, 4));
      SetChar8KeyVal(L"pvar-smbios-product-sku", GetSmBiosString (&Index, 5));
      SetChar8KeyVal(L"pvar-smbios-product-family", GetSmBiosString (&Index, 6));
    }

    GetSmBiosString (&Index, (UINT16) (-1));
  }
}

UINTN
CalculateSmBios64Length (
                         IN SMBIOS_TABLE_3_0_ENTRY_POINT *SmBios64EntryPoint
                         )
{
  UINT8 *Raw;
  SMBIOS_STRUCTURE_POINTER SmBios;
  UINTN SmBios64TableLength = 0;

  SmBios.Raw = (UINT8 *)(UINTN)(SmBios64EntryPoint->TableAddress);
  while (TRUE) {
    if (SmBios.Hdr->Type == 127) {
      //
      // Reach the end of table type 127.
      //
      SmBios64TableLength += sizeof (SMBIOS_STRUCTURE);
      return SmBios64TableLength;
    }

    Raw = SmBios.Raw;
    //
    // Walk to next structure.
    //
    GetSmBiosString (&SmBios, (UINT16) (-1));
    //
    // Length = Next structure head - this structure head.
    //
    SmBios64TableLength += (UINTN) (SmBios.Raw - Raw);
    if (SmBios64TableLength > SmBios64EntryPoint->TableMaximumSize) {
      //
      // The actual table length exceeds maximum table size,
      // There should be something wrong with SMBIOS table.
      //
      return SmBios64TableLength;
    }
  }
}

static
VOID
SanitizeVarName (
                 IN CHAR16 *Name,
                 IN UINTN Length
                 )
{
  int i;
  for (i = 0; i < Length; i++) {
    if (Name[i] == L' ') {
      Name[i] = '_';
    }
  }
}

static
VOID
SetACPITableVar (
                 IN EFI_ACPI_DESCRIPTION_HEADER *Table
                 )
{
  CHAR16 OemId[6 + 1];
  CHAR16 OemTableId[8 + 1];

  union {
    CHAR16 OemId[sizeof("pvar-acpi-") + 6];
    CHAR16 OemTableId[sizeof("pvar-acpi-") + 8];
    CHAR16 Sig[sizeof("pvar-acpi--rev") + 4];
    CHAR16 SigOem[sizeof("pvar-acpi--oem-id") + 4];
    CHAR16 SigOemRev[sizeof("pvar-acpi--oem-rev") + 4];
    CHAR16 SigOemTable[sizeof("pvar-acpi--tab-id") + 4];
  } vars;

  union {
    CHAR16 Sig[sizeof("0x") + 8];
    CHAR16 SigOemRev[sizeof("0x") + 8];
  } vals;

  UnicodeSPrint(OemId, sizeof(OemId), L"%.6a", (CHAR8 *) Table->OemId);
  UnicodeSPrint(OemTableId, sizeof(OemTableId), L"%.8a", (CHAR8 *) &Table->OemTableId);

  SanitizeVarName(OemId, ARRAY_SIZE(OemId));
  SanitizeVarName(OemTableId, ARRAY_SIZE(OemTableId));

  UnicodeSPrint(vars.OemId, sizeof(vars.OemId), L"pvar-acpi-%s", OemId);
  ShellSetEnvironmentVariable(vars.OemId, L"True", TRUE);

  UnicodeSPrint(vars.OemTableId, sizeof(vars.OemTableId), L"pvar-acpi-%s", OemTableId);
  ShellSetEnvironmentVariable(vars.OemTableId, L"True", TRUE);

  UnicodeSPrint(vars.Sig, sizeof(vars.Sig), L"pvar-acpi-%.4a-rev", (CHAR8 *) &Table->Signature);
  UnicodeSPrint(vals.Sig, sizeof(vals.Sig), L"0x%x", Table->Revision);
  ShellSetEnvironmentVariable(vars.Sig, vals.Sig, TRUE);

  UnicodeSPrint(vars.SigOem, sizeof(vars.SigOem), L"pvar-acpi-%.4a-oem-id", &Table->Signature);
  ShellSetEnvironmentVariable(vars.SigOem, OemId, TRUE);

  UnicodeSPrint(vars.SigOemRev, sizeof(vars.SigOemRev), L"pvar-acpi-%.4a-oem-rev", &Table->Signature);
  UnicodeSPrint(vals.SigOemRev, sizeof(vals.SigOemRev), L"0x%x", Table->OemRevision);
  ShellSetEnvironmentVariable(vars.SigOemRev,  vals.SigOemRev, TRUE);

  UnicodeSPrint(vars.SigOemTable, sizeof(vars.SigOemTable), L"pvar-acpi-%.4a-tab-id", &Table->Signature);
  ShellSetEnvironmentVariable(vars.SigOemTable, OemTableId, TRUE);
}

VOID
SetACPIVars (
             IN EFI_ACPI_5_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp
             )
{
  UINTN SdtEntrySize;
  EFI_PHYSICAL_ADDRESS SdtTable;
  EFI_PHYSICAL_ADDRESS SdtTableEnd;
  EFI_ACPI_DESCRIPTION_HEADER *Rsdt;
  EFI_ACPI_DESCRIPTION_HEADER *Xsdt;

  if (Rsdp->Revision >= EFI_ACPI_2_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION) {
    Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->XsdtAddress;
  } else {
    Xsdt = NULL;
  }

  Rsdt = (EFI_ACPI_DESCRIPTION_HEADER *) (UINTN) Rsdp->RsdtAddress;
  if (Xsdt != 0 && RangeIsMapped(&RangeCheck, (UINTN) Xsdt,
                                 sizeof(EFI_ACPI_DESCRIPTION_HEADER))
      == EFI_SUCCESS) {
    SdtTable = (UINTN) Xsdt;
    SdtTableEnd = SdtTable + Xsdt->Length;
    SdtEntrySize = sizeof(UINT64);
  } else if (Rsdt != 0 && RangeIsMapped(&RangeCheck, (UINTN) Rsdt,
                                        sizeof(EFI_ACPI_DESCRIPTION_HEADER))
             == EFI_SUCCESS) {
    SdtTable = (UINTN) Rsdt;
    SdtTableEnd = SdtTable + Rsdt->Length;
    SdtEntrySize = sizeof(UINT32);
  } else {
    return;
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

    if (TableHeader == NULL ||
        RangeIsMapped(&RangeCheck, (UINTN) TableHeader,
                      sizeof(EFI_ACPI_DESCRIPTION_HEADER))
        != EFI_SUCCESS) {
      continue;
    }

    SetACPITableVar(TableHeader);

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

      if (DsdtHeader != NULL &&
          RangeIsMapped(&RangeCheck, (UINTN) DsdtHeader,
                        sizeof(EFI_ACPI_DESCRIPTION_HEADER))
          == EFI_SUCCESS) {
        SetACPITableVar(DsdtHeader);
      }

      if (FacsHeader != NULL &&
          RangeIsMapped(&RangeCheck, (UINTN) FacsHeader,
                        sizeof(EFI_ACPI_DESCRIPTION_HEADER))
          == EFI_SUCCESS) {
        SetACPITableVar(FacsHeader);
      }
    }
  }
}

#ifdef WITH_FDT
static VOID
HandleFdt (
           VOID
           )
{
  VOID *Fdt;
  CONST CHAR8 *Node;
  CONST CHAR8 *Compatible;
  INT32 Size;
  UINTN Index = 0;
  BOOLEAN HaveFdt = FALSE;

  Fdt = GetTable(&gFdtTableGuid);
  if (Fdt != NULL &&
      RangeIsMapped(&RangeCheck, (UINTN) Fdt,
                    sizeof(struct fdt_header))
      == EFI_SUCCESS &&
      fdt_check_header(Fdt) == 0) {
    HaveFdt = TRUE;
  }

  ShellSetEnvironmentVariable(L"pvar-have-fdt",
                              HaveFdt ? L"True" : L"False", TRUE);

  if (!HaveFdt) {
    return;
  }

  Print(L"Parsing Device Tree\n");

  Node = fdt_getprop (Fdt, 0, "compatible", &Size);
  if (Node == NULL) {
    return;
  }

  for (Compatible = Node; Compatible < Node + Size &&
         *Compatible != '\0' &&
         Index < 10;
       Compatible += 1 + AsciiStrLen (Compatible), Index++) {
    CHAR16 Var[sizeof("pvar-fdt-compat-0")];

    UnicodeSPrint(Var, sizeof(Var), L"pvar-fdt-compat-%u", Index);
    SetChar8KeyVal(Var, Compatible);
  }
}
#endif

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  UINTN i;
  EFI_STATUS Status;
  EFI_ACPI_5_0_ROOT_SYSTEM_DESCRIPTION_POINTER *Rsdp;
  SMBIOS_TABLE_ENTRY_POINT *SmBiosTable;
  SMBIOS_TABLE_3_0_ENTRY_POINT *SmBios64Table;

  EFI_GUID AcpiGuids[2] = {
    EFI_ACPI_20_TABLE_GUID,
    ACPI_10_TABLE_GUID
  };

  Status = ShellInitialize();
  if (EFI_ERROR(Status)) {
    Print(L"This program requires Microsoft Windows.\n"
          "Just kidding...only the UEFI Shell!\n");
    return EFI_ABORTED;
  }

  Status = InitRangeCheckContext(TRUE, FALSE, &RangeCheck);
  if (EFI_ERROR(Status)) {
    Print(L"Couldn't initialize range checking: %r\n", Status);
    return Status;
  }

  for (i = 0; i < 2; i++) {
    Rsdp = GetTable(&AcpiGuids[i]);
    if (Rsdp != NULL) {
      break;
    }
  }

  SmBiosTable = GetTable(&gEfiSmbiosTableGuid);
  ShellSetEnvironmentVariable(L"pvar-have-smbios",
                              SmBiosTable == NULL ? L"False" : L"True", TRUE);
  if (SmBiosTable != NULL &&
      RangeIsMapped(&RangeCheck, (UINTN) SmBiosTable,
                    sizeof(SMBIOS_TABLE_ENTRY_POINT))
      == EFI_SUCCESS &&
      SmBiosTable->TableAddress != 0 &&
      SmBiosTable->TableLength != 0 &&
      RangeIsMapped(&RangeCheck, SmBiosTable->TableAddress,
                    SmBiosTable->TableLength) == EFI_SUCCESS) {
    Print(L"Parsing 32-bit SMBIOS\n");
    ParseSmBios((UINT8 *) (UINTN) SmBiosTable->TableAddress,
                SmBiosTable->TableLength);
  }

  SmBios64Table = GetTable(&gEfiSmbios3TableGuid);
  ShellSetEnvironmentVariable(L"pvar-have-smbios64",
                              SmBios64Table == NULL ? L"False" : L"True", TRUE);
  if (SmBios64Table != NULL &&
      RangeIsMapped(&RangeCheck, (UINTN) SmBios64Table,
                    sizeof(SMBIOS_TABLE_3_0_ENTRY_POINT))
                    == EFI_SUCCESS &&
      SmBios64Table->TableAddress != 0 &&
      RangeIsMapped(&RangeCheck, SmBios64Table->TableAddress,
                    sizeof(UINTN)) == EFI_SUCCESS) {
    Print(L"Parsing 64-bit SMBIOS\n");
    UINTN Length = CalculateSmBios64Length(SmBios64Table);
    ParseSmBios((UINT8 *) (UINTN) SmBios64Table->TableAddress,
                Length);
  }

  ShellSetEnvironmentVariable(L"pvar-have-acpi",
                              Rsdp == NULL ? L"False" : L"True", TRUE);
  if (Rsdp != NULL) {
    Print(L"Parsing ACPI\n");
    SetACPIVars(Rsdp);
  }

#ifdef WITH_FDT
  HandleFdt();
#endif

  CleanRangeCheckContext(&RangeCheck);
  return Status;
}
