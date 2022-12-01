//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_client.h"

#include "iwd_agent.h"
#include "iwd_network.h"
#include "iwd_proxies.h"
#include "iwd_util.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

#include <ell/ell.h>

struct l_dbus_client *s_client;

static iwd_client_ready_cb_t s_ready_cb;
static iwd_client_scanning_updated_cb_t s_scanning_updated_cb;
static iwd_client_connected_ssid_updated_cb_t s_connected_ssid_updated_cb;

static void update_property_scanning(const char *device_name, bool scanning, bool startup)
{
    assert(device_name);

    if (startup) {
        l_info("iwd_client: Startup: Scan %s on %s", scanning ? "running" : "not running", device_name);
    }
    else {
        l_info("iwd_client: Scan %s on %s", scanning ? "started" : "finished", device_name);
    }

    s_scanning_updated_cb(device_name, scanning, startup);
}

static void update_property_state(const char *device_name, const char *state, bool startup)
{
    assert(device_name);
    assert(state);

    // State can be one of:
    // "connected", "disconnected", "connecting", "disconnecting", "roaming" (from iwd docs)
    // And also "unknown" due to error inside iwd_client

    if (startup) {
        l_info("iwd_client: Startup: State on %s is '%s'", device_name, state);
    }
    else {
        l_info("iwd_client: State on %s changed to '%s'", device_name, state);
    }

    // No callback for this. Only logging.
    // Use connected_ssid instead which is more of an connected or disconnected only.
}

static void update_property_connected_network(const char *device_name,
                                              const char *connected_path, // NULL = disconnected
                                              bool startup)
{
    assert(device_name);

    const char *ssid = NULL;

    if (connected_path) {
        struct l_dbus_proxy *connected_proxy = iwd_proxies_get_network(connected_path);
        if (connected_proxy == NULL) {
            l_warn("iwd_client: Connected path=Can't find network proxy at path='%s'", connected_path);
            return;
        }

        if (!l_dbus_proxy_get_property(connected_proxy, "Name", "s", &ssid)) {
            l_warn("iwd_client: Can't get 'Name' property of connected network at path='%s'", connected_path);
            return;
        }
    }

    if (startup) {
        if (ssid) {
            l_info("iwd_client: Startup: Connected network on %s is CONNECTED to SSID='%s'", device_name, ssid);
        }
        else {
            l_info("iwd_client: Startup: Connected network on %s is DISCONNECTED", device_name);
        }
    }
    else {
        if (ssid) {
            l_info("iwd_client: Connected network on %s changed to CONNECTED to SSID='%s'", device_name, ssid);
        }
        else {
            l_info("iwd_client: Connected network on %s changed to DISCONNECTED", device_name);
        }
    }

    s_connected_ssid_updated_cb(device_name, ssid, startup);
}

// Called for each Station in client_ready(), eg. at startup
static void each_station_on_ready(struct l_dbus_proxy *proxy, __attribute__((unused)) void *user_data)
{
    // Grab properties for Station

    const char *device_name = iwd_proxies_get_device_name_for_station(proxy);
    if (device_name == NULL) {
        return;
    }

    // Scanning
    bool scanning;
    if (l_dbus_proxy_get_property(proxy, "Scanning", "b", &scanning)) {
        update_property_scanning(device_name, scanning, /*startup=*/true);
    }

    // State
    const char *state = "unknown";
    l_dbus_proxy_get_property(proxy, "State", "s", &state);
    update_property_state(device_name, state, /*startup=*/true);

    // ConnectedNetwork
    const char *connected_path = NULL;
    l_dbus_proxy_get_property(proxy, "ConnectedNetwork", "o", &connected_path);
    update_property_connected_network(device_name,
                                      connected_path, // If connected_path == NULL -> Disconnected
                                      /*startup=*/true);
}

static void client_connected(__attribute__((unused)) struct l_dbus *dbus, __attribute__((unused)) void *user_data)
{
    // Happens on connect. dbus client will now start getting all the proxies for us.
    // When done client_ready() is called.

    l_info("iwd_client: Connected to iwd");
    iwd_proxies_clear(); // Should already be cleared, but make sure
}

static void client_disconnected(__attribute__((unused)) struct l_dbus *dbus, __attribute__((unused)) void *user_data)
{
    l_error("iwd_client: Disconnected from iwd");
    iwd_proxies_clear();
}

static void client_ready(__attribute__((unused)) struct l_dbus_client *client, __attribute__((unused)) void *user_data)
{
    // All proxies are now created
    l_debug("iwd_client: Client is DBUS ready (proxies all created)");

    iwd_agent_manager_register_agent();

    // Grab station properties
    iwd_proxies_foreach_station(each_station_on_ready, NULL);

    // Run the ready callback
    s_ready_cb();
}

static void proxy_added(struct l_dbus_proxy *proxy, __attribute__((unused)) void *user_data)
{
    const char *interface = l_dbus_proxy_get_interface(proxy);
    const char *path = l_dbus_proxy_get_path(proxy);

    l_debug("iwd_client: proxy added: %s %s", path, interface);

    iwd_proxies_add(proxy);
}

static void proxy_removed(struct l_dbus_proxy *proxy, __attribute__((unused)) void *user_data)
{
    l_debug("iwd_client: proxy removed: %s %s", l_dbus_proxy_get_path(proxy),
            l_dbus_proxy_get_interface(proxy));

    iwd_proxies_remove(proxy);
}

static void property_changed(struct l_dbus_proxy *proxy, const char *name,
                struct l_dbus_message *msg, __attribute__((unused)) void *user_data)
{
    const char *path = l_dbus_proxy_get_path(proxy);
    const char *interface = l_dbus_proxy_get_interface(proxy);

    l_debug("iwd_client: property changed: %s (%s %s)", name, path, interface);

    if (!streq(interface, "net.connman.iwd.Station")) {
        // We are only interested in Station changes
        return;
    }

    const char *device_name = iwd_proxies_get_device_name_for_station(proxy);
    if (device_name == NULL) {
        l_warn("iwd_client: Got property update on unknown interface for station path=%s", path);
        return;
    }

    if (streq(name, "Scanning")) {
        bool scanning;
        if (!l_dbus_message_get_arguments(msg, "b", &scanning)) {
            return;
        }
        update_property_scanning(device_name, scanning, /*startup=*/false);
    }
    else if (streq(name, "State")) {
        const char *state = "unknown";
        l_dbus_message_get_arguments(msg, "s", &state);
        update_property_state(device_name, state, /*startup=*/false);
    }
    else if (streq(name, "ConnectedNetwork")) {
        const char *connected_path = NULL;
        l_dbus_message_get_arguments(msg, "o", &connected_path);
        update_property_connected_network(device_name,
                                          connected_path, // If connected_path == NULL -> Disconnected
                                          /*startup=*/false);
    }
}

//
// KnownNetworks
// This call is NOT async
//

static void each_known_network(struct l_dbus_proxy *proxy, void *user_data)
{
    struct l_queue *list = user_data;

    const char *path = l_dbus_proxy_get_path(proxy);

    const char *name = NULL;
    if (!l_dbus_proxy_get_property(proxy, "Name", "s", &name)) {
        l_warn("iwd_client: Can't get 'Name' property of known network at path='%s'", path);
        return;
    }

    const char *type = NULL;
    if (!l_dbus_proxy_get_property(proxy, "Type", "s", &type)) {
        l_warn("iwd_client: Can't get 'Type' property of known network at path='%s'", path);
        return;
    }

    bool hidden = false;
    if (!l_dbus_proxy_get_property(proxy, "Hidden", "b", &hidden)) {
       l_warn("iwd_client: Can't get 'Hidden' property of known network at path='%s'", path);
       return;
    }

    l_debug("iwd_client: KnownNetwork type=%s hidden=%u name=%-32s path=%s", type, hidden, name, path);

    iwd_known_network_t *known_network = iwd_known_network_create(name, type, hidden, path);
    l_queue_push_tail(list, known_network);
}

struct l_queue *iwd_client_known_networks(void)
{
    struct l_queue *list = l_queue_new();

    iwd_proxies_foreach_known_network(each_known_network, list);

    return list;
}

//
// Init/Deinit
//

// Exists in iwd_client_connect.c
const char *iwd_client_connect_agent_get_passphrase(const char *network_path);

bool iwd_client_init(struct l_dbus *dbus,
                     iwd_client_ready_cb_t ready_cb,
                     iwd_client_scanning_updated_cb_t scanning_updated_cb,
                     iwd_client_connected_ssid_updated_cb_t connected_ssid_updated_cb)
{
    assert(ready_cb != NULL);
    s_ready_cb = ready_cb;

    assert(scanning_updated_cb != NULL);
    s_scanning_updated_cb = scanning_updated_cb;

    assert(connected_ssid_updated_cb != NULL);
    s_connected_ssid_updated_cb = connected_ssid_updated_cb;


    iwd_proxies_init();

    iwd_agent_init(dbus, iwd_client_connect_agent_get_passphrase);

    // Connect to iwd
    s_client = l_dbus_client_new(dbus, "net.connman.iwd", "");

    l_dbus_client_set_connect_handler(s_client, client_connected, NULL, NULL);
    l_dbus_client_set_ready_handler(s_client, client_ready, NULL, NULL);
    l_dbus_client_set_disconnect_handler(s_client, client_disconnected, NULL, NULL);

    l_dbus_client_set_proxy_handlers(s_client, proxy_added, proxy_removed, property_changed, NULL, NULL);

    return true;
}

void iwd_client_deinit(struct l_dbus *dbus)
{
    // It is too late to run iwd_agent_manager_unregister_agent() here. The DBUS message will not go out.
    // We are destroying the iwd  DBUS client.
    // Besides. There is a bug in ELL where canceling any pending operation will crash.
    //iwd_agent_manager_unregister_agent();

    iwd_agent_deinit(dbus); // Only takes down the receiving agent object+interface

    l_dbus_client_destroy(s_client);

    // Must be after l_dbus_client_destroy() as it will call disconnect callback which will try to clear the iwd proxies
    iwd_proxies_deinit();
}
