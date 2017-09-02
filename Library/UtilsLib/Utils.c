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
#include <Library/SortLib.h>
#include <Library/UtilsLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
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

STATIC INTN EFIAPI
MemoryMapSort (
               IN CONST VOID *Buffer1,
               IN CONST VOID *Buffer2
               )
{
  CONST EFI_MEMORY_DESCRIPTOR *D1 = Buffer1;
  CONST EFI_MEMORY_DESCRIPTOR *D2 = Buffer2;

  if (D1->PhysicalStart < D2->PhysicalStart) {
    return -1;
  } else if (D1->PhysicalStart == D2->PhysicalStart) {
    return 0;
  } else {
    return 1;
  }
}

EFI_STATUS
RangeIsMapped (
               IN UINTN RangeStart,
               IN UINTN RangeLength,
               IN BOOLEAN WarnIfNotFound
               )
{
  UINTN Pages;
  UINTN MapKey;
  UINTN MapSize;
  UINTN Index;
  UINTN RangeNext;
  UINTN RangePages;
  EFI_STATUS Status;
  UINTN DescriptorSize;
  UINT32 DescriptorVersion;
  EFI_MEMORY_DESCRIPTOR *Map;
  EFI_MEMORY_DESCRIPTOR *Next;

  if (RangeLength == 0) {
    Print(L"0x%lx-0x%lx is zero length\n", RangeStart, RangeStart);
    return EFI_INVALID_PARAMETER;
  }

  Map = NULL;
  MapSize = 0;
  Status = gBS->GetMemoryMap(&MapSize, Map, &MapKey,
                             &DescriptorSize, &DescriptorVersion);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"gBS->GetMemoryMap failed: %r\n", Status);
    return EFI_UNSUPPORTED;
  }

  //
  // The UEFI specification advises to allocate more memory for
  // the MemoryMap buffer between successive calls to GetMemoryMap(),
  // since allocation of the new buffer may potentially increase
  // memory map size.
  //
  Pages = EFI_SIZE_TO_PAGES(MapSize) + 1;
  Map = AllocatePages(Pages);
  if (Map == NULL) {
    Print(L"AllocatePages failed\n");
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->GetMemoryMap(&MapSize, Map, &MapKey,
                             &DescriptorSize, &DescriptorVersion);
  if (EFI_ERROR(Status)) {
    Print(L"gBS->GetMemoryMap failed: %r\n", Status);
    FreePages(Map, Pages);
    return Status;
  }

  PerformQuickSort(Map, MapSize / DescriptorSize, DescriptorSize, MemoryMapSort);
  RangePages = EFI_SIZE_TO_PAGES(RangeLength);

  for (RangeNext = RangeStart, Next = Map, Index = 0;
       Index < (MapSize / DescriptorSize) &&
         RangePages != 0;
       Index++, Next = (VOID *)((UINTN)Next + DescriptorSize)) {
    UINTN NextLast = Next->PhysicalStart - 1 + (Next->NumberOfPages * EFI_PAGE_SIZE);

    if (RangeNext < Next->PhysicalStart) {
      break;
    }

    if (RangeNext >= Next->PhysicalStart &&
        RangeNext <= NextLast) {
      UINTN RemPages = (NextLast - RangeNext + 1) / EFI_PAGE_SIZE;
      RemPages = MIN(RemPages, RangePages);
      RangePages -= RemPages;
      RangeNext += RemPages * EFI_PAGE_SIZE;
    }
  }

  FreePages(Map, Pages);

  if (RangePages != 0) {
    if (WarnIfNotFound) {
      Print(L"0x%lx-0x%lx not in memory map (starting at 0x%lx)\n", RangeStart,
            RangeLength - 1 + RangeStart, RangeNext);
    }

    return EFI_NOT_FOUND;
  }

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
