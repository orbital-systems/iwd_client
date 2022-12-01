//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_client.h"

#include "iwd_proxies.h"

#include <assert.h>

typedef struct {
    iwd_client_forget_done_cb_t done_cb;
    void *user_data;
} forget_oper_t;

static forget_oper_t *forget_oper_create(iwd_client_forget_done_cb_t done_cb,
                                         void *user_data)
{
    forget_oper_t *oper = l_new(forget_oper_t, 1);
    oper->done_cb = done_cb;
    oper->user_data = user_data;
    return oper;
}

static void forget_oper_run_callback(forget_oper_t *oper, iwd_status_t status)
{
    assert(oper);
    assert(oper->done_cb);
    oper->done_cb(status, oper->user_data);
    oper->done_cb = NULL; // Mark it called
}

static void forget_oper_destroy(forget_oper_t *oper)
{
    assert(oper);

    // Callback has not been run yet. Operation was propably aborted
    if (oper->done_cb) {
        l_error("iwd_client: Forget was DBUS-aborted?");
        forget_oper_run_callback(oper, IWD_STATUS_DBUS_ABORTED);
    }
    l_free(oper);
}

static void forget_oper_error_and_destroy(forget_oper_t *oper, iwd_status_t status)
{
    forget_oper_run_callback(oper, status);
    forget_oper_destroy(oper);
}

static void forget_reply_handler(__attribute__((unused)) struct l_dbus_proxy *unused_proxy,
                                  struct l_dbus_message *msg,
                                  void *user_data)
{
    forget_oper_t *oper = (forget_oper_t *)user_data;
    assert(oper);

    if (l_dbus_message_is_error(msg)) {
        const char *name = NULL;
        const char *text = NULL;
        if (l_dbus_message_get_error(msg, &name, &text)) {
            l_error("iwd_client: Forget failed. name='%s' text='%s'", name, text);
        }
        iwd_status_t status = iwd_status_parse_dbus_error(name);
        forget_oper_run_callback(oper, status);
    }
    else {
        l_info("iwd_client: Forget was successful!");
        forget_oper_run_callback(oper, IWD_STATUS_SUCCESS);
    }
}

static void forget_destroy_handler(void *user_data)
{
    forget_oper_t *oper = (forget_oper_t *)user_data;
    forget_oper_destroy(oper);
}

bool iwd_client_forget(const char *ssid, iwd_client_forget_done_cb_t forget_done_cb, void *user_data)
{
    assert(forget_done_cb);
    assert(ssid);

    l_info("iwd_client: Forgetting ssid='%s'", ssid);

    forget_oper_t *oper = forget_oper_create(forget_done_cb, user_data);

    struct l_dbus_proxy *proxy_knownnetwork = iwd_proxies_get_knownnetwork_for_ssid(ssid);
    if (!proxy_knownnetwork) {
        l_error("iwd_client: Known-network for ssid='%s' is not found during forget", ssid);
        forget_oper_error_and_destroy(oper, IWD_STATUS_NOT_FOUND);
        return false;
    }

    uint32_t callid = l_dbus_proxy_method_call(proxy_knownnetwork, "Forget",
                                               NULL, // No arguments needs setup into message
                                               forget_reply_handler,
                                               oper, // user_data
                                               forget_destroy_handler);
    if (callid == 0) {
        forget_oper_error_and_destroy(oper, IWD_STATUS_DBUS_SEND_FAILED);
        return false;
    }

    return true;
}
