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
#include <Library/SortLib.h>
#include <Library/UtilsLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

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

VOID
CleanRangeCheckContext (
                        IN OUT RANGE_CHECK_CONTEXT *Context
                        )
{
  if (!Context->Enabled) {
    return;
  }


  if (Context->MapPages != 0) {
    FreePages(Context->Map, Context->MapPages);
  }

  Context->Enabled = FALSE;
  Context->WarnIfNotFound = FALSE;
  Context->MapSize = 0;
  Context->MapPages = 0;
  Context->DescriptorSize = 0;
  Context->Map = NULL;
}

EFI_STATUS
InitRangeCheckContext (
                       IN BOOLEAN Enabled,
                       IN BOOLEAN WarnIfNotFound,
                       OUT RANGE_CHECK_CONTEXT *Context
                       )
{
  UINTN MapKey;
  EFI_STATUS Status;
  UINT32 DescriptorVersion;

  Context->Enabled = Enabled;
  Context->WarnIfNotFound = WarnIfNotFound;
  Context->MapSize = 0;
  Context->MapPages = 0;
  Context->DescriptorSize = 0;
  Context->Map = NULL;

  if (!Enabled) {
    return EFI_SUCCESS;
  }

  Status = gBS->GetMemoryMap(&Context->MapSize, NULL, &MapKey,
                             &Context->DescriptorSize, &DescriptorVersion);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    Print(L"%a: gBS->GetMemoryMap failed to size: %r\n",
          __FUNCTION__, Status);
    return EFI_UNSUPPORTED;
  }

  do {
    //
    // The UEFI specification advises to allocate more memory for
    // the MemoryMap buffer between successive calls to GetMemoryMap(),
    // since allocation of the new buffer may potentially increase
    // memory map size.
    //
    Context->MapPages = EFI_SIZE_TO_PAGES(Context->MapSize) + 1;
    Context->Map = AllocatePages(Context->MapPages);
    if (Context->Map == NULL) {
      Print(L"%a: AllocatePages failed\n", __FUNCTION__);
      return EFI_OUT_OF_RESOURCES;
    }

    Status = gBS->GetMemoryMap(&Context->MapSize, Context->Map, &MapKey,
                               &Context->DescriptorSize,
                               &DescriptorVersion);
    if (!EFI_ERROR(Status)) {
      break;
    }

    if (EFI_ERROR(Status)) {
      CleanRangeCheckContext(Context);

      if (Status != EFI_BUFFER_TOO_SMALL) {
        Print(L"%a: gBS->GetMemoryMap failed: %r\n", __FUNCTION__,
              Status);
        return Status;
      }
    }
  } while (1);

  PerformQuickSort(Context->Map, Context->MapSize / Context->DescriptorSize,
                   Context->DescriptorSize, MemoryMapSort);

  return EFI_SUCCESS;
}

EFI_STATUS
RangeIsMapped (
               IN RANGE_CHECK_CONTEXT *Context,
               IN UINTN RangeStart,
               IN UINTN RangeLength
               )
{

  UINTN Index;
  UINTN RangeNext;
  UINTN RangePages;
  EFI_MEMORY_DESCRIPTOR *Next;

  if (!Context->Enabled) {
    return EFI_SUCCESS;
  }

  if (RangeLength == 0) {
    Print(L"0x%lx-0x%lx is zero length\n", RangeStart, RangeStart);
    return EFI_INVALID_PARAMETER;
  }

  RangePages = EFI_SIZE_TO_PAGES(RangeLength);

  for (RangeNext = RangeStart, Next = Context->Map, Index = 0;
       Index < (Context->MapSize / Context->DescriptorSize) &&
         RangePages != 0;
       Index++, Next = (VOID *)((UINTN)Next + Context->DescriptorSize)) {
    UINTN NextLast = Next->PhysicalStart - 1 +
      (Next->NumberOfPages * EFI_PAGE_SIZE);

    if (RangeNext < Next->PhysicalStart) {
      break;
    }

    if (RangeNext >= Next->PhysicalStart &&
        RangeNext <= NextLast) {
      UINTN RemPages = EFI_SIZE_TO_PAGES(NextLast - RangeNext + 1);
      RemPages = MIN(RemPages, RangePages);
      RangePages -= RemPages;
      RangeNext += RemPages * EFI_PAGE_SIZE;
    }
  }

  if (RangePages != 0) {
    if (Context->WarnIfNotFound) {
      Print(L"0x%lx-0x%lx not in memory map (starting at 0x%lx)\n",
            RangeStart,
            RangeLength - 1 + RangeStart,
            RangeNext);
    }

    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

