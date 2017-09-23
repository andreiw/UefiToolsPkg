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
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>

typedef struct ERR_VAR {
  CHAR8 *Name;
  RETURN_STATUS Value;
} ERR_VAR;

#define EFI_ERRORS                              \
  EFI_ERROR_D(RETURN_SUCCESS)                   \
  EFI_ERROR_D(RETURN_LOAD_ERROR)                \
  EFI_ERROR_D(RETURN_INVALID_PARAMETER)         \
  EFI_ERROR_D(RETURN_UNSUPPORTED)               \
  EFI_ERROR_D(RETURN_BAD_BUFFER_SIZE)           \
  EFI_ERROR_D(RETURN_BUFFER_TOO_SMALL)          \
  EFI_ERROR_D(RETURN_NOT_READY)                 \
  EFI_ERROR_D(RETURN_DEVICE_ERROR)              \
  EFI_ERROR_D(RETURN_WRITE_PROTECTED)           \
  EFI_ERROR_D(RETURN_OUT_OF_RESOURCES)          \
  EFI_ERROR_D(RETURN_VOLUME_CORRUPTED)          \
  EFI_ERROR_D(RETURN_VOLUME_FULL)               \
  EFI_ERROR_D(RETURN_NO_MEDIA)                  \
  EFI_ERROR_D(RETURN_MEDIA_CHANGED)             \
  EFI_ERROR_D(RETURN_NOT_FOUND)                 \
  EFI_ERROR_D(RETURN_ACCESS_DENIED)             \
  EFI_ERROR_D(RETURN_NO_RESPONSE)               \
  EFI_ERROR_D(RETURN_NO_MAPPING)                \
  EFI_ERROR_D(RETURN_TIMEOUT)                   \
  EFI_ERROR_D(RETURN_NOT_STARTED)               \
  EFI_ERROR_D(RETURN_ALREADY_STARTED)           \
  EFI_ERROR_D(RETURN_ABORTED)                   \
  EFI_ERROR_D(RETURN_ICMP_ERROR)                \
  EFI_ERROR_D(RETURN_TFTP_ERROR)                \
  EFI_ERROR_D(RETURN_PROTOCOL_ERROR)            \
  EFI_ERROR_D(RETURN_INCOMPATIBLE_VERSION)      \
  EFI_ERROR_D(RETURN_SECURITY_VIOLATION)        \
  EFI_ERROR_D(RETURN_CRC_ERROR)                 \
  EFI_ERROR_D(RETURN_END_OF_MEDIA)              \
  EFI_ERROR_D(RETURN_END_OF_FILE)               \
  EFI_ERROR_D(RETURN_INVALID_LANGUAGE)          \
  EFI_ERROR_D(RETURN_COMPROMISED_DATA)          \
  EFI_ERROR_D(RETURN_HTTP_ERROR)                \
  EFI_ERROR_D(RETURN_WARN_UNKNOWN_GLYPH)        \
  EFI_ERROR_D(RETURN_WARN_DELETE_FAILURE)       \
  EFI_ERROR_D(RETURN_WARN_WRITE_FAILURE)        \
  EFI_ERROR_D(RETURN_WARN_BUFFER_TOO_SMALL)     \
  EFI_ERROR_D(RETURN_WARN_STALE_DATA)           \
  EFI_ERROR_D(RETURN_WARN_FILE_SYSTEM)

static ERR_VAR mVars[] = {
#define EFI_ERROR_D(x) { #x, x },
EFI_ERRORS
};

EFI_STATUS
Usage (
       IN CHAR16 *Name
       )
{
  Print(L"Usage: %s [-i]\n", Name);
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
EFIAPI
UefiMain (
          IN EFI_HANDLE ImageHandle,
          IN EFI_SYSTEM_TABLE *SystemTable
          )
{
  UINTN i;
  UINTN Argc;
  CHAR16 **Argv;
  EFI_STATUS Status;
  EFI_STATUS Status2;
  BOOLEAN InteractiveVars;
  GET_OPT_CONTEXT GetOptContext;
  CHAR16 ValString[5 + 2 + 16 + 1]; /* 0x[16 hex chars]\0 or evar_0x[16 hex chars]\0 */
  CHAR16 NameString[5 + 30 + 1]; /* evar_RETURN_XXX\0 or RETURN_XXX\0 */

  Status = ShellInitialize();
  Status2 = GetShellArgcArgv(ImageHandle, &Argc, &Argv);
  if (EFI_ERROR(Status) || EFI_ERROR(Status2)) {
    Print(L"This program requires Microsoft Windows.\n"
          "Just kidding...only the UEFI Shell!\n");
    return EFI_ABORTED;
  }

  InteractiveVars = FALSE;
  INIT_GET_OPT_CONTEXT(&GetOptContext);
  while ((Status = GetOpt(Argc,
                          Argv, NULL,
                          &GetOptContext)) == EFI_SUCCESS) {
    switch (GetOptContext.Opt) {
    case L'i':
      InteractiveVars = TRUE;
      break;
    default:
      Print(L"Unknown option '%c'\n", GetOptContext.Opt);
      return Usage(Argv[0]);
    }
  }

  for (i = 0; i < ARRAY_SIZE(mVars); i++) {
    CHAR8 *p;
    RETURN_STATUS x;
    ERR_VAR *var = &mVars[i];

    if (var->Value == RETURN_SUCCESS) {
      p = "evar";
      x = 0;
    } else if ((var->Value & MAX_BIT) != 0) {
      p = "evar";
      x = MAX_BIT;
    } else if (InteractiveVars) {
      p = "wvar";
      x = 0;
    } else {
      /*
       * Skip warning RETURN_STATUSes as those aren't useful
       * in a script setting (not reflected via %lasterror%).
       */
      continue;
    }

    UnicodeSPrint(NameString, sizeof(NameString), L"%a_%a", p, var->Name);
    UnicodeSPrint(ValString, sizeof(ValString), L"0x%lx", var->Value ^ x);
    ShellSetEnvironmentVariable(NameString, ValString, TRUE);

    if (InteractiveVars) {
      /*
       * Value -> Name aliases are not useful in script use.
       */
      UnicodeSPrint(NameString, sizeof(NameString), L"%a", var->Name);
      UnicodeSPrint(ValString, sizeof(ValString), L"%a_0x%lx", p, var->Value ^ x);
      ShellSetEnvironmentVariable(ValString, NameString, TRUE);

      /*
       * Neither are the full status codes.
       */
      UnicodeSPrint(NameString, sizeof(NameString), L"svar_%a", var->Name);
      UnicodeSPrint(ValString, sizeof(ValString), L"0x%lx", var->Value);
      ShellSetEnvironmentVariable(NameString, ValString, TRUE);

      UnicodeSPrint(NameString, sizeof(NameString), L"%a", var->Name);
      UnicodeSPrint(ValString, sizeof(ValString), L"svar_0x%lx", var->Value);
      ShellSetEnvironmentVariable(ValString, NameString, TRUE);
    }
  }

  return EFI_SUCCESS;
}
