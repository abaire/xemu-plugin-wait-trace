#ifndef XEMU_WAIT_TRACE_PLUGIN_TRACERINTERFACE_H
#define XEMU_WAIT_TRACE_PLUGIN_TRACERINTERFACE_H

#include <stdint.h>

//! Track entry into a function of interest.
void TracerPushEntry(const char *function, uint32_t object_param, uint32_t esp);

//! Track exit from a function of interest. The tracer entry is fuzzy matched
//! by comparing the ESP register.
void TracerPopEntry(const char *function, uint32_t esp);

//! Handles input from the debug monitor.
void TracerCmdCallback(const char *args);

#endif // XEMU_WAIT_TRACE_PLUGIN_TRACERINTERFACE_H
