/* Time-stamp: <2016-06-11 01:03:15 andreiw>
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
#include <Protocol/EfiShellParameters.h>

EFI_STATUS
Usage (
       IN CHAR16 *Name
       )
{
  Print(L"Usage: %s [-q] range-start range-end\n", Name);
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  BOOLEAN Quiet;
  UINTN RangeStart;
  UINTN RangeLength;
  EFI_STATUS Status;
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;
  GET_OPT_CONTEXT GetOptContext;

  Status = gBS->HandleProtocol(ImageHandle, &gEfiShellParametersProtocolGuid,
                               (VOID **) &ShellParameters);
  if (Status != EFI_SUCCESS || ShellParameters->Argc < 1) {
    Print(L"This program requires Microsoft Windows. Just kidding...only the UEFI Shell!\n");
    return EFI_ABORTED;
  }

  Quiet = FALSE;
  INIT_GET_OPT_CONTEXT(&GetOptContext);
  while ((Status = GetOpt(ShellParameters->Argc,
                          ShellParameters->Argv, NULL,
                          &GetOptContext)) == EFI_SUCCESS) {
    switch (GetOptContext.Opt) {
    case L'q':
      Quiet = TRUE;
      break;
    default:
      Print(L"Unknown option '%c'\n", GetOptContext.Opt);
      return Usage(ShellParameters->Argv[0]);
    }
  }

  if (ShellParameters->Argc - GetOptContext.OptIndex < 2) {
    return Usage(ShellParameters->Argv[0]);
  }

  RangeStart = StrHexToUintn(ShellParameters->Argv[GetOptContext.OptIndex + 0]);
  RangeLength = StrHexToUintn(ShellParameters->Argv[GetOptContext.OptIndex + 1]);

  if (RangeLength == 0 ||
      (RangeStart + RangeLength) < RangeStart) {
    Print(L"Invalid range passed\n");
    return EFI_INVALID_PARAMETER;
  }

  Status = RangeIsMapped(RangeStart, RangeLength, !Quiet);
  if (!Quiet) {
    if (Status == EFI_SUCCESS) {
      Print(L"0x%lx-0x%lx is in the memory map\n", RangeStart,
            RangeLength - 1 + RangeStart);
    } else if (Status != EFI_NOT_FOUND) {
      Print(L"Error: %r\n", Status);
    }
  }

  return Status;
}
