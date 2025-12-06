#ifndef XEMU_QEMU_PLUGIN_EXT_H
#define XEMU_QEMU_PLUGIN_EXT_H

#include <qemu-plugin.h>

typedef void (*qemu_plugin_command_cb_t)(const char *args);

/**
 * qemu_plugin_register_command() - registers a debug monitor command
 * @name: String prefix used to route arguments to the callback.
 * @cb: The callback to be invoked when "plugin <name>" is entered in the debug
 *      monitor.
 */
QEMU_PLUGIN_API
void qemu_plugin_register_command(const char *name,
                                  qemu_plugin_command_cb_t cb);

/**
 * qemu_plugin_unregister_command() - unregisters a debug monitor command.
 * @name: String previously passed to qemu_plugin_register_command.
 */
QEMU_PLUGIN_API
void qemu_plugin_unregister_command(const char *name);

#endif // XEMU_QEMU_PLUGIN_EXT_H
