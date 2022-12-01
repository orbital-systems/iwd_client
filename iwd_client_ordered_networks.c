//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_client.h"

#include "iwd_proxies.h"

#include <assert.h>

typedef struct {
    iwd_client_ordered_networks_done_cb_t done_cb;
    void *user_data;
} ordered_networks_oper_t;

static ordered_networks_oper_t *ordered_networks_oper_create(iwd_client_ordered_networks_done_cb_t done_cb,
                                                             void *user_data)
{
    ordered_networks_oper_t *oper = l_new(ordered_networks_oper_t, 1);
    oper->done_cb = done_cb;
    oper->user_data = user_data;
    return oper;
}

static void ordered_networks_oper_run_callback(ordered_networks_oper_t *oper, iwd_status_t status,
                                               struct l_queue *networks)
{
    assert(oper);
    assert(oper->done_cb);
    oper->done_cb(status, networks, oper->user_data);
    oper->done_cb = NULL; // Mark it called
}

static void ordered_networks_oper_destroy(ordered_networks_oper_t *oper)
{
    assert(oper);

    // Callback has not been run yet. Operation was propably aborted
    if (oper->done_cb) {
        l_error("iwd_client: GetOrderedNetworks was DBUS-aborted?");
        ordered_networks_oper_run_callback(oper, IWD_STATUS_DBUS_ABORTED, NULL);
    }
    l_free(oper);
}

static void ordered_networks_oper_error_and_destroy(ordered_networks_oper_t *oper, iwd_status_t status)
{
    ordered_networks_oper_run_callback(oper, status, NULL);
    ordered_networks_oper_destroy(oper);
}

static void ordered_networks_reply_handler(__attribute__((unused)) struct l_dbus_proxy *unused_proxy,
                                           struct l_dbus_message *msg,
                                           void *user_data)
{
    ordered_networks_oper_t *oper = (ordered_networks_oper_t *)user_data;
    assert(oper);

    if (l_dbus_message_is_error(msg)) {
        l_error("iwd_client: GetOrderedNetworks failed");
        ordered_networks_oper_run_callback(oper, IWD_STATUS_DBUS_REPLY_ERROR, NULL);
        return;
    }

    struct l_dbus_message_iter array;
    if (!l_dbus_message_get_arguments(msg, "a(on)", &array)) {
        l_error("iwd_client: GetOrderedNetworks failed to parse message");
        ordered_networks_oper_run_callback(oper, IWD_STATUS_DBUS_PARSE_FAILED, NULL);
        return;
    }

    // Our output of list of iwd_network_t
    struct l_queue *list = l_queue_new();

    const char *path;
    int16_t rssi100;
    while (l_dbus_message_iter_next_entry(&array, &path, &rssi100)) {
        struct l_dbus_proxy *proxy = iwd_proxies_get_network(path);
        if (!proxy) {
            l_error("iwd_client: Can't find proxy for network '%s'", path);
            continue;
        }

        const char *name = NULL;
        if (!l_dbus_proxy_get_property(proxy, "Name", "s", &name)) {
            l_warn("iwd_client: Can't get 'Name' property of network at path='%s'", path);
            continue;
        }

        const char *type = NULL;
        if (!l_dbus_proxy_get_property(proxy, "Type", "s", &type)) {
            l_warn("iwd_client: Can't get 'Type' property of network at path='%s'", path);
            continue;
        }

        bool connected = false;
        if (!l_dbus_proxy_get_property(proxy, "Connected", "b", &connected)) {
            l_warn("iwd_client: Can't get 'Connected' property of network at path='%s'", path);
            continue;
        }

        const char *known_path = NULL;
        struct l_dbus_proxy *known_proxy = NULL;
        bool hidden = false;
        if (l_dbus_proxy_get_property(proxy, "KnownNetwork", "o", &known_path)) {
            known_proxy = iwd_proxies_get_knownnetwork(known_path);

            // This property only exists if it is Hidden.
            (void)l_dbus_proxy_get_property(proxy, "Hidden", "b", &connected);
        }

        l_debug("iwd_client: "
                "Network connected=%u known=%u rssi=%d type=%s hidden=%u ssid=%-32s path=%s known_path=%s",
                connected, !!known_proxy, rssi100, type, hidden, name, path, known_path);

        iwd_network_t *network = iwd_network_create(name, type, rssi100, connected, hidden, path, known_path);
        l_queue_push_tail(list, network);
    }

    ordered_networks_oper_run_callback(oper, IWD_STATUS_SUCCESS, list);
}

static void ordered_networks_destroy_handler(void *user_data)
{
    ordered_networks_oper_t *oper = (ordered_networks_oper_t *)user_data;
    ordered_networks_oper_destroy(oper);
}

bool iwd_client_ordered_networks_async(const char *device_name,
                                       iwd_client_ordered_networks_done_cb_t ordered_network_done_cb,
                                       void *user_data)
{
    l_debug("iwd_client: Calling GetOrderedNetworks on %s", device_name);

    ordered_networks_oper_t *oper = ordered_networks_oper_create(ordered_network_done_cb, user_data);

    struct l_dbus_proxy *proxy_station = iwd_proxies_get_station_for_device(device_name);
    if (!proxy_station) {
        l_error("iwd_client: Station for device='%s' is not found", device_name);
        ordered_networks_oper_error_and_destroy(oper, IWD_STATUS_STATION_NOT_FOUND);
        return false;
    }

    uint32_t callid = l_dbus_proxy_method_call(proxy_station, "GetOrderedNetworks",
                                               NULL, // No arguments needs setup into message
                                               ordered_networks_reply_handler,
                                               oper, // user_data
                                               ordered_networks_destroy_handler);
    if (callid == 0) {
        ordered_networks_oper_error_and_destroy(oper, IWD_STATUS_DBUS_SEND_FAILED);
        return false;
    }

    return true;
}
