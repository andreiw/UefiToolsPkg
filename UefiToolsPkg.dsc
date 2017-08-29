#
# Time-stamp: <2017-03-13 13:10:16 andreiw>
# Copyright (C) 2016 Andrei Evgenievich Warkentin
#
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

[Defines]
  PLATFORM_NAME                  = UefiToolsPkg
  PLATFORM_GUID                  = 1234dade-8b6e-4e45-b773-1b27cbda3e06
  PLATFORM_VERSION               = 0.01
  DSC_SPECIFICATION              = 0x00010006
  OUTPUT_DIRECTORY               = Build/UefiToolsPkg
  SUPPORTED_ARCHITECTURES        = IA32|X64|ARM|AARCH64|IPF|EBC|PPC64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

  DEFINE DEBUG_ENABLE_OUTPUT      = FALSE       # Set to TRUE to enable debug output
  DEFINE DEBUG_PRINT_ERROR_LEVEL  = 0x80000040  # Flags to control amount of debug output
  DEFINE DEBUG_PROPERTY_MASK      = 0

[BuildOptions]
  #
  # PPC64 only (https://github.com/andreiw/ppc64le-edk2):
  #
  # You can also build as elfv1, although the artifacts
  # produced are not interchangeable (and won't mix
  # on purpose due to different COFF architecture value).
  #
  *_*_PPC64_ABI_FLAGS   = elfv2

[PcdsFeatureFlag]

[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|$(DEBUG_PROPERTY_MASK)
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|$(DEBUG_PRINT_ERROR_LEVEL)

[LibraryClasses]
  #
  # These are the libraries provided.
  #
  UtilsLib|UefiToolsPkg/Library/UtilsLib/UtilsLib.inf
  SoftFloatLib|UefiToolsPkg/Library/SoftFloatLib/SoftFloatLib.inf
  #
  # Everything else below is a dependency.
  #
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  !if $(DEBUG_ENABLE_OUTPUT)
    DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
    DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  !else   ## DEBUG_ENABLE_OUTPUT
    DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  !endif  ## DEBUG_ENABLE_OUTPUT
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf
  #
  # Shell lib pulls these dependencies.
  #
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf

[LibraryClasses.ARM,LibraryClasses.AARCH64,LibraryClasses.PPC64]
  FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf

[Components]
  UefiToolsPkg/Applications/GdbSyms/GdbSyms.inf
  UefiToolsPkg/Applications/AcpiDump/AcpiDump.inf
  UefiToolsPkg/Applications/AcpiLoader/AcpiLoader.inf
  UefiToolsPkg/Applications/ShellPlatVars/ShellPlatVars.inf
  UefiToolsPkg/Applications/MemResv/MemResv.inf

[Components.ARM,Components.AARCH64,Components.PPC64]
  UefiToolsPkg/Applications/FdtDump/FdtDump.inf
