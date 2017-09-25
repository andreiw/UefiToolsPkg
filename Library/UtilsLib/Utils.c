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
#include <Library/BaseMemoryLib.h>
#include <Library/UtilsLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/EfiShellInterface.h>
#include <Protocol/ShellParameters.h>
#include <Guid/FileInfo.h>

EFI_STATUS
GetOpt(IN UINTN Argc,
       IN CHAR16 **Argv,
       IN CHAR16 *OptionsWithArgs,
       IN OUT GET_OPT_CONTEXT *Context)
{
  UINTN Index;
  UINTN SkipCount;

  if (Context->OptIndex >= Argc ||
      *Argv[Context->OptIndex] != L'-') {
    return EFI_END_OF_MEDIA;
  }

  if (*(Argv[Context->OptIndex] + 1) == L'\0') {
    /*
     * A lone dash is used to signify end of options list.
     *
     * Like above, but we want to skip the dash.
     */
    Context->OptIndex++;
    return EFI_END_OF_MEDIA;
  }

  SkipCount = 1;
  Context->Opt = *(Argv[Context->OptIndex] + 1);

  if (OptionsWithArgs != NULL) {
    UINTN ArgsLen = StrLen(OptionsWithArgs);

    for (Index = 0; Index < ArgsLen; Index++) {
      if (OptionsWithArgs[Index] == Context->Opt) {
        if (*(Argv[Context->OptIndex] + 2) != L'\0') {
          /*
           * Argument to the option may immediately follow
           * the option (not separated by space).
           */
          Context->OptArg = Argv[Context->OptIndex] + 2;
        } else if (Context->OptIndex + 1 < Argc &&
                   *(Argv[Context->OptIndex + 1]) != L'-') {
          /*
           * If argument is separated from option by space, it
           * cannot look like an option (i.e. begin with a dash).
           */
          Context->OptArg = Argv[Context->OptIndex + 1];
          SkipCount++;
        } else {
          /*
           * No argument. Maybe it was optional? Up to the caller
           * to decide.
           */
          Context->OptArg = NULL;
        }

        break;
      }
    }
  }

  Context->OptIndex += SkipCount;
  return EFI_SUCCESS;
}

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

  Status = gBS->HandleProtocol(Handle, &gEfiSimpleFileSystemProtocolGuid,
                               (void **) &FsProtocol);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open filesystem: %r\n", Status);
    return Status;
  }

  Status = FsProtocol->OpenVolume(FsProtocol, &Fs);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open volume: %r\n", Status);
    return Status;
  }

  Status = Fs->Open(Fs, &Dir, VolSubDir, EFI_FILE_MODE_CREATE |
                     EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                     EFI_FILE_DIRECTORY);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open '\\%s': %r\n", VolSubDir, Status);
    return Status;
  }

  if (Dir->Open(Dir, &File, Path, EFI_FILE_MODE_READ |
                EFI_FILE_MODE_WRITE, 0) == EFI_SUCCESS) {
    /*
     * Delete existing file.
     */
    Print(L"Overwriting existing'\\%s\\%s': %r\n", VolSubDir, Path);
    Status = Dir->Delete(File);
    File = NULL;
    if (Status != EFI_SUCCESS) {
      Print(L"Could not delete existing '\\%s\\%s': %r\n", VolSubDir, Path, Status);
      goto closeDir;
    }
  }

  Status = Dir->Open(Dir, &File, Path, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ |
                      EFI_FILE_MODE_WRITE, 0);
  if (Status != EFI_SUCCESS) {
    Print(L"Could not open '\\%s\\%s': %r\n", VolSubDir, Path, Status);
    goto closeDir;
    return Status;
  }

  Size = TableSize;
  Status = File->Write(File, &Size, (void *) Table);
  if (Status != EFI_SUCCESS || Size != TableSize) {
    Print(L"Writing '\\%s\\%s' failed: %r\n", VolSubDir, Path, Status);
  } else {
    File->Flush(File);
  }

  Dir->Close(File);
 closeDir:
  Fs->Close(Dir);
  return Status;
}

VOID *
GetTable (
          IN EFI_GUID *Guid
          )
{
  UINTN i;

  for (i = 0; i < gST->NumberOfTableEntries; i++) {
    if (CompareGuid(&gST->ConfigurationTable[i].VendorGuid, Guid)) {
      return gST->ConfigurationTable[i].VendorTable;
    }
  }

  return NULL;
}

CHAR16
CharToUpper (
             IN CHAR16 Char
             )
{
  if (Char >= L'a' && Char <= L'z') {
    return (CHAR16) (Char - (L'a' - L'A'));
  }

  return Char;
}

INTN
StriCmp (
         IN CONST CHAR16 *FirstString,
         IN CONST CHAR16 *SecondString
         )
{
  CHAR16 UpperFirstString;
  CHAR16 UpperSecondString;

  UpperFirstString  = CharToUpper(*FirstString);
  UpperSecondString = CharToUpper(*SecondString);
  while ((*FirstString != '\0') && (UpperFirstString == UpperSecondString)) {
    FirstString++;
    SecondString++;
    UpperFirstString  = CharToUpper(*FirstString);
    UpperSecondString = CharToUpper(*SecondString);
  }

  return UpperFirstString - UpperSecondString;
}

CHAR16 *
StrDuplicate (
              IN CONST CHAR16 *Src
              )
{
  CHAR16 *Dest;
  UINTN Size;

  Size = StrSize (Src);
  Dest = AllocateZeroPool (Size);

  if (Dest != NULL) {
    CopyMem (Dest, Src, Size);
  }

  return Dest;
}

EFI_STATUS
GetShellArgcArgv(
                 IN  EFI_HANDLE ImageHandle,
                 OUT UINTN *Argcp,
                 OUT CHAR16 ***Argvp
                 )
{
  EFI_STATUS Status;
  EFI_SHELL_PARAMETERS_PROTOCOL *EfiShellParametersProtocol;
  EFI_SHELL_INTERFACE           *EfiShellInterface;

  Status = gBS->OpenProtocol(ImageHandle,
                             &gEfiShellParametersProtocolGuid,
                             (VOID **)&EfiShellParametersProtocol,
                             ImageHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL
                             );
  if (!EFI_ERROR(Status)) {
    /*
     * Shell 2.0 interface.
     */
    *Argcp = EfiShellParametersProtocol->Argc;
    *Argvp = EfiShellParametersProtocol->Argv;
    return EFI_SUCCESS;
  }

  Status = gBS->OpenProtocol(ImageHandle,
                             &gEfiShellInterfaceGuid,
                             (VOID **)&EfiShellInterface,
                             ImageHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL
                             );
  if (!EFI_ERROR(Status)) {
    /*
     * 1.0 interface.
     */
    *Argcp = EfiShellInterface->Argc;
    *Argvp = EfiShellInterface->Argv;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}
