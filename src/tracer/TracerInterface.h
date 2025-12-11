#ifndef XEMU_WAIT_TRACE_PLUGIN_TRACERINTERFACE_H
#define XEMU_WAIT_TRACE_PLUGIN_TRACERINTERFACE_H

#include <stdint.h>

//! Track entry into a function of interest.
void TracerPushEntry(const char* function, uint32_t esp, size_t num_params,
                     const uint32_t* params);

//! Track exit from a function of interest. The tracer entry is fuzzy matched
//! by comparing the ESP register.
void TracerPopEntry(const char* function, uint32_t esp);

//! Handles input from the debug monitor.
void TracerCmdCallback(const char* args);

//! Increments the counter for the given function and params combination.
void TracerCount(const char* function, uint32_t esp, size_t num_params,
                 const uint32_t* params);

#endif  // XEMU_WAIT_TRACE_PLUGIN_TRACERINTERFACE_H
