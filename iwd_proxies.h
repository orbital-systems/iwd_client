//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#pragma once

#include <ell/ell.h>

void iwd_proxies_init(void);
void iwd_proxies_deinit(void);

void iwd_proxies_add(struct l_dbus_proxy *proxy);
void iwd_proxies_remove(struct l_dbus_proxy *proxy);

void iwd_proxies_clear(void);

struct l_dbus_proxy *iwd_proxies_get_device_by_name(const char *device_name);
struct l_dbus_proxy *iwd_proxies_get_device_by_path(const char *path);

struct l_dbus_proxy *iwd_proxies_get_agent_manager(void);

struct l_dbus_proxy *iwd_proxies_get_station(const char *path);
struct l_dbus_proxy *iwd_proxies_get_network(const char *path);
struct l_dbus_proxy *iwd_proxies_get_knownnetwork(const char *path);

struct l_dbus_proxy *iwd_proxies_get_station_for_device(const char *device_name);
const char *iwd_proxies_get_device_name_for_station(struct l_dbus_proxy *proxy);

struct l_dbus_proxy *iwd_proxies_get_network_for_ssid(const char *device_name, const char *ssid);
struct l_dbus_proxy *iwd_proxies_get_knownnetwork_for_ssid(const char *ssid);

typedef void (*iwd_proxies_foreach_func_t)(struct l_dbus_proxy *proxy, void *user_data);
void iwd_proxies_foreach_known_network(iwd_proxies_foreach_func_t func, void *user_data);
void iwd_proxies_foreach_station(iwd_proxies_foreach_func_t func, void *user_data);
