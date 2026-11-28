// SPDX-License-Identifier: GPL-3.0-or-later
//
// libk code_sync: machine code a program wrote runs only once the data cache
// has written it out to where instructions are fetched from, and the
// instruction cache has dropped what it held there. EL0 may do both (and
// read the line sizes from CTR_EL0) because the kernel sets SCTLR_EL1.UCI
// and UCT (src/mm/mmu.c).

#include "koraos.h"

void code_sync(const void *addr, size_t length) {
    uint64_t ctr;
    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
    uintptr_t dline = (uintptr_t)4 << ((ctr >> 16) & 0xf);  // DminLine, in words
    uintptr_t iline = (uintptr_t)4 << (ctr & 0xf);          // IminLine
    uintptr_t start = (uintptr_t)addr, end = start + length;
    for (uintptr_t p = start & ~(dline - 1); p < end; p += dline) {
        asm volatile("dc cvau, %0" ::"r"(p) : "memory");
    }
    asm volatile("dsb ish" ::: "memory");
    for (uintptr_t p = start & ~(iline - 1); p < end; p += iline) {
        asm volatile("ic ivau, %0" ::"r"(p) : "memory");
    }
    asm volatile("dsb ish\n\tisb" ::: "memory");
}
