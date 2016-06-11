/* Time-stamp: <2016-06-11 01:08:12 andreiw>
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
#include <Library/BaseMemoryLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Guid/FileInfo.h>

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
