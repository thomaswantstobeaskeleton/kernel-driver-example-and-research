#pragma once

#include <ntddk.h>
#include <ntddmou.h>
#include <ntstrsafe.h>
#include <ntdef.h>
#include "includes.hpp"

/* Pattern scan - implemented in driver.cpp, used by nmi_spoof */
extern uintptr_t get_kernel_base(void);
extern intptr_t search_pattern(void* module_handle, const char* section, const char* signature_value);