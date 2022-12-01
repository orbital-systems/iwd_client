//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_proxies.h"

#include "iwd_util.h"

static struct l_queue *s_iwd_proxies_list = NULL;

void iwd_proxies_init(void)
{
    s_iwd_proxies_list = l_queue_new();
}

void iwd_proxies_deinit(void)
{
    l_queue_destroy(s_iwd_proxies_list, NULL);
}

void iwd_proxies_add(struct l_dbus_proxy *proxy)
{
    l_queue_push_tail(s_iwd_proxies_list, proxy);
}

void iwd_proxies_remove(struct l_dbus_proxy *proxy)
{
    l_queue_remove(s_iwd_proxies_list, proxy);
}

void iwd_proxies_clear(void)
{
    l_queue_clear(s_iwd_proxies_list, NULL);
}

static struct l_dbus_proxy *iwd_proxies_find(const char *interface, const char *path)
{
    for (const struct l_queue_entry *entry = l_queue_get_entries(s_iwd_proxies_list); entry; entry = entry->next) {
        struct l_dbus_proxy *proxy = entry->data;

        if (streq(interface, l_dbus_proxy_get_interface(proxy)) && streq(path, l_dbus_proxy_get_path(proxy))) {
            return proxy;
        }
    }

    return NULL;
}

struct l_dbus_proxy *iwd_proxies_get_device_by_name(const char *device_name)
{
    for (const struct l_queue_entry *entry = l_queue_get_entries(s_iwd_proxies_list); entry; entry = entry->next) {
        struct l_dbus_proxy *proxy = entry->data;

        const char *interface = l_dbus_proxy_get_interface(proxy);
        if (!streq(interface, "net.connman.iwd.Device")) {
            continue;
        }

        const char *name;
        if (!l_dbus_proxy_get_property(proxy, "Name", "s", &name)) {
            continue;
        }

        if (streq(device_name, name)) {
            return proxy; // Found
        }
    }

    return NULL;
}

struct l_dbus_proxy *iwd_proxies_get_device_by_path(const char *path)
{
    return iwd_proxies_find("net.connman.iwd.Device", path);
}

struct l_dbus_proxy *iwd_proxies_get_agent_manager(void)
{
    return iwd_proxies_find("net.connman.iwd.AgentManager", "/net/connman/iwd");
}

struct l_dbus_proxy *iwd_proxies_get_station(const char *path)
{
    return iwd_proxies_find("net.connman.iwd.Station", path);
}

struct l_dbus_proxy *iwd_proxies_get_network(const char *path)
{
    return iwd_proxies_find("net.connman.iwd.Network", path);
}

struct l_dbus_proxy *iwd_proxies_get_knownnetwork(const char *path)
{
    return iwd_proxies_find("net.connman.iwd.KnownNetwork", path);
}

struct l_dbus_proxy *iwd_proxies_get_station_for_device(const char *device_name)
{
    struct l_dbus_proxy *proxy_device = iwd_proxies_get_device_by_name(device_name);
    if (!proxy_device) {
        return NULL;
    }

    const char *path_device = l_dbus_proxy_get_path(proxy_device);
    struct l_dbus_proxy *proxy_station = iwd_proxies_get_station(path_device);
    return proxy_station;
}

const char *iwd_proxies_get_device_name_for_station(struct l_dbus_proxy *proxy)
{
    const char *path = l_dbus_proxy_get_path(proxy);
    struct l_dbus_proxy *proxy_device = iwd_proxies_get_device_by_path(path);
    if (!proxy_device) {
        return NULL;
    }

    const char *name;
    if (!l_dbus_proxy_get_property(proxy_device, "Name", "s", &name)) {
        return NULL;
    }

    return name;
}

struct l_dbus_proxy *iwd_proxies_get_network_for_ssid(const char *device_name, const char *ssid)
{
    struct l_dbus_proxy *device_proxy = iwd_proxies_get_device_by_name(device_name);
    if (!device_proxy) {
        return NULL;
    }

    const char *device_path = l_dbus_proxy_get_path(device_proxy);

    for (const struct l_queue_entry *entry = l_queue_get_entries(s_iwd_proxies_list); entry; entry = entry->next) {
        struct l_dbus_proxy *proxy = entry->data;

        // Is it a Network?
        if (!streq(l_dbus_proxy_get_interface(proxy), "net.connman.iwd.Network")) {
            continue;
        }

        // Does it have the correct SSID?
        const char *name;
        if (!l_dbus_proxy_get_property(proxy, "Name", "s", &name)) {
            continue;
        }
        if (!streq(ssid, name)) {
            continue;
        }

        // Is it on the correct device/interface?
        const char *network_device_path;
        if (!l_dbus_proxy_get_property(proxy, "Device", "o", &network_device_path)) {
            continue;
        }
        if (!streq(network_device_path, device_path)) {
            continue;
        }

        return proxy;
    }

    return NULL;
}

struct l_dbus_proxy *iwd_proxies_get_knownnetwork_for_ssid(const char *ssid)
{
    for (const struct l_queue_entry *entry = l_queue_get_entries(s_iwd_proxies_list); entry; entry = entry->next) {
        struct l_dbus_proxy *proxy = entry->data;

        if (!streq(l_dbus_proxy_get_interface(proxy), "net.connman.iwd.KnownNetwork")) {
            continue;
        }

        const char *name;
        if (!l_dbus_proxy_get_property(proxy, "Name", "s", &name)) {
            continue;
        }

        if (streq(ssid, name)) {
            return proxy;
        }
    }

    return NULL;
}

static void foreach_interface(const char *interface, iwd_proxies_foreach_func_t func, void *user_data)
{
    for (const struct l_queue_entry *entry = l_queue_get_entries(s_iwd_proxies_list); entry; entry = entry->next) {
        struct l_dbus_proxy *proxy = entry->data;

        if (!streq(l_dbus_proxy_get_interface(proxy), interface)) {
            continue;
        }

        func(proxy, user_data);
    }
}

void iwd_proxies_foreach_known_network(iwd_proxies_foreach_func_t func, void *user_data)
{
    foreach_interface("net.connman.iwd.KnownNetwork", func, user_data);
}

void iwd_proxies_foreach_station(iwd_proxies_foreach_func_t func, void *user_data)
{
    foreach_interface("net.connman.iwd.Station", func, user_data);
}
