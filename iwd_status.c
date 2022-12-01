//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_status.h"

#include "iwd_util.h"

#include <ell/ell.h>

static const char * const error_table[] = {
    [IWD_STATUS_ABORTED] = "Aborted",
    [IWD_STATUS_BUSY] = "Busy",
    [IWD_STATUS_FAILED] = "Failed",
    [IWD_STATUS_NO_AGENT] = "NoAgent",
    [IWD_STATUS_NOT_SUPPORTED] = "NotSupported",
    [IWD_STATUS_TIMEOUT] = "Timeout",
    [IWD_STATUS_IN_PROGRESS] = "InProgress",
    [IWD_STATUS_NOT_CONFIGURED] = "NotConfigured",
    [IWD_STATUS_INVALID_ARGUMENTS] = "InvalidArguments",
    [IWD_STATUS_NOT_CONNECTED] = "NotConnected",
    [IWD_STATUS_NOT_FOUND] = "NotFound",
    [IWD_STATUS_SERVICE_SET_OVERLAP] = "ServiceSetOverlap",
    [IWD_STATUS_ALREADY_PROVISIONED] = "AlreadyProvisioned",
    [IWD_STATUS_NOT_HIDDEN] = "NotHidden",
    [IWD_STATUS_INVALID_FORMAT] = "InvalidFormat",
};

iwd_status_t iwd_status_parse_dbus_error(const char *errstr)
{
    // Error is on the form "net.connman.iwd.<Error>". Only match the <Error> part
    const char *part = strrchr(errstr, '.');
    if (part == NULL) {
        return IWD_STATUS_OTHER_ERROR;
    }
    part++; // Move past the last '.'

    for (iwd_status_t status = IWD_STATUS_ABORTED; status <= IWD_STATUS_INVALID_FORMAT; status++) {
        if (streq(part, error_table[status])) {
            return status;
        }
    }

    return IWD_STATUS_OTHER_ERROR;
}
