//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_network.h"

#include "iwd_util.h"

//
// network
//

iwd_network_t *iwd_network_create(const char *name, const char *type, int16_t rssi100,
                                  bool connected, bool hidden,
                                  const char *path,
                                  const char *known_path) // Can be NULL
{
    iwd_network_t *network = l_new(iwd_network_t, 1);

    network->name = l_strdup(name);
    network->type = l_strdup(type);
    network->rssi100 = rssi100;
    network->connected = connected;
    network->hidden = hidden;
    network->path = l_strdup(path);
    network->known_path = known_path ? l_strdup(known_path) : NULL;

    return network;
}

void iwd_network_destroy(iwd_network_t *network)
{
    l_free(network->name);
    l_free(network->type);
    l_free(network->path);
    if (network->known_path) {
        l_free(network->known_path);
    }
    l_free(network);
}

static void iwd_network_destroy_void(void *data)
{
    iwd_network_destroy(data);
}

void iwd_network_list_destroy(struct l_queue *list)
{
    l_queue_destroy(list, iwd_network_destroy_void);
}

static bool iwd_network_match_by_known_path(const void *a, const void *b)
{
    const iwd_network_t *network = a;
    const char *known_path = b;

    return network->known_path && streq(network->known_path, known_path);
}

iwd_network_t *iwd_network_list_find_by_known_path(struct l_queue *list, const char *known_path)
{
    return l_queue_find(list, iwd_network_match_by_known_path, known_path);
}

//
// known_network
//

iwd_known_network_t *iwd_known_network_create(const char *name, const char *type, bool hidden, const char *path)
{
    iwd_known_network_t *known_network = l_new(iwd_known_network_t, 1);

    known_network->name = l_strdup(name);
    known_network->type = l_strdup(type);
    known_network->hidden = hidden;
    known_network->path = l_strdup(path);

    return known_network;
}

void iwd_known_network_destroy(iwd_known_network_t *known_network)
{
    l_free(known_network->name);
    l_free(known_network->type);
    l_free(known_network->path);
    l_free(known_network);
}

static void iwd_known_network_destroy_void(void *data)
{
    iwd_known_network_destroy(data);
}

void iwd_known_network_list_destroy(struct l_queue *list)
{
    l_queue_destroy(list, iwd_known_network_destroy_void);
}
