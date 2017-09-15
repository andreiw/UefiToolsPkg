/** @file
  Definitions for the Interactive IO library.

  The functions assume that isatty() is TRUE at the time they are called.

  Copyright (C) 2017 Andrei Evgenievich Warkentin
  Copyright (c) 2016, Daryl McDaniel. All rights reserved.<BR>
  Copyright (c) 2012, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License which accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
#include  <Uefi.h>
#include  <Library/MemoryAllocationLib.h>

#include  <LibConfig.h>

#include  <assert.h>
#include  <errno.h>
#include  <sys/syslimits.h>
#include  <sys/termios.h>
#include  <Device/IIO.h>
#include  <MainData.h>
#include  "IIOutilities.h"
#include  "IIOechoCtrl.h"

#include <Library/DebugLib.h>

// Instrumentation used for debugging
#define   IIO_C_DEBUG   0       ///< Set to 1 to enable instrumentation, 0 to disable

#if IIO_C_DEBUG
  static volatile size_t  IIO_C_WRemainder = 0;   ///< Characters in Out buffer after IIO_Write
  static volatile size_t  IIO_C_RRemainder = 0;   ///< Characters in In buffer after IIO_Read

  #define W_INSTRUMENT  IIO_C_WRemainder =
  #define R_INSTRUMENT  IIO_C_RRemainder =
#else // ! IIO_C_DEBUG -- don't instrument code
  #define W_INSTRUMENT  (void)
  #define R_INSTRUMENT  (void)
#endif  // IIO_C_DEBUG

static void
buffer_narrow(const wchar_t *src, char *dest, size_t count)
{
  size_t i;

  for (i = 0; i < count; i++) {
    if (*src > 0xFF) {
      *dest = '?';
    } else {
      *dest = *src;
    }

    src++;
    dest++;
  }
}

/** Read from an Interactive IO device.

  NOTE: If _S_IWTTY is set, the internal buffer contains WIDE characters.
        They will need to be converted to narrow when returned.

    Input is line buffered if ICANON is set,
    otherwise MIN determines how many characters to input.
    Currently MIN is always zero, meaning 0 or 1 character is input in
    noncanonical mode.

    @param[in]    filp        Pointer to the descriptor of the device (file) to be read.
    @param[in]    BufferSize  Maximum number of bytes to be returned to the caller.
    @param[out]   Buffer      Pointer to the buffer where the input is to be stored.

    @retval   -1    An error occurred.  No data is available.
    @retval    0    No data was available.  Try again later.
    @retval   >0    The number of bytes consumed by the returned data.
**/
static
ssize_t
EFIAPI
IIO_Read(
  struct __filedes *filp,
  size_t BufferSize,
  VOID *Buffer
  )
{
  cIIO     *This;
  ssize_t   NumRead;
  tcflag_t  Flags;

  NumRead = -1;
  This = filp->devdata;

  if (This == NULL) {
    return -1;
  }

  Flags = This->Termio.c_lflag;
  if(Flags & ICANON) {
    NumRead = IIO_CanonRead(filp);
  }
  else {
    NumRead = IIO_NonCanonRead(filp);
  }

  if (NumRead <= 0) {
    return NumRead;
  }

  /*
   * NumRead here is elements in FIFO, not bytes.
   */

  // At this point, the input has been accumulated in the input buffer.
  if(filp->f_iflags & _S_IWTTY) {
    // Data in InBuf is wide characters. Narrow down.
    // First, convert the most required FIFO elements into a linear buffer.
    NumRead = This->InBuf->Copy(This->InBuf, gMD->UString2,
                                MIN(MIN(UNICODE_STRING_MAX-1, NumRead), BufferSize));
    buffer_narrow((const wchar_t *)gMD->UString2, (char *) Buffer, NumRead);

    // Consume the translated characters
    (void) This->InBuf->Flush(This->InBuf, NumRead);
  } else {
    // Data in InBuf is narrow characters.  Use verbatim.
    NumRead = This->InBuf->Read(This->InBuf, Buffer,
                                MIN(NumRead, (INT32)BufferSize));
  }
#if IIO_C_DEBUG
  IIO_C_RRemainder = This->InBuf->Count(This->InBuf, AsElements);
#endif // IIO_C_DEBUG

  return NumRead;
}

/** Handle write to a Terminal (Interactive) device.

    Processes characters from buffer buf and writes them to the Terminal device
    specified by filp.

    The parameter buf points to a narrow buffer to be output. This is processed
    and buffered one character at a time by IIO_WriteOne() which handles TAB
    expansion, NEWLINE to CARRIAGE_RETURN + NEWLINE expansion, as well as
    basic line editing functions. The number of characters actually written to
    the output device will seldom equal the number of characters consumed from
    buf.

    In this implementation, all of the special characters processed by
    IIO_WriteOne() are single-byte characters with values less than 128.
    (7-bit ASCII or the single-byte UTF-8 characters)

    Every byte that is not one of the recognized special characters is passed,
    unchanged, to the Terminal device.

    @param[in]      filp      Pointer to a file descriptor structure.
    @param[in]      buf       Pointer to the narrow buffer to be output.
    @param[in]      N         Number of bytes in buf.

    @retval   >=0     Number of bytes consumed from buf and sent to the
                      Terminal device.
**/
static
ssize_t
EFIAPI
IIO_Write(
  struct __filedes *filp,
  const char *buf,
  ssize_t N
  )
{
  cIIO       *This;
  cFIFO      *OutBuf;
  ssize_t     NumConsumed;
  size_t      CharLen;
  UINTN       MaxColumn;
  UINTN       MaxRow;
  wchar_t     OutChar;
  int         OutMode;

  NumConsumed = -1;

  /*
   * Determine what the current screen size is. Also validates the output device.
   *
   * andreiw: even if we're redirecting via a Shell SHELL_FILE_HANDLE object,
   * we still want to use the "real" ConOut sizing info.
   */
  OutMode = IIO_GetOutputSize(filp->MyFD, &MaxColumn, &MaxRow);
  if (OutMode < 0) {
    return -1;
  }

  This = filp->devdata;
  if (This == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(filp->MyFD == STDERR_FILENO) {
    OutBuf = This->ErrBuf;
  } else {
    OutBuf = This->OutBuf;
  }

  /*  Set the maximum screen dimensions. */
  This->MaxColumn = MaxColumn;
  This->MaxRow    = MaxRow;

  /*  Record where the cursor is at the beginning of the Output operation. */
  (void)IIO_GetCursorPosition(filp->MyFD, &This->InitialXY.Column, &This->InitialXY.Row);
  This->CurrentXY.Column  = This->InitialXY.Column;
  This->CurrentXY.Row     = This->InitialXY.Row;

  NumConsumed = 0;
  while(NumConsumed < N) {
    OutChar = buf[NumConsumed] & 0xFF;

    if (IIO_WriteOne(filp, OutBuf, OutChar) >= 0) {
      NumConsumed++;
    } else {
      if (errno == ENOSPC) {
        break;
      }

      return -1; // Ummm?
    }
  }

  // At this point, the characters to write are in OutBuf
  CharLen = OutBuf->Copy(OutBuf, gMD->UString, UNICODE_STRING_MAX-1);

  if (CharLen != 0) {
    if(filp->f_iflags & _S_IWTTY) {
       /*
       * This stupidity accomodates ConOut->OutputString not actually
       * taking a size. This is bad, but calling OutputString a million
       * time would make I/O slower, and only IIO knows how to
       * call daConsole's fo_write, anyway.
       *
       * There's an assert in daConsole.c to catch this now.
       */
      gMD->UString[CharLen] = 0;
      CharLen = filp->f_ops->fo_write(filp, NULL, CharLen, gMD->UString);
    } else {
      buffer_narrow((const wchar_t *) gMD->UString,
                    (char *) gMD->UString2,
                    CharLen);
      CharLen = filp->f_ops->fo_write(filp, NULL, CharLen, (char *) gMD->UString2);
    }

    W_INSTRUMENT OutBuf->Flush(OutBuf, CharLen);
  }

  return NumConsumed;
}

/** Echo a character to an output device.
    Performs translation and edit processing depending upon termios flags.

    @param[in]    filp      A pointer to a file descriptor structure.
    @param[in]    EChar     The character to echo.
    @param[in]    EchoIsOK  TRUE if the caller has determined that characters
                            should be echoed.  Otherwise, just buffer.

    @return   Returns the number of characters actually output.
**/
static
ssize_t
EFIAPI
IIO_Echo(
  struct __filedes *filp,
  wchar_t           EChar,
  BOOLEAN           EchoIsOK
  )
{
  cIIO     *This;
  ssize_t   NumWritten;
  cFIFO    *OutBuf;
  tcflag_t  LFlags;

  NumWritten = -1;
  This = filp->devdata;

  if (This == NULL) {
    errno = EINVAL;
    return -1;
  }

  OutBuf = This->OutBuf;
  LFlags = This->Termio.c_lflag & (ECHOK | ECHOE);

  if((EChar >= TtyFunKeyMin) && (EChar < TtyFunKeyMax)) {
    // A special function key was pressed, buffer it, don't echo, and activate.
    // Process and buffer the character.  May produce multiple characters.
    IIO_EchoOne(filp, EChar, FALSE); // Don't echo this character
    EChar   = CHAR_LINEFEED;         // Every line must end with '\n' (legacy)
  }

  // Process and buffer the character.  May produce multiple characters.
  IIO_EchoOne(filp, EChar, EchoIsOK);

  // At this point, the character(s) to write are in OutBuf
  NumWritten = OutBuf->Copy(OutBuf, gMD->UString, UNICODE_STRING_MAX-1);

  if((EChar == IIO_ECHO_KILL) && (LFlags & ECHOE) && EchoIsOK) {
    // Position the cursor to the start of input.
    (void)IIO_SetCursorPosition(filp, &This->InitialXY);
  }

  if (NumWritten) {
    if(filp->f_iflags & _S_IWTTY) {
      /*
       * This stupidity accomodates ConOut->OutputString not actually
       * taking a size. This is bad, but calling OutputString a million
       * time would make I/O slower, and only IIO knows how to
       * call daConsole's fo_write, anyway.
       *
       * There's an assert in daConsole.c to catch this now.
       */
      gMD->UString[NumWritten] = 0;
      NumWritten = filp->f_ops->fo_write(filp, NULL, NumWritten, gMD->UString);
    } else {
      buffer_narrow((const wchar_t *) gMD->UString,  (char *) gMD->UString2, NumWritten);
      NumWritten = filp->f_ops->fo_write(filp, NULL, NumWritten, (char *) gMD->UString2);
    }

    (void)OutBuf->Flush(OutBuf, NumWritten);
  }

  if(EChar == IIO_ECHO_KILL) {
    if(LFlags == ECHOK) {
      NumWritten = IIO_WriteOne(filp, OutBuf, CHAR_LINEFEED);
    } else if((LFlags & ECHOE) && EchoIsOK) {
      // Position the cursor to the start of input.
      (void)IIO_SetCursorPosition(filp, &This->InitialXY);
    }

    NumWritten = 0;
  }

  return NumWritten;
}

static
void
FifoDelete(cFIFO *Member)
{
  if(Member != NULL) {
    Member->Delete(Member);
  }
}

/** Destructor for an IIO instance.

    Releases all resources used by a particular IIO instance.
**/
static
void
EFIAPI
IIO_Delete(
  cIIO *Self
  )
{
  if(Self != NULL) {
    FifoDelete(Self->ErrBuf);
    FifoDelete(Self->OutBuf);
    FifoDelete(Self->InBuf);
    if(Self->AttrBuf != NULL) {
      FreePool(Self->AttrBuf);
    }
    FreePool(Self);
  }
}

/** Constructor for new IIO instances.

    @return   Returns NULL or a pointer to a new IIO instance.
**/
cIIO *
EFIAPI
New_cIIO(void)
{
  cIIO     *IIO;
  cc_t     *TempBuf;
  int       i;

  IIO = (cIIO *)AllocateZeroPool(sizeof(cIIO));
  if(IIO != NULL) {
    IIO->InBuf    = New_cFIFO(MAX_INPUT, sizeof(wchar_t));
    IIO->OutBuf   = New_cFIFO(MAX_OUTPUT, sizeof(wchar_t));
    IIO->ErrBuf   = New_cFIFO(MAX_OUTPUT, sizeof(wchar_t));
    IIO->AttrBuf  = (UINT8 *)AllocateZeroPool(MAX_OUTPUT);

    if((IIO->InBuf   == NULL) || (IIO->OutBuf   == NULL) ||
       (IIO->ErrBuf  == NULL) || (IIO->AttrBuf  == NULL))
    {
      IIO_Delete(IIO);
      IIO = NULL;
    }
    else {
      IIO->Delete = IIO_Delete;
      IIO->Read   = IIO_Read;
      IIO->Write  = IIO_Write;
      IIO->Echo   = IIO_Echo;
    }
    // Initialize Termio member
    TempBuf = &IIO->Termio.c_cc[0];
    TempBuf[0] = 8;                 // Default length for TABs
    for(i=1; i < NCCS; ++i) {
      TempBuf[i] = _POSIX_VDISABLE;
    }
    TempBuf[VMIN]         = 0;
    TempBuf[VTIME]        = 0;
    IIO->Termio.c_ispeed  = B115200;
    IIO->Termio.c_ospeed  = B115200;
    IIO->Termio.c_iflag   = 0;
    IIO->Termio.c_oflag   = 0;
    IIO->Termio.c_cflag   = 0;
    IIO->Termio.c_lflag   = 0;
  }
  return IIO;
}
