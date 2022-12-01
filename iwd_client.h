//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#pragma once

#include "iwd_network.h"
#include "iwd_status.h"

#include <ell/ell.h>

#include <stdbool.h>

typedef void (*iwd_client_ready_cb_t)(void);

typedef void (*iwd_client_scanning_updated_cb_t)(const char *device_name, bool scan_running, bool startup);
typedef void (*iwd_client_connected_ssid_updated_cb_t)(const char *device_name,
                                                       const char *ssid, // NULL means disconnected
                                                       bool startup);

bool iwd_client_init(struct l_dbus *dbus,
                     iwd_client_ready_cb_t ready_cb,
                     iwd_client_scanning_updated_cb_t scanning_updated_cb,
                     iwd_client_connected_ssid_updated_cb_t connected_ssid_updated_cb);
void iwd_client_deinit(struct l_dbus *dbus);

struct l_queue *iwd_client_known_networks(void); // Returns l_queue list of iwd_known_network_t

// Callbacks will always be called, even on any error.
// This means that early errors can have the callback executed even before the _async() call has returned.

typedef void (*iwd_client_scan_started_cb_t)(iwd_status_t status, void *user_data);
bool iwd_client_scan_start_async(const char *device_name,
                                 iwd_client_scan_started_cb_t scan_started_cb,
                                 void *user_data);

typedef void (*iwd_client_ordered_networks_done_cb_t)(iwd_status_t status, struct l_queue *networks, void *user_data);
bool iwd_client_ordered_networks_async(const char *device_name,
                                       iwd_client_ordered_networks_done_cb_t ordered_networks_done_cb,
                                       void *user_data);

typedef enum {
    IWD_CONNECT_NOT_HIDDEN = false,
    IWD_CONNECT_HIDDEN = true,
    IWD_CONNECT_AUTO_HIDDEN = 2, // Will try with hidden if SSID wasn't found as a Network
} iwd_connect_hidden_t;

typedef void (*iwd_client_connect_done_cb_t)(iwd_status_t status, void *user_data);
bool iwd_client_connect(const char *device_name,
                        const char *ssid,
                        const char *passphrase, // Allowed to be NULL for open wifi
                        iwd_connect_hidden_t hidden,
                        iwd_client_connect_done_cb_t connect_done_cb,
                        void *user_data);

typedef void (*iwd_client_forget_done_cb_t)(iwd_status_t status, void *user_data);
bool iwd_client_forget(const char *ssid,
                       iwd_client_forget_done_cb_t forget_done_cb,
                       void *user_data);
