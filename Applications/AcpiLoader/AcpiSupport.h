/* Time-stamp: <2016-06-10 01:17:29 andreiw>
 *
 * This is based on AcpiSupportDxe, but removes the legacy cruft (and bugs).
 *
 * Copyright (C) 2016 Andrei Evgenievich Warkentin
 * Copyright (c) 2006 - 2014, Intel Corporation. All rights reserved
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */

#ifndef _ACPI_SUPPORT_H_
#define _ACPI_SUPPORT_H_

#include <PiDxe.h>
#include <Guid/Acpi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PcdLib.h>
#include <IndustryStandard/Acpi.h>

//
// Private Driver Data
//
//
// ACPI Table Linked List Signature.
//
#define EFI_ACPI_TABLE_LIST_SIGNATURE SIGNATURE_32 ('E', 'A', 'T', 'L')

//
// ACPI Table Linked List Entry definition.
//
//  Signature must be set to EFI_ACPI_TABLE_LIST_SIGNATURE
//  Link is the linked list data.
//  Version is the versions of the ACPI tables that this table belongs in.
//  Table is a pointer to the table.
//  PageAddress is the address of the pages allocated for the table.
//  NumberOfPages is the number of pages allocated at PageAddress.
//  Handle is used to identify a particular table.
//
typedef struct {
  UINT32                  Signature;
  LIST_ENTRY              Link;
  EFI_ACPI_COMMON_HEADER  *Table;
  EFI_PHYSICAL_ADDRESS    PageAddress;
  UINTN                   NumberOfPages;
  UINTN                   Handle;
} EFI_ACPI_TABLE_LIST;

//
// Containment record for linked list.
//
#define EFI_ACPI_TABLE_LIST_FROM_LINK(_link)  CR (_link, EFI_ACPI_TABLE_LIST, Link, EFI_ACPI_TABLE_LIST_SIGNATURE)

//
// The maximum number of tables this driver supports
//
#define EFI_ACPI_MAX_NUM_TABLES 100

//
// Protocol private structure definition
//
//
// ACPI support protocol instance signature definition.
//
#define EFI_ACPI_SUPPORT_SIGNATURE  SIGNATURE_32 ('S', 'S', 'A', 'E')

//
// ACPI support protocol instance data structure
//
typedef struct {
  EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER  *Rsdp3;                 // Pointer to RSD_PTR structure
  EFI_ACPI_DESCRIPTION_HEADER                   *Xsdt;                  // Pointer to XSDT table header
  EFI_ACPI_3_0_FIXED_ACPI_DESCRIPTION_TABLE     *Fadt3;                 // Pointer to FADT table header
  EFI_ACPI_3_0_FIRMWARE_ACPI_CONTROL_STRUCTURE  *Facs3;                 // Pointer to FACS table header
  EFI_ACPI_DESCRIPTION_HEADER                   *Dsdt3;                 // Pointer to DSDT table header
  LIST_ENTRY                                    TableList;
  UINTN                                         NumberOfTableEntries3;  // Number of ACPI 3.0 tables
  UINTN                                         CurrentHandle;
  BOOLEAN                                       TablesInstalled3;       // ACPI 3.0 tables published
} EFI_ACPI_SUPPORT_INSTANCE;

EFI_STATUS
AcpiSupportConstructor (
                        IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance
                        );

EFI_STATUS
InstallAcpiTable (
                  IN  EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
                  IN  VOID                      *AcpiTableBuffer,
                  IN  UINTN                     AcpiTableBufferSize,
                  OUT UINTN                     *TableKey
                  );

#endif /* _ACPI_SUPPORT_H_ */
