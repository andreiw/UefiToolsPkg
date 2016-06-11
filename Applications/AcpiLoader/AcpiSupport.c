/* Time-stamp: <2016-06-10 01:17:18 andreiw>
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

#include "AcpiSupport.h"

#define  OEM_ID           "LOADED"
#define  OEM_TABLE_ID     "_LOADED_"
#define  OEM_REVISION     0x1337
#define  CREATOR_ID       "LOAD"
#define  CREATOR_REVISION 0xfeed

UINTN mEfiAcpiMaxNumTables = EFI_ACPI_MAX_NUM_TABLES;

EFI_STATUS
AddTableToList (
                IN  EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
                IN  VOID                      *Table,
                IN  BOOLEAN                   Checksum,
                OUT UINTN                    *Handle
                );
EFI_STATUS
RemoveTableFromList (
                     IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
                     IN UINTN                     Handle
                     );
EFI_STATUS
AcpiPlatformChecksum (
                      IN VOID       *Buffer,
                      IN UINTN      Size,
                      IN UINTN      ChecksumOffset
                      );
EFI_STATUS
ChecksumCommonTables (
                      IN OUT EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance
                      );

/**
   This function returns a table specified by an index if it exists.

   The function returns a buffer containing the table that the caller must free.
   The function also returns a handle used to identify the table for update or
   deletion using the SetAcpiTable function.

   @param AcpiSupportInstance EFI_ACPI_SUPPORT_INSTANCE.
   @param Index       Zero-based index of the table to retrieve.
   @param Table       Returned pointer to the table.
   @param Handle      Handle of the table, used in updating tables.

   @retval EFI_SUCCESS             The function completed successfully.
   @retval EFI_NOT_FOUND           The requested table does not exist.

**/
EFI_STATUS
GetAcpiTable (
              IN  EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
              IN  INTN                      Index,
              OUT VOID                      **Table,
              OUT UINTN                     *Handle
              )

{
  INTN                  TempIndex;
  LIST_ENTRY            *CurrentLink;
  LIST_ENTRY            *StartLink;
  EFI_ACPI_TABLE_LIST       *CurrentTable;

  //
  // Check for invalid input parameters
  //
  ASSERT (AcpiSupportInstance);
  ASSERT (Table);
  ASSERT (Handle);

  //
  // Find the table
  //
  CurrentLink = AcpiSupportInstance->TableList.ForwardLink;
  StartLink   = &AcpiSupportInstance->TableList;
  for (TempIndex = 0; (TempIndex < Index) && (CurrentLink != StartLink) && (CurrentLink != NULL); TempIndex++) {
    CurrentLink = CurrentLink->ForwardLink;
  }

  if (TempIndex != Index || CurrentLink == StartLink) {
    return EFI_NOT_FOUND;
  }
  //
  // Get handle
  //
  CurrentTable  = EFI_ACPI_TABLE_LIST_FROM_LINK (CurrentLink);
  *Handle       = CurrentTable->Handle;

  //
  // Copy the table
  //
  *Table = AllocateCopyPool (CurrentTable->Table->Length, CurrentTable->Table);
  ASSERT (*Table);

  return EFI_SUCCESS;
}

/**
   This function adds, removes, or updates ACPI tables.  If the address is not
   null and the handle value is null, the table is added.  If both the address and
   handle are not null, the table at handle is updated with the table at address.
   If the address is null and the handle is not, the table at handle is deleted.

   @param  AcpiSupportInstance  EFI_ACPI_SUPPORT_INSTANCE.
   @param  Table                Pointer to a table.
   @param  Checksum             Boolean indicating if the checksum should be calculated.
   @param  Handle               Handle of the table.

   @return EFI_SUCCESS             The function completed successfully.
   @return EFI_INVALID_PARAMETER   Both the Table and *Handle were NULL.
   @return EFI_ABORTED             Could not complete the desired request.

**/
EFI_STATUS
SetAcpiTable (
              IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
              IN VOID                      *Table OPTIONAL,
              IN BOOLEAN                    Checksum,
              IN OUT UINTN                 *Handle
              )
{
  UINTN                     SavedHandle;
  EFI_STATUS                Status;

  //
  // Check for invalid input parameters
  //
  ASSERT (AcpiSupportInstance);
  ASSERT (Handle != NULL);

  //
  // Initialize locals
  //
  //
  // Determine desired action
  //
  if (*Handle == 0) {
    if (Table == NULL) {
      //
      // Invalid parameter combination
      //
      return EFI_INVALID_PARAMETER;
    } else {
      //
      // Add table
      //
      Status = AddTableToList (AcpiSupportInstance, Table, Checksum, Handle);
    }
  } else {
    if (Table != NULL) {
      //
      // Update table
      //
      //
      // Delete the table list entry
      //
      Status = RemoveTableFromList (AcpiSupportInstance, *Handle);
      if (EFI_ERROR (Status)) {
        //
        // Should not get an error here ever, but abort if we do.
        //
        return EFI_ABORTED;
      }
      //
      // Set the handle to replace the table at the same handle
      //
      SavedHandle                         = AcpiSupportInstance->CurrentHandle;
      AcpiSupportInstance->CurrentHandle  = *Handle;

      //
      // Add the table
      //
      Status = AddTableToList (AcpiSupportInstance, Table, Checksum, Handle);

      //
      // Restore the saved current handle
      //
      AcpiSupportInstance->CurrentHandle = SavedHandle;
    } else {
      //
      // Delete table
      //
      Status = RemoveTableFromList (AcpiSupportInstance, *Handle);
    }
  }

  if (EFI_ERROR (Status)) {
    //
    // Should not get an error here ever, but abort if we do.
    //
    return EFI_ABORTED;
  }
  //
  // Done
  //
  return EFI_SUCCESS;
}

/**
   This function publishes  the ACPI tables by installing EFI
   configuration table entries for them.

   @param  AcpiSupportInstance EFI_ACPI_SUPPORT_INSTANCE

   @return EFI_SUCCESS  The function completed successfully.
   @return EFI_ABORTED  The function could not complete successfully.

**/
EFI_STATUS
PublishTables (
               IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance
               )
{
  EFI_STATUS Status;

  //
  // Do checksum again because Dsdt/Xsdt is updated.
  //
  ChecksumCommonTables (AcpiSupportInstance);

  //
  // Add the RSD_PTR to the system table and store that we have installed the
  // tables.
  //
  if (!AcpiSupportInstance->TablesInstalled3) {
    Status = gBS->InstallConfigurationTable (&gEfiAcpiTableGuid, AcpiSupportInstance->Rsdp3);
    if (EFI_ERROR (Status)) {
      return EFI_ABORTED;
    }

    AcpiSupportInstance->TablesInstalled3 = TRUE;
  }

  return EFI_SUCCESS;
}

/**
   Installs an ACPI table into the XSDT.
   Note that the ACPI table should be checksumed before installing it.
   Otherwise it will assert.

   @param  AcpiSupportInstance  EFI_ACPI_SUPPORT_INSTANCE
   @param  AcpiTableBuffer      A pointer to a buffer containing the ACPI table to be installed.
   @param  AcpiTableBufferSize  Specifies the size, in bytes, of the AcpiTableBuffer buffer.
   @param  TableKey             Reurns a key to refer to the ACPI table.

   @return EFI_SUCCESS            The table was successfully inserted.
   @return EFI_INVALID_PARAMETER  Either AcpiTableBuffer is NULL, TableKey is NULL, or AcpiTableBufferSize
   and the size field embedded in the ACPI table pointed to by AcpiTableBuffer
   are not in sync.
   @return EFI_OUT_OF_RESOURCES   Insufficient resources exist to complete the request.
   @retval EFI_ACCESS_DENIED      The table signature matches a table already
   present in the system and platform policy
   does not allow duplicate tables of this type.

**/
EFI_STATUS
InstallAcpiTable (
                  IN  EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
                  IN  VOID                      *AcpiTableBuffer,
                  IN  UINTN                     AcpiTableBufferSize,
                  OUT UINTN                     *TableKey
                  )
{
  EFI_STATUS Status;
  VOID       *AcpiTableBufferConst;

  //
  // Check for invalid input parameters
  //
  if ((AcpiTableBuffer == NULL) || (TableKey == NULL)
      || (((EFI_ACPI_DESCRIPTION_HEADER *) AcpiTableBuffer)->Length != AcpiTableBufferSize)) {
    return EFI_INVALID_PARAMETER;
  }


  //
  // Install the ACPI table by using ACPI support protocol
  //
  AcpiTableBufferConst = AllocateCopyPool (AcpiTableBufferSize, AcpiTableBuffer);
  *TableKey = 0;
  Status = AddTableToList (
                           AcpiSupportInstance,
                           AcpiTableBufferConst,
                           TRUE,
                           TableKey
                           );
  if (!EFI_ERROR (Status)) {
    Status = PublishTables (
                            AcpiSupportInstance
                            );
  }
  FreePool (AcpiTableBufferConst);

  return Status;
}

/**
   Removes an ACPI table from the XSDT.

   @param  AcpiSupportInstance EFI_ACPI_SUPPORT_INSTANCE
   @param  TableKey  Specifies the table to uninstall.  The key was returned from InstallAcpiTable().

   @return EFI_SUCCESS    The table was successfully uninstalled.
   @return EFI_NOT_FOUND  TableKey does not refer to a valid key for a table entry.

**/
EFI_STATUS
UninstallAcpiTable (
                    IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
                    IN UINTN                     TableKey
                    )
{
  EFI_STATUS                Status;

  //
  // Uninstall the ACPI table by using ACPI support protocol
  //
  Status = RemoveTableFromList (
                                AcpiSupportInstance,
                                TableKey
                                );
  if (!EFI_ERROR (Status)) {
    Status = PublishTables (
                            AcpiSupportInstance
                            );
  }

  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  } else {
    return EFI_SUCCESS;
  }
}

/**
   If the number of APCI tables exceeds the preallocated max table number, enlarge the table buffer.

   @param  AcpiSupportInstance       ACPI support protocol instance data structure

   @return EFI_SUCCESS             reallocate the table beffer successfully.
   @return EFI_OUT_OF_RESOURCES    Unable to allocate required resources.

**/
EFI_STATUS
ReallocateAcpiTableBuffer (
                           IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance
                           )
{
  UINTN                      NewMaxTableNumber;
  UINTN                      TotalSize;
  UINT8                      *Pointer;
  EFI_PHYSICAL_ADDRESS       PageAddress;
  EFI_ACPI_SUPPORT_INSTANCE  TempPrivateData;
  EFI_STATUS                 Status;
  UINT64                     CurrentData;

  CopyMem (&TempPrivateData, AcpiSupportInstance, sizeof (EFI_ACPI_SUPPORT_INSTANCE));
  //
  // Enlarge the max table number from mEfiAcpiMaxNumTables to mEfiAcpiMaxNumTables + EFI_ACPI_MAX_NUM_TABLES
  //
  NewMaxTableNumber = mEfiAcpiMaxNumTables + EFI_ACPI_MAX_NUM_TABLES;
  //
  // Create XSDT structure and allocate buffers.
  //
  TotalSize = sizeof (EFI_ACPI_DESCRIPTION_HEADER) +
    NewMaxTableNumber * sizeof (UINT64);

  Status = gBS->AllocatePages (
                               AllocateAnyPages,
                               EfiACPIReclaimMemory,
                               EFI_SIZE_TO_PAGES (TotalSize),
                               &PageAddress
                               );
  if (EFI_ERROR (Status)) {
    return EFI_OUT_OF_RESOURCES;
  }

  Pointer = (UINT8 *) (UINTN) PageAddress;
  ZeroMem (Pointer, TotalSize);
  AcpiSupportInstance->Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *) Pointer;

  //
  // Update RSDP to point to the new Xsdt address.
  //
  CurrentData = (UINT64) (UINTN) AcpiSupportInstance->Xsdt;
  CopyMem (&AcpiSupportInstance->Rsdp3->XsdtAddress, &CurrentData, sizeof (UINT64));

  //
  // copy the original Xsdt structure to new buffer
  //
  CopyMem (AcpiSupportInstance->Xsdt, TempPrivateData.Xsdt,
           (sizeof (EFI_ACPI_DESCRIPTION_HEADER) + mEfiAcpiMaxNumTables * sizeof (UINT64)));

  //
  // Calculate orignal ACPI table buffer size
  //
  TotalSize = sizeof (EFI_ACPI_DESCRIPTION_HEADER) +
    mEfiAcpiMaxNumTables * sizeof (UINT64);
  gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)TempPrivateData.Xsdt,
                  EFI_SIZE_TO_PAGES (TotalSize));

  //
  // Update the Max ACPI table number
  //
  mEfiAcpiMaxNumTables = NewMaxTableNumber;
  return EFI_SUCCESS;
}

/**
   This function adds an ACPI table to the table list.  It will detect FACS and
   allocate the correct type of memory and properly align the table.

   @param  AcpiSupportInstance       Instance of the protocol.
   @param  Table                     Table to add.
   @param  Checksum                  Does the table require checksumming.
   @param  Handle                    Pointer for returning the handle.

   @return EFI_SUCCESS               The function completed successfully.
   @return EFI_OUT_OF_RESOURCES      Could not allocate a required resource.
   @retval EFI_ACCESS_DENIED         The table signature matches a table already
   present in the system and platform policy
   does not allow duplicate tables of this type.
**/
EFI_STATUS
AddTableToList (
                IN  EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
                IN  VOID                      *Table,
                IN  BOOLEAN                   Checksum,
                OUT UINTN                     *Handle
                )
{
  EFI_STATUS          Status;
  EFI_ACPI_TABLE_LIST *CurrentTableList;
  UINT32              CurrentTableSignature;
  UINT32              CurrentTableSize;
  VOID                *CurrentXsdtEntry;
  UINT64              Buffer64;
  BOOLEAN             AddToXsdt;

  //
  // Check for invalid input parameters
  //
  ASSERT (AcpiSupportInstance);
  ASSERT (Table);
  ASSERT (Handle);

  //
  // Init locals
  //
  AddToXsdt = TRUE;

  //
  // Create a new list entry
  //
  CurrentTableList = AllocatePool (sizeof (EFI_ACPI_TABLE_LIST));
  ASSERT (CurrentTableList);

  //
  // Determine table type and size
  //
  CurrentTableSignature = ((EFI_ACPI_COMMON_HEADER *) Table)->Signature;
  CurrentTableSize      = ((EFI_ACPI_COMMON_HEADER *) Table)->Length;

  //
  // Allocate a buffer for the table.
  //
  CurrentTableList->NumberOfPages = EFI_SIZE_TO_PAGES (CurrentTableSize);

  //
  // Allocation memory type depends on the type of the table
  //
  if ((CurrentTableSignature == EFI_ACPI_2_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE) ||
      (CurrentTableSignature == EFI_ACPI_4_0_UEFI_ACPI_DATA_TABLE_SIGNATURE)) {
    //
    // Allocate memory for the FACS.  This structure must be aligned
    // on a 64 byte boundary and must be ACPI NVS memory.
    // Using AllocatePages should ensure that it is always aligned.
    //
    // UEFI table also need to be in ACPI NVS memory, because some data field
    // could be updated by OS present agent. For example, BufferPtrAddress in
    // SMM communication ACPI table.
    //
    ASSERT ((EFI_PAGE_SIZE % 64) == 0);
    Status = gBS->AllocatePages (
                                 AllocateAnyPages,
                                 EfiACPIMemoryNVS,
                                 CurrentTableList->NumberOfPages,
                                 &CurrentTableList->PageAddress
                                 );
  } else {
    //
    // All other tables are ACPI reclaim memory, no alignment requirements.
    //
    Status = gBS->AllocatePages (
                                 AllocateAnyPages,
                                 EfiACPIReclaimMemory,
                                 CurrentTableList->NumberOfPages,
                                 &CurrentTableList->PageAddress
                                 );
  }
  //
  // Check return value from memory alloc.
  //
  if (EFI_ERROR (Status)) {
    gBS->FreePool (CurrentTableList);
    return EFI_OUT_OF_RESOURCES;
  }
  //
  // Update the table pointer with the allocated memory start
  //
  CurrentTableList->Table = (EFI_ACPI_COMMON_HEADER *) (UINTN) CurrentTableList->PageAddress;

  //
  // Initialize the table contents
  //
  CurrentTableList->Signature = EFI_ACPI_TABLE_LIST_SIGNATURE;
  CopyMem (CurrentTableList->Table, Table, CurrentTableSize);
  CurrentTableList->Handle  = AcpiSupportInstance->CurrentHandle++;
  *Handle                   = CurrentTableList->Handle;

  //
  // Update internal pointers if this is a required table.  If it is a required
  // table and a table of that type already exists, return an error.
  //
  // Calculate the checksum if the table is not FACS.
  //
  switch (CurrentTableSignature) {
  case EFI_ACPI_1_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE:
    //
    // Check that the table has not been previously added.
    //
    if (AcpiSupportInstance->Fadt3 != NULL) {
      gBS->FreePages (CurrentTableList->PageAddress, CurrentTableList->NumberOfPages);
      gBS->FreePool (CurrentTableList);
      return EFI_ACCESS_DENIED;
    }

    //
    // Save a pointer to the table
    //
    AcpiSupportInstance->Fadt3 = (EFI_ACPI_3_0_FIXED_ACPI_DESCRIPTION_TABLE *) CurrentTableList->Table;

    //
    // Update pointers in FADT.
    //
    AcpiSupportInstance->Fadt3->FirmwareCtrl = 0;
    Buffer64 = (UINT64) (UINTN) AcpiSupportInstance->Facs3;
    CopyMem (
             &AcpiSupportInstance->Fadt3->XFirmwareCtrl,
             &Buffer64,
             sizeof (UINT64)
             );

    AcpiSupportInstance->Fadt3->Dsdt  = 0;
    Buffer64                          = (UINT64) (UINTN) AcpiSupportInstance->Dsdt3;
    CopyMem (
             &AcpiSupportInstance->Fadt3->XDsdt,
             &Buffer64,
             sizeof (UINT64)
             );

    //
    // RSDP OEM information is updated to match the FADT OEM information
    //
    CopyMem (
             &AcpiSupportInstance->Rsdp3->OemId,
             &AcpiSupportInstance->Fadt3->Header.OemId,
             6
             );

    //
    // XSDT OEM information is updated to match FADT OEM information.
    //
    CopyMem (
             &AcpiSupportInstance->Xsdt->OemId,
             &AcpiSupportInstance->Fadt3->Header.OemId,
             6
             );
    CopyMem (
             &AcpiSupportInstance->Xsdt->OemTableId,
             &AcpiSupportInstance->Fadt3->Header.OemTableId,
             sizeof (UINT64)
             );
    AcpiSupportInstance->Xsdt->OemRevision = AcpiSupportInstance->Fadt3->Header.OemRevision;

    //
    // Checksum the table
    //
    if (Checksum) {
      AcpiPlatformChecksum (
                            CurrentTableList->Table,
                            CurrentTableList->Table->Length,
                            OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                       Checksum)
                            );
    }
    break;

  case EFI_ACPI_1_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE:
    //
    // Check that the table has not been previously added.
    //
    if (AcpiSupportInstance->Facs3 != NULL) {
      gBS->FreePages (CurrentTableList->PageAddress, CurrentTableList->NumberOfPages);
      gBS->FreePool (CurrentTableList);
      return EFI_ACCESS_DENIED;
    }
    //
    // FACS is referenced by FADT and is not part of XSDT.
    //
    AddToXsdt = FALSE;

    //
    // Save a pointer to the table
    //
    AcpiSupportInstance->Facs3 = (EFI_ACPI_3_0_FIRMWARE_ACPI_CONTROL_STRUCTURE *) CurrentTableList->Table;

    //
    // If FADT already exists, update table pointers.
    //
    if (AcpiSupportInstance->Fadt3 != NULL) {
      AcpiSupportInstance->Fadt3->FirmwareCtrl = 0;
      Buffer64 = (UINT64) (UINTN) AcpiSupportInstance->Facs3;
      CopyMem (
               &AcpiSupportInstance->Fadt3->XFirmwareCtrl,
               &Buffer64,
               sizeof (UINT64)
               );

      //
      // Checksum FADT table
      //
      AcpiPlatformChecksum (
                            AcpiSupportInstance->Fadt3,
                            AcpiSupportInstance->Fadt3->Header.Length,
                            OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                       Checksum)
                            );
    }
    break;

  case EFI_ACPI_1_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE:
    //
    // Check that the table has not been previously added.
    //
    if (AcpiSupportInstance->Dsdt3 != NULL) {
      gBS->FreePages (CurrentTableList->PageAddress, CurrentTableList->NumberOfPages);
      gBS->FreePool (CurrentTableList);
      return EFI_ACCESS_DENIED;
    }
    //
    // DSDT is referenced by FADT and is not part of XSDT.
    //
    AddToXsdt = FALSE;

    //
    // Save a pointer to the table
    //
    AcpiSupportInstance->Dsdt3 = (EFI_ACPI_DESCRIPTION_HEADER *) CurrentTableList->Table;

    //
    // If FADT already exists, update table pointers.
    //
    if (AcpiSupportInstance->Fadt3 != NULL) {
      AcpiSupportInstance->Fadt3->Dsdt  = 0;
      Buffer64                          = (UINT64) (UINTN) AcpiSupportInstance->Dsdt3;
      CopyMem (
               &AcpiSupportInstance->Fadt3->XDsdt,
               &Buffer64,
               sizeof (UINT64)
               );

      //
      // Checksum FADT table
      //
      AcpiPlatformChecksum (
                            AcpiSupportInstance->Fadt3,
                            AcpiSupportInstance->Fadt3->Header.Length,
                            OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                       Checksum)
                            );
    }

    //
    // Checksum the table
    //
    if (Checksum) {
      AcpiPlatformChecksum (
                            CurrentTableList->Table,
                            CurrentTableList->Table->Length,
                            OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                       Checksum)
                            );
    }
    break;

  default:
    //
    // Checksum the table
    //
    if (Checksum) {
      AcpiPlatformChecksum (
                            CurrentTableList->Table,
                            CurrentTableList->Table->Length,
                            OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                       Checksum)
                            );
    }
    break;
  }
  //
  // Add the table to the current list of tables
  //
  InsertTailList (&AcpiSupportInstance->TableList, &CurrentTableList->Link);

  //
  // Add the table to the XSDT table entry lists.
  //

  //
  // Add to ACPI 2.0/3.0 table tree
  //
  if (AddToXsdt) {
    //
    // If the table number exceed the gEfiAcpiMaxNumTables, enlarge the table buffer
    //
    if (AcpiSupportInstance->NumberOfTableEntries3 >= mEfiAcpiMaxNumTables) {
      Status = ReallocateAcpiTableBuffer (AcpiSupportInstance);
      ASSERT_EFI_ERROR (Status);
    }

    //
    // This pointer must not be directly dereferenced as the XSDT entries may not
    // be 64 bit aligned resulting in a possible fault.  Use CopyMem to update.
    //
    CurrentXsdtEntry = (VOID *)
      (
       (UINT8 *) AcpiSupportInstance->Xsdt +
       sizeof (EFI_ACPI_DESCRIPTION_HEADER) +
       AcpiSupportInstance->NumberOfTableEntries3 *
       sizeof (UINT64)
       );

    //
    // Add entry to XSDT, XSDT expects 64 bit pointers, but
    // the table pointers in XSDT are not aligned on 8 byte boundary.
    //
    Buffer64 = (UINT64) (UINTN) CurrentTableList->Table;
    CopyMem (
             CurrentXsdtEntry,
             &Buffer64,
             sizeof (UINT64)
             );

    //
    // Update length
    //
    AcpiSupportInstance->Xsdt->Length = AcpiSupportInstance->Xsdt->Length + sizeof (UINT64);
    AcpiSupportInstance->NumberOfTableEntries3++;
  }

  ChecksumCommonTables (AcpiSupportInstance);
  return EFI_SUCCESS;
}

/**
   This function finds the table specified by the handle and returns a pointer to it.
   If the handle is not found, EFI_NOT_FOUND is returned and the contents of Table are
   undefined.

   @param  Handle      Table to find.
   @param  TableList   Table list to search
   @param  Table       Pointer to table found.

   @return EFI_SUCCESS    The function completed successfully.
   @return EFI_NOT_FOUND  No table found matching the handle specified.

**/
EFI_STATUS
FindTableByHandle (
                   IN  UINTN                Handle,
                   IN  LIST_ENTRY           *TableList,
                   OUT EFI_ACPI_TABLE_LIST **Table
                   )
{
  LIST_ENTRY      *CurrentLink;
  EFI_ACPI_TABLE_LIST *CurrentTable;

  //
  // Check for invalid input parameters
  //
  ASSERT (Table);

  //
  // Find the table
  //
  CurrentLink = TableList->ForwardLink;

  while (CurrentLink != TableList) {
    CurrentTable = EFI_ACPI_TABLE_LIST_FROM_LINK (CurrentLink);
    if (CurrentTable->Handle == Handle) {
      //
      // Found handle, so return this table.
      //
      *Table = CurrentTable;
      return EFI_SUCCESS;
    }

    CurrentLink = CurrentLink->ForwardLink;
  }
  //
  // Table not found
  //
  return EFI_NOT_FOUND;
}

/**
   This function removes a basic table from the XSDT.

   @param  Table                 Pointer to table found.
   @param  NumberOfTableEntries  Current number of table entries in the XSDT
   @param  Xsdt                  Pointer to the Xsdt to remove from

   @return EFI_SUCCESS            The function completed successfully.
   @return EFI_INVALID_PARAMETER  The table was not found in the Xsdt.

**/
EFI_STATUS
RemoveTableFromXsdt (
                     IN OUT EFI_ACPI_TABLE_LIST         * Table,
                     IN OUT UINTN                       *NumberOfTableEntries,
                     IN OUT EFI_ACPI_DESCRIPTION_HEADER * Xsdt
                     )
{
  VOID    *CurrentXsdtEntry;
  UINT64  CurrentTablePointer64;
  UINTN   TempIndex;

  //
  // Check for invalid input parameters
  //
  ASSERT (Table);
  ASSERT (NumberOfTableEntries);
  ASSERT (Xsdt);

  //
  // Find the table entry in the XSDT
  //
  for (TempIndex = 0; TempIndex < *NumberOfTableEntries; TempIndex++) {
    //
    // This pointer must not be directly dereferenced as the XSDT entries may not
    // be 64 bit aligned resulting in a possible fault.  Use CopyMem to update.
    //
    CurrentXsdtEntry = (VOID *) ((UINT8 *) Xsdt + sizeof (EFI_ACPI_DESCRIPTION_HEADER) + TempIndex * sizeof (UINT64));

    //
    // Read the entry value out of the XSDT
    //
    CopyMem (&CurrentTablePointer64, CurrentXsdtEntry, sizeof (UINT64));

    //
    // Check if we have found the corresponding entry in XSDT
    //
    if ((Xsdt == NULL) || CurrentTablePointer64 == (UINT64) (UINTN) Table->Table) {
      //
      // Found entry, so copy all following entries and shrink table
      // We actually copy all + 1 to copy the initialized value of memory over
      // the last entry.
      //
      CopyMem (CurrentXsdtEntry, ((UINT64 *) CurrentXsdtEntry) + 1,
               (*NumberOfTableEntries - TempIndex) * sizeof (UINT64));
      Xsdt->Length = Xsdt->Length - sizeof (UINT64);
      break;
    } else if (TempIndex + 1 == *NumberOfTableEntries) {
      //
      // At the last entry, and table not found
      //
      return EFI_INVALID_PARAMETER;
    }
  }

  AcpiPlatformChecksum (
                        Xsdt,
                        Xsdt->Length,
                        OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                   Checksum)
                        );

  //
  // Decrement the number of tables
  //
  (*NumberOfTableEntries)--;
  return EFI_SUCCESS;
}

/**
   This function removes a table and frees any associated memory.

   @param  AcpiSupportInstance  Instance of the protocol.
   @param  Table              Pointer to table found.

   @return EFI_SUCCESS  The function completed successfully.

**/
EFI_STATUS
DeleteTable (
             IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
             IN OUT EFI_ACPI_TABLE_LIST   *Table
             )
{
  UINT32  CurrentTableSignature;
  BOOLEAN RemoveFromXsdt;

  //
  // Check for invalid input parameters
  //
  ASSERT (AcpiSupportInstance);
  ASSERT (Table);

  //
  // Init locals
  //
  RemoveFromXsdt        = TRUE;

  if (Table->Table != NULL) {
    CurrentTableSignature = ((EFI_ACPI_COMMON_HEADER *) Table->Table)->Signature;

    //
    // Basic tasks to accomplish delete are:
    //   Determine removal requirements (in XSDT or not)
    //   Remove entry from XSDT
    //   Remove any table references to the table
    //   If no one is using the table
    //      Free the table (removing pointers from private data and tables)
    //      Remove from list
    //      Free list structure
    //
    //
    // Determine if this table is in the XSDT
    //
    if ((CurrentTableSignature == EFI_ACPI_1_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE) ||
        (CurrentTableSignature == EFI_ACPI_2_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE) ||
        (CurrentTableSignature == EFI_ACPI_3_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE)
        ) {
      RemoveFromXsdt = FALSE;
    }

    //
    // Remove from the Xsdt.  We don't care about the return value
    // because it is acceptable for the table to not exist in Xsdt.
    // We didn't add some tables so we don't remove them.
    //
    if (RemoveFromXsdt) {
      RemoveTableFromXsdt (
                           Table,
                           &AcpiSupportInstance->NumberOfTableEntries3,
                           AcpiSupportInstance->Xsdt
                           );
    }

    //
    // Free the table, clean up any dependent tables and our private data pointers.
    //
    switch (Table->Table->Signature) {
    case EFI_ACPI_3_0_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE:
      AcpiSupportInstance->Fadt3 = NULL;
      break;

    case EFI_ACPI_3_0_FIRMWARE_ACPI_CONTROL_STRUCTURE_SIGNATURE:
      AcpiSupportInstance->Facs3 = NULL;

      //
      // Update FADT table pointers
      //0
      if (AcpiSupportInstance->Fadt3 != NULL) {
        AcpiSupportInstance->Fadt3->FirmwareCtrl = 0;
        ZeroMem (&AcpiSupportInstance->Fadt3->XFirmwareCtrl, sizeof (UINT64));

        //
        // Checksum table
        //
        AcpiPlatformChecksum (
                              AcpiSupportInstance->Fadt3,
                              AcpiSupportInstance->Fadt3->Header.Length,
                              OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                         Checksum)
                              );
      }
      break;

    case EFI_ACPI_3_0_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE:
      AcpiSupportInstance->Dsdt3 = NULL;

      //
      // Update FADT table pointers
      //
      if (AcpiSupportInstance->Fadt3 != NULL) {
        AcpiSupportInstance->Fadt3->Dsdt = 0;
        ZeroMem (&AcpiSupportInstance->Fadt3->XDsdt, sizeof (UINT64));

        //
        // Checksum table
        //
        AcpiPlatformChecksum (
                              AcpiSupportInstance->Fadt3,
                              AcpiSupportInstance->Fadt3->Header.Length,
                              OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                         Checksum)
                              );
      }

      break;
    default:
      //
      // Do nothing
      //
      break;
    }
  }

  //
  // Free the Table
  //
  gBS->FreePages (Table->PageAddress, Table->NumberOfPages);
  RemoveEntryList (&(Table->Link));
  gBS->FreePool (Table);

  //
  // Done
  //
  return EFI_SUCCESS;
}

/**
   This function finds and removes the table specified by the handle.

   @param  AcpiSupportInstance  Instance of the protocol.
   @param  Handle               Table to remove.

   @return EFI_SUCCESS    The function completed successfully.
   @return EFI_ABORTED    An error occurred.
   @return EFI_NOT_FOUND  Handle not found in table list.
**/
EFI_STATUS
RemoveTableFromList (
                     IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance,
                     IN UINTN                     Handle
                     )
{
  EFI_ACPI_TABLE_LIST *Table;
  EFI_STATUS          Status;

  Table = NULL;

  //
  // Check for invalid input parameters
  //
  ASSERT (AcpiSupportInstance);

  //
  // Find the table
  //
  Status = FindTableByHandle (
                              Handle,
                              &AcpiSupportInstance->TableList,
                              &Table
                              );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }
  //
  // Remove the table
  //
  Status = DeleteTable (AcpiSupportInstance, Table);
  if (EFI_ERROR (Status)) {
    return EFI_ABORTED;
  }
  //
  // Completed successfully
  //
  return EFI_SUCCESS;
}

/**
   This function calculates and updates an UINT8 checksum.

   @param  Buffer          Pointer to buffer to checksum
   @param  Size            Number of bytes to checksum
   @param  ChecksumOffset  Offset to place the checksum result in

   @return EFI_SUCCESS             The function completed successfully.

**/
EFI_STATUS
AcpiPlatformChecksum (
                      IN VOID  *Buffer,
                      IN UINTN Size,
                      IN UINTN ChecksumOffset
                      )
{
  UINT8 Sum;
  UINT8 *Ptr;

  Sum = 0;
  //
  // Initialize pointer
  //
  Ptr = Buffer;

  //
  // set checksum to 0 first
  //
  Ptr[ChecksumOffset] = 0;

  //
  // add all content of buffer
  //
  while ((Size--) != 0) {
    Sum = (UINT8) (Sum + (*Ptr++));
  }
  //
  // set checksum
  //
  Ptr                 = Buffer;
  Ptr[ChecksumOffset] = (UINT8) (0xff - Sum + 1);

  return EFI_SUCCESS;
}

/**
   Checksum common tables, RSDP, XSDT.

   @param  AcpiSupportInstance  Protocol instance private data.

   @return EFI_SUCCESS        The function completed successfully.

**/
EFI_STATUS
ChecksumCommonTables (
                      IN OUT EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance
                      )
{
  AcpiPlatformChecksum (
                        AcpiSupportInstance->Rsdp3,
                        sizeof (EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER),
                        OFFSET_OF (EFI_ACPI_1_0_ROOT_SYSTEM_DESCRIPTION_POINTER,
                                   Checksum)
                        );
  AcpiPlatformChecksum (
                        AcpiSupportInstance->Rsdp3,
                        sizeof (EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER),
                        OFFSET_OF (EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER,
                                   ExtendedChecksum)
                        );

  //
  // XSDT checksum
  //
  AcpiPlatformChecksum (
                        AcpiSupportInstance->Xsdt,
                        AcpiSupportInstance->Xsdt->Length,
                        OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER,
                                   Checksum)
                        );

  return EFI_SUCCESS;
}

/**
   Constructor for the ACPI support protocol to initializes instance data.

   @param AcpiSupportInstance   Instance to construct

   @retval EFI_SUCCESS             Instance initialized.
   @retval  EFI_OUT_OF_RESOURCES  Unable to allocate required resources.
**/
EFI_STATUS
AcpiSupportConstructor (
                        IN EFI_ACPI_SUPPORT_INSTANCE *AcpiSupportInstance
                        )
{
  EFI_STATUS            Status;
  UINT64                CurrentData;
  UINTN                 TotalSize;
  UINTN                 RsdpTableSize;
  UINT8                 *Pointer;
  EFI_PHYSICAL_ADDRESS  PageAddress;

  //
  // Check for invalid input parameters
  //
  ASSERT (AcpiSupportInstance);

  InitializeListHead (&AcpiSupportInstance->TableList);
  AcpiSupportInstance->CurrentHandle              = 1;

  //
  // Create RSDP table
  //
  RsdpTableSize = sizeof (EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER);

  Status = gBS->AllocatePages (
                               AllocateAnyPages,
                               EfiACPIReclaimMemory,
                               EFI_SIZE_TO_PAGES (RsdpTableSize),
                               &PageAddress
                               );

  if (EFI_ERROR (Status)) {
    return EFI_OUT_OF_RESOURCES;
  }

  Pointer = (UINT8 *) (UINTN) PageAddress;
  ZeroMem (Pointer, RsdpTableSize);

  AcpiSupportInstance->Rsdp3 = (EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER *) Pointer;

  //
  // Create XSDT structures
  //
  TotalSize = sizeof (EFI_ACPI_DESCRIPTION_HEADER) + // for ACPI 2.0/3.0 XSDT
    mEfiAcpiMaxNumTables * sizeof (UINT64);

  Status = gBS->AllocatePages (
                               AllocateAnyPages,
                               EfiACPIReclaimMemory,
                               EFI_SIZE_TO_PAGES (TotalSize),
                               &PageAddress
                               );

  if (EFI_ERROR (Status)) {
    gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)AcpiSupportInstance->Rsdp3, EFI_SIZE_TO_PAGES (RsdpTableSize));
    return EFI_OUT_OF_RESOURCES;
  }

  Pointer = (UINT8 *) (UINTN) PageAddress;
  ZeroMem (Pointer, TotalSize);

  Pointer += (sizeof (EFI_ACPI_DESCRIPTION_HEADER) + EFI_ACPI_MAX_NUM_TABLES * sizeof (UINT32));
  AcpiSupportInstance->Xsdt = (EFI_ACPI_DESCRIPTION_HEADER *) Pointer;

  //
  // Initialize RSDP
  //
  CurrentData = EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER_SIGNATURE;
  CopyMem (&AcpiSupportInstance->Rsdp3->Signature, &CurrentData, sizeof (UINT64));
  CopyMem (AcpiSupportInstance->Rsdp3->OemId, OEM_ID, sizeof (AcpiSupportInstance->Rsdp3->OemId));
  AcpiSupportInstance->Rsdp3->Revision = EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER_REVISION;
  AcpiSupportInstance->Rsdp3->RsdtAddress = 0;
  AcpiSupportInstance->Rsdp3->Length = sizeof (EFI_ACPI_3_0_ROOT_SYSTEM_DESCRIPTION_POINTER);
  CurrentData = (UINT64) (UINTN) AcpiSupportInstance->Xsdt;
  CopyMem (&AcpiSupportInstance->Rsdp3->XsdtAddress, &CurrentData, sizeof (UINT64));
  SetMem (AcpiSupportInstance->Rsdp3->Reserved, 3, EFI_ACPI_RESERVED_BYTE);

  //
  // Initialize Xsdt
  //
  AcpiSupportInstance->Xsdt->Signature = EFI_ACPI_3_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
  AcpiSupportInstance->Xsdt->Length = sizeof (EFI_ACPI_DESCRIPTION_HEADER);
  AcpiSupportInstance->Xsdt->Revision = EFI_ACPI_3_0_EXTENDED_SYSTEM_DESCRIPTION_TABLE_REVISION;
  CopyMem (AcpiSupportInstance->Xsdt->OemId, OEM_ID, sizeof (AcpiSupportInstance->Xsdt->OemId));
  CopyMem (&AcpiSupportInstance->Xsdt->OemTableId, OEM_TABLE_ID, sizeof (UINT64));
  AcpiSupportInstance->Xsdt->OemRevision = OEM_REVISION;
  CopyMem (&AcpiSupportInstance->Xsdt->CreatorId, CREATOR_ID, sizeof (AcpiSupportInstance->Xsdt->CreatorId));
  AcpiSupportInstance->Xsdt->CreatorRevision  = CREATOR_REVISION;
  ChecksumCommonTables (AcpiSupportInstance);

  //
  // Completed successfully
  //
  return EFI_SUCCESS;
}
