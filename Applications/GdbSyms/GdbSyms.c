/* Time-stamp: <2016-06-10 01:44:57 andreiw>
 * Copyright (C) 2016 Andrei Evgenievich Warkentin
 *
 * Bare-minimum GDB symbols needed for reloading symbols.
 * Do not add this into an FV/FD. It is just needed for
 * Scripts/gdb_uefi.py.
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */

#include <PiDxe.h>
#include <Library/UefiLib.h>
#include <Guid/DebugImageInfoTable.h>

EFI_STATUS
EFIAPI
Initialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_SYSTEM_TABLE_POINTER ESTP;
  EFI_DEBUG_IMAGE_INFO_TABLE_HEADER EDIITH;
  EFI_IMAGE_DOS_HEADER EIDH;
  EFI_IMAGE_OPTIONAL_HEADER_UNION EIOHU;
  EFI_IMAGE_DEBUG_DIRECTORY_ENTRY EIDDE;
  EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY EIDCNE;
  EFI_IMAGE_DEBUG_CODEVIEW_RSDS_ENTRY EIDCRE;
  EFI_IMAGE_DEBUG_CODEVIEW_MTOC_ENTRY EIDCME;
  UINTN Dummy =
    (UINTN) &ESTP   |
    (UINTN) &EDIITH |
    (UINTN) &EIDH   |
    (UINTN) &EIOHU  |
    (UINTN) &EIDDE  |
    (UINTN) &EIDCNE |
    (UINTN) &EIDCRE |
    (UINTN) &EIDCME |
     1
    ;
  return !!Dummy & EFI_SUCCESS;
}


