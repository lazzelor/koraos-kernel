// SPDX-License-Identifier: GPL-3.0-or-later
//
// The kinds of fault a program takes, from the exception syndrome (Arm ARM,
// ESR_EL1): pure, so the host tests check it (tests/host/fault_test.c).

#include "proc/fault.h"

#define EC(esr) (((esr) >> 26) & 0x3f)
#define EC_UNKNOWN 0x00     // an undefined instruction
#define EC_ILLEGAL 0x0e     // an illegal execution state
#define EC_SYSTEM 0x18      // MSR, MRS or a system instruction EL0 may not run
#define EC_IABT_LOWER 0x20  // an instruction abort from EL0
#define EC_PC_ALIGN 0x22
#define EC_DABT_LOWER 0x24  // a data abort from EL0
#define EC_SP_ALIGN 0x26
#define EC_BRK 0x3c

#define ISS_FNV (1u << 10)  // FAR not valid (aborts)
#define ISS_WNR (1u << 6)   // a write (data aborts)
#define DFSC(esr) ((esr) & 0x3f)
#define DFSC_ALIGNMENT 0x21

unsigned fault_kind(uint64_t esr) {
    switch (EC(esr)) {
    case EC_DABT_LOWER: {
        unsigned write = esr & ISS_WNR ? FAULT_WRITE : 0;
        return (DFSC(esr) == DFSC_ALIGNMENT ? FAULT_ALIGN : FAULT_DATA) | write;
    }
    case EC_IABT_LOWER:
        return FAULT_EXEC;
    case EC_PC_ALIGN:
    case EC_SP_ALIGN:
        return FAULT_ALIGN;
    case EC_UNKNOWN:
    case EC_SYSTEM:
        return FAULT_UNDEF;
    case EC_BRK:
        return FAULT_BREAK;
    default:
        return FAULT_OTHER;
    }
}

bool fault_has_addr(uint64_t esr) {
    switch (EC(esr)) {
    case EC_DABT_LOWER:
    case EC_IABT_LOWER:
        return !(esr & ISS_FNV);
    case EC_PC_ALIGN:
        return true;
    default:
        return false;
    }
}

const char *fault_name(unsigned kind) {
    switch (FAULT_KIND(kind)) {
    case FAULT_DATA: return "data abort";
    case FAULT_EXEC: return "instruction abort";
    case FAULT_ALIGN: return "alignment fault";
    case FAULT_UNDEF: return "undefined instruction";
    case FAULT_BREAK: return "breakpoint";
    default: return "fault";
    }
}
