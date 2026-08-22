/* K4510: no hypervisor. cpu65.c tests in_hypervisor in its IRQ/NMI gates. */
#ifndef K4510_HYPERVISOR_H
#define K4510_HYPERVISOR_H
#include <stdbool.h>
static const bool in_hypervisor = false;
#endif
