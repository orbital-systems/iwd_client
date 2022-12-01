//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#pragma once

#include <ell/ell.h>

#include <stdbool.h>

typedef const char *(*iwd_agent_get_passphrase_cb_t)(const char *network_path);

bool iwd_agent_init(struct l_dbus *dbus, iwd_agent_get_passphrase_cb_t get_passphrase_cb);
void iwd_agent_deinit(struct l_dbus *dbus);

bool iwd_agent_manager_register_agent(void);
bool iwd_agent_manager_unregister_agent(void);

bool iwd_agent_is_registered(void);
