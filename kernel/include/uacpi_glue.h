#pragma once

#include "common.h"

/* Set to true after uacpi_namespace_initialize() completes successfully. */
extern bool g_uacpi_ready;

/* Called from the SCI IRQ handler in idt.c instead of acpi_sci_handle().
 * Routes to the uACPI handler when ready, otherwise falls back to the
 * hand-rolled fixed-event handler. */
void uacpi_sci_dispatch(void);
