/* Time-stamp: <2016-06-11 00:26:27 andreiw>
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */

#ifndef _UTILS_LIB_H_
#define _UTILS_LIB_H_

#include <Uefi.h>

EFI_STATUS
FileSystemSave (
                IN EFI_HANDLE Handle,
                IN CHAR16 *VolSubDir,
                IN CHAR16 *Path,
                IN VOID *Table,
                IN UINTN TableSize
                );

VOID *
GetTable (
          IN EFI_GUID *Guid
          );

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

CHAR16
CharToUpper (
             IN CHAR16 Char
             );

INTN
StriCmp (
         IN CONST CHAR16 *FirstString,
         IN CONST CHAR16 *SecondString
         );

#endif /* _UTILS_LIB_H_ */
