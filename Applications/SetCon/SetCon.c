/*
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
#include <Library/UtilsLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/DevicePathLib.h>

static EFI_STATUS
GetVarPath (
            IN  CHAR16 *Var,
            OUT EFI_DEVICE_PATH_PROTOCOL **OutPath,
            OUT UINT32 *Attributes
            )
{
  EFI_STATUS Status;
  UINTN VariableSize = 0;
  VOID *Variable = NULL;

  Status = gRT->GetVariable (Var, &gEfiGlobalVariableGuid, NULL,
                             &VariableSize, NULL);
  if (Status == EFI_NOT_FOUND) {
    goto out;
  }

  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto out;
  }

  Variable = AllocatePool (VariableSize);
  if (Variable == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto out;
  }

  Status = gRT->GetVariable (Var, &gEfiGlobalVariableGuid,
                             Attributes, &VariableSize, Variable);
  if (EFI_ERROR (Status)) {
    goto out;
  }

  if (!IsDevicePathValid (Variable, VariableSize)) {
    Status = EFI_SUCCESS;
    *OutPath = NULL;
    goto out;
  }

  *OutPath = Variable;
  return EFI_SUCCESS;

 out:
  if (VariableSize != 0) {
    FreePool (Variable);
  }

  return Status;
}

static void
PrintVarPath (
              IN CHAR16 *Var,
              IN EFI_DEVICE_PATH_PROTOCOL *Paths
              )
{
  UINTN PathSize;
  EFI_DEVICE_PATH_PROTOCOL *Path;

  Print (L"%s:\n", Var);

  while ((Path = GetNextDevicePathInstance (&Paths, &PathSize)) != NULL) {
    CHAR16 *PathString = ConvertDevicePathToText (Path, FALSE, FALSE);
    if (PathString == NULL) {
      Print (L"   (corrupted)\n");
    } else {
      Print (L"   %s\n", PathString);
    }

    FreePool (PathString);
    FreePool (Path);
  }
}

static EFI_STATUS
Usage (
       IN CHAR16 *Name
       )
{
  Print (L"Usage: %s [-A] [-i handle-index] [-h handle] [-p path] [-a] VariableName\n", Name);
  return EFI_INVALID_PARAMETER;
}

static EFI_STATUS
AutoPath (
          IN CHAR16 *VarName,
          OUT EFI_DEVICE_PATH_PROTOCOL **Paths
          )
{
  UINTN Count;
  UINTN Index;
  EFI_GUID *Proto;
  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  EFI_DEVICE_PATH_PROTOCOL *Build;

  if (!StrnCmp (VarName, L"ConOut", StrLen(L"ConOut")) ||
      !StrnCmp (VarName, L"ErrOut", StrLen(L"ErrOut"))) {
    Proto = &gEfiSimpleTextOutProtocolGuid;
  } else if (!StrnCmp (VarName, L"ConIn", StrLen(L"ConIn"))) {
    Proto = &gEfiSimpleTextInProtocolGuid;
  } else {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateHandleBuffer(ByProtocol, Proto, NULL,
                                   &Count, &Handles);
  if (Status != EFI_SUCCESS) {
    return EFI_NOT_FOUND;
  }

  Build = NULL;
  for (Index = 0; Index < Count; Index++) {
    EFI_DEVICE_PATH_PROTOCOL *Path;
    EFI_DEVICE_PATH_PROTOCOL *New;

    Status = gBS->HandleProtocol(Handles[Index],
                                 &gEfiDevicePathProtocolGuid,
                                 (VOID**) &Path);
    if (Status != EFI_SUCCESS) {
      Status = EFI_SUCCESS;
      continue;
    }

    New = AppendDevicePathInstance (Build, Path);
    if (New == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto out;
    }

    FreePool (Build);
    Build = New;
  }

  if (Build == NULL) {
    Status = EFI_NOT_FOUND;
  }

 out:
  if (Status != EFI_SUCCESS) {
    if (Build != NULL) {
      FreePool (Build);
    }
  } else {
    *Paths = Build;
  }

  FreePool (Handles);

  return Status;
}


EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  BOOLEAN Set;
  BOOLEAN Append;
  BOOLEAN Auto;
  UINTN Argc;
  CHAR16 **Argv;
  EFI_STATUS Status;
  CHAR16 *VarName;
  UINT32 CurrentAttributes;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_DEVICE_PATH_PROTOCOL *CurrentPath;
  GET_OPT_CONTEXT GetOptContext;

  Status = GetShellArgcArgv(ImageHandle, &Argc, &Argv);
  if (Status != EFI_SUCCESS || Argc < 1) {
    Print(L"This program requires Microsoft Windows.\n"
          "Just kidding...only the UEFI Shell!\n");
    return EFI_ABORTED;
  }

  Set = FALSE;
  Append = FALSE;
  Auto = FALSE;
  Path = NULL;
  INIT_GET_OPT_CONTEXT (&GetOptContext);
  while ((Status = GetOpt (Argc, Argv, L"ihp",
                           &GetOptContext)) == EFI_SUCCESS) {
    switch (GetOptContext.Opt) {
    case L'A':
      if (Set || Append) {
        return Usage (Argv[0]);
      }

      Auto = TRUE;
      Set = TRUE;
      break;
    case L'a':

      if (Auto) {
        return Usage (Argv[0]);
      }

      Append = TRUE;
      break;
    case L'p': {
      if (GetOptContext.OptArg == NULL || Set || Auto) {
        return Usage (Argv[0]);
      }

      Path = ConvertTextToDevicePath (GetOptContext.OptArg);
      if (Path == NULL) {
        Print (L"'%s' is not a valid device path\n");
        Status = EFI_INVALID_PARAMETER;
      }

      Set = TRUE;
      break;
    }
    case L'h': {
      EFI_HANDLE Handle;

      if (GetOptContext.OptArg == NULL || Set || Auto) {
        return Usage (Argv[0]);
      }

      Handle = (void *) StrHexToUintn (GetOptContext.OptArg);

      Path = DuplicateDevicePath (DevicePathFromHandle (Handle));
      if (Path == NULL) {
        Print (L"Could not get path for EFI_HANDLE %p\n",
               Handle);
        return EFI_INVALID_PARAMETER;
      }

      Set = TRUE;
      break;
    }
    case L'i': {
      UINTN HandleIndex;
      EFI_HANDLE Handle;

      if (GetOptContext.OptArg == NULL || Set || Auto) {
        return Usage (Argv[0]);
      }

      HandleIndex = StrHexToUintn (GetOptContext.OptArg);
      Handle = ConvertHandleIndexToHandle (HandleIndex);
      if (Handle == NULL) {
        Print (L"Invalid EFI_HANDLE index %x\n", HandleIndex);
        return EFI_INVALID_PARAMETER;
      }

      Path = DuplicateDevicePath (DevicePathFromHandle (Handle));
      if (Path == NULL) {
        Print (L"Could not get path for EFI_HANDLE index %x\n",
               HandleIndex);
        return EFI_INVALID_PARAMETER;
      }

      Set = TRUE;
      break;
    }
    default:
      Print (L"Unknown option '%c'\n", GetOptContext.Opt);
      return Usage (Argv[0]);
    }
  }

  if (Argc - GetOptContext.OptIndex < 1) {
    return Usage (Argv[0]);
  }

  CurrentPath = NULL;
  VarName = Argv[GetOptContext.OptIndex + 0];
  Status = GetVarPath(VarName, &CurrentPath, &CurrentAttributes);
  if (Status != EFI_SUCCESS) {
    Print (L"Error reading variable '%s': %r\n", VarName, Status);
    goto out;
  }

  if (!Set) {
    if (CurrentPath != NULL) {
      PrintVarPath (VarName, CurrentPath);
    }

    goto out;
  }

  if (Auto) {
    Status = AutoPath (VarName, &Path);
    if (Status != EFI_SUCCESS) {
      Print (L"Could not auto-build path for '%s': %r\n",
             VarName, Status);
      goto out;
    }
  } else if (Append) {
    EFI_DEVICE_PATH_PROTOCOL *NewPath;

    NewPath = AppendDevicePathInstance (CurrentPath, Path);
    if (NewPath == NULL) {
      Print (L"Error appending path: %r\n");
      Status = EFI_OUT_OF_RESOURCES;
      goto out;
    }

    FreePool (Path);
    Path = NewPath;
  }

  Status = gRT->SetVariable (VarName,
                             &gEfiGlobalVariableGuid,
                             CurrentAttributes,
                             GetDevicePathSize (Path),
                             Path);

out:

  if (CurrentPath != NULL) {
    FreePool (CurrentPath);
  }

  if (Path != NULL) {
    FreePool (Path);
  }

  return Status;
}
