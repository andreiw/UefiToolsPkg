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
#include <Library/UtilsLib.h>
#include <Library/ShellLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/LoadedImage.h>

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  CHAR16 *MapValue;
  CHAR16 *Temp;
  EFI_STATUS Status;
  CONST CHAR16 *Mapping;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_LOADED_IMAGE_PROTOCOL *ImageProtocol;

  Status = gBS->HandleProtocol (ImageHandle,
                                &gEfiLoadedImageProtocolGuid,
                                (void **) &ImageProtocol);
  if (Status != EFI_SUCCESS) {
    return EFI_ABORTED;
  }

  if (gEfiShellProtocol == NULL) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->HandleProtocol(ImageProtocol->DeviceHandle,
                               &gEfiDevicePathProtocolGuid,
                               (VOID **) &Path);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  Mapping = gEfiShellProtocol->GetMapFromDevicePath (&Path);
  if (Mapping == NULL) {
    return EFI_NOT_FOUND;
  }

  MapValue = StrDuplicate (Mapping);
  if (MapValue == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Temp = StrStr(MapValue, L";");
  if (Temp != NULL) {
    *Temp = L'\0';
  }

  Status = ShellSetEnvironmentVariable(L"mapvar", MapValue, TRUE);
  FreePool (MapValue);

  return Status;
}
