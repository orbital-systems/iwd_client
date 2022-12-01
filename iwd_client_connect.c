//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_client.h"

#include "iwd_agent.h"
#include "iwd_proxies.h"
#include "iwd_util.h"

#include <assert.h>

// Need to keep track of current operation as Agent callback is used
typedef struct {
    iwd_client_connect_done_cb_t done_cb;
    void *user_data;
    char *network_path; // DBUS path of iwd network we are connecting to
    char *ssid; // // Our own copy of the ssid, used to setup a Hidden connect
    char *passphrase; // Our own copy of the passphrase to feed to the Agent
    bool hidden;
} connect_oper_t;

// Single operation can be running
static connect_oper_t *s_connect_oper;

static connect_oper_t *connect_oper_create(iwd_client_connect_done_cb_t done_cb,
                                           void *user_data,
                                           const char *network_path,
                                           const char *ssid,
                                           const char *passphrase,
                                           bool hidden)
{
    connect_oper_t *oper = l_new(connect_oper_t, 1);
    oper->done_cb = done_cb;
    oper->user_data = user_data;
    oper->network_path = l_strdup(network_path);
    oper->ssid = l_strdup(ssid);

    // Make sure passphrase is not NULL. If we want to connect to an open wifi the passphrase won't be used,
    // but we make sure we have something, just in case we get an agent call. Also makes free() easier.
    if (passphrase == NULL) {
        passphrase = "";
    }
    oper->passphrase = l_strdup(passphrase); // Make our own copy to feed to Agent later

    oper->hidden = hidden;

    return oper;
}

static void connect_oper_run_callback(connect_oper_t *oper, iwd_status_t status)
{
    assert(oper);
    assert(oper->done_cb);
    oper->done_cb(status, oper->user_data);
    oper->done_cb = NULL; // Mark it called
}

static void connect_oper_destroy(connect_oper_t *oper)
{
    assert(oper);

    // Callback has not been run yet. Operation was propably aborted
    if (oper->done_cb) {
        l_error("iwd_client: Connect was DBUS-aborted?");
        connect_oper_run_callback(oper, IWD_STATUS_DBUS_ABORTED);
    }

    l_free(oper->network_path);
    l_free(oper->ssid);
    l_free(oper->passphrase);
    l_free(oper);
}

static void connect_oper_error_and_destroy(connect_oper_t *oper, iwd_status_t status)
{
    connect_oper_run_callback(oper, status);
    connect_oper_destroy(oper);
}

static void connect_reply_handler(__attribute__((unused)) struct l_dbus_proxy *unused_proxy,
                                  struct l_dbus_message *msg,
                                  void *user_data)
{
    l_debug("iwd_client: connect_reply_handler user_data=%p", user_data);

    connect_oper_t *oper = (connect_oper_t *)user_data;
    assert(oper);
    assert(oper->done_cb);

    if (oper != s_connect_oper) { // Make sure its our oper
        l_warn("iwd_client: Got connect_reply_handler() for incorrect/old oper data?");
        return;
    }

    if (l_dbus_message_is_error(msg)) {
        const char *name = "";
        const char *text = "";
        (void)l_dbus_message_get_error(msg, &name, &text);

        l_error("iwd_client: Connect failed. name='%s' text='%s'", name, text);

        // Possible errors:
        // net.connman.iwd.Aborted
        // net.connman.iwd.Busy
        // net.connman.iwd.Failed   Given on wrong passphrase
        // net.connman.iwd.NoAgent
        // net.connman.iwd.NotSupported
        // net.connman.iwd.Timeout
        // net.connman.iwd.InProgress
        // net.connman.iwd.NotConfigured

        // Also (not documented)
        // net.connman.iwd.InvalidFormat  Given on too short (or long) passphrase (Must be 8-63 chars)

        iwd_status_t status = iwd_status_parse_dbus_error(name);
        connect_oper_run_callback(oper, status);
    }
    else {
        l_info("iwd_client: Connect was successful!");
        connect_oper_run_callback(oper, IWD_STATUS_SUCCESS);
    }
}

static void connect_destroy_handler(void *user_data)
{
    l_debug("iwd_client: destroy_handler user_data=%p", user_data);

    connect_oper_t *oper = (connect_oper_t *)user_data;
    assert(oper);

    if (oper != s_connect_oper) { // Make sure its our oper we destroy
        l_warn("iwd_client: Got connect_destroy_handler() for incorrect/old oper data?");
        return;
    }

    connect_oper_destroy(oper);
    s_connect_oper = NULL;
}

// Called by iwd_agent
// Internal function of iwd_client.c + iwd_client_connect.c
// Given as callback to iwd_agent.c in iwd_client_init().
const char *iwd_client_connect_agent_get_passphrase(const char *network_path);

const char *iwd_client_connect_agent_get_passphrase(const char *network_path)
{
    l_debug("iwd_client: connect_agent_get_passphrase() path=%s", network_path);

    if (s_connect_oper == NULL) {
        l_error("iwd_client: Got connect_agent_get_passphrase() without any CONNECT oper in progress");
        return NULL;
    }

    // Check that Agent asks for the network we are currently trying to connect to

    // If it is a hidden connect, we only have the first part of the path. That of the station.
    if (s_connect_oper->hidden) {
        if (strncmp(s_connect_oper->network_path, network_path, strlen(s_connect_oper->network_path)) != 0) {
            l_error("iwd_client: connect_agent_get_passphrase() asks for hidden network=%s, "
                    "but we have passphrase for a hidden network at station=%s",
                    network_path, s_connect_oper->network_path);
            return NULL;
        }
    }
    else {
        if (!streq(s_connect_oper->network_path, network_path)) {
            l_error("iwd_client: connect_agent_get_passphrase() asks for network=%s, "
                    "but we have passphrase for network=%s",
                    network_path, s_connect_oper->network_path);
            return NULL;
        }
    }

    return s_connect_oper->passphrase;
}

static void connect_setup_handler(struct l_dbus_message *message,
                                  void *user_data)
{
    connect_oper_t *oper = (connect_oper_t *)user_data;
    assert(oper);

    l_debug("iwd_client: connect_setup() ssid=%s hidden=%u", oper->ssid, oper->hidden);

    if (oper->hidden) {
        l_dbus_message_set_arguments(message, "s", oper->ssid);
    }
    else {
        l_dbus_message_set_arguments(message, ""); // Must set blank arguments
    }
}

bool iwd_client_connect(const char *device_name,
                        const char *ssid,
                        const char *passphrase, // Allowed to be NULL for open wifi
                        iwd_connect_hidden_t hidden,
                        iwd_client_connect_done_cb_t connect_done_cb,
                        void *user_data)
{
    assert(connect_done_cb);
    assert(ssid);

    l_info("iwd_client: Connecting to ssid='%s' on %s", ssid, device_name);

    if (!iwd_agent_is_registered()) {
        l_error("iwd_client: Agent is not registered. Trying to connect anyway");
    }

    bool do_hidden = false;
    struct l_dbus_proxy *proxy = NULL;
    if (hidden != IWD_CONNECT_HIDDEN) { // NotHidden + Auto
        proxy = iwd_proxies_get_network_for_ssid(device_name, ssid);
    }
    if (proxy == NULL) {
        switch (hidden) {
        case IWD_CONNECT_NOT_HIDDEN:
            l_error("iwd_client: Network for ssid='%s' is not found on '%s", ssid, device_name);
            connect_done_cb(IWD_STATUS_NETWORK_NOT_FOUND, user_data);
            return false;

        case IWD_CONNECT_AUTO_HIDDEN:
            l_info("iwd_client: Network for ssid='%s' is not found on '%s'. Trying with a Hidden connect",
                    ssid, device_name);
            // fall through

        case IWD_CONNECT_HIDDEN:
            proxy = iwd_proxies_get_station_for_device(device_name);
            if (!proxy) {
                l_error("iwd_client: Station for '%s' not found", device_name);
                connect_done_cb(IWD_STATUS_STATION_NOT_FOUND, user_data);
                return false;
            }
            do_hidden = true;
            break;
        }
    }

    if (s_connect_oper) {
        l_warn("iwd_client: Another Connect is already started. Overriding");
        // Always override the existing operation in order to not block new operations if the old failed somehow
        connect_oper_error_and_destroy(s_connect_oper, IWD_STATUS_CONNECT_OVERRIDEN);
        s_connect_oper = NULL;
    }

    connect_oper_t *oper = connect_oper_create(connect_done_cb, user_data,
                                               l_dbus_proxy_get_path(proxy),
                                               ssid, passphrase, do_hidden);
    s_connect_oper = oper;
    l_debug("iwd_client: Connect do_hidden=%u oper=%p path=%s interface=%s",
            do_hidden, oper, l_dbus_proxy_get_path(proxy), l_dbus_proxy_get_interface(proxy));
    uint32_t callid = l_dbus_proxy_method_call(proxy,
                                               do_hidden ? "ConnectHiddenNetwork" : "Connect",
                                               connect_setup_handler,
                                               connect_reply_handler,
                                               oper, // user_data
                                               connect_destroy_handler);
    if (callid == 0) {
        connect_oper_error_and_destroy(oper, IWD_STATUS_DBUS_SEND_FAILED);
        s_connect_oper = NULL;
        return false;
    }

    return true;
}
