//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_client.h"

#include "iwd_proxies.h"

#include <assert.h>

typedef struct {
    iwd_client_scan_started_cb_t done_cb;
    void *user_data;
} scan_oper_t;

static scan_oper_t *scan_oper_create(iwd_client_scan_started_cb_t done_cb,
                                     void *user_data)
{
    scan_oper_t *oper = l_new(scan_oper_t, 1);
    oper->done_cb = done_cb;
    oper->user_data = user_data;
    return oper;
}

static void scan_oper_run_callback(scan_oper_t *oper, iwd_status_t status)
{
    assert(oper);
    assert(oper->done_cb);
    oper->done_cb(status, oper->user_data);
    oper->done_cb = NULL; // Mark it called
}

static void scan_oper_destroy(scan_oper_t *oper)
{
    assert(oper);

    // Callback has not been run yet. Operation was propably aborted
    if (oper->done_cb) {
        l_error("iwd_client: Scan was DBUS-aborted?");
        scan_oper_run_callback(oper, IWD_STATUS_DBUS_ABORTED);
    }
    l_free(oper);
}

static void scan_oper_error_and_destroy(scan_oper_t *oper, iwd_status_t status)
{
    scan_oper_run_callback(oper, status);
    scan_oper_destroy(oper);
}

static void scan_reply_handler(__attribute__((unused)) struct l_dbus_proxy *unused_proxy,
                               struct l_dbus_message *msg,
                               void *user_data)
{
    scan_oper_t *oper = (scan_oper_t *)user_data;
    assert(oper);

    iwd_status_t status = IWD_STATUS_SUCCESS;

    if (l_dbus_message_is_error(msg)) {
        const char *name = "";
        const char *text = "";
        (void)l_dbus_message_get_error(msg, &name, &text);

        l_error("iwd_client: Scan failed. name='%s' text='%s'", name, text);

        // Possible errors:
        // net.connman.iwd.Busy
        // net.connman.iwd.Failed
        status = iwd_status_parse_dbus_error(name);
    }

    scan_oper_run_callback(oper, status);
}

static void scan_destroy_handler(void *user_data)
{
    scan_oper_t *oper = (scan_oper_t *)user_data;
    scan_oper_destroy(oper);
}

bool iwd_client_scan_start_async(const char *device_name,
                                 iwd_client_scan_started_cb_t scan_started_cb,
                                 void *user_data)
{
    assert(scan_started_cb);

    l_info("iwd_client: Calling Scan on %s", device_name);

    scan_oper_t *oper = scan_oper_create(scan_started_cb, user_data);

    struct l_dbus_proxy *proxy_station = iwd_proxies_get_station_for_device(device_name);
    if (!proxy_station) {
        l_error("iwd_client: Station for device='%s' is not found", device_name);
        scan_oper_error_and_destroy(oper, IWD_STATUS_STATION_NOT_FOUND);
        return false;
    }

    uint32_t callid = l_dbus_proxy_method_call(proxy_station, "Scan",
                                               NULL, // No arguments needs setup into message
                                               scan_reply_handler,
                                               oper, // user_data
                                               scan_destroy_handler);
    if (callid == 0) {
        scan_oper_error_and_destroy(oper, IWD_STATUS_DBUS_SEND_FAILED);
        return false;
    }

    return true;
}
