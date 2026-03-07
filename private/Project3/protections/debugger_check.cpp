/* Defines is_debugger_or_tool_detected - isolates full debugger-detection.hpp include to this TU */
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "debugger-detection.hpp"

bool is_debugger_or_tool_detected() {
    if (nt_debugger_attached()) return true;
    if (debugger_window_present()) return true;
    if (kernel_debugger_attached()) return true;
    if (instrumentation_callback_set()) return true;
    if (text_section_tampered()) return true;
    return false;
}

void init_custom_anti_debug() {
    _init_custom_anti_debug_impl();
}
