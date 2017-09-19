# ShellErrVars

This tool is meant to be run from the UEFI Shell,
and will set shell environment variables to
quickly test for and look up EFI return statuses
and error codes.

Usage:

    fs16:> ShellErrVars.efi [-i]

A few examples of what gets set:

    evar_RETURN_SUCCESS = 0x0
    evar_0x0 = RETURN_SUCCESS
    svar_RETURN_SUCCESS = 0x0
    svar_0x0 = RETURN_SUCCESS
    ...
    evar_RETURN_HTTP_ERROR = 0x23
    evar_0x23 = RETURN_HTTP_ERROR
    svar_RETURN_HTTP_ERROR = 0x8000000000000023
    svar_0x8000000000000023 = RETURN_HTTP_ERROR
    ...
    wvar_RETURN_WARN_WRITE_FAILURE = 0x3
    wvar_0x3 = RETURN_WARN_WRITE_FAILURE
    svar_RETURN_WARN_WRITE_FAILURE = 0x3
    svar_0x3 = RETURN_WARN_WRITE_FAILURE

`svar_` refers to the raw RETURN_STATUS value, while
`evar_` will strip the high error bit, since that's
the way the UEFI Shell `%lasterror%` reports status.

Note: `svar_`, `wvar_` and reverse (value to name) variables
are only set with the `-i` (interactive) flag, as they
are mostly useless in a script setting. Sadly, the UEFI shell is too
dumb to do double expansion.

Examples of use:

    FS1:\> echo %evar_0x0%
    RETURN_SUCCESS
    FS1:\> echo %evar_0xE%
    RETURN_NOT_FOUND
    FS1:\> echo %svar_RETURN_NOT_FOUND%
    0x800000000000000E

In a script:

    if x%lasterror% eq x%evar_RETURN_NOT_FOUND%
        echo Something wasn't found.
    endif
