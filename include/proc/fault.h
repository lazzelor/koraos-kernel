// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A fault a program takes at EL0, as the syndrome (ESR_EL1) tells it: what
// the kernel reports when it ends the program, and what a fault handler gets
// (fault_handler, syscall 70; user/libk/koraos.h has the same values).

#include "common.h"

#define FAULT_DATA 1   // a load or store: nothing mapped there, or read-only
#define FAULT_EXEC 2   // an instruction fetched from where none may run
#define FAULT_ALIGN 3  // a misaligned PC, SP or access
#define FAULT_UNDEF 4  // an undefined instruction, or one EL0 may not run
#define FAULT_BREAK 5  // a BRK instruction
#define FAULT_OTHER 6
#define FAULT_WRITE 0x100  // added for a store
#define FAULT_KIND(kind) ((kind) & 0xff)

// The FAULT_ kind of syndrome `esr`, FAULT_WRITE added for a store.
unsigned fault_kind(uint64_t esr);

// Whether FAR holds the address that faulted: for data and instruction
// aborts (unless the syndrome says it is not valid) and a misaligned PC.
bool fault_has_addr(uint64_t esr);

// "data abort", "undefined instruction", ... for a report.
const char *fault_name(unsigned kind);
