#include <fifo-common.h>
#include <windows.h>
#include "last-error-win.h"
#include <assert.h>

void last_error_msg(const char *msg, DWORD err)
{
    assert(msg);
    char msgerr[256];

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgerr,
        sizeof(msgerr), NULL);

    FIFO_PRINT_ERROR("%s Error: %lu - %s\n", msg, err, msgerr);
}
