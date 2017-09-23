/** @file
  Abstract device driver for the UEFI Console.

  This creates stdin:/stdout:/stderr: as wide character
  devices (_S_IWTTY) and nstdin:/nstdout:/nstderr: as
  their "narrow" variants.

  It this the responsibility of the caller to convert between
  narrow and wide characters in order to perform the desired operations.

  Copyright (c) 2017 Andrei Warkentin <andrey.warkentin@gmail.com>
  Copyright (c) 2016, Daryl McDaniel. All rights reserved.
  Copyright (c) 2010 - 2014, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials are licensed and made available under
  the terms and conditions of the BSD License that accompanies this distribution.
  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include  <Uefi.h>
#include  <Library/BaseLib.h>
#include  <Library/MemoryAllocationLib.h>
#include  <Library/UefiBootServicesTableLib.h>
#include  <Library/DebugLib.h>
#include  <Library/ShellLib.h>
#include  <Protocol/SimpleTextIn.h>
#include  <Protocol/SimpleTextOut.h>
#include  <Protocol/UnicodeCollation.h>
#include  <LibConfig.h>

#include  <errno.h>
#include  <wctype.h>
#include  <wchar.h>
#include  <stdarg.h>
#include  <sys/fcntl.h>
#include  <unistd.h>
#include  <sys/termios.h>
#include  <Efi/SysEfi.h>
#include  <kfile.h>
#include  <Device/Device.h>
#include  <Device/IIO.h>
#include  <MainData.h>

#define DEVCON_NUM          6

#define DEVCON_STDIN        0
#define DEVCON_STDOUT       1
#define DEVCON_STDERR       2
#define DEVCON_NSTDIN       3
#define DEVCON_NSTDOUT      4
#define DEVCON_NSTDERR      5

#define DEVCON_IS_IN(I)     ((I) == DEVCON_STDIN || (I) == DEVCON_NSTDIN)
#define DEVCON_IS_OUT(I)    ((I) == DEVCON_STDOUT || (I) == DEVCON_NSTDOUT)
#define DEVCON_IS_ERR(I)    ((I) == DEVCON_STDERR || (I) == DEVCON_NSTDERR)
#define DEVCON_IS_WIDE(I)   ((I) == DEVCON_STDIN ||     \
                             (I) == DEVCON_STDOUT ||    \
                             (I) == DEVCON_STDERR)

static const CHAR16* const
stdioNames[DEVCON_NUM]   = {
  L"stdin:", L"stdout:", L"stderr:",
  L"nstdin:", L"nstdout:", L"nstderr:"
};

static const int stdioFlags[DEVCON_NUM] = {
  O_RDONLY,             // stdin
  O_WRONLY,             // stdout
  O_WRONLY,             // stderr
  O_RDONLY,             // ntdin
  O_WRONLY,             // ntdout
  O_WRONLY              // ntderr
};

static SHELL_FILE_HANDLE ShellHandles[DEVCON_NUM];

typedef enum {
  SH_HNDL_CON,
  SH_HDNL_FILE,
  SH_HNDL_PIPE
} SH_HNDL_TYPE;

static SH_HNDL_TYPE ShellHandleTypes[DEVCON_NUM];

static DeviceNode    *ConNode[DEVCON_NUM];
static ConInstance   *ConInstanceList;

static cIIO          *IIO;

/* Flags settable by Ioctl */
static BOOLEAN        TtyCooked;
static BOOLEAN        TtyEcho;

/**
  Remove the unicode file tag from the begining of the file buffer.

  Regardless of what read (console, redirected file or pipe), the input
  is UTF16, so this is always valid.

  @param[in]  Handle    Handle to process.
**/
static void
RemoveFileTag(
  IN SHELL_FILE_HANDLE Handle
  )
{
  UINTN             CharSize;
  CHAR16            CharBuffer;

  CharSize    = sizeof(CHAR16);
  CharBuffer  = 0;

  ShellSetFilePosition(Handle, 0);
  ShellReadFile(Handle, &CharSize, &CharBuffer);
  if (CharBuffer != EFI_UNICODE_BYTE_ORDER_MARK) {
    ShellSetFilePosition(Handle, 0);
  }
}

/** Position the console cursor to the coordinates specified by Position.

    @param[in]  filp      Pointer to the file descriptor structure for this file.
    @param[in]  Position  A value containing the target X and Y coordinates.
    @param[in]  whence    Ignored by the Console device.

    @retval   Position    Success.  Returns a copy of the Position argument.
    @retval   -1          filp is not associated with a valid console stream.
    @retval   -1          This console stream is attached to stdin.
    @retval   -1          The SetCursorPosition operation failed.
**/
static
off_t
EFIAPI
da_ConSeek(
  struct __filedes   *filp,
  off_t               Position,
  int                 whence      ///< Ignored by Console
)
{
  ConInstance                       *Stream;

  Stream = BASE_CR(filp->f_ops, ConInstance, Abstraction);
  // Quick check to see if Stream looks reasonable
  if (Stream->Cookie != CON_COOKIE) {    // Cookie == 'IoAb'
    EFIerrno = RETURN_INVALID_PARAMETER;
    errno = EINVAL;
    return -1;    // Looks like a bad This pointer
  }
  if (DEVCON_IS_IN(Stream->InstanceNum) &&
      ShellHandleTypes[Stream->InstanceNum] == SH_HNDL_CON) {
    // Seek is not valid for seeking non-redirected stdin.
    EFIerrno = RETURN_UNSUPPORTED;
    errno = EIO;
    return -1;
  }

  // Everything is OK to do the final verification and "seek".
  if (ShellHandleTypes[Stream->InstanceNum] != SH_HNDL_CON) {
    EFIerrno = ShellSetFilePosition(ShellHandles[Stream->InstanceNum],
                                    Position);
  } else {
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Proto;
    XY_OFFSET                       CursorPos;

    Proto = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)Stream->Dev;
    CursorPos.Offset = Position;

    EFIerrno = Proto->SetCursorPosition(Proto,
                                        (INTN)CursorPos.XYpos.Column,
                                        (INTN)CursorPos.XYpos.Row);
  }

  if(RETURN_ERROR(EFIerrno)) {
    errno = EINVAL;
    return -1;
  }

  return Position;
}

/**
  Write a buffer.

  If wide, the buffer is expected to contain wchar_t elements.
  If not redirecting (actually printing to console), the buffer
  is expected to contain a NUL-terminated UTF16 string.

  If wide, BufferSize refers to count of wchar_t elements, else
  count of char_t elements.

  @param[in,out]  BufferSize  Number of elements in Buffer.
  @param[in]      Buffer      Buffer

  @return   The number of buffer elements processed.
**/
static
ssize_t
EFIAPI
da_ConWrite(
  IN  struct __filedes     *filp,
  IN  off_t                *Position,
  IN  size_t                BufferSize,
  IN  const void           *Buffer
  )
{
  EFI_STATUS                          Status;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL    *Proto;
  ConInstance                        *Stream;
  ssize_t                             NumChar;
  XY_OFFSET                          CursorPos;

  NumChar = -1;
  Stream = BASE_CR(filp->f_ops, ConInstance, Abstraction);

  // Quick check to see if Stream looks reasonable
  if(Stream->Cookie != CON_COOKIE) {    // Cookie == 'IoAb'
    EFIerrno = RETURN_INVALID_PARAMETER;
    errno = EINVAL;
    return -1;    // Looks like a bad This pointer
  }

  if (DEVCON_IS_IN(Stream->InstanceNum)) {
    // Write is not valid for stdin
    EFIerrno = RETURN_UNSUPPORTED;
    errno = EIO;
    return -1;
  }

  if (BufferSize == 0) {
    return 0;
  }

  // Everything is OK to do the write.
  Proto = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)Stream->Dev;

  Status = EFI_SUCCESS;
  if (ShellHandleTypes[Stream->InstanceNum] != SH_HNDL_CON) {
    if (Position != NULL) {
      Status = ShellSetFilePosition(ShellHandles[Stream->InstanceNum],
                                    *Position);
    }

    if(!RETURN_ERROR(Status)) {
      UINTN BufSize = BufferSize;

      if (DEVCON_IS_WIDE(Stream->InstanceNum)) {
        BufSize *=  sizeof(wchar_t);
      }

      Status = ShellWriteFile(ShellHandles[Stream->InstanceNum],
                              (UINTN *) &BufSize, (void *) Buffer);
    }
  } else {
    if(Position != NULL) {
      CursorPos.Offset = *Position;

      Status = Proto->SetCursorPosition(Proto,
                                        (INTN)CursorPos.XYpos.Column,
                                        (INTN)CursorPos.XYpos.Row);
    }

    if(!RETURN_ERROR(Status)) {
      ASSERT(StrLen(Buffer) == BufferSize);
      /*
       * Danger, retarded interface doesn't take size, string better
       * be terminated and printable.
       */
      Status = Proto->OutputString( Proto, (CHAR16 *)Buffer);
    }
  }

  // Depending on status, update BufferSize and return
  if(!RETURN_ERROR(Status)) {
    NumChar = BufferSize;
    Stream->NumWritten += NumChar;
  }

  if (RETURN_ERROR(Status)) {
    /*
     * Should try harder to decode.
     */
    errno = EIO;
  }

  EFIerrno = Status;
  return NumChar;
}

/**
   Read a single element from the console input device.

   If wide, element is a wchar_t. Else a char.

    @param[in]      filp          Pointer to file descriptor for this file.
    @param[out]     Buffer        Buffer in which to place the read character.

    @retval    EFI_DEVICE_ERROR   A hardware error has occurred.
    @retval    EFI_NOT_READY      No data is available.  Try again later.
    @retval    EFI_END_OF_FILE    No more data is available.
    @retval    EFI_SUCCESS        One wide character has been placed in Character
**/
static
EFI_STATUS
da_ConRawRead (
  IN OUT  struct __filedes   *filp,
     OUT  wchar_t            *Character
)
{
  ConInstance                      *Stream;
  cIIO                             *Self;
  EFI_STATUS                        Status;

  Self    = (cIIO *)filp->devdata;
  Stream  = BASE_CR(filp->f_ops, ConInstance, Abstraction);

  ASSERT(DEVCON_IS_IN(Stream->InstanceNum));

  if (ShellHandleTypes[Stream->InstanceNum] != SH_HNDL_CON) {
    UINT64 Pos;
    ssize_t BufSize = DEVCON_IS_WIDE(Stream->InstanceNum) ?
      sizeof(wchar_t) : sizeof(char);

    ShellGetFilePosition(ShellHandles[Stream->InstanceNum], &Pos);
    *Character = 0;
    Status = ShellReadFile(ShellHandles[Stream->InstanceNum],
                           (UINTN *) &BufSize, Character);
    // DEBUG((EFI_D_ERROR, "read 0x%x bytes\n", BufSize));
    if (BufSize == 0) {
      Status = EFI_END_OF_FILE;
    }

    return Status;
  } else {
    wchar_t                           RetChar;
    EFI_INPUT_KEY                     Key = {0,0};
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *Proto = (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *)Stream->Dev;

    ASSERT(DEVCON_IS_WIDE(Stream->InstanceNum));

    if(Stream->UnGetKey == CHAR_NULL) {
      Status = Proto->ReadKeyStroke(Proto, &Key);
    } else {
      Status  = EFI_SUCCESS;
      // Use the data in the Un-get buffer
      // Guaranteed that ScanCode and UnicodeChar are not both NUL
      Key.ScanCode        = SCAN_NULL;
      Key.UnicodeChar     = Stream->UnGetKey;
      Stream->UnGetKey    = CHAR_NULL;
    }

    if (Status != EFI_SUCCESS) {
      return Status;
    }

    // Translate the Escape Scan Code to an ESC character
    if (Key.ScanCode != 0) {
      if (Key.ScanCode == SCAN_ESC) {
        RetChar = CHAR_ESC;
      } else if ((Self->Termio.c_iflag & IGNSPEC) != 0) {
        // If we are ignoring special characters, pretend this key didn't happen.
        return EFI_NOT_READY;
      } else {
        // Must be a control, function, or other non-printable key.
        // Map it into the Platform portion of the Unicode private use area
        RetChar = TtyFunKeyMax - Key.ScanCode;
      }
    } else {
      RetChar = Key.UnicodeChar;
    }

    *Character = RetChar;
  }

  return EFI_SUCCESS;
}

/**

   Read a single element from the console input device.

   If wide, element is a wchar_t. Else a char.

    @param[in]      filp          Pointer to file descriptor for this file.
    @param[in]      offset        Ignored.
    @param[in]      BufferSize    Buffer size, in bytes.
    @param[out]     Buffer        Buffer in which to place the read characters.

    @retval    -1   An error has occurred.  Reason in errno and EFIerrno.
    @retval    -1   No data is available.  errno is set to EAGAIN
    @retval     1   One wide character has been placed in Buffer
**/
static
ssize_t
EFIAPI
da_ConRead(
  IN OUT  struct __filedes   *filp,
  IN OUT  off_t              *offset,         // Console ignores this
  IN      size_t              BufferSize,
     OUT  VOID               *Buffer
)
{
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL   *Proto;
  ConInstance                      *Stream;
  //cIIO                              *Self;
  EFI_STATUS                        Status;
  UINTN                             Edex;
  BOOLEAN                           BlockingMode;
  wchar_t                           RetChar;

  Stream = BASE_CR(filp->f_ops, ConInstance, Abstraction);
  if (!DEVCON_IS_IN(Stream->InstanceNum)) {
    // Read is not valid for stdout and stderr.
    EFIerrno = RETURN_UNSUPPORTED;
    errno = EIO;
    return -1;
  }

  if(BufferSize < sizeof(wchar_t)) {
    errno = EINVAL; // Buffer is too small to hold one character
    return -1;
  }

  Stream = BASE_CR(filp->f_ops, ConInstance, Abstraction);
  Proto = (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *)Stream->Dev;
  BlockingMode = (BOOLEAN)((filp->Oflags & O_NONBLOCK) == 0);

  if (ShellHandleTypes[Stream->InstanceNum] != SH_HNDL_CON) {
    /*
     * ShellLib file interfaces have no notion of waiting on data.
     * Data is either available, or da_ConRawRead returns EFI_END_OF_FILE.
     */
    BlockingMode = FALSE;
  }

  do {
    Status = da_ConRawRead(filp, &RetChar);

    if (Status == EFI_NOT_READY && BlockingMode) {
      ASSERT(ShellHandleTypes[Stream->InstanceNum] == SH_HNDL_CON);
      ASSERT(DEVCON_IS_WIDE(Stream->InstanceNum));
      gBS->WaitForEvent(1, &Proto->WaitForKey, &Edex);
      continue;
    }

    break;
  } while (1);

  switch (Status) {
  case EFI_SUCCESS:
    if (DEVCON_IS_WIDE(Stream->InstanceNum)) {
      *((wchar_t *)Buffer) = RetChar;
    } else {
      *((char *)Buffer) = RetChar;
    }
    return 1;
  case EFI_NOT_READY:
    errno = EAGAIN;
    EFIerrno = Status;
    return -1;
  case EFI_END_OF_FILE:
    return 0;
  }

  errno = EIO;
  EFIerrno = Status;
  return -1;
}

/** Console-specific helper function for the fstat() function.

    st_size       Set to number of characters read for stdin and number written for stdout and stderr.
    st_physsize   1 for stdin, 0 if QueryMode error, else max X and Y coordinates for the current mode.
    st_curpos     0 for stdin, current X & Y coordinates for stdout and stderr
    st_blksize    Set to 1 since this is a character device

    All other members of the stat structure are left unchanged.

    @param[in]      filp          Pointer to file descriptor for this file.
    @param[out]     Buffer        Pointer to a stat structure to receive the information.
    @param[in,out]  Something     Ignored.

    @retval   0   Successful completion.
    @retval   -1  Either filp is not associated with a console stream, or
                  Buffer is NULL.  errno is set to EINVAL.
**/
static
int
EFIAPI
da_ConStat(
  struct __filedes   *filp,
  struct stat        *Buffer,
  void               *Something
  )
{
  ConInstance                        *Stream;

  Stream = BASE_CR(filp->f_ops, ConInstance, Abstraction);
  if ((Stream->Cookie != CON_COOKIE) ||    // Cookie == 'IoAb'
      (Buffer == NULL)) {
    errno     = EINVAL;
    EFIerrno = RETURN_INVALID_PARAMETER;
    return -1;
  }

  /*
   * Because we don't provide an st_blocks,
   * applications usually compute blocks by
   * dividing by the block size. Setting this
   * to 0 would be a mistake.
   */
  Buffer->st_blksize  = 1;
  Buffer->st_mode     = filp->f_iflags;
  Buffer->st_birthtime = time(NULL);
  Buffer->st_atime = Buffer->st_mtime = Buffer->st_birthtime;

  // ConGetPosition
  if(DEVCON_IS_IN(Stream->InstanceNum)) {
    // This is stdin
    Buffer->st_curpos    = 0;
    Buffer->st_size      = (off_t)Stream->NumRead;
    Buffer->st_physsize  = 1;
    Buffer->st_mode |= READ_PERMS;
  } else {
    Buffer->st_size = (off_t)Stream->NumWritten;
    Buffer->st_mode |= WRITE_PERMS;

    if (ShellHandleTypes[Stream->InstanceNum] != SH_HNDL_CON) {
      UINT64 Pos = 0;

      ShellGetFilePosition(ShellHandles[Stream->InstanceNum], &Pos);
      Buffer->st_curpos  = (off_t)Pos;
      Buffer->st_physsize = 1;
    } else {
      EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL    *Proto;
      XY_OFFSET                           CursorPos;
      INT32                               OutMode;
      UINTN                               ModeCol;
      UINTN                               ModeRow;

      Proto = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)Stream->Dev;
      CursorPos.XYpos.Column  = (UINT32)Proto->Mode->CursorColumn;
      CursorPos.XYpos.Row     = (UINT32)Proto->Mode->CursorRow;
      Buffer->st_curpos       = (off_t)CursorPos.Offset;

      OutMode  = Proto->Mode->Mode;
      EFIerrno = Proto->QueryMode(Proto, (UINTN)OutMode, &ModeCol, &ModeRow);
      if(RETURN_ERROR(EFIerrno)) {
        Buffer->st_physsize = 0;
      } else {
        CursorPos.XYpos.Column  = (UINT32)ModeCol;
        CursorPos.XYpos.Row     = (UINT32)ModeRow;
        Buffer->st_physsize     = (off_t)CursorPos.Offset;
      }
    }
  }
  return 0;
}

/** Console-specific helper for the ioctl system call.

    The console device does not directly participate in ioctl operations.
    This function completes the device abstraction and returns an error value
    to indicate that the function is not supported for this device.

    @retval   -1    Function is not supported for this device.
**/
static
int
EFIAPI
da_ConIoctl(
  struct __filedes   *filp,
  ULONGN              cmd,
  va_list             argp
  )
{
  errno   = ENODEV;
  return  -1;
}

STATIC SH_HNDL_TYPE
ShellHandleType(
  IN SHELL_FILE_HANDLE Handle
)
{
  UINT64 Pos;
  EFI_FILE_INFO *FileInfo;

  FileInfo = ShellGetFileInfo(Handle);
  if (FileInfo != NULL) {
    FreePool(FileInfo);
    return SH_HDNL_FILE;
  }

  if (ShellGetFilePosition(Handle, &Pos) == EFI_SUCCESS) {
    return SH_HNDL_PIPE;
  }

  return SH_HNDL_CON;
}

/** Open an abstract Console Device.

    @param[in]    DevNode       Pointer to the Device control structure for this stream.
    @param[in]    filp          Pointer to the new file control structure for this stream.
    @param[in]    DevInstance   Not used for the console device.
    @param[in]    Path          Not used for the console device.
    @param[in]    MPath         Not used for the console device.

    @retval   0   This console stream has been successfully opened.
    @retval   -1  The DevNode or filp pointer is NULL.
    @retval   -1  DevNode does not point to a valid console stream device.
**/
int
EFIAPI
da_ConOpen(
  DeviceNode         *DevNode,
  struct __filedes   *filp,
  int                 DevInstance,    // Not used for console devices
  wchar_t            *Path,           // Not used for console devices
  wchar_t            *MPath           // Not used for console devices
  )
{
  ConInstance    *Stream;
  UINT32          Instance;
  int             RetVal = -1;
  struct termios *Termio;

  if (filp == NULL || DevNode == NULL) {
    goto out;
  }

  Stream = (ConInstance *)DevNode->InstanceList;
  if (Stream->Cookie != CON_COOKIE) {
    goto out;
  }

  Termio = &IIO->Termio;
  Instance = Stream->InstanceNum;

  if (!DEVCON_IS_WIDE(Instance) &&
      ShellHandleTypes[Instance] == SH_HNDL_CON) {
    /*
     * SimpleText is inherently wide, no narrow support.
     */
    errno = ENOTSUP;
    EFIerrno = EFI_UNSUPPORTED;
    return -1;
  }

  if (ShellHandleTypes[Instance] != SH_HNDL_CON) {
    if (DEVCON_IS_WIDE(Instance) && DEVCON_IS_IN(Instance)) {
      /*
       * For wide input, ignore the first UTF16 tag.
       */
      RemoveFileTag(ShellHandles[Instance]);
    } else if ((DEVCON_IS_OUT(Instance) ||
                DEVCON_IS_ERR(Instance)) &&
               !DEVCON_IS_WIDE(Instance)) {
      /*
       * For narrow devices, undo the damage that Shell might do
       * writing the UTF16 tag on | and >.
       */
      ShellSetFilePosition(ShellHandles[Instance], 0);
    }
  }

  if(Instance < DEVCON_NUM) {
    filp->f_iflags |= _S_IFCHR | _S_ITTY;
    if (DEVCON_IS_WIDE(Instance)) {
      filp->f_iflags |= _S_IWTTY;
    }
    filp->f_offset = 0;
    filp->f_ops = &Stream->Abstraction;
    filp->devdata = (void *)IIO;
    RetVal = 0;

    if((filp->Oflags & O_TTY_INIT) != 0) {
      if (ShellHandleTypes[Instance] == SH_HNDL_CON &&
          DEVCON_IS_WIDE(Instance)) {
        if (DEVCON_IS_IN(Instance)) {
          Termio->c_iflag |= ICRNL | IGNSPEC;
          Termio->c_lflag |= ICANON;
        } else {
          Termio->c_lflag |= ECHO | ECHOE | ECHONL;
          Termio->c_oflag |= OPOST | ONLCR | OXTABS | ONOEOT | ONOCR | ONLRET | OCTRL;
        }
      }
    }
  }

 out:
  if (RetVal < 0) {
    EFIerrno = RETURN_INVALID_PARAMETER;
    errno = EINVAL;
  }

  return RetVal;
}

/** Flush a console device's IIO buffers.

    Flush the IIO Input or Output buffers associated with the specified file.

    If the console is open for output, write any unwritten data in the associated
    output buffer (stdout or stderr) to the console.

    If the console is open for input, discard any remaining data
    in the input buffer.

    @param[in]    filp    Pointer to the target file's descriptor structure.

    @retval     0     Always succeeds
**/
static
int
EFIAPI
da_ConFlush(
  struct __filedes *filp
)
{
  cFIFO      *OutBuf;
  ssize_t     NumProc;
  int         Flags;

  if(filp->MyFD == STDERR_FILENO) {
    OutBuf = IIO->ErrBuf;
  } else {
    OutBuf = IIO->OutBuf;
  }

  Flags = filp->Oflags & O_ACCMODE;   // Get the device's open mode
  if (Flags != O_WRONLY)  {   // (Flags == O_RDONLY) || (Flags == O_RDWR)
    // Readable so discard the contents of the input buffer
    IIO->InBuf->Flush(IIO->InBuf, UNICODE_STRING_MAX);
  }

  if (Flags != O_RDONLY)  {   // (Flags == O_WRONLY) || (Flags == O_RDWR)
    NumProc = OutBuf->Read(OutBuf, gMD->UString, UNICODE_STRING_MAX-1);
    if (NumProc > 0) {
      (void) da_ConWrite(filp, NULL, NumProc, gMD->UString);
      OutBuf->Flush(OutBuf, UNICODE_STRING_MAX);
    }
  }

  return 0;
}

/** Close an open file.

    @param[in]  filp    Pointer to the file descriptor structure for this file.

    @retval   0     The file has been successfully closed.
    @retval   -1    filp does not point to a valid console descriptor.
**/
static
int
EFIAPI
da_ConClose(
  IN      struct __filedes   *filp
)
{
  ConInstance    *Stream;

  Stream = BASE_CR(filp->f_ops, ConInstance, Abstraction);
  // Quick check to see if Stream looks reasonable
  if(Stream->Cookie != CON_COOKIE) {    // Cookie == 'IoAb'
    errno     = EINVAL;
    EFIerrno = RETURN_INVALID_PARAMETER;
    return -1;    // Looks like a bad File Descriptor pointer
  }
  // Stream and filp look OK, so continue.
  // Flush the I/O buffers
  (void) da_ConFlush(filp);

  // Break the connection to IIO
  filp->devdata = NULL;
  return 0;
}

#include  <sys/poll.h>
/*  Returns a bit mask describing which operations could be completed immediately.

    Testable Events for this device are:
    (POLLIN | POLLRDNORM)   A Unicode character is available to read
    (POLLIN)                A ScanCode is ready.
    (POLLOUT)               The device is ready for output - always set on stdout and stderr.

    Non-testable Events which are only valid in return values are:
      POLLERR                 The specified device is not one of stdin, stdout, or stderr.
      POLLHUP                 The specified stream has been disconnected
      POLLNVAL                da_ConPoll was called with an invalid parameter.

  NOTE: The "Events" handled by this function are not UEFI events.

    @param[in]  filp      Pointer to the file control structure for this stream.
    @param[in]  events    A bit mask identifying the events to be examined
                          for this device.

    @return   Returns a bit mask comprised of both testable and non-testable
              event codes indicating both the state of the operation and the
              status of the device.
*/
static
short
EFIAPI
da_ConPoll(
  struct __filedes   *filp,
  short              events
  )
{
  ConInstance                      *Stream;
  EFI_STATUS                        Status = RETURN_SUCCESS;
  short                             RdyMask = 0;

  Stream = BASE_CR(filp->f_ops, ConInstance, Abstraction);
  // Quick check to see if Stream looks reasonable
  if(Stream->Cookie != CON_COOKIE) {    // Cookie == 'IoAb'
    errno     = EINVAL;
    EFIerrno = RETURN_INVALID_PARAMETER;
    return POLLNVAL;    // Looks like a bad filp pointer
  }

  if (DEVCON_IS_IN(Stream->InstanceNum)) {
    // STDIN: Only input is supported for this device

    if (ShellHandleTypes[Stream->InstanceNum] != SH_HNDL_CON) {
      RdyMask = POLLIN | POLLRDNORM;
    } else {
      Status = da_ConRawRead (filp, &Stream->UnGetKey);
      if(Status == RETURN_SUCCESS) {
        RdyMask = POLLIN;
        if ((Stream->UnGetKey <  TtyFunKeyMin)   ||
            (Stream->UnGetKey >= TtyFunKeyMax)) {
          RdyMask |= POLLRDNORM;
        }
      } else {
        Stream->UnGetKey  = CHAR_NULL;
      }
    }
  } else if(Stream->InstanceNum < DEVCON_NUM) {  // Not 0, is it 1 or 2?
    // (STDOUT || STDERR): Only output is supported for this device
    RdyMask = POLLOUT;
  } else {
    RdyMask = POLLERR;    // Not one of the standard streams
  }
  EFIerrno = Status;

  return (RdyMask & (events | POLL_RETONLY));
}

/** Construct the Console stream devices: stdin, stdout, stderr.

    Allocate the instance structure and populate it with the information for
    each stream device.
**/
RETURN_STATUS
EFIAPI
__Cons_construct(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  ConInstance    *Stream;
  RETURN_STATUS   Status;
  int             i;
  struct termios *Termio;
  SHELL_FILE_HANDLE StdIn;
  SHELL_FILE_HANDLE StdOut;
  SHELL_FILE_HANDLE StdErr;
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters;
  EFI_SHELL_INTERFACE           *EfiShellInterface;

  Status = gBS->HandleProtocol(ImageHandle, &gEfiShellParametersProtocolGuid,
                               (VOID **) &ShellParameters);
  if (Status == EFI_SUCCESS) {
    StdIn = ShellParameters->StdIn;
    StdOut = ShellParameters->StdOut;
    StdErr = ShellParameters->StdErr;
  } else {
    Status = gBS->HandleProtocol(ImageHandle,
                                 &gEfiShellInterfaceGuid,
                                 (VOID **) &EfiShellInterface);
    if (EFI_ERROR(Status)) {
      return Status;
    }

    StdIn = EfiShellInterface->StdIn;
    StdOut = EfiShellInterface->StdOut;
    StdErr = EfiShellInterface->StdErr;
  }

  Status = RETURN_OUT_OF_RESOURCES;
  ConInstanceList = (ConInstance *)AllocateZeroPool(DEVCON_NUM * sizeof(ConInstance));
  if(ConInstanceList == NULL) {
    return Status;
  }

  IIO = New_cIIO();
  if(IIO == NULL) {
    FreePool(ConInstanceList);
    return Status;
  }

  Termio = &IIO->Termio;
  Termio->c_cc[VERASE]  = 0x08;   // ^H Backspace
  Termio->c_cc[VKILL]   = 0x15;   // ^U
  Termio->c_cc[VINTR]   = 0x03;   // ^C Interrupt character
  Termio->c_cc[VEOF]    = 0x04;   // ^D EOF

  Status = RETURN_SUCCESS;
  for( i = 0; i < DEVCON_NUM; ++i) {
    // Get pointer to instance.
    Stream = &ConInstanceList[i];

    Stream->Cookie      = CON_COOKIE;
    Stream->InstanceNum = i;
    Stream->CharState.A = 0;    // Start in the initial state

    switch(i) {
    case DEVCON_STDIN:
    case DEVCON_NSTDIN:
      Stream->Dev = SystemTable->ConIn;
      ShellHandles[i] = StdIn;
      ShellHandleTypes[i] = ShellHandleType(ShellHandles[i]);
      break;
    case DEVCON_STDOUT:
    case DEVCON_NSTDOUT:
      Stream->Dev = SystemTable->ConOut;
      ShellHandles[i] = StdOut;
      ShellHandleTypes[i] = ShellHandleType(ShellHandles[i]);
      break;
    case DEVCON_STDERR:
    case DEVCON_NSTDERR:
      Stream->Dev = SystemTable->StdErr;
      ShellHandles[i] = StdErr;
      ShellHandleTypes[i] = ShellHandleType(ShellHandles[i]);

      if (SystemTable->StdErr == NULL ||
          ShellHandleTypes[i] == SH_HNDL_CON) {
        /*
         * If we're not redirecting StdErr anywhere, use
         * StdOut instead, to deal with systems where
         * StdErr goes to serial or some other random
         * black hole.
         */
        Stream->Dev = SystemTable->ConOut;
        ShellHandles[i] = StdOut;
      }
      break;
    default:
      return RETURN_VOLUME_CORRUPTED;     // This is a "should never happen" case.
    }

    Stream->Abstraction.fo_close    = &da_ConClose;
    Stream->Abstraction.fo_read     = &da_ConRead;
    Stream->Abstraction.fo_write    = &da_ConWrite;
    Stream->Abstraction.fo_stat     = &da_ConStat;
    Stream->Abstraction.fo_lseek    = &da_ConSeek;
    Stream->Abstraction.fo_fcntl    = &fnullop_fcntl;
    Stream->Abstraction.fo_ioctl    = &da_ConIoctl;
    Stream->Abstraction.fo_poll     = &da_ConPoll;
    Stream->Abstraction.fo_flush    = &da_ConFlush;
    Stream->Abstraction.fo_delete   = &fbadop_delete;
    Stream->Abstraction.fo_mkdir    = &fbadop_mkdir;
    Stream->Abstraction.fo_rmdir    = &fbadop_rmdir;
    Stream->Abstraction.fo_rename   = &fbadop_rename;

    Stream->NumRead     = 0;
    Stream->NumWritten  = 0;
    Stream->UnGetKey    = CHAR_NULL;

    if(Stream->Dev == NULL) {
      continue;                 // No device for this stream.
    }

    ConNode[i] = __DevRegister(stdioNames[i], NULL, &da_ConOpen, Stream,
                               1, sizeof(ConInstance), stdioFlags[i]);
    if(ConNode[i] == NULL) {
      Status = EFIerrno;    // Grab error code that DevRegister produced.
      break;
    }
    Stream->Parent = ConNode[i];
  }

  /* Initialize Ioctl flags until Ioctl is really implemented. */
  TtyCooked = TRUE;
  TtyEcho   = TRUE;
  return  Status;
}

RETURN_STATUS
EFIAPI
__Cons_deconstruct(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  int   i;

  for(i = 0; i < DEVCON_NUM; ++i) {
    if(ConNode[i] != NULL) {
      FreePool(ConNode[i]);
    }
  }
  if(ConInstanceList != NULL) {
    FreePool(ConInstanceList);
  }
  if(IIO != NULL) {
    IIO->Delete(IIO);
    IIO = NULL;
  }

  return RETURN_SUCCESS;
}

/* ######################################################################### */
#if 0 /* Not implemented (yet?) for Console */

static
int
EFIAPI
da_ConCntl(
  struct __filedes *filp,
  UINT32,
  void *,
  void *
  )
{
}
#endif  /* Not implemented for Console */
