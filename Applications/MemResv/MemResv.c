/* Time-stamp: <2017-09-23 00:06:58 andreiw>
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
#include <Pi/PiDxeCis.h>
#include <Library/UtilsLib.h>

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  UINTN Argc;
  CHAR16 **Argv;
  UINTN RangeStart;
  UINTN RangeLength;
  EFI_STATUS Status;
  EFI_MEMORY_TYPE type;
  EFI_DXE_SERVICES *DS = NULL;

  EfiGetSystemConfigurationTable(&gEfiDxeServicesTableGuid, (VOID **) &DS);
  if (DS == NULL) {
    Print(L"This program requires EDK2 DXE Services\n");
    return EFI_ABORTED;
  }

  Status = GetShellArgcArgv(ImageHandle, &Argc, &Argv);
  if (Status != EFI_SUCCESS || Argc < 1) {
    Print(L"This program requires Microsoft Windows.\n"
          "Just kidding...only the UEFI Shell!\n");
    return EFI_ABORTED;
  }

  if (Argc < 3) {
    Print(L"Usage: %s range-start range-pages [mmio]\n", Argv[0]);
    return EFI_INVALID_PARAMETER;
  }

  RangeStart = StrHexToUintn(Argv[1]);
  RangeLength = StrHexToUintn(Argv[2]) << EFI_PAGE_SHIFT;

  if (RangeLength == 0 ||
      (RangeStart + RangeLength) < RangeStart) {
    Print(L"Invalid range passed\n");
    return EFI_INVALID_PARAMETER;
  }

  type = EfiReservedMemoryType;
  if (Argc == 4) {
    if (!StriCmp(Argv[3], L"mmio")) {
      type = EfiMemoryMappedIO;
    } else {
      Print(L"Warning: Unknown range type, assuming reserved\n");
    }
  }

  DS->RemoveMemorySpace(RangeStart, RangeLength);

  //
  // This seems like the only reasonable way to ensure
  // that a range is added if it is absent. Adding
  // EfiGcdMemoryTypeMemoryMappedIo results in
  // an EFI_NOT_FOUND error for AllocatePages
  // and nothing is reported in the memory map.
  //
  Status = DS->AddMemorySpace(EfiGcdMemoryTypeSystemMemory,
                              RangeStart,
                              RangeLength,
                              EFI_MEMORY_UC | EFI_MEMORY_RUNTIME
                              );
  if (Status != EFI_SUCCESS) {
    Print(L"Warning: couldn't add reserved memory space 0x%lx-0x%lx: %r\n",
          RangeStart, RangeStart + RangeLength, Status);
  }

  Status = gBS->AllocatePages(AllocateAddress, type,
                              EFI_SIZE_TO_PAGES(RangeLength), &RangeStart);
  if (Status != EFI_SUCCESS) {
    Print(L"Warning: couldn't allocate reserved memory space 0x%lx-0x%lx: %r\n",
          RangeStart, RangeStart + RangeLength, Status);
  }

  return Status;
}
