#pragma once
/* Minimal declarations - entrypoint uses these to avoid pulling full debugger-detection.hpp */
bool is_debugger_or_tool_detected();
void init_custom_anti_debug();
