#pragma once
/* WSK (Winsock Kernel) TCP server - IOCTL alternative.
 * Driver listens on loopback; usermode connects via Winsock. */

#include <ntifs.h>
#include "../flush_comm_config.h"

#if FLUSHCOMM_USE_WSK

/* Initialize WSK server on port (0 = use FLUSHCOMM_WSK_PORT or default 41891).
 * Returns STATUS_SUCCESS if listening. */
NTSTATUS WskServer_Init(USHORT port);

void WskServer_Shutdown(void);

/* Check if WSK transport is active (usermode can use socket instead of DeviceIoControl) */
bool WskServer_IsActive(void);

/* Get port in use (for usermode to connect) */
USHORT WskServer_GetPort(void);

#endif
