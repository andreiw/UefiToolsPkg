/*
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
#include <Library/UefiBootServicesTableLib.h>
#include <Pi/PiDxeCis.h>
#include <Library/UtilsLib.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>

EFI_STATUS
Usage (
       IN CHAR16 *Name
       )
{
  Print(L"Usage: %s [-i index -b [new framebuffer base]]\n", Name);
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )

{
  UINTN Argc;
  CHAR16 **Argv;
  UINTN GopIndex;
  UINTN GopCount = 0;
  UINTN NewFBase = 0;
  UINTN GopSelect = 0;
  BOOLEAN HaveNewFBase = FALSE;
  BOOLEAN GopSelected = FALSE;
  EFI_HANDLE *GopHandles = NULL;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
  EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode = NULL;
  GET_OPT_CONTEXT GetOptContext;
  EFI_STATUS Status;

  Status = GetShellArgcArgv(ImageHandle, &Argc, &Argv);
  if (Status != EFI_SUCCESS || Argc < 1) {
    Print(L"This program requires Microsoft Windows.\n"
          "Just kidding...only the UEFI Shell!\n");
    return EFI_ABORTED;
  }

  INIT_GET_OPT_CONTEXT(&GetOptContext);
  while ((Status = GetOpt(Argc, Argv, L"ib",
                          &GetOptContext)) == EFI_SUCCESS) {
    switch (GetOptContext.Opt) {
    case L'i':
      GopSelect = StrDecimalToUintn(GetOptContext.OptArg);
      GopSelected = TRUE;
      break;
    case L'b':
      NewFBase = StrHexToUintn(GetOptContext.OptArg);
      HaveNewFBase = TRUE;
      break;
    default:
      Print(L"Unknown option '%c'\n", GetOptContext.Opt);
      return Usage(Argv[0]);
    }
  }

  if (HaveNewFBase && !GopSelected) {
    return Usage(Argv[0]);
  }

  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid,
                                   NULL, &GopCount, &GopHandles);
  if (Status != EFI_SUCCESS) {
    Print(L"No GOP handles\n");
    return EFI_SUCCESS;
  }

  if (GopSelected && GopSelect >= GopCount) {
    Print(L"GOP %u doesn't exist\n", GopSelect);
    return EFI_INVALID_PARAMETER;
  }

  for (GopIndex = 0; GopIndex < GopCount; GopIndex++) {
    UINTN Segment;
    UINTN Bus;
    UINTN Device;
    UINTN Func;
    EFI_HANDLE PciDevice;
    EFI_PCI_IO_PROTOCOL *PciIo;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathOrig = NULL;

    Status = gBS->HandleProtocol(GopHandles[GopIndex], &gEfiGraphicsOutputProtocolGuid,
                                 (VOID *) &Gop);

    if (Status != EFI_SUCCESS) {
      continue;
    }

    if (GopSelected && GopSelect != GopIndex) {
      continue;
    }

    if (GopIndex > 0) {
      Print(L"\n");
    }

    Mode = Gop->Mode;
    Print(L"%u: EFI_HANDLE 0x%lx, Mode %u/%u (%ux%u, format %u)\n",
          GopIndex, GopHandles[GopIndex],
          Mode->Mode, Mode->MaxMode,
          Mode->Info->HorizontalResolution,
          Mode->Info->VerticalResolution,
          Mode->Info->PixelFormat);
    Print(L"%u: framebuffer is 0x%lx-0x%lx\n",
          GopIndex, Mode->FrameBufferBase,
          Mode->FrameBufferBase + Mode->FrameBufferSize);

    if (HaveNewFBase) {
      Mode->FrameBufferBase = NewFBase;
      Print(L"%u: framebuffer is 0x%lx-0x%lx [overidden]\n",
            GopIndex, Mode->FrameBufferBase,
            Mode->FrameBufferBase + Mode->FrameBufferSize);

      break;
    }

    Status = gBS->HandleProtocol(GopHandles[GopIndex], &gEfiDevicePathProtocolGuid, (VOID**) &DevicePathOrig);
    if (Status != EFI_SUCCESS) {
      Print(L"%u: probably not a PCI device\n", GopIndex);
      continue;
    }

    DevicePath = DevicePathOrig;
    Status = gBS->LocateDevicePath(&gEfiPciIoProtocolGuid,  &DevicePath, &PciDevice);
    if (Status != EFI_SUCCESS) {
      Print(L"%u: not a PCI device\n", GopIndex);
      continue;
    }

    Status = gBS->HandleProtocol(PciDevice, &gEfiPciIoProtocolGuid, (VOID *) &PciIo);
    if (Status != EFI_SUCCESS) {
      Print(L"%u: PCI I/O protocol should have been found!\n", GopIndex);
      continue;
    }

    Status = PciIo->GetLocation(PciIo, &Segment, &Bus, &Device, &Func);
    Print(L"%u: PCI(e) device %x:%x:%x:%x\n", GopIndex, Segment, Bus, Device, Func);
  }

  gBS->FreePool(GopHandles);
  return EFI_SUCCESS;
}
