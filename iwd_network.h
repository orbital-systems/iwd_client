//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#pragma once

#include <ell/ell.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char *name;
    char *type;
    int16_t rssi100; // Access Point's signal strength expressed in 100 * dBm.
                     // Range of 0 (strongest signal) to -10000 (weakest signal).
    bool connected;
    bool hidden;
    char *path;
    char *known_path;
} iwd_network_t;

iwd_network_t *iwd_network_create(const char *name, const char *type, int16_t rssi100,
                                  bool connected, bool hidden,
                                  const char *path,
                                  const char *known_path); // Can be NULL
void iwd_network_destroy(iwd_network_t *network);

void iwd_network_list_destroy(struct l_queue *list);

typedef struct {
    char *name;
    char *type;
    bool hidden;
    char *path;
} iwd_known_network_t;

iwd_known_network_t *iwd_known_network_create(const char *name, const char *type, bool hidden, const char *path);
void iwd_known_network_destroy(iwd_known_network_t *known_network);

void iwd_known_network_list_destroy(struct l_queue *list);

iwd_network_t *iwd_network_list_find_by_known_path(struct l_queue *list, const char *known_path);
