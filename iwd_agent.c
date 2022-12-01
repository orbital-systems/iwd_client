//****************************************************************************
//    Copyright (C) 2022 Orbital Systems AB.
//    All rights reserved
//****************************************************************************
#include "iwd_agent.h"

#include "iwd_client.h"  // Get passphrase from connect
#include "iwd_proxies.h"

#include <ell/ell.h>

#include <assert.h>

#define IWD_AGENT_INTERFACE "net.connman.iwd.Agent"
#define LOCAL_AGENT_PATH "/iwd_agent" // No need to make a complicated unique path

static bool s_agent_registered;
static iwd_agent_get_passphrase_cb_t s_get_passphrase_cb;

// Used both with RegisterAgent and UnregisterAgent as they take the same argument (the object path)
static void agent_setup(struct l_dbus_message *message,
                        __attribute__((unused)) void *user_data)
{
    l_dbus_message_set_arguments(message, "o", LOCAL_AGENT_PATH);
}

// Used both with RegisterAgent and UnregisterAgent
static void agent_reply(__attribute__((unused)) struct l_dbus_proxy *proxy,
                        struct l_dbus_message *msg,
                        __attribute__((unused)) void *user_data)
{
    if (l_dbus_message_is_error(msg)) {
        const char *name = NULL;
        const char *text = NULL;
        (void)l_dbus_message_get_error(msg, &name, &text);
        l_error("iwd_agent: AgentManager RegisterAgent failed: name='%s' text='%s'", name, text);

        // So, what do we do if registering the agent fails?
        // Not much, any Connect() that requires a passphrase will fail.
        s_agent_registered = false;
    }
    else {
        l_debug("iwd_agent: Agent registered successfully");
        s_agent_registered = true;
    }
}

bool iwd_agent_manager_register_agent(void)
{
    struct l_dbus_proxy *proxy_manager = iwd_proxies_get_agent_manager();
    if (!proxy_manager) {
        l_error("iwd_agent: Can't get AgentManager proxy");
        return false;
    }

    uint32_t callid =  l_dbus_proxy_method_call(proxy_manager, "RegisterAgent",
                                               agent_setup,
                                               agent_reply,
                                               NULL, // user_data
                                               NULL); // Nothing to cleanup if proxy was destroyed during call
    if (callid == 0) {
        l_error("iwd_agent: Failed to call RegisterAgent over DBUS to iwd");
        return false;
    }

    return true;
}

bool iwd_agent_manager_unregister_agent(void)
{
    struct l_dbus_proxy *proxy_manager = iwd_proxies_get_agent_manager();
    if (!proxy_manager) {
        l_error("iwd_agent: Can't get AgentManager proxy");
        return false;
    }

    s_agent_registered = false;

    uint32_t callid = l_dbus_proxy_method_call(proxy_manager, "UnregisterAgent",
                                               agent_setup,
                                               agent_reply,
                                               NULL, // user_data
                                               NULL); // Nothing to cleanup if proxy was destroyed during call
    if (callid == 0) {
        l_error("iwd_agent: Failed to call UnregisterAgent over DBUS to iwd");
        return false;
    }

    return true;
}

bool iwd_agent_is_registered(void)
{
    return s_agent_registered;
}

static struct l_dbus_message *method_passphrase(__attribute__((unused)) struct l_dbus *dbus,
                                                struct l_dbus_message *message,
                                                __attribute__((unused)) void *user_data)
{
    const char *network_path = NULL;
    if (!l_dbus_message_get_arguments(message, "o", &network_path)) {
        l_error("iwd_agent: request_passphrase_method() No network path given");
        return l_dbus_message_new_error(message, IWD_AGENT_INTERFACE ".Error.Failed",
                                        "Error: Invalid argument");
   }

    const char *passphrase = s_get_passphrase_cb(network_path);
    if (passphrase == NULL) { // We have no passphrase for the network path prepared
        return l_dbus_message_new_error(message, IWD_AGENT_INTERFACE ".Error.Failed",
                                        "Error: Invalid network object");
    }

    struct l_dbus_message *reply = l_dbus_message_new_method_return(message);
    l_dbus_message_set_arguments(reply, "s", passphrase);

    return reply;
}

static struct l_dbus_message *method_release(__attribute__((unused)) struct l_dbus *dbus,
                                             struct l_dbus_message *message,
                                             __attribute__((unused)) void *user_data)
{
    // Called when iwd kicks us out as Agent. Shouldn't happen.
    l_error("iwd_agent: Got RELEASE call from iwd. Should not happen!");

    // Try register again
    iwd_agent_manager_register_agent();

    return l_dbus_message_new_method_return(message);
}

static struct l_dbus_message *method_cancel(__attribute__((unused)) struct l_dbus *dbus,
                                            struct l_dbus_message *message,
                                            __attribute__((unused)) void *user_data)
{
    // Called when iwd doesn't want a passphrase anymore. Should not really happen either we are a lib
    l_error("iwd_agent: Got CANCEL call from iwd. Ignoring!");

    // Just ignore the call. Any saved ssid will be overwritten by next connect attempt.
    // Any connect attempt will simply fail.

    return l_dbus_message_new_method_return(message);
}

static struct l_dbus_message *method_unsupported(__attribute__((unused)) struct l_dbus *dbus,
                                                 struct l_dbus_message *message,
                                                 __attribute__((unused)) void *user_data)
{
    l_error("iwd_agent: Got unsupported %s call", l_dbus_message_get_member(message));

    return l_dbus_message_new_error(message, IWD_AGENT_INTERFACE ".Error.Unsupported",
                                    "Error: Unsupported method");
}

static void agent_interface_setup(struct l_dbus_interface *interface)
{
    // Our only real interface:
    l_dbus_interface_method(interface, "RequestPassphrase", 0,
                            method_passphrase, "s", "o",
                            "passphrase", "network");

    // Should never be called
    l_dbus_interface_method(interface, "Release", 0, method_release, "", "");
    l_dbus_interface_method(interface, "Cancel", 0, method_cancel, "", "s", "reason");

    // Unsupported
    l_dbus_interface_method(interface, "RequestPrivateKeyPassphrase", 0,
                            method_unsupported,
                            "s", "o", "private_key_path", "network");

    l_dbus_interface_method(interface, "RequestUserNameAndPassword", 0,
                            method_unsupported,
                            "ss", "o", "user", "password", "network");

    l_dbus_interface_method(interface, "RequestUserPassword", 0,
                            method_unsupported, "s", "os",
                            "password", "network", "user");
}

bool iwd_agent_init(struct l_dbus *dbus, iwd_agent_get_passphrase_cb_t get_passphrase_cb)
{
    assert(get_passphrase_cb != NULL);

    s_get_passphrase_cb = get_passphrase_cb;

    if (!l_dbus_register_interface(dbus,
                                   IWD_AGENT_INTERFACE,
                                   agent_interface_setup,
                                   NULL, // No destroy handling
                                   false)) { // handle_old_style_properties (we don't have any properties)
        l_error("iwd_agent: Can't register Agent interface");
        return false;
    }

    if (!l_dbus_object_add_interface(dbus,
                                     LOCAL_AGENT_PATH,
                                     IWD_AGENT_INTERFACE,
                                     NULL)) { // user_data
        l_error("iwd_agent: Can't register the agent manager object");
        l_dbus_unregister_interface(dbus, IWD_AGENT_INTERFACE);
        return false;
    }

    return true;
}

void iwd_agent_deinit(struct l_dbus *dbus)
{
    l_dbus_unregister_object(dbus, LOCAL_AGENT_PATH);
    l_dbus_unregister_interface(dbus, IWD_AGENT_INTERFACE);
}
