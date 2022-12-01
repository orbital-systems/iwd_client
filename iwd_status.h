//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#pragma once

typedef enum {
    IWD_STATUS_SUCCESS = 0,

    // Maps to iwd DBUS errors
    IWD_STATUS_ABORTED,
    IWD_STATUS_BUSY,
    IWD_STATUS_FAILED,
    IWD_STATUS_NO_AGENT,
    IWD_STATUS_NOT_SUPPORTED,
    IWD_STATUS_TIMEOUT,
    IWD_STATUS_IN_PROGRESS,
    IWD_STATUS_NOT_CONFIGURED,
    IWD_STATUS_INVALID_ARGUMENTS,
    IWD_STATUS_NOT_CONNECTED,
    IWD_STATUS_NOT_FOUND,
    IWD_STATUS_SERVICE_SET_OVERLAP,
    IWD_STATUS_ALREADY_PROVISIONED,
    IWD_STATUS_NOT_HIDDEN,
    IWD_STATUS_INVALID_FORMAT,

    IWD_STATUS_STATION_NOT_FOUND, // Wifi interface not found
    IWD_STATUS_NETWORK_NOT_FOUND, // Wifi SSID not found
    IWD_STATUS_CONNECT_OVERRIDEN, // A later connect was run before this connect could complete

    IWD_STATUS_DBUS_SEND_FAILED,
    IWD_STATUS_DBUS_ABORTED, // DBUS call was destroyed before we got a proper reply
    IWD_STATUS_DBUS_REPLY_ERROR, // Error in DBUS reply. See logs
    IWD_STATUS_DBUS_PARSE_FAILED, // Can't parse the reply properly. See logs

    IWD_STATUS_OTHER_ERROR, // Some other unknown error. See logs

    // Connect hidden
    // net.connman.iwd.Busy
    // net.connman.iwd.Failed
    // net.connman.iwd.InvalidArguments
    // net.connman.iwd.NotConfigured
    // net.connman.iwd.NotConnected
    // net.connman.iwd.NotFound
    // net.connman.iwd.ServiceSetOverlap
    // net.connman.iwd.AlreadyProvisioned
    // net.connman.iwd.NotHidden

} iwd_status_t;

iwd_status_t iwd_status_parse_dbus_error(const char *errstr);
